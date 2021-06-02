static inline struct blk_flush_queue *
blk_get_flush_queue(struct request_queue *q, struct blk_mq_ctx *ctx)
{
	return blk_mq_map_queue(q, REQ_OP_FLUSH, ctx)->fq;
}


/**
 * blk_insert_flush - insert a new PREFLUSH/FUA request
 * @rq: request to insert
 *
 * To be called from __elv_add_request() for %ELEVATOR_INSERT_FLUSH insertions.
 * or __blk_mq_run_hw_queue() to dispatch request.
 * @rq is being submitted.  Analyze what needs to be done and put it on the
 * right queue.
 */
void blk_insert_flush(struct request *rq)
{
	struct request_queue *q = rq->q;
	unsigned long fflags = q->queue_flags;	/* may change, cache */
	unsigned int policy = blk_flush_policy(fflags, rq);
	struct blk_flush_queue *fq = blk_get_flush_queue(q, rq->mq_ctx);  // 先找到硬件队列上下文，在取出fq

	/*
	 * @policy now records what operations need to be done.  Adjust
	 * REQ_PREFLUSH and FUA for the driver.
	 */
	rq->cmd_flags &= ~REQ_PREFLUSH;
	if (!(fflags & (1UL << QUEUE_FLAG_FUA)))
		rq->cmd_flags &= ~REQ_FUA;

	/*
	 * REQ_PREFLUSH|REQ_FUA implies REQ_SYNC, so if we clear any
	 * of those flags, we have to set REQ_SYNC to avoid skewing
	 * the request accounting.
	 */
	rq->cmd_flags |= REQ_SYNC;                                       // 设置rq->cmd_flags包含REQ_SYNC

	/*
	 * An empty flush handed down from a stacking driver may
	 * translate into nothing if the underlying device does not
	 * advertise a write-back cache.  In this case, simply
	 * complete the request.
	 */
	if (!policy) {
		blk_mq_end_request(rq, 0);
		return;
	}

	BUG_ON(rq->bio != rq->biotail); /*assumes zero or single bio rq */

	/*
	 * If there's data but flush is not necessary, the request can be
	 * processed directly without going through flush machinery.  Queue
	 * for normal execution.
	 */
	if ((policy & REQ_FSEQ_DATA) &&
	    !(policy & (REQ_FSEQ_PREFLUSH | REQ_FSEQ_POSTFLUSH))) {
		blk_mq_request_bypass_insert(rq, false);                      // list_add_tail(&rq->queuelist, &hctx->dispatch)，false表示不调用blk_mq_run_hw_queue
		return;                                                       // 返回
	}

	/*
	 * @rq should go through flush machinery.  Mark it part of flush
	 * sequence and submit for further processing.
	 */
	memset(&rq->flush, 0, sizeof(rq->flush));
	INIT_LIST_HEAD(&rq->flush.list);
	rq->rq_flags |= RQF_FLUSH_SEQ;
	rq->flush.saved_end_io = rq->end_io; /* Usually NULL */

	rq->end_io = mq_flush_data_end_io;

	spin_lock_irq(&fq->mq_flush_lock);
	blk_flush_complete_seq(rq, fq, REQ_FSEQ_ACTIONS & ~policy, 0);
	spin_unlock_irq(&fq->mq_flush_lock);
}

static blk_qc_t blk_mq_make_request(struct request_queue *q, struct bio *bio)
{
	const int is_sync = op_is_sync(bio->bi_opf);                                              // 从bio->bi_opf判断是否为sync
	const int is_flush_fua = op_is_flush(bio->bi_opf);                                        // 从bio->bi_opf判断是否为flush操作
	struct blk_mq_alloc_data data = { .flags = 0};                                            // 定义临时变量data
	struct request *rq;                                                                       // 定义临时变量rq
	struct blk_plug *plug;                                                                    // 定义临时变量plug
	struct request *same_queue_rq = NULL;                                                     // 定义临时变量same_queue_rq
	unsigned int nr_segs;
	blk_qc_t cookie;

	blk_queue_bounce(q, &bio);                                                                // 
	__blk_queue_split(q, &bio, &nr_segs);                                                     // 

	if (!bio_integrity_prep(bio))                                                             // 
		return BLK_QC_T_NONE;

	if (!is_flush_fua && !blk_queue_nomerges(q) &&                                            // 
	    blk_attempt_plug_merge(q, bio, nr_segs, &same_queue_rq))
		return BLK_QC_T_NONE;

	if (blk_mq_sched_bio_merge(q, bio, nr_segs))
		return BLK_QC_T_NONE;

	rq_qos_throttle(q, bio);                                                                  // quality of service

	data.cmd_flags = bio->bi_opf;                                                             // 这个成员在dispatch时用到
	rq = blk_mq_get_request(q, bio, &data);                                                   // 获取request
	if (unlikely(!rq)) {                                                                      // 如果没有获取到
		rq_qos_cleanup(q, bio);                                                               // 
		if (bio->bi_opf & REQ_NOWAIT)                                                         // 
			bio_wouldblock_error(bio);                                                        // 
		return BLK_QC_T_NONE;                                                                 // 
	}

	trace_block_getrq(q, bio, bio->bi_opf);                                                   // 

	rq_qos_track(q, rq, bio);                                                                 // 

	cookie = request_to_qc_t(data.hctx, rq);                                                  // 

	blk_mq_bio_to_request(rq, bio, nr_segs);

	plug = blk_mq_plug(q, bio);                                                               // 
	if (unlikely(is_flush_fua)) {
		/* bypass scheduler for flush rq */
		blk_insert_flush(rq);                                                                 // 
		blk_mq_run_hw_queue(data.hctx, true);                                                 // 调用派发的顶级接口，async为true
	} else if (plug && (q->nr_hw_queues == 1 || q->mq_ops->commit_rqs ||
				!blk_queue_nonrot(q))) {
		/*
		 * Use plugging if we have a ->commit_rqs() hook as well, as
		 * we know the driver uses bd->last in a smart fashion.
		 *
		 * Use normal plugging if this disk is slow HDD, as sequential
		 * IO may benefit a lot from plug merging.
		 */
		unsigned int request_count = plug->rq_count;
		struct request *last = NULL;

		if (!request_count)
			trace_block_plug(q);
		else
			last = list_entry_rq(plug->mq_list.prev);

		if (request_count >= BLK_MAX_REQUEST_COUNT || (last &&
		    blk_rq_bytes(last) >= BLK_PLUG_FLUSH_SIZE)) {
			blk_flush_plug_list(plug, false);
			trace_block_plug(q);
		}

		blk_add_rq_to_plug(plug, rq);
	} else if (q->elevator) {                                                                 // 如果使用了调度器
		blk_mq_sched_insert_request(rq, false, true, true);                                   // 调用blk_mq_sched_insert_request
	} else if (plug && !blk_queue_nomerges(q)) {                                              // 因为使用了调度器，因此以下if判断都不考虑
		/*
		 * We do limited plugging. If the bio can be merged, do that.
		 * Otherwise the existing request in the plug list will be
		 * issued. So the plug list will have one request at most
		 * The plug list might get flushed before this. If that happens,
		 * the plug list is empty, and same_queue_rq is invalid.
		 */
		if (list_empty(&plug->mq_list))
			same_queue_rq = NULL;
		if (same_queue_rq) {
			list_del_init(&same_queue_rq->queuelist);
			plug->rq_count--;
		}
		blk_add_rq_to_plug(plug, rq);
		trace_block_plug(q);

		if (same_queue_rq) {
			data.hctx = same_queue_rq->mq_hctx;
			trace_block_unplug(q, 1, true);
			blk_mq_try_issue_directly(data.hctx, same_queue_rq,
					&cookie);
		}
	} else if ((q->nr_hw_queues > 1 && is_sync) ||
			!data.hctx->dispatch_busy) {
		blk_mq_try_issue_directly(data.hctx, rq, &cookie);
	} else {
		blk_mq_sched_insert_request(rq, false, true, true);
	}

	return cookie;
}


void blk_mq_sched_insert_request(struct request *rq, bool at_head,  // 这个接口在blk_execute_rq_nowait/blk_mq_requeue_work/blk_mq_make_request被调用
				 bool run_queue, bool async)                        // 在blk_mq_make_request调用中使用的参数为(rq, false, true, true)
{
	struct request_queue *q = rq->q;
	struct elevator_queue *e = q->elevator;
	struct blk_mq_ctx *ctx = rq->mq_ctx;
	struct blk_mq_hw_ctx *hctx = rq->mq_hctx;

	/* flush rq in flush machinery need to be dispatched directly */
	if (!(rq->rq_flags & RQF_FLUSH_SEQ) && op_is_flush(rq->cmd_flags)) {
		blk_insert_flush(rq);
		goto run;
	}

	WARN_ON(e && (rq->tag != -1));                                   // 如果使用了调度器并且rq->tag不为-1则警告

	if (blk_mq_sched_bypass_insert(hctx, !!e, rq))                   // 如果request被直接发送到hctx->dispatch中则返回true
		goto run;                                                    // 跳转到run

	if (e && e->type->ops.insert_requests) {                         // 如果使用调度器，并且调度器实现了insert_requests接口
		LIST_HEAD(list);                                             // 定义临时链表

		list_add(&rq->queuelist, &list);                             // 将request加入临时链表
		e->type->ops.insert_requests(hctx, &list, at_head);          // 将临时链表中的request插入到调度其中
		                                                             // blk_mq_make_request调用时传递的at_head为true
	} else {                                                         // 因为使用了调度器，因此不考虑这种情况
		spin_lock(&ctx->lock);
		__blk_mq_insert_request(hctx, rq, at_head);
		spin_unlock(&ctx->lock);
	}

run:
	if (run_queue)                                                   // 如果需要queue，blk_mq_make_request调用时传递的run_queue为true
		blk_mq_run_hw_queue(hctx, async);                            // 调用派发的顶级接口，blk_mq_make_request调用时传递的async为true
	                                                                 // async为true意味着派发将由kworker发起
}

static bool blk_mq_sched_bypass_insert(struct blk_mq_hw_ctx *hctx,   // 理解这个接口对理解dispatch非常有意义
				       bool has_sched,                               // 这个接口的意图是判断request是否可以跳过调度器直接插入到hctx->dispatch中
				       struct request *rq)
{
	/* dispatch flush rq directly */
	if (rq->rq_flags & RQF_FLUSH_SEQ) {                              // 如果rq->rq_flags包含RQF_FLUSH_SEQ
		spin_lock(&hctx->lock);                                      // 获取锁
		list_add(&rq->queuelist, &hctx->dispatch);                   // 这个request将直接发送到hctx->dispatch中，不经过调度器
		spin_unlock(&hctx->lock);                                    // 释放锁
		return true;                                                 // 返回true表示这个request将直接发送到hctx->dispatch中，不经过调度器
	}

	if (has_sched)                                                   // 如果使用调度器
		rq->rq_flags |= RQF_SORTED;                                  // 添加RQF_SORTED到rq->rq_flags

	return false;                                                    // 返回false表示没有将request直接发送到hctx->dispatch中
}

void blk_mq_sched_insert_requests(struct blk_mq_hw_ctx *hctx,      // 这个接口只有在blk_mq_flush_plug_list中调用
				  struct blk_mq_ctx *ctx,
				  struct list_head *list, bool run_queue_async)
{
	struct elevator_queue *e;
	struct request_queue *q = hctx->queue;

	/*
	 * blk_mq_sched_insert_requests() is called from flush plug
	 * context only, and hold one usage counter to prevent queue
	 * from being released.
	 */
	percpu_ref_get(&q->q_usage_counter);

	e = hctx->queue->elevator;
	if (e && e->type->ops.insert_requests)
		e->type->ops.insert_requests(hctx, list, false);
	else {
		/*
		 * try to issue requests directly if the hw queue isn't
		 * busy in case of 'none' scheduler, and this way may save
		 * us one extra enqueue & dequeue to sw queue.
		 */
		if (!hctx->dispatch_busy && !e && !run_queue_async) {
			blk_mq_try_issue_list_directly(hctx, list);
			if (list_empty(list))
				goto out;
		}
		blk_mq_insert_requests(hctx, ctx, list);
	}

	blk_mq_run_hw_queue(hctx, run_queue_async);
 out:
	percpu_ref_put(&q->q_usage_counter);
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

void blk_mq_sched_dispatch_requests(struct blk_mq_hw_ctx *hctx)               // 这个接口只做两件事：
{                                                                             // 派发hctx->dispatch中的request
	                                                                          // 派发调度器中的request
	struct request_queue *q = hctx->queue;
	struct elevator_queue *e = q->elevator;
	const bool has_sched_dispatch = e && e->type->ops.dispatch_request;       // 调度器是否定义了调度器并且调度器是否定义了ops.dispatch_request
	LIST_HEAD(rq_list);                                                       // 临时链表rq_list

	/* RCU or SRCU read lock is needed before checking quiesced flag */
	if (unlikely(blk_mq_hctx_stopped(hctx) || blk_queue_quiesced(q)))         // 这里的含义还有待研究
		return;

	hctx->run++;                                                              // 统计数据

	/*
	 * If we have previous entries on our dispatch list, grab them first for
	 * more fair dispatch.
	 */
	if (!list_empty_careful(&hctx->dispatch)) {                               // 参考注释理解这段代码的意图
		                                                                      // 如果此前下发request时queue_rq返回BLK_STS_RESOURCE或BLK_STS_DEV_RESOURCE
		                                                                      // 则request会被重新放入hctx->dispatch中
		                                                                      // 这里优先下发hctx->dispatch中的request
		spin_lock(&hctx->lock);                                               // 获取锁
		if (!list_empty(&hctx->dispatch))                                     // 如果hctx->dispatch非empty
			list_splice_init(&hctx->dispatch, &rq_list);                      // 将hctx->dispatch合并到rq_list
		spin_unlock(&hctx->lock);                                             // 释放锁
	}

	/*
	 * Only ask the scheduler for requests, if we didn't have residual
	 * requests from the dispatch list. This is to avoid the case where
	 * we only ever dispatch a fraction of the requests available because
	 * of low device queue depth. Once we pull requests out of the IO
	 * scheduler, we can no longer merge or sort them. So it's best to
	 * leave them there for as long as we can. Mark the hw queue as
	 * needing a restart in that case.
	 *
	 * We want to dispatch from the scheduler if there was nothing
	 * on the dispatch list or we were able to dispatch from the
	 * dispatch list.
	 */
	if (!list_empty(&rq_list)) {                                              // 如果rq_list非empty
		blk_mq_sched_mark_restart_hctx(hctx);                                 // set_bit(BLK_MQ_S_SCHED_RESTART, &hctx->state)
		if (blk_mq_dispatch_rq_list(q, &rq_list, false)) {                    // 先下发hctx->dispatch中的request，不需要从调度器中派发，因此直接调用blk_mq_dispatch_rq_list接口
			                                                                  // blk_mq_dispatch_rq_list是负责具体派发的入口函数
			if (has_sched_dispatch)                                           // 如果调度器ops定义了dispatch_request
				blk_mq_do_dispatch_sched(hctx);                               // 处理完hctx->dispatch中的request，再从调度其中取request下发
			else                                                              // deadline调度器ops定义了dispatch_request，因此不考虑这种情况
				blk_mq_do_dispatch_ctx(hctx);
		}
	} else if (has_sched_dispatch) {                                          // 如果调度器ops定义了dispatch_request
		blk_mq_do_dispatch_sched(hctx);
	} else if (hctx->dispatch_busy) {                                         // deadline调度器ops定义了dispatch_request，因此以下两种情况都不考虑
		/* dequeue request one by one from sw queue if queue is busy */
		blk_mq_do_dispatch_ctx(hctx);
	} else {
		blk_mq_flush_busy_ctxs(hctx, &rq_list);
		blk_mq_dispatch_rq_list(q, &rq_list, false);
	}
}

/*
 * Only SCSI implements .get_budget and .put_budget, and SCSI restarts
 * its queue by itself in its completion handler, so we don't need to
 * restart queue if .get_budget() returns BLK_STS_NO_RESOURCE.
 */
static void blk_mq_do_dispatch_sched(struct blk_mq_hw_ctx *hctx)
{
	struct request_queue *q = hctx->queue;
	struct elevator_queue *e = q->elevator;
	LIST_HEAD(rq_list);                                                   // 临时链表

	do {
		struct request *rq;

		if (e->type->ops.has_work && !e->type->ops.has_work(hctx))        // 另作分析
			break;                                                        // 退出循环

		if (!blk_mq_get_dispatch_budget(hctx))                            // 参考接口注释，这里暂不考虑SCSI，则blk_mq_get_dispatch_budget返回true
			break;                                                        // 退出循环

		rq = e->type->ops.dispatch_request(hctx);                         // 通过调度器获取要派发的request
		if (!rq) {                                                        // 如果rq为NULL
			blk_mq_put_dispatch_budget(hctx);                             // blk_mq_put_dispatch_budget
			break;                                                        // 退出循环
		}

		/*
		 * Now this rq owns the budget which has to be released
		 * if this rq won't be queued to driver via .queue_rq()
		 * in blk_mq_dispatch_rq_list().
		 */
		list_add(&rq->queuelist, &rq_list);                               // 将rq添加到rq_list中
	} while (blk_mq_dispatch_rq_list(q, &rq_list, true));
}

static inline bool blk_mq_get_dispatch_budget(struct blk_mq_hw_ctx *hctx)
{
	struct request_queue *q = hctx->queue;

	if (q->mq_ops->get_budget)
		return q->mq_ops->get_budget(hctx);
	return true;
}

static inline void blk_mq_put_dispatch_budget(struct blk_mq_hw_ctx *hctx)
{
	struct request_queue *q = hctx->queue;

	if (q->mq_ops->put_budget)
		q->mq_ops->put_budget(hctx);
}

static void __blk_mq_requeue_request(struct request *rq)
{
	struct request_queue *q = rq->q;

	blk_mq_put_driver_tag(rq);

	trace_block_rq_requeue(q, rq);
	rq_qos_requeue(q, rq);

	if (blk_mq_request_started(rq)) {
		WRITE_ONCE(rq->state, MQ_RQ_IDLE);
		rq->rq_flags &= ~RQF_TIMED_OUT;
		if (q->dma_drain_size && blk_rq_bytes(rq))
			rq->nr_phys_segments--;
	}
}

/*
 * Should only be used carefully, when the caller knows we want to
 * bypass a potential IO scheduler on the target device.
 */
void blk_mq_request_bypass_insert(struct request *rq, bool run_queue)
{
	struct blk_mq_hw_ctx *hctx = rq->mq_hctx;

	spin_lock(&hctx->lock);
	list_add_tail(&rq->queuelist, &hctx->dispatch);
	spin_unlock(&hctx->lock);

	if (run_queue)
		blk_mq_run_hw_queue(hctx, false);
}

static bool blk_mq_sched_bypass_insert(struct blk_mq_hw_ctx *hctx,
				       bool has_sched,
				       struct request *rq)
{
	/* dispatch flush rq directly */
	if (rq->rq_flags & RQF_FLUSH_SEQ) {
		spin_lock(&hctx->lock);
		list_add(&rq->queuelist, &hctx->dispatch);
		spin_unlock(&hctx->lock);
		return true;
	}

	if (has_sched)
		rq->rq_flags |= RQF_SORTED;

	return false;
}

/*
 * Check if any of the ctx, dispatch list or elevator
 * have pending work in this hardware queue.
 */
static bool blk_mq_hctx_has_pending(struct blk_mq_hw_ctx *hctx)
{
	return !list_empty_careful(&hctx->dispatch) ||
		sbitmap_any_bit_set(&hctx->ctx_map) ||
			blk_mq_sched_has_work(hctx);
}

struct blk_mq_queue_data {
	struct request *rq;
	bool last;
};

/**
 * list_is_singular - tests whether a list has just one entry.
 * @head: the list to test.
 */
static inline int list_is_singular(const struct list_head *head)
{
	return !list_empty(head) && (head->next == head->prev);
}

/*
 * For shared tag users, we track the number of currently active users
 * and attempt to provide a fair share of the tag depth for each of them.
 */
static inline bool hctx_may_queue(struct blk_mq_hw_ctx *hctx,
				  struct sbitmap_queue *bt)
{
	unsigned int depth, users;

	if (!hctx || !(hctx->flags & BLK_MQ_F_TAG_SHARED))
		return true;
	if (!test_bit(BLK_MQ_S_TAG_ACTIVE, &hctx->state))
		return true;

	/*
	 * Don't try dividing an ant
	 */
	if (bt->sb.depth == 1)
		return true;

	users = atomic_read(&hctx->tags->active_queues);
	if (!users)
		return true;

	/*
	 * Allow at least some tags
	 */
	depth = max((bt->sb.depth + users - 1) / users, 4U);
	return atomic_read(&hctx->nr_active) < depth;
}

/**
 * __sbitmap_queue_get() - Try to allocate a free bit from a &struct
 * sbitmap_queue with preemption already disabled.
 * @sbq: Bitmap queue to allocate from.
 *
 * Return: Non-negative allocated bit number if successful, -1 otherwise.
 */
int __sbitmap_queue_get(struct sbitmap_queue *sbq);

/**
 * __sbitmap_queue_get_shallow() - Try to allocate a free bit from a &struct
 * sbitmap_queue, limiting the depth used from each word, with preemption
 * already disabled.
 * @sbq: Bitmap queue to allocate from.
 * @shallow_depth: The maximum number of bits to allocate from a single word.
 * See sbitmap_get_shallow().
 *
 * If you call this, make sure to call sbitmap_queue_min_shallow_depth() after
 * initializing @sbq.
 *
 * Return: Non-negative allocated bit number if successful, -1 otherwise.
 */
int __sbitmap_queue_get_shallow(struct sbitmap_queue *sbq,
				unsigned int shallow_depth);


static int __blk_mq_get_tag(struct blk_mq_alloc_data *data,
			    struct sbitmap_queue *bt)
{
	if (!(data->flags & BLK_MQ_REQ_INTERNAL) &&                       // BLK_MQ_REQ_INTERNAL: allocate internal/sched tag
		                                                              // BLK_MQ_REQ_INTERNAL在blk_mq_get_request被添加
	    !hctx_may_queue(data->hctx, bt))
		return -1;
	if (data->shallow_depth)                                          // 在blk_mq_get_driver_tag中data->shallow_depth默认为0
		return __sbitmap_queue_get_shallow(bt, data->shallow_depth);  // Try to allocate a free bit from a &struct sbitmap_queue, limiting the depth used from each word, with preemption already disabled
	                                                                  // @shallow_depth: The maximum number of bits to allocate from a single word
	                                                                  // Return: Non-negative allocated bit number if successful, -1 otherwise
	else
		return __sbitmap_queue_get(bt);                               // Try to allocate a free bit from a &struct sbitmap_queue with preemption already disabled
	                                                                  // Return: Non-negative allocated bit number if successful, -1 otherwise
}

static inline struct blk_mq_tags *blk_mq_tags_from_data(struct blk_mq_alloc_data *data)
{
	if (data->flags & BLK_MQ_REQ_INTERNAL)
		return data->hctx->sched_tags;

	return data->hctx->tags;
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

static inline struct sbq_wait_state *bt_wait_ptr(struct sbitmap_queue *bt,
						 struct blk_mq_hw_ctx *hctx)
{
	if (!hctx)
		return &bt->ws[0];
	return sbq_wait_ptr(bt, &hctx->wait_index);
}

void sbitmap_prepare_to_wait(struct sbitmap_queue *sbq,
			     struct sbq_wait_state *ws,
			     struct sbq_wait *sbq_wait, int state)
{
	if (!sbq_wait->sbq) {
		atomic_inc(&sbq->ws_active);
		sbq_wait->sbq = sbq;
	}
	prepare_to_wait_exclusive(&ws->wait, &sbq_wait->wait, state);
}
EXPORT_SYMBOL_GPL(sbitmap_prepare_to_wait);

unsigned int blk_mq_get_tag(struct blk_mq_alloc_data *data)
{
	struct blk_mq_tags *tags = blk_mq_tags_from_data(data);           // data->flags & BLK_MQ_REQ_INTERNAL则return data->hctx->sched_tags，否则data->hctx->tags
	struct sbitmap_queue *bt;
	struct sbq_wait_state *ws;                                        // wait state
	DEFINE_SBQ_WAIT(wait);                                            // 定义sbitmap queue等待队列
	unsigned int tag_offset;
	int tag;

	if (data->flags & BLK_MQ_REQ_RESERVED) {                          // 如果tag < tags->nr_reserved_tags则flags包含BLK_MQ_REQ_RESERVED
		if (unlikely(!tags->nr_reserved_tags)) {                      // 如果tags->nr_reserved_tags为0
			WARN_ON_ONCE(1);                                          // 发出警告
			return BLK_MQ_TAG_FAIL;                                   // 返回BLK_MQ_TAG_FAIL
		}
		bt = &tags->breserved_tags;                                   // 取出sbitmap queue
		tag_offset = 0;                                               // 如果tag是reserved的则tag_offset设为0
	} else {
		bt = &tags->bitmap_tags;                                      // 取出sbitmap queue
		tag_offset = tags->nr_reserved_tags;                          // 如果tag不是reserved的则tags->nr_reserved_tags
	}

	tag = __blk_mq_get_tag(data, bt);                                 // 返回一个空闲的bit位
	if (tag != -1)                                                    // 不为-1表示找到空位
		goto found_tag;                                               // 跳转到found_tag

	if (data->flags & BLK_MQ_REQ_NOWAIT)                              // 到这里说明tag为-1，如果data->flags包含BLK_MQ_REQ_NOWAIT
		return BLK_MQ_TAG_FAIL;                                       // 直接返回BLK_MQ_TAG_FAIL

	ws = bt_wait_ptr(bt, data->hctx);                                 // 
	do {
		struct sbitmap_queue *bt_prev;

		/*
		 * We're out of tags on this hardware queue, kick any
		 * pending IO submits before going to sleep waiting for
		 * some to complete.
		 */
		blk_mq_run_hw_queue(data->hctx, false);                       // 参考注释理解调用这个接口的意图，false表示为sync

		/*
		 * Retry tag allocation after running the hardware queue,
		 * as running the queue may also have found completions.
		 */
		tag = __blk_mq_get_tag(data, bt);                             // 获取tag，参考注释
		if (tag != -1)                                                // 如果获取到了
			break;                                                    // 则退出循环

		sbitmap_prepare_to_wait(bt, ws, &wait, TASK_UNINTERRUPTIBLE); // 准备等待

		tag = __blk_mq_get_tag(data, bt);                             // 再次获取tag
		if (tag != -1)                                                // 如果获取到了
			break;                                                    // 则退出循环

		bt_prev = bt;
		io_schedule();                                                // 等待

		sbitmap_finish_wait(bt, ws, &wait);                           // 完成等待

		data->ctx = blk_mq_get_ctx(data->q);                          // 获取软件队列上下文
		data->hctx = blk_mq_map_queue(data->q, data->cmd_flags,       // 从软件队列上下文映射到硬件队列上下文
						data->ctx);
		tags = blk_mq_tags_from_data(data);                           // 取出queue的tags
		if (data->flags & BLK_MQ_REQ_RESERVED)                        // 如果data->flags包含BLK_MQ_REQ_RESERVED
			bt = &tags->breserved_tags;                               // 取出bt
		else
			bt = &tags->bitmap_tags;                                  // 取出bt

		/*
		 * If destination hw queue is changed, fake wake up on
		 * previous queue for compensating the wake up miss, so
		 * other allocations on previous queue won't be starved.
		 */
		if (bt != bt_prev)                                            // 参考注释
			sbitmap_queue_wake_up(bt_prev);

		ws = bt_wait_ptr(bt, data->hctx);
	} while (1);                                                      // 在循环中等待tag获取成功

	sbitmap_finish_wait(bt, ws, &wait);                               // 完成等待

found_tag:
	return tag + tag_offset;                                          // tag_offset为根据是否为reserved tag确定的数组分段起点
}

static inline bool blk_mq_tag_is_reserved(struct blk_mq_tags *tags,
					  unsigned int tag)
{
	return tag < tags->nr_reserved_tags;
}

static inline bool blk_mq_tag_busy(struct blk_mq_hw_ctx *hctx)
{
	if (!(hctx->flags & BLK_MQ_F_TAG_SHARED))
		return false;

	return __blk_mq_tag_busy(hctx);
}

/*
 * If a previously inactive queue goes active, bump the active user count.
 * We need to do this before try to allocate driver tag, then even if fail
 * to get tag when first time, the other shared-tag users could reserve
 * budget for it.
 */
bool __blk_mq_tag_busy(struct blk_mq_hw_ctx *hctx)
{
	if (!test_bit(BLK_MQ_S_TAG_ACTIVE, &hctx->state) &&
	    !test_and_set_bit(BLK_MQ_S_TAG_ACTIVE, &hctx->state))
		atomic_inc(&hctx->tags->active_queues);

	return true;
}

struct blk_mq_alloc_data {
	/* input parameter */
	struct request_queue *q;
	blk_mq_req_flags_t flags;
	unsigned int shallow_depth;
	unsigned int cmd_flags;

	/* input & output parameter */
	struct blk_mq_ctx *ctx;
	struct blk_mq_hw_ctx *hctx;
};

bool blk_mq_get_driver_tag(struct request *rq)
{
	struct blk_mq_alloc_data data = {                                     // 默认shallow_depth为0，ctx为NULL
		.q = rq->q,
		.hctx = rq->mq_hctx,
		.flags = BLK_MQ_REQ_NOWAIT,                                       // 默认的flags为BLK_MQ_REQ_NOWAIT
		.cmd_flags = rq->cmd_flags,
	};
	bool shared;

	if (rq->tag != -1)                                                    // 如果不为-1则表示已存在tag
		goto done;                                                        // 跳转到done

	if (blk_mq_tag_is_reserved(data.hctx->sched_tags, rq->internal_tag))  // return rq->internal_tag < data.hctx->sched_tags->nr_reserved_tags
		data.flags |= BLK_MQ_REQ_RESERVED;                                // 添加BLK_MQ_REQ_RESERVED到data.flags

	shared = blk_mq_tag_busy(data.hctx);
	rq->tag = blk_mq_get_tag(&data);                                      // 获取tag
	if (rq->tag >= 0) {                                                   // 说明tag获取成功
		if (shared) {
			rq->rq_flags |= RQF_MQ_INFLIGHT;
			atomic_inc(&data.hctx->nr_active);
		}
		data.hctx->tags->rqs[rq->tag] = rq;                               // 填充data.hctx->tags->rqs[rq->tag]
	}

done:
	return rq->tag != -1;                                                 // 获取tag成功，则不为-1
}

static inline unsigned int queued_to_index(unsigned int queued)
{
	if (!queued)
		return 0;

	return min(BLK_MQ_MAX_DISPATCH_ORDER - 1, ilog2(queued) + 1);
}

/*
 * Update dispatch busy with the Exponential Weighted Moving Average(EWMA):
 * - EWMA is one simple way to compute running average value
 * - weight(7/8 and 1/8) is applied so that it can decrease exponentially
 * - take 4 as factor for avoiding to get too small(0) result, and this
 *   factor doesn't matter because EWMA decreases exponentially
 */
static void blk_mq_update_dispatch_busy(struct blk_mq_hw_ctx *hctx, bool busy)
{
	unsigned int ewma;

	if (hctx->queue->elevator)                                     // 如果使用了调度器
		return;                                                    // 则直接返回

	ewma = hctx->dispatch_busy;

	if (!ewma && !busy)
		return;

	ewma *= BLK_MQ_DISPATCH_BUSY_EWMA_WEIGHT - 1;
	if (busy)
		ewma += 1 << BLK_MQ_DISPATCH_BUSY_EWMA_FACTOR;
	ewma /= BLK_MQ_DISPATCH_BUSY_EWMA_WEIGHT;

	hctx->dispatch_busy = ewma;
}

static inline bool blk_mq_sched_needs_restart(struct blk_mq_hw_ctx *hctx)
{
	return test_bit(BLK_MQ_S_SCHED_RESTART, &hctx->state);
}

/*
 * Returns true if we did some work AND can potentially do more.
 */
bool blk_mq_dispatch_rq_list(struct request_queue *q, struct list_head *list,
			     bool got_budget)                                          // 从blk_mq_do_dispatch_sched传递进来的got_budget为true
{                                                                          // 从blk_mq_sched_dispatch_requests传递进来的got_budget为false
	struct blk_mq_hw_ctx *hctx;
	struct request *rq, *nxt;
	bool no_tag = false;
	int errors, queued;
	blk_status_t ret = BLK_STS_OK;                                         // 默认返回值为BLK_STS_OK

	if (list_empty(list))                                                  // list为empty
		return false;                                                      // 返回false

	WARN_ON(!list_is_singular(list) && got_budget);                        // 如果list中不止有一个entry，并且got_budget为true则警告

	/*
	 * Now process all the entries, sending them to the driver.            // 留意，这里注释：sending them to the driver
	 */
	errors = queued = 0;                                                   // 初始化返回值
	do {
		struct blk_mq_queue_data bd;                                       // 定义block data，这个结构体仅有两个成员

		rq = list_first_entry(list, struct request, queuelist);            // 从list中取出request

		hctx = rq->mq_hctx;                                                // 取出硬件队列上下文
		if (!got_budget && !blk_mq_get_dispatch_budget(hctx))              // 不考虑SCSI，blk_mq_get_dispatch_budget返回true
			break;

		if (!blk_mq_get_driver_tag(rq)) {                                  // blk_mq_get_driver_tag：如果tag获取到则返回true，否则返回false
			                                                               // blk_mq_get_driver_tag还做了一个操作：data.hctx->tags->rqs[rq->tag] = rq
			                                                               // 该接口可能发生阻塞，这种情况将必成功获取到tag
			                                                               // 如果rq->cmd_flags & BLK_MQ_REQ_NOWAIT为true，则返回BLK_MQ_TAG_FAIL
			                                                               // 获取成功则将tag保存在request的tag成员中，返回true，获取失败返回false
			/*
			 * The initial allocation attempt failed, so we need to
			 * rerun the hardware queue when a tag is freed. The
			 * waitqueue takes care of that. If the queue is run
			 * before we add this entry back on the dispatch list,
			 * we'll re-run it below.
			 */
			if (!blk_mq_mark_tag_wait(hctx, rq)) {
				blk_mq_put_dispatch_budget(hctx);
				/*
				 * For non-shared tags, the RESTART check
				 * will suffice.
				 */
				if (hctx->flags & BLK_MQ_F_TAG_SHARED)
					no_tag = true;
				break;
			}
		}

		list_del_init(&rq->queuelist);                                     // 将request从list中移除

		bd.rq = rq;                                                        // 填充block data数据结构

		/*
		 * Flag last if we have no more requests, or if we have more
		 * but can't assign a driver tag to it.
		 */
		if (list_empty(list))                                              // 如果此时list为empty
			bd.last = true;                                                // bd.last设为true
		else {                                                             // 如果list不为empty
			nxt = list_first_entry(list, struct request, queuelist);       // 再取出一个request
			bd.last = !blk_mq_get_driver_tag(nxt);                         // 参考注释，如果还有request，但是无法为这个request获取到tag
			                                                               // 则将当前的block data标记为last
		}

		ret = q->mq_ops->queue_rq(hctx, &bd);                              // 调用gendisk驱动ops的queue_rq接口执行数据的下发
		if (ret == BLK_STS_RESOURCE || ret == BLK_STS_DEV_RESOURCE) {      // 如果返回值为BLK_STS_RESOURCE或BLK_STS_DEV_RESOURCE
			/*
			 * If an I/O scheduler has been configured and we got a
			 * driver tag for the next request already, free it
			 * again.
			 */
			if (!list_empty(list)) {                                       // 如果list不为empty
				nxt = list_first_entry(list, struct request, queuelist);   // 从list中取出request赋值给nxt
				blk_mq_put_driver_tag(nxt);
			}
			list_add(&rq->queuelist, list);                                // 重新将request添加到list中
			__blk_mq_requeue_request(rq);                                  // 将重新下发这个request，因此在这里完成清理
			break;                                                         // 退出循环
		}

		if (unlikely(ret != BLK_STS_OK)) {                                 // 如果queue_rq返回值不为BLK_STS_OK，表示出现error
			errors++;                                                      // errors自加
			blk_mq_end_request(rq, BLK_STS_IOERR);                         // 以BLK_STS_IOERR状态结束request
			continue;                                                      // 继续下一轮循环
		}

		queued++;                                                          // 到这里说明queue_rq下发成功，queued自加
	} while (!list_empty(list));                                           // 如果list不为empty，则继续下发

	hctx->dispatched[queued_to_index(queued)]++;

	/*
	 * Any items that need requeuing? Stuff them into hctx->dispatch,
	 * that is where we will continue on next queue run.
	 */
	if (!list_empty(list)) {                                              // 如果list不为empty
		bool needs_restart;

		/*
		 * If we didn't flush the entire list, we could have told
		 * the driver there was more coming, but that turned out to
		 * be a lie.
		 */
		if (q->mq_ops->commit_rqs)                                        // 如果commit_rqs有定义
			q->mq_ops->commit_rqs(hctx);                                  // 则执行commit_rqs

		spin_lock(&hctx->lock);                                           // 获取锁
		list_splice_init(list, &hctx->dispatch);                          // 将list合并到hctx->dispatch中
		spin_unlock(&hctx->lock);                                         // 释放锁

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
		needs_restart = blk_mq_sched_needs_restart(hctx);                // test_bit(BLK_MQ_S_SCHED_RESTART, &hctx->state)
		if (!needs_restart ||                                            // 如果不需要重启派发
		    (no_tag && list_empty_careful(&hctx->dispatch_wait.entry)))  // 或者，
			blk_mq_run_hw_queue(hctx, true);                             // 
		else if (needs_restart && (ret == BLK_STS_RESOURCE))             // 
			blk_mq_delay_run_hw_queue(hctx, BLK_MQ_RESOURCE_DELAY);      // 

		blk_mq_update_dispatch_busy(hctx, true);                         // 如果queue->elevator为true则什么也不做
		return false;                                                    // 返回false
	} else
		blk_mq_update_dispatch_busy(hctx, false);                        // 如果queue->elevator为true则什么也不做

	/*
	 * If the host/device is unable to accept more work, inform the
	 * caller of that.
	 */
		                                                                  // 参考注释理解
	if (ret == BLK_STS_RESOURCE || ret == BLK_STS_DEV_RESOURCE)           // queue_rq返回BLK_STS_RESOURCE或BLK_STS_DEV_RESOURCE
		return false;                                                     // 返回false

	return (queued + errors) != 0;                                        // 返回结果
}

struct elevator_mq_ops {
	int (*init_sched)(struct request_queue *, struct elevator_type *);
	void (*exit_sched)(struct elevator_queue *);
	int (*init_hctx)(struct blk_mq_hw_ctx *, unsigned int);
	void (*exit_hctx)(struct blk_mq_hw_ctx *, unsigned int);
	void (*depth_updated)(struct blk_mq_hw_ctx *);

	bool (*allow_merge)(struct request_queue *, struct request *, struct bio *);
	bool (*bio_merge)(struct blk_mq_hw_ctx *, struct bio *, unsigned int);
	int (*request_merge)(struct request_queue *q, struct request **, struct bio *);
	void (*request_merged)(struct request_queue *, struct request *, enum elv_merge);
	void (*requests_merged)(struct request_queue *, struct request *, struct request *);
	void (*limit_depth)(unsigned int, struct blk_mq_alloc_data *);
	void (*prepare_request)(struct request *, struct bio *bio);
	void (*finish_request)(struct request *);
	void (*insert_requests)(struct blk_mq_hw_ctx *, struct list_head *, bool);
	struct request *(*dispatch_request)(struct blk_mq_hw_ctx *);
	bool (*has_work)(struct blk_mq_hw_ctx *);
	void (*completed_request)(struct request *, u64);
	void (*requeue_request)(struct request *);
	struct request *(*former_request)(struct request_queue *, struct request *);
	struct request *(*next_request)(struct request_queue *, struct request *);
	void (*init_icq)(struct io_cq *);
	void (*exit_icq)(struct io_cq *);
};

static struct elevator_type mq_deadline = {
	.ops = {
		.insert_requests	= dd_insert_requests,
		.dispatch_request	= dd_dispatch_request,
		.prepare_request	= dd_prepare_request,
		.finish_request		= dd_finish_request,
		.next_request		= elv_rb_latter_request,
		.former_request		= elv_rb_former_request,
		.bio_merge		= dd_bio_merge,
		.request_merge		= dd_request_merge,
		.requests_merged	= dd_merged_requests,
		.request_merged		= dd_request_merged,
		.has_work		= dd_has_work,
		.init_sched		= dd_init_queue,
		.exit_sched		= dd_exit_queue,
	},

#ifdef CONFIG_BLK_DEBUG_FS
	.queue_debugfs_attrs = deadline_queue_debugfs_attrs,
#endif
	.elevator_attrs = deadline_attrs,
	.elevator_name = "mq-deadline",
	.elevator_alias = "deadline",
	.elevator_features = ELEVATOR_F_ZBD_SEQ_WRITE,
	.elevator_owner = THIS_MODULE,
};

struct deadline_data {
	/*
	 * run time data
	 */

	/*
	 * requests (deadline_rq s) are present on both sort_list and fifo_list
	 */
	struct rb_root sort_list[2];
	struct list_head fifo_list[2];

	/*
	 * next in sort order. read, write or both are NULL
	 */
	struct request *next_rq[2];
	unsigned int batching;		/* number of sequential requests made */
	unsigned int starved;		/* times reads have starved writes */

	/*
	 * settings that change how the i/o scheduler behaves
	 */
	int fifo_expire[2];
	int fifo_batch;
	int writes_starved;
	int front_merges;

	spinlock_t lock;
	spinlock_t zone_lock;
	struct list_head dispatch;
};

static void dd_insert_requests(struct blk_mq_hw_ctx *hctx,
			       struct list_head *list, bool at_head)
{
	struct request_queue *q = hctx->queue;
	struct deadline_data *dd = q->elevator->elevator_data;

	spin_lock(&dd->lock);
	while (!list_empty(list)) {
		struct request *rq;

		rq = list_first_entry(list, struct request, queuelist);
		list_del_init(&rq->queuelist);
		dd_insert_request(hctx, rq, at_head);
	}
	spin_unlock(&dd->lock);
}

/*
 * add rq to rbtree and fifo
 */
static void dd_insert_request(struct blk_mq_hw_ctx *hctx, struct request *rq,
			      bool at_head)
{
	struct request_queue *q = hctx->queue;
	struct deadline_data *dd = q->elevator->elevator_data;
	const int data_dir = rq_data_dir(rq);

	/*
	 * This may be a requeue of a write request that has locked its
	 * target zone. If it is the case, this releases the zone lock.
	 */
	blk_req_zone_write_unlock(rq);

	if (blk_mq_sched_try_insert_merge(q, rq))
		return;

	blk_mq_sched_request_inserted(rq);

	if (at_head || blk_rq_is_passthrough(rq)) {
		if (at_head)
			list_add(&rq->queuelist, &dd->dispatch);
		else
			list_add_tail(&rq->queuelist, &dd->dispatch);
	} else {
		deadline_add_rq_rb(dd, rq);

		if (rq_mergeable(rq)) {
			elv_rqhash_add(q, rq);
			if (!q->last_merge)
				q->last_merge = rq;
		}

		/*
		 * set expire time and add to fifo list
		 */
		rq->fifo_time = jiffies + dd->fifo_expire[data_dir];
		list_add_tail(&rq->queuelist, &dd->fifo_list[data_dir]);
	}
}

/*
 * One confusing aspect here is that we get called for a specific
 * hardware queue, but we may return a request that is for a
 * different hardware queue. This is because mq-deadline has shared
 * state for all hardware queues, in terms of sorting, FIFOs, etc.
 */
static struct request *dd_dispatch_request(struct blk_mq_hw_ctx *hctx)
{
	struct deadline_data *dd = hctx->queue->elevator->elevator_data;
	struct request *rq;

	spin_lock(&dd->lock);
	rq = __dd_dispatch_request(dd);
	spin_unlock(&dd->lock);

	return rq;
}

/*
 * deadline_dispatch_requests selects the best request according to
 * read/write expire, fifo_batch, etc
 */
static struct request *__dd_dispatch_request(struct deadline_data *dd)           // 参考注释理解接口功能
{
	struct request *rq, *next_rq;
	bool reads, writes;
	int data_dir;

	if (!list_empty(&dd->dispatch)) {                                            // 如果dd->dispatch不为空，可以看出优先派发dd->dispatch中的request
		rq = list_first_entry(&dd->dispatch, struct request, queuelist);         // 取出第一个rq
		list_del_init(&rq->queuelist);                                           // 将rq从dd->dispatch摘除
		goto done;                                                               // 跳转到done
	}                                                                            // 可以看出优先从dd->dispatch中派发request
                                                                                 // 如果执行下面的流程则说明dd->dispatch中没有request
	reads = !list_empty(&dd->fifo_list[READ]);                                   // dd->fifo_list[READ]不为empty则reads为true
	writes = !list_empty(&dd->fifo_list[WRITE]);                                 // dd->fifo_list[WRITE]不为empty则writes为true

	/*
	 * batches are currently reads XOR writes
	 */
	rq = deadline_next_request(dd, WRITE);                                       // 在dd->dispatch中没有request的情况下优先取WRITE的request
	if (!rq)                                                                     // 如果没有WRITE的request
		rq = deadline_next_request(dd, READ);                                    // 则取READ的request

	if (rq && dd->batching < dd->fifo_batch)                                     // 后面再分析
		/* we have a next request are still entitled to batch */
		goto dispatch_request;                                                   // 跳转到dispatch_request

	/*
	 * at this point we are not running a batch. select the appropriate
	 * data direction (read / write)
	 */

	if (reads) {                                                                 // 参考注释理解意图，如果d->fifo_list[READ]有request
		BUG_ON(RB_EMPTY_ROOT(&dd->sort_list[READ]));                             // d->fifo_list[READ]中有request则dd->sort_list[READ]不应当为empty

		if (deadline_fifo_request(dd, WRITE) &&                                  // 不考虑zoned支持，deadline_fifo_request作用是返回fifo中第一个request
		    (dd->starved++ >= dd->writes_starved))                               // 因为read优先派发，为防止write被饿死引入了dd->writes_starved
		                                                                         // dd->writes_starved默认设定为2
			                                                                     // 因此这个条件的含义是如果fifo中有WRITE request等待派发
			                                                                     // 并且WRITE派发即将达到writes_starved阈值
			goto dispatch_writes;                                                // 则跳转到dispatch_writes，派发WRITE request

		data_dir = READ;                                                         // 到这里说明WRITE没有饥饿，确定传输方向为READ

		goto dispatch_find_request;                                              // 跳转到dispatch_find_request
	}

	/*
	 * there are either no reads or writes have been starved
	 */
                                                                                 // 参考注释：there are either no reads or writes have been starved
	if (writes) {                                                                // 如果d->fifo_list[READ]有request
dispatch_writes:
		BUG_ON(RB_EMPTY_ROOT(&dd->sort_list[WRITE]));                            // dd->fifo_list[WRITE]中有request则dd->sort_list[WRITE]不应当为empty

		dd->starved = 0;                                                         // 即将dispatch WRITE request，因此这里重置dd->starved为0

		data_dir = WRITE;                                                        // 确定传输方向为WRITE

		goto dispatch_find_request;                                              // 跳转至dispatch_find_request
	}

	return NULL;                                                                 // 没有read和write request则返回NULL

dispatch_find_request:
	/*
	 * we are not running a batch, find best request for selected data_dir
	 */
	next_rq = deadline_next_request(dd, data_dir);
	if (deadline_check_fifo(dd, data_dir) || !next_rq) {
		/*
		 * A deadline has expired, the last request was in the other
		 * direction, or we have run out of higher-sectored requests.
		 * Start again from the request with the earliest expiry time.
		 */
		rq = deadline_fifo_request(dd, data_dir);
	} else {
		/*
		 * The last req was the same dir and we have a next request in
		 * sort order. No expired requests so continue on from here.
		 */
		rq = next_rq;
	}

	/*
	 * For a zoned block device, if we only have writes queued and none of
	 * them can be dispatched, rq will be NULL.
	 */
	if (!rq)
		return NULL;

	dd->batching = 0;

dispatch_request:
	/*
	 * rq is the selected appropriate request.
	 */
	dd->batching++;
	deadline_move_request(dd, rq);
done:
	/*
	 * If the request needs its target zone locked, do it.
	 */
	blk_req_zone_write_lock(rq);
	rq->rq_flags |= RQF_STARTED;
	return rq;
}

/*
 * Zoned block device models (zoned limit).
 */
enum blk_zoned_model {
	BLK_ZONED_NONE,	/* Regular block device */
	BLK_ZONED_HA,	/* Host-aware zoned block device */
	BLK_ZONED_HM,	/* Host-managed zoned block device */
};

static inline bool blk_queue_is_zoned(struct request_queue *q)
{
	switch (blk_queue_zoned_model(q)) {
	case BLK_ZONED_HA:
	case BLK_ZONED_HM:
		return true;
	default:
		return false;
	}
}

/*
 * For the specified data direction, return the next request to
 * dispatch using sector position sorted lists.
 */
static struct request *
deadline_next_request(struct deadline_data *dd, int data_dir)
{
	struct request *rq;
	unsigned long flags;

	if (WARN_ON_ONCE(data_dir != READ && data_dir != WRITE))                  // 如果传输方向既不是读也不是写，则警告
		return NULL;                                                          // 返回NULL

	rq = dd->next_rq[data_dir];                                               // 从dd->next_rq数组中取出request
	if (!rq)                                                                  // 如果request为NULL
		return NULL;                                                          // 返回NULL

	if (data_dir == READ || !blk_queue_is_zoned(rq->q))
		return rq;

	/*
	 * Look for a write request that can be dispatched, that is one with
	 * an unlocked target zone.
	 */
	spin_lock_irqsave(&dd->zone_lock, flags);
	while (rq) {
		if (blk_req_can_dispatch_to_zone(rq))
			break;
		rq = deadline_latter_request(rq);
	}
	spin_unlock_irqrestore(&dd->zone_lock, flags);

	return rq;
}

/*
 * For the specified data direction, return the next request to
 * dispatch using arrival ordered lists.
 */
static struct request *
deadline_fifo_request(struct deadline_data *dd, int data_dir)       // 不考虑zoned支持，该接口作用是从fifo链表中取出第一个request

{
	struct request *rq;
	unsigned long flags;

	if (WARN_ON_ONCE(data_dir != READ && data_dir != WRITE))        // 如果传输方向既不是读也不是写，则警告
		return NULL;                                                // 返回NULL

	if (list_empty(&dd->fifo_list[data_dir]))                       // 如果fifo链表是empty
		return NULL;                                                // 返回NULL

	rq = rq_entry_fifo(dd->fifo_list[data_dir].next);               // kernel中的链表是带头的，因此这里实际是取链表的第一个entry，参考list_first_entry理解，即取fifo链表中第一个request
	if (data_dir == READ || !blk_queue_is_zoned(rq->q))             // 暂时不考虑zoned device的支持，默认blk_queue_is_zoned返回false
		return rq;                                                  // 返回rq

	/*
	 * Look for a write request that can be dispatched, that is one with
	 * an unlocked target zone.
	 */
	spin_lock_irqsave(&dd->zone_lock, flags);                      // 以下是zoned的处理，zoned是需要硬件支持的，暂时不考虑
	list_for_each_entry(rq, &dd->fifo_list[WRITE], queuelist) {
		if (blk_req_can_dispatch_to_zone(rq))
			goto out;
	}
	rq = NULL;
out:
	spin_unlock_irqrestore(&dd->zone_lock, flags);

	return rq;
}

/*
 * move an entry to dispatch queue
 */
static void
deadline_move_request(struct deadline_data *dd, struct request *rq)
{
	const int data_dir = rq_data_dir(rq);

	dd->next_rq[READ] = NULL;
	dd->next_rq[WRITE] = NULL;
	dd->next_rq[data_dir] = deadline_latter_request(rq);

	/*
	 * take it off the sort and fifo list
	 */
	deadline_remove_request(rq->q, rq);
}

/*
 * remove rq from rbtree and fifo.
 */
static void deadline_remove_request(struct request_queue *q, struct request *rq)
{
	struct deadline_data *dd = q->elevator->elevator_data;

	list_del_init(&rq->queuelist);

	/*
	 * We might not be on the rbtree, if we are doing an insert merge
	 */
	if (!RB_EMPTY_NODE(&rq->rb_node))
		deadline_del_rq_rb(dd, rq);

	elv_rqhash_del(q, rq);
	if (q->last_merge == rq)
		q->last_merge = NULL;
}

static inline void
deadline_del_rq_rb(struct deadline_data *dd, struct request *rq)
{
	const int data_dir = rq_data_dir(rq);

	if (dd->next_rq[data_dir] == rq)
		dd->next_rq[data_dir] = deadline_latter_request(rq);

	elv_rb_del(deadline_rb_root(dd, rq), rq);
}

/*
 * get the request after `rq' in sector-sorted order
 */
static inline struct request *
deadline_latter_request(struct request *rq)
{
	struct rb_node *node = rb_next(&rq->rb_node);

	if (node)
		return rb_entry_rq(node);

	return NULL;
}




