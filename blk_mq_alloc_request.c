
blk_mq_dispatch_rq_list
	|-- blk_mq_get_driver_tag
			|-- 

/*
 * Returns true if we did some work AND can potentially do more.
 */
bool blk_mq_dispatch_rq_list(struct request_queue *q, struct list_head *list,
			     bool got_budget)
{
	struct blk_mq_hw_ctx *hctx;
	struct request *rq, *nxt;
	bool no_tag = false;
	int errors, queued;
	blk_status_t ret = BLK_STS_OK;

	if (list_empty(list))                                                                // 若list为empty
		return false;                                                                    // 返回false

	WARN_ON(!list_is_singular(list) && got_budget);                                      // 自检警告

	/*
	 * Now process all the entries, sending them to the driver.
	 */
	errors = queued = 0;                                                                 // 设置errors和queued为0
	do {
		struct blk_mq_queue_data bd;

		rq = list_first_entry(list, struct request, queuelist);                          // 从list取出rq

		hctx = rq->mq_hctx;                                                              // 重点：从rq中获取hctx
		if (!got_budget && !blk_mq_get_dispatch_budget(hctx))                            // 
			break;                                                                       // 退出循环

		if (!blk_mq_get_driver_tag(rq)) {                                                // 重点：从hctx->sched_tags映射到hctx->tags，成功返回true，失败返回false
			/*
			 * The initial allocation attempt failed, so we need to
			 * rerun the hardware queue when a tag is freed. The
			 * waitqueue takes care of that. If the queue is run
			 * before we add this entry back on the dispatch list,
			 * we'll re-run it below.
			 */
			if (!blk_mq_mark_tag_wait(hctx, rq)) {                                       // 该接口将调用blk_mq_get_driver_tag尝试获取tag，成功返回true，若获取不成功，对于hctx->flags & BLK_MQ_F_TAG_SHARED为true的request，则将hctx->dispatch_wait添加到等待队列bt_wait_ptr(sbq, hctx)->wait中
				blk_mq_put_dispatch_budget(hctx);                                        // 若q->mq_ops->put_budget不为NULL，则调用q->mq_ops->put_budget
				/*
				 * For non-shared tags, the RESTART check
				 * will suffice.
				 */
				if (hctx->flags & BLK_MQ_F_TAG_SHARED)                                   // 若hctx->flags包含BLK_MQ_F_TAG_SHARED
					no_tag = true;                                                       // 设置no_tag为true，表示hctx无空闲tag可用
				break;                                                                   // 退出循环，将不会执行queue_rq
			}
		}

		list_del_init(&rq->queuelist);                                                   // 将rq->queuelist从所在链表中移除，并初始化

		bd.rq = rq;                                                                      // 设置bd.rq为rq

		/*
		 * Flag last if we have no more requests, or if we have more
		 * but can't assign a driver tag to it.
		 */
		if (list_empty(list))                                                            // 若list为empty
			bd.last = true;                                                              // 设置bd.last为true
		else {
			nxt = list_first_entry(list, struct request, queuelist);                     // 获取下一个
			bd.last = !blk_mq_get_driver_tag(nxt);                                       // 
		}

		ret = q->mq_ops->queue_rq(hctx, &bd);                                            // 重点：向具体的block设备驱动下发request
		if (ret == BLK_STS_RESOURCE || ret == BLK_STS_DEV_RESOURCE) {                    // 若ret为BLK_STS_RESOURCE，或ret为BLK_STS_DEV_RESOURCE
			/*
			 * If an I/O scheduler has been configured and we got a
			 * driver tag for the next request already, free it
			 * again.
			 */
			if (!list_empty(list)) {
				nxt = list_first_entry(list, struct request, queuelist);
				blk_mq_put_driver_tag(nxt);
			}
			list_add(&rq->queuelist, list);
			__blk_mq_requeue_request(rq);                                                // 
			break;
		}

		if (unlikely(ret != BLK_STS_OK)) {
			errors++;
			blk_mq_end_request(rq, BLK_STS_IOERR);
			continue;
		}

		queued++;
	} while (!list_empty(list));                                                         // 若list不为empty

	hctx->dispatched[queued_to_index(queued)]++;                                         // 

	/*
	 * Any items that need requeuing? Stuff them into hctx->dispatch,
	 * that is where we will continue on next queue run.
	 */
	if (!list_empty(list)) {                                                             // 若list不为empty
		bool needs_restart;

		/*
		 * If we didn't flush the entire list, we could have told
		 * the driver there was more coming, but that turned out to
		 * be a lie.
		 */
		if (q->mq_ops->commit_rqs)                                                       // 
			q->mq_ops->commit_rqs(hctx);                                                 // 

		spin_lock(&hctx->lock);                                                          // 获取锁hctx->lock
		list_splice_init(list, &hctx->dispatch);                                         // 
		spin_unlock(&hctx->lock);                                                        // 释放锁hctx->lock

		/*
		 * If SCHED_RESTART was set by the caller of this function and
		 * it is no longer set that means that it was cleared by another
		 * thread and hence that a queue rerun is needed.
		 *
		 * If 'no_tag' is set, that means that we failed getting
		 * a driver tag with an I/O scheduler attached. If our dispatch
		 * waitqueue is no longer active, ensure that we run the queue
		 * AFTER adding our entries back to the list.
		 *
		 * If no I/O scheduler has been configured it is possible that
		 * the hardware queue got stopped and restarted before requests
		 * were pushed back onto the dispatch list. Rerun the queue to
		 * avoid starvation. Notes:
		 * - blk_mq_run_hw_queue() checks whether or not a queue has
		 *   been stopped before rerunning a queue.
		 * - Some but not all block drivers stop a queue before
		 *   returning BLK_STS_RESOURCE. Two exceptions are scsi-mq
		 *   and dm-rq.
		 *
		 * If driver returns BLK_STS_RESOURCE and SCHED_RESTART
		 * bit is set, run queue after a delay to avoid IO stalls
		 * that could otherwise occur if the queue is idle.
		 */
		needs_restart = blk_mq_sched_needs_restart(hctx);                                // 
		if (!needs_restart ||
		    (no_tag && list_empty_careful(&hctx->dispatch_wait.entry)))                  // 
			blk_mq_run_hw_queue(hctx, true);                                             // 
		else if (needs_restart && (ret == BLK_STS_RESOURCE))
			blk_mq_delay_run_hw_queue(hctx, BLK_MQ_RESOURCE_DELAY);                      // 

		blk_mq_update_dispatch_busy(hctx, true);                                         // 
		return false;                                                                    // 
	} else
		blk_mq_update_dispatch_busy(hctx, false);                                        // 

	/*
	 * If the host/device is unable to accept more work, inform the
	 * caller of that.
	 */
	if (ret == BLK_STS_RESOURCE || ret == BLK_STS_DEV_RESOURCE)                          // 若ret为BLK_STS_RESOURCE，或ret为BLK_STS_DEV_RESOURCE
		return false;                                                                    // 返回false

	return (queued + errors) != 0;                                                       // 
}

/*
 * Mark us waiting for a tag. For shared tags, this involves hooking us into
 * the tag wakeups. For non-shared tags, we can simply mark us needing a
 * restart. For both cases, take care to check the condition again after
 * marking us as waiting.
 */
static bool blk_mq_mark_tag_wait(struct blk_mq_hw_ctx *hctx,
				 struct request *rq)                                                     // 该接口将调用blk_mq_get_driver_tag尝试获取tag，成功返回true，若获取不成功，对于hctx->flags & BLK_MQ_F_TAG_SHARED为true的request，则将hctx->dispatch_wait添加到等待队列bt_wait_ptr(sbq, hctx)->wait中
{
	struct sbitmap_queue *sbq = &hctx->tags->bitmap_tags;                                // 获取hctx->tags->bitmap_tags作为sbq
	struct wait_queue_head *wq;
	wait_queue_entry_t *wait;
	bool ret;

	if (!(hctx->flags & BLK_MQ_F_TAG_SHARED)) {                                          // 若hctx->flags不包含BLK_MQ_F_TAG_SHARED
		blk_mq_sched_mark_restart_hctx(hctx);                                            // 设置hctx->state的BLK_MQ_S_SCHED_RESTART位

		/*
		 * It's possible that a tag was freed in the window between the
		 * allocation failure and adding the hardware queue to the wait
		 * queue.
		 *
		 * Don't clear RESTART here, someone else could have set it.
		 * At most this will cost an extra queue run.
		 */
		return blk_mq_get_driver_tag(rq);                                                // 从hctx->sched_tags映射到hctx->tags，成功返回true，失败返回false
	}

	wait = &hctx->dispatch_wait;                                                         // 获取wait
	if (!list_empty_careful(&wait->entry))                                               // 若wait->entry不为empty，表示wait已经在等待队列上了，因为该接口可能并发调用，因此可能其他task已经将hctx->dispatch_wait添加到了等待队列中了
		return false;                                                                    // 返回false

	wq = &bt_wait_ptr(sbq, hctx)->wait;                                                  // ？获取wq

	spin_lock_irq(&wq->lock);                                                            // 获取锁wq->lock
	spin_lock(&hctx->dispatch_wait_lock);                                                // 获取锁hctx->dispatch_wait_lock
	if (!list_empty(&wait->entry)) {                                                     // 若wait->entry不为empty，表示wait已经在等待队列上了，因为该接口可能并发调用，因此可能其他task已经将hctx->dispatch_wait添加到了等待队列中了
		spin_unlock(&hctx->dispatch_wait_lock);                                          // 释放锁hctx->dispatch_wait_lock
		spin_unlock_irq(&wq->lock);                                                      // 释放锁wq->lock
		return false;                                                                    // 返回false
	}

	atomic_inc(&sbq->ws_active);                                                         // sbq->ws_active自加
	wait->flags &= ~WQ_FLAG_EXCLUSIVE;                                                   // 清除wait->flags中的WQ_FLAG_EXCLUSIVE
	__add_wait_queue(wq, wait);                                                          // list_add(&wait->entry, &wq->head)，即将wait添加到等待队列wq上

	/*
	 * It's possible that a tag was freed in the window between the
	 * allocation failure and adding the hardware queue to the wait
	 * queue.
	 */
	ret = blk_mq_get_driver_tag(rq);                                                     // 从hctx->sched_tags映射到hctx->tags，成功返回true，失败返回false
	if (!ret) {                                                                          // 若ret为false
		spin_unlock(&hctx->dispatch_wait_lock);                                          // 释放锁hctx->dispatch_wait_lock
		spin_unlock_irq(&wq->lock);                                                      // 释放锁wq->lock
		return false;                                                                    // 返回false
	}

	/*
	 * We got a tag, remove ourselves from the wait queue to ensure
	 * someone else gets the wakeup.
	 */
	list_del_init(&wait->entry);                                                          // 将wait->entry从所在链表中移除，并初始化
	atomic_dec(&sbq->ws_active);                                                          // sbq->ws_active自减
	spin_unlock(&hctx->dispatch_wait_lock);                                               // 释放锁hctx->dispatch_wait_lock
	spin_unlock_irq(&wq->lock);                                                           // 释放锁wq->lock

	return true;                                                                          // 返回true
}

static int blk_mq_dispatch_wake(wait_queue_entry_t *wait, unsigned mode,
				int flags, void *key)
{
	struct blk_mq_hw_ctx *hctx;

	hctx = container_of(wait, struct blk_mq_hw_ctx, dispatch_wait);                      // 获取hctx

	spin_lock(&hctx->dispatch_wait_lock);                                                // 获取锁hctx->dispatch_wait_lock
	if (!list_empty(&wait->entry)) {                                                     // 若wait->entry不为empty，表示wait已经在等待队列上了
		struct sbitmap_queue *sbq;

		list_del_init(&wait->entry);                                                     // 将wait->entry从所在链表中移除，并初始化
		sbq = &hctx->tags->bitmap_tags;                                                  // 获取sbq
		atomic_dec(&sbq->ws_active);                                                     // sbq->ws_active自减
	}
	spin_unlock(&hctx->dispatch_wait_lock);                                              // 释放锁hctx->dispatch_wait_lock

	blk_mq_run_hw_queue(hctx, true);                                                     // ？
	return 1;                                                                            // 返回1
}

/*
 * Mark a hardware queue as needing a restart. For shared queues, maintain
 * a count of how many hardware queues are marked for restart.
 */
void blk_mq_sched_mark_restart_hctx(struct blk_mq_hw_ctx *hctx)
{
	if (test_bit(BLK_MQ_S_SCHED_RESTART, &hctx->state))
		return;

	set_bit(BLK_MQ_S_SCHED_RESTART, &hctx->state);
}
EXPORT_SYMBOL_GPL(blk_mq_sched_mark_restart_hctx);


bool blk_mq_get_driver_tag(struct request *rq)
{
	struct blk_mq_alloc_data data = {
		.q = rq->q,
		.hctx = rq->mq_hctx,                                                             // 重点：这是本接口的核心
		.flags = BLK_MQ_REQ_NOWAIT,
		.cmd_flags = rq->cmd_flags,
	};
	bool shared;

	if (rq->tag != -1)                                                                   // 重点：若rq->tag不为-1，表示rq->tag来自driver tag，不需要再获取driver tag了
		goto done;                                                                       // 跳转至done

	if (blk_mq_tag_is_reserved(data.hctx->sched_tags, rq->internal_tag))                 // 若rq->internal_tag < data.hctx->sched_tags->nr_reserved_tags
		data.flags |= BLK_MQ_REQ_RESERVED;                                               // 包含BLK_MQ_REQ_RESERVED到data.flags

	shared = blk_mq_tag_busy(data.hctx);                                                 // 
	rq->tag = blk_mq_get_tag(&data);                                                     // 重点：获取driver tag
	if (rq->tag >= 0) {                                                                  // 若rq->tag >= 0为true
		if (shared) {                                                                    // 
			rq->rq_flags |= RQF_MQ_INFLIGHT;                                             // 
			atomic_inc(&data.hctx->nr_active);                                           // 
		}
		data.hctx->tags->rqs[rq->tag] = rq;                                              // 重点：设置data.hctx->tags->rqs[rq->tag]为rq
	}

done:
	return rq->tag != -1;                                                                // 返回rq->tag != -1，即返回false表示失败，返回true表示成功
}

/**
 * blk_get_request - allocate a request
 * @q: request queue to allocate a request for
 * @op: operation (REQ_OP_*) and REQ_* flags, e.g. REQ_SYNC.
 * @flags: BLK_MQ_REQ_* flags, e.g. BLK_MQ_REQ_NOWAIT.
 */
struct request *blk_get_request(struct request_queue *q, unsigned int op,
				blk_mq_req_flags_t flags)
{
	struct request *req;

	WARN_ON_ONCE(op & REQ_NOWAIT);                                                       // 自检警告
	WARN_ON_ONCE(flags & ~(BLK_MQ_REQ_NOWAIT | BLK_MQ_REQ_PREEMPT));                     // 自检警告

	req = blk_mq_alloc_request(q, op, flags);                                            // 重点：获取req
	if (!IS_ERR(req) && q->mq_ops->initialize_rq_fn)                                     // 重点：若IS_ERR(req)为false，并且q->mq_ops->initialize_rq_fn不为NULL
		q->mq_ops->initialize_rq_fn(req);                                                // 重点：调用q->mq_ops->initialize_rq_fn，对于scsi框架，该接口非常重要

	return req;                                                                          // 返回req
}
EXPORT_SYMBOL(blk_get_request);

struct request *blk_mq_alloc_request(struct request_queue *q, unsigned int op,
		blk_mq_req_flags_t flags)
{
	struct blk_mq_alloc_data alloc_data = { .flags = flags, .cmd_flags = op };
	struct request *rq;
	int ret;

	ret = blk_queue_enter(q, flags);                                                     // 
	if (ret)                                                                             // 若ret不为0
		return ERR_PTR(ret);                                                             // 返回ERR_PTR(ret)

	rq = blk_mq_get_request(q, NULL, &alloc_data);                                       // 重点：获取rq
	blk_queue_exit(q);                                                                   // 

	if (!rq)                                                                             // 若rq为NULL
		return ERR_PTR(-EWOULDBLOCK);                                                    // 返回ERR_PTR(-EWOULDBLOCK)

	rq->__data_len = 0;
	rq->__sector = (sector_t) -1;
	rq->bio = rq->biotail = NULL;
	return rq;                                                                           // 返回rq
}
EXPORT_SYMBOL(blk_mq_alloc_request);

static struct request *blk_mq_get_request(struct request_queue *q,
					  struct bio *bio,
					  struct blk_mq_alloc_data *data)
{
	struct elevator_queue *e = q->elevator;                                              // 获取e
	struct request *rq;
	unsigned int tag;
	bool clear_ctx_on_error = false;                                                     // 设置clear_ctx_on_error为false
	u64 alloc_time_ns = 0;

	blk_queue_enter_live(q);

	/* alloc_time includes depth and tag waits */
	if (blk_queue_rq_alloc_time(q))
		alloc_time_ns = ktime_get_ns();

	data->q = q;                                                                         // 设置data->q为q
	if (likely(!data->ctx)) {                                                            // 若data->ctx为true
		data->ctx = blk_mq_get_ctx(q);                                                   // 获取ctx，保存在data->ctx中
		clear_ctx_on_error = true;                                                       // 设置clear_ctx_on_error为true
	}
	if (likely(!data->hctx))                                                             // 若data->hctx为true
		data->hctx = blk_mq_map_queue(q, data->cmd_flags,
						data->ctx);                                                      // 通过data->ctx映射hctx，保存到data->hctx
	if (data->cmd_flags & REQ_NOWAIT)                                                    // 若data->cmd_flags包含REQ_NOWAIT
		data->flags |= BLK_MQ_REQ_NOWAIT;                                                // 设置data->flags为BLK_MQ_REQ_NOWAIT

	if (e) {                                                                             // 若e不为NULL
		data->flags |= BLK_MQ_REQ_INTERNAL;                                              // 重点：这里可以了解到BLK_MQ_REQ_INTERNAL的用途（使用e时将带有BLK_MQ_REQ_INTERNAL标志）

		/*
		 * Flush requests are special and go directly to the
		 * dispatch list. Don't include reserved tags in the
		 * limiting, as it isn't useful.
		 */
		if (!op_is_flush(data->cmd_flags) &&
		    e->type->ops.limit_depth &&
		    !(data->flags & BLK_MQ_REQ_RESERVED))                                        // 
			e->type->ops.limit_depth(data->cmd_flags, data);                             // 
	} else {
		blk_mq_tag_busy(data->hctx);                                                     // 
	}

	tag = blk_mq_get_tag(data);                                                          // 重点：
	if (tag == BLK_MQ_TAG_FAIL) {                                                        // 
		if (clear_ctx_on_error)                                                          // 
			data->ctx = NULL;                                                            // 
		blk_queue_exit(q);                                                               // 
		return NULL;                                                                     // 
	}

	rq = blk_mq_rq_ctx_init(data, tag, data->cmd_flags, alloc_time_ns);                  // 重点：
	if (!op_is_flush(data->cmd_flags)) {                                                 // 
		rq->elv.icq = NULL;                                                              // 
		if (e && e->type->ops.prepare_request) {                                         // 
			if (e->type->icq_cache)                                                      // 
				blk_mq_sched_assign_ioc(rq);                                             // 

			e->type->ops.prepare_request(rq, bio);                                       // 
			rq->rq_flags |= RQF_ELVPRIV;                                                 // 
		}
	}
	data->hctx->queued++;                                                                // 
	return rq;                                                                           // 返回rq
}

#define DEFINE_SBQ_WAIT(name)							\
	struct sbq_wait name = {						\
		.sbq = NULL,							\
		.wait = {							\
			.private	= current,				\
			.func		= autoremove_wake_function,		\
			.entry		= LIST_HEAD_INIT((name).wait.entry),	\
		}								\
	}

enum {
	/* return when out of requests */
	BLK_MQ_REQ_NOWAIT	= (__force blk_mq_req_flags_t)(1 << 0),
	/* allocate from reserved pool */
	BLK_MQ_REQ_RESERVED	= (__force blk_mq_req_flags_t)(1 << 1),
	/* allocate internal/sched tag */
	BLK_MQ_REQ_INTERNAL	= (__force blk_mq_req_flags_t)(1 << 2),
	/* set RQF_PREEMPT */
	BLK_MQ_REQ_PREEMPT	= (__force blk_mq_req_flags_t)(1 << 3),
};

unsigned int blk_mq_get_tag(struct blk_mq_alloc_data *data)
{
	struct blk_mq_tags *tags = blk_mq_tags_from_data(data);                              // 重点：若data->flags & BLK_MQ_REQ_INTERNAL为true，则返回data->hctx->sched_tags，否则返回data->hctx->tags
	struct sbitmap_queue *bt;
	struct sbq_wait_state *ws;
	DEFINE_SBQ_WAIT(wait);                                                               // 定义struct sbq_wait实例，并初始化，其中包含struct wait_queue_entry实例
	unsigned int tag_offset;
	int tag;

	if (data->flags & BLK_MQ_REQ_RESERVED) {                                             // 若data->flags包含BLK_MQ_REQ_RESERVED
		if (unlikely(!tags->nr_reserved_tags)) {                                         // 若tags->nr_reserved_tags为0
			WARN_ON_ONCE(1);                                                             // 警告
			return BLK_MQ_TAG_FAIL;                                                      // 返回BLK_MQ_TAG_FAIL
		}
		bt = &tags->breserved_tags;                                                      // 设置bt为&tags->breserved_tags
		tag_offset = 0;                                                                  // 设置tag_offset为0
	} else {
		bt = &tags->bitmap_tags;                                                         // 设置bt为&tags->bitmap_tags
		tag_offset = tags->nr_reserved_tags;                                             // 设置tag_offset为tags->nr_reserved_tags
	}

	tag = __blk_mq_get_tag(data, bt);                                                    // 
	if (tag != -1)                                                                       // 若tag不为-1
		goto found_tag;                                                                  // 跳转至found_tag

	if (data->flags & BLK_MQ_REQ_NOWAIT)                                                 // 若data->flags包含BLK_MQ_REQ_NOWAIT
		return BLK_MQ_TAG_FAIL;                                                          // 返回BLK_MQ_TAG_FAIL

	ws = bt_wait_ptr(bt, data->hctx);                                                    // 
	do {
		struct sbitmap_queue *bt_prev;                                                   // 

		/*
		 * We're out of tags on this hardware queue, kick any
		 * pending IO submits before going to sleep waiting for
		 * some to complete.
		 */
		blk_mq_run_hw_queue(data->hctx, false);                                          // 

		/*
		 * Retry tag allocation after running the hardware queue,
		 * as running the queue may also have found completions.
		 */
		tag = __blk_mq_get_tag(data, bt);                                                // 
		if (tag != -1)                                                                   // 若tag不为-1
			break;                                                                       // 退出循环

		sbitmap_prepare_to_wait(bt, ws, &wait, TASK_UNINTERRUPTIBLE);                    // 

		tag = __blk_mq_get_tag(data, bt);                                                // 
		if (tag != -1)                                                                   // 若tag不为-1
			break;                                                                       // 退出循环

		bt_prev = bt;                                                                    // 设置bt_prev为bt
		io_schedule();                                                                   // 调度

		sbitmap_finish_wait(bt, ws, &wait);                                              // 

		data->ctx = blk_mq_get_ctx(data->q);                                             // 
		data->hctx = blk_mq_map_queue(data->q, data->cmd_flags,
						data->ctx);                                                      // 
		tags = blk_mq_tags_from_data(data);                                              // 重点：若data->flags & BLK_MQ_REQ_INTERNAL为true，则返回data->hctx->sched_tags，否则返回data->hctx->tags
		if (data->flags & BLK_MQ_REQ_RESERVED)                                           // 若data->flags包含BLK_MQ_REQ_RESERVED
			bt = &tags->breserved_tags;                                                  // 设置bt为&tags->breserved_tags
		else
			bt = &tags->bitmap_tags;                                                     // 设置bt为&tags->bitmap_tags

		/*
		 * If destination hw queue is changed, fake wake up on
		 * previous queue for compensating the wake up miss, so
		 * other allocations on previous queue won't be starved.
		 */
		if (bt != bt_prev)                                                               // 若bt不为bt_prev
			sbitmap_queue_wake_up(bt_prev);                                              // 

		ws = bt_wait_ptr(bt, data->hctx);                                                // 
	} while (1);

	sbitmap_finish_wait(bt, ws, &wait);                                                  // 

found_tag:
	return tag + tag_offset;                                                             // 返回tag + tag_offset
}

static inline struct blk_mq_tags *blk_mq_tags_from_data(struct blk_mq_alloc_data *data)
{
	if (data->flags & BLK_MQ_REQ_INTERNAL)                                               // 重点：若data->flags包含BLK_MQ_REQ_INTERNAL
		return data->hctx->sched_tags;                                                   // 重点：返回data->hctx->sched_tags

	return data->hctx->tags;                                                             // 重点：返回data->hctx->tags
}

static int __blk_mq_get_tag(struct blk_mq_alloc_data *data,
			    struct sbitmap_queue *bt)
{
	if (!(data->flags & BLK_MQ_REQ_INTERNAL) &&
	    !hctx_may_queue(data->hctx, bt))                                                 // 
		return -1;                                                                       // 
	if (data->shallow_depth)                                                             // 
		return __sbitmap_queue_get_shallow(bt, data->shallow_depth);                     // 
	else
		return __sbitmap_queue_get(bt);                                                  // 
}

static struct request *blk_mq_rq_ctx_init(struct blk_mq_alloc_data *data,
		unsigned int tag, unsigned int op, u64 alloc_time_ns)
{
	struct blk_mq_tags *tags = blk_mq_tags_from_data(data);                              // 重点：若data->flags & BLK_MQ_REQ_INTERNAL为true，则返回data->hctx->sched_tags，否则返回data->hctx->tags
	struct request *rq = tags->static_rqs[tag];                                          // 重点：设置rq为tags->static_rqs[tag]
	req_flags_t rq_flags = 0;

	if (data->flags & BLK_MQ_REQ_INTERNAL) {                                             // 若data->flags包含BLK_MQ_REQ_INTERNAL
		rq->tag = -1;                                                                    // 设置rq->tag为-1
		rq->internal_tag = tag;                                                          // 设置rq->internal_tag为tag
	} else {
		if (data->hctx->flags & BLK_MQ_F_TAG_SHARED) {                                   // 
			rq_flags = RQF_MQ_INFLIGHT;                                                  // 
			atomic_inc(&data->hctx->nr_active);                                          // 
		}
		rq->tag = tag;                                                                   // 设置rq->tag为tag
		rq->internal_tag = -1;                                                           // 设置rq->internal_tag为-1
		data->hctx->tags->rqs[rq->tag] = rq;                                             // 设置data->hctx->tags->rqs[rq->tag]为rq
	}

	/* csd/requeue_work/fifo_time is initialized before use */
	rq->q = data->q;                                                                     // 
	rq->mq_ctx = data->ctx;                                                              // 
	rq->mq_hctx = data->hctx;                                                            // 
	rq->rq_flags = rq_flags;                                                             // 
	rq->cmd_flags = op;                                                                  // 
	if (data->flags & BLK_MQ_REQ_PREEMPT)                                                // 
		rq->rq_flags |= RQF_PREEMPT;                                                     // 
	if (blk_queue_io_stat(data->q))                                                      // 
		rq->rq_flags |= RQF_IO_STAT;                                                     // 
	INIT_LIST_HEAD(&rq->queuelist);                                                      // 
	INIT_HLIST_NODE(&rq->hash);                                                          // 
	RB_CLEAR_NODE(&rq->rb_node);                                                         // 
	rq->rq_disk = NULL;                                                                  // 
	rq->part = NULL;                                                                     // 
#ifdef CONFIG_BLK_RQ_ALLOC_TIME
	rq->alloc_time_ns = alloc_time_ns;                                                   // 
#endif
	if (blk_mq_need_time_stamp(rq))                                                      // 
		rq->start_time_ns = ktime_get_ns();                                              // 
	else
		rq->start_time_ns = 0;                                                           // 
	rq->io_start_time_ns = 0;                                                            // 
	rq->stats_sectors = 0;                                                               // 
	rq->nr_phys_segments = 0;                                                            // 
#if defined(CONFIG_BLK_DEV_INTEGRITY)
	rq->nr_integrity_segments = 0;                                                       // 
#endif
	/* tag was already set */
	rq->extra_len = 0;                                                                   // 
	WRITE_ONCE(rq->deadline, 0);                                                         // 

	rq->timeout = 0;                                                                     // 

	rq->end_io = NULL;                                                                   // 
	rq->end_io_data = NULL;                                                              // 

	data->ctx->rq_dispatched[op_is_sync(op)]++;                                          // 
	refcount_set(&rq->ref, 1);                                                           // 
	return rq;                                                                           // 返回rq
}














