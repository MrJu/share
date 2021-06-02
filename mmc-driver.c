debug时中要的参考信息：
struct mmc_queue: mq->busy = true                              // 处于分发状态的mmc queue要设置mq->busy为true，这个成员仅在mmc_mq_queue_rq出现4次
struct mmc_queue: mq->in_flight[issue_type]






enum mmc_issue_type {
	MMC_ISSUE_SYNC,
	MMC_ISSUE_DCMD,
	MMC_ISSUE_ASYNC,
	MMC_ISSUE_MAX,
};

enum req_opf {
	/* read sectors from the device */
	REQ_OP_READ		= 0,                                 // MMC_ISSUE_ASYNC
	/* write sectors to the device */
	REQ_OP_WRITE		= 1,                             // MMC_ISSUE_ASYNC
	/* flush the volatile write cache */
	REQ_OP_FLUSH		= 2,                             // MMC_ISSUE_DCMD/MMC_ISSUE_SYNC
	/* discard sectors */
	REQ_OP_DISCARD		= 3,                             // MMC_ISSUE_SYNC
	/* securely erase sectors */
	REQ_OP_SECURE_ERASE	= 5,                             // MMC_ISSUE_SYNC
	/* reset a zone write pointer */
	REQ_OP_ZONE_RESET	= 6,                             // MMC_ISSUE_ASYNC
	/* write the same sector many times */
	REQ_OP_WRITE_SAME	= 7,                             // MMC_ISSUE_ASYNC
	/* reset all the zone present on the device */
	REQ_OP_ZONE_RESET_ALL	= 8,                         // MMC_ISSUE_ASYNC
	/* write the zero filled sector many times */
	REQ_OP_WRITE_ZEROES	= 9,                             // MMC_ISSUE_ASYNC

	/* SCSI passthrough using struct scsi_request */
	REQ_OP_SCSI_IN		= 32,                            // MMC_ISSUE_ASYNC
	REQ_OP_SCSI_OUT		= 33,                            // MMC_ISSUE_ASYNC
	/* Driver private requests */
	REQ_OP_DRV_IN		= 34,                            // MMC_ISSUE_SYNC
	REQ_OP_DRV_OUT		= 35,                            // MMC_ISSUE_SYNC

	REQ_OP_LAST,
};

enum mmc_issued {
	MMC_REQ_STARTED,
	MMC_REQ_BUSY,
	MMC_REQ_FAILED_TO_START,
	MMC_REQ_FINISHED,
};

enum mmc_issue_type mmc_issue_type(struct mmc_queue *mq, struct request *req)
{
	struct mmc_host *host = mq->card->host;

	if (mq->use_cqe)
		return mmc_cqe_issue_type(host, req);

	if (req_op(req) == REQ_OP_READ || req_op(req) == REQ_OP_WRITE)
		return MMC_ISSUE_ASYNC;

	return MMC_ISSUE_SYNC;
}

#define req_op(req) \
	((req)->cmd_flags & REQ_OP_MASK)

static inline bool mmc_cqe_can_dcmd(struct mmc_host *host)
{
	return host->caps2 & MMC_CAP2_CQE_DCMD;
}

static enum mmc_issue_type mmc_cqe_issue_type(struct mmc_host *host,
					      struct request *req)
{
	switch (req_op(req)) {
	case REQ_OP_DRV_IN:
	case REQ_OP_DRV_OUT:
	case REQ_OP_DISCARD:
	case REQ_OP_SECURE_ERASE:
		return MMC_ISSUE_SYNC;
	case REQ_OP_FLUSH:
		return mmc_cqe_can_dcmd(host) ? MMC_ISSUE_DCMD : MMC_ISSUE_SYNC;
	default:
		return MMC_ISSUE_ASYNC;
	}
}

static blk_status_t mmc_mq_queue_rq(struct blk_mq_hw_ctx *hctx,
				    const struct blk_mq_queue_data *bd)
{
	struct request *req = bd->rq;                                     // 从block data中取出request
	struct request_queue *q = req->q;                                 // 从request中取出request queue
	struct mmc_queue *mq = q->queuedata;                              // 从request queue中取出mmc queue
	struct mmc_card *card = mq->card;                                 // 从mmc queue中取出card
	struct mmc_host *host = card->host;                               // 从card中取出host
	enum mmc_issue_type issue_type;                                   // 定义好issue type变量
	enum mmc_issued issued;                                           // 分发结果
	bool get_card, cqe_retune_ok;
	int ret;

	if (mmc_card_removed(mq->card)) {                                 // 如果card已被移除
		req->rq_flags |= RQF_QUIET;                                   // 添加RQF_QUIET到req->rq_flags
		return BLK_STS_IOERR;                                         // 返回BLK_STS_IOERR
	}

	issue_type = mmc_issue_type(mq, req);                             // 确定issue type

	spin_lock_irq(&mq->lock);                                         // 获取锁

	if (mq->recovery_needed || mq->busy) {                            // mq->recovery_needed || mq->busy
		spin_unlock_irq(&mq->lock);                                   // 释放锁
		return BLK_STS_RESOURCE;                                      // 返回BLK_STS_RESOURCE
	}

	switch (issue_type) {                                             // 根据分发类型确定执行流程
	case MMC_ISSUE_DCMD:

		if (mmc_cqe_dcmd_busy(mq)) {
			mq->cqe_busy |= MMC_CQE_DCMD_BUSY;
			spin_unlock_irq(&mq->lock);
			return BLK_STS_RESOURCE;
		}
		break;
	case MMC_ISSUE_ASYNC:                                             // 异步分发
		break;
	default:                                                          // default为同步分发
		/*
		 * Timeouts are handled by mmc core, and we don't have a host
		 * API to abort requests, so we can't handle the timeout anyway.
		 * However, when the timeout happens, blk_mq_complete_request()
		 * no longer works (to stop the request disappearing under us).
		 * To avoid racing with that, set a large timeout.
		 */
		req->timeout = 600 * HZ;                                      // 同步分发强调超时概念
		break;
	}

	/* Parallel dispatch of requests is not supported at the moment */
	mq->busy = true;                                                  // 处于分发过程的mmc queue，mq->busy将设置为true

	mq->in_flight[issue_type] += 1;                                   // 
	get_card = (mmc_tot_in_flight(mq) == 1);
	cqe_retune_ok = (mmc_cqe_qcnt(mq) == 1);

	spin_unlock_irq(&mq->lock);                                       // 释放锁

	if (!(req->rq_flags & RQF_DONTPREP)) {
		req_to_mmc_queue_req(req)->retries = 0;
		req->rq_flags |= RQF_DONTPREP;
	}

	if (get_card)
		mmc_get_card(card, &mq->ctx);

	if (mq->use_cqe) {
		host->retune_now = host->need_retune && cqe_retune_ok &&
				   !host->hold_retune;
	}

	blk_mq_start_request(req);                                       // 该接口标志传输即将将开始

	issued = mmc_blk_mq_issue_rq(mq, req);                           // 这个接口将进行数据的分发

	switch (issued) {                                                // 根据分发结果结果确定返回值
	case MMC_REQ_BUSY:                                               // MMC_REQ_BUSY
		ret = BLK_STS_RESOURCE;                                      // 返回结果BLK_STS_RESOURCE
		break;
	case MMC_REQ_FAILED_TO_START:                                    // MMC_REQ_FAILED_TO_START
		ret = BLK_STS_IOERR;                                         // 返回结果BLK_STS_IOERR
		break;
	default:
		ret = BLK_STS_OK;                                            // 如果不是上面两种情况则返回结果BLK_STS_OK
		break;
	}

	if (issued != MMC_REQ_STARTED) {                                 // 如果分发结果不是MMC_REQ_STARTED
		bool put_card = false;

		spin_lock_irq(&mq->lock);                                    // 获取锁
		mq->in_flight[issue_type] -= 1;
		if (mmc_tot_in_flight(mq) == 0)
			put_card = true;
		mq->busy = false;
		spin_unlock_irq(&mq->lock);                                  // 释放锁
		if (put_card)
			mmc_put_card(card, &mq->ctx);
	} else {                                                         // 这种情况是MMC_REQ_STARTED
		WRITE_ONCE(mq->busy, false);
	}

	return ret;                                                      // 返回分发的结果
}


enum mmc_issued mmc_blk_mq_issue_rq(struct mmc_queue *mq, struct request *req)
{
	struct mmc_blk_data *md = mq->blkdata;
	struct mmc_card *card = md->queue.card;
	struct mmc_host *host = card->host;
	int ret;

	ret = mmc_blk_part_switch(card, md->part_type);
	if (ret)
		return MMC_REQ_FAILED_TO_START;

	switch (mmc_issue_type(mq, req)) {                                               // 参考mmc_cqe_issue_type和mmc_issue_type
	case MMC_ISSUE_SYNC:
		ret = mmc_blk_wait_for_idle(mq, host);                                       // 等待mq->rw_wait状态
		if (ret)                                                                     // 不为零则是-EBUSY
			return MMC_REQ_BUSY;                                                     // 返回MMC_REQ_BUSY
		switch (req_op(req)) {
		case REQ_OP_DRV_IN:                                                          // Driver private requests
		case REQ_OP_DRV_OUT:                                                         // Driver private requests
			mmc_blk_issue_drv_op(mq, req);
			break;
		case REQ_OP_DISCARD:
			mmc_blk_issue_discard_rq(mq, req);
			break;
		case REQ_OP_SECURE_ERASE:
			mmc_blk_issue_secdiscard_rq(mq, req);
			break;
		case REQ_OP_FLUSH:
			mmc_blk_issue_flush(mq, req);
			break;
		default:
			WARN_ON_ONCE(1);
			return MMC_REQ_FAILED_TO_START;
		}
		return MMC_REQ_FINISHED;
	case MMC_ISSUE_DCMD:
	case MMC_ISSUE_ASYNC:
		switch (req_op(req)) {
		case REQ_OP_FLUSH:
			ret = mmc_blk_cqe_issue_flush(mq, req);
			break;
		case REQ_OP_READ:
		case REQ_OP_WRITE:
			if (mq->use_cqe)
				ret = mmc_blk_cqe_issue_rw_rq(mq, req);
			else
				ret = mmc_blk_mq_issue_rw_rq(mq, req);
			break;
		default:
			WARN_ON_ONCE(1);
			ret = -EINVAL;
		}
		if (!ret)
			return MMC_REQ_STARTED;
		return ret == -EBUSY ? MMC_REQ_BUSY : MMC_REQ_FAILED_TO_START;
	default:
		WARN_ON_ONCE(1);
		return MMC_REQ_FAILED_TO_START;
	}
}

/*
 * The non-block commands come back from the block layer after it queued it and
 * processed it with all other requests and then they get issued in this
 * function.
 */
static void mmc_blk_issue_drv_op(struct mmc_queue *mq, struct request *req)
{
	struct mmc_queue_req *mq_rq;
	struct mmc_card *card = mq->card;
	struct mmc_blk_data *md = mq->blkdata;
	struct mmc_blk_ioc_data **idata;
	bool rpmb_ioctl;
	u8 **ext_csd;
	u32 status;
	int ret;
	int i;

	mq_rq = req_to_mmc_queue_req(req);
	rpmb_ioctl = (mq_rq->drv_op == MMC_DRV_OP_IOCTL_RPMB);

	switch (mq_rq->drv_op) {
	case MMC_DRV_OP_IOCTL:
	case MMC_DRV_OP_IOCTL_RPMB:
		idata = mq_rq->drv_op_data;
		for (i = 0, ret = 0; i < mq_rq->ioc_count; i++) {
			ret = __mmc_blk_ioctl_cmd(card, md, idata[i]);
			if (ret)
				break;
		}
		/* Always switch back to main area after RPMB access */
		if (rpmb_ioctl)
			mmc_blk_part_switch(card, 0);
		break;
	case MMC_DRV_OP_BOOT_WP:
		ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BOOT_WP,
				 card->ext_csd.boot_ro_lock |
				 EXT_CSD_BOOT_WP_B_PWR_WP_EN,
				 card->ext_csd.part_time);
		if (ret)
			pr_err("%s: Locking boot partition ro until next power on failed: %d\n",
			       md->disk->disk_name, ret);
		else
			card->ext_csd.boot_ro_lock |=
				EXT_CSD_BOOT_WP_B_PWR_WP_EN;
		break;
	case MMC_DRV_OP_GET_CARD_STATUS:
		ret = mmc_send_status(card, &status);
		if (!ret)
			ret = status;
		break;
	case MMC_DRV_OP_GET_EXT_CSD:
		ext_csd = mq_rq->drv_op_data;
		ret = mmc_get_ext_csd(card, ext_csd);
		break;
	default:
		pr_err("%s: unknown driver specific operation\n",
		       md->disk->disk_name);
		ret = -EINVAL;
		break;
	}
	mq_rq->drv_op_result = ret;
	blk_mq_end_request(req, ret ? BLK_STS_IOERR : BLK_STS_OK);
}

static void mmc_blk_issue_discard_rq(struct mmc_queue *mq, struct request *req)
{
	struct mmc_blk_data *md = mq->blkdata;
	struct mmc_card *card = md->queue.card;
	unsigned int from, nr;
	int err = 0, type = MMC_BLK_DISCARD;
	blk_status_t status = BLK_STS_OK;

	if (!mmc_can_erase(card)) {
		status = BLK_STS_NOTSUPP;
		goto fail;
	}

	from = blk_rq_pos(req);
	nr = blk_rq_sectors(req);

	do {
		err = 0;
		if (card->quirks & MMC_QUIRK_INAND_CMD38) {
			err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					 INAND_CMD38_ARG_EXT_CSD,
					 card->erase_arg == MMC_TRIM_ARG ?
					 INAND_CMD38_ARG_TRIM :
					 INAND_CMD38_ARG_ERASE,
					 0);
		}
		if (!err)
			err = mmc_erase(card, from, nr, card->erase_arg);
	} while (err == -EIO && !mmc_blk_reset(md, card->host, type));
	if (err)
		status = BLK_STS_IOERR;
	else
		mmc_blk_reset_success(md, type);
fail:
	blk_mq_end_request(req, status);
}

static int mmc_blk_wait_for_idle(struct mmc_queue *mq, struct mmc_host *host)
{
	if (mq->use_cqe)
		return host->cqe_ops->cqe_wait_for_idle(host);

	return mmc_blk_rw_wait(mq, NULL);
}

static int cqhci_wait_for_idle(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	int ret;

	wait_event(cq_host->wait_queue, cqhci_is_idle(cq_host, &ret));

	return ret;
}

static int mmc_blk_rw_wait(struct mmc_queue *mq, struct request **prev_req)
{
	int err = 0;

	wait_event(mq->wait, mmc_blk_rw_wait_cond(mq, &err));

	/* Always complete the previous request if there is one */
	mmc_blk_mq_complete_prev_req(mq, prev_req);

	return err;
}

static bool mmc_blk_rw_wait_cond(struct mmc_queue *mq, int *err)
{
	unsigned long flags;
	bool done;

	/*
	 * Wait while there is another request in progress, but not if recovery
	 * is needed. Also indicate whether there is a request waiting to start.
	 */
	spin_lock_irqsave(&mq->lock, flags);
	if (mq->recovery_needed) {
		*err = -EBUSY;
		done = true;
	} else {
		done = !mq->rw_wait;
	}
	mq->waiting = !done;
	spin_unlock_irqrestore(&mq->lock, flags);

	return done;
}

static int mmc_blk_mq_issue_rw_rq(struct mmc_queue *mq,
				  struct request *req)
{
	struct mmc_queue_req *mqrq = req_to_mmc_queue_req(req);
	struct mmc_host *host = mq->card->host;
	struct request *prev_req = NULL;
	int err = 0;

	mmc_blk_rw_rq_prep(mqrq, mq->card, 0, mq);                      // 这是非常关键的接口，确定要传输的数据

	mqrq->brq.mrq.done = mmc_blk_mq_req_done;

	mmc_pre_req(host, &mqrq->brq.mrq);

	err = mmc_blk_rw_wait(mq, &prev_req);
	if (err)
		goto out_post_req;

	mq->rw_wait = true;

	err = mmc_start_request(host, &mqrq->brq.mrq);                 // 这个接口中将调用host的接口启动传输

	if (prev_req)
		mmc_blk_mq_post_req(mq, prev_req);

	if (err)
		mq->rw_wait = false;

	/* Release re-tuning here where there is no synchronization required */
	if (err || mmc_host_done_complete(host))
		mmc_retune_release(host);

out_post_req:
	if (err)
		mmc_post_req(host, &mqrq->brq.mrq, err);

	return err;
}

int mmc_start_request(struct mmc_host *host, struct mmc_request *mrq)
{
	int err;

	init_completion(&mrq->cmd_completion);                        // 初始化mrq->cmd_completion

	mmc_retune_hold(host);

	if (mmc_card_removed(host->card))
		return -ENOMEDIUM;

	mmc_mrq_pr_debug(host, mrq, false);                           // 调试信息

	WARN_ON(!host->claimed);

	err = mmc_mrq_prep(host, mrq);
	if (err)
		return err;

	led_trigger_event(host->led, LED_FULL);
	__mmc_start_request(host, mrq);                                // 这个接口将调用host的接口启动传输

	return 0;
}
EXPORT_SYMBOL(mmc_start_request);

static void __mmc_start_request(struct mmc_host *host, struct mmc_request *mrq)
{
	int err;

	/* Assumes host controller has been runtime resumed by mmc_claim_host */
	err = mmc_retune(host);
	if (err) {
		mrq->cmd->error = err;
		mmc_request_done(host, mrq);
		return;
	}

	/*
	 * For sdio rw commands we must wait for card busy otherwise some
	 * sdio devices won't work properly.
	 * And bypass I/O abort, reset and bus suspend operations.
	 */
	if (sdio_is_io_busy(mrq->cmd->opcode, mrq->cmd->arg) &&
	    host->ops->card_busy) {
		int tries = 500; /* Wait aprox 500ms at maximum */

		while (host->ops->card_busy(host) && --tries)
			mmc_delay(1);

		if (tries == 0) {
			mrq->cmd->error = -EBUSY;
			mmc_request_done(host, mrq);
			return;
		}
	}

	if (mrq->cap_cmd_during_tfr) {
		host->ongoing_mrq = mrq;
		/*
		 * Retry path could come through here without having waiting on
		 * cmd_completion, so ensure it is reinitialised.
		 */
		reinit_completion(&mrq->cmd_completion);
	}

	trace_mmc_request_start(host, mrq);

	if (host->cqe_on)
		host->cqe_ops->cqe_off(host);

	host->ops->request(host, mrq);                                     // 在这里就调到了host的接口了
}

