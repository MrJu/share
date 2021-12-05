
struct request *blk_mq_alloc_request(struct request_queue *q, unsigned int op,
		blk_mq_req_flags_t flags)
{
	struct blk_mq_alloc_data alloc_data = { .flags = flags, .cmd_flags = op };
	struct request *rq;
	int ret;

	ret = blk_queue_enter(q, flags);
	if (ret)
		return ERR_PTR(ret);

	rq = blk_mq_get_request(q, NULL, &alloc_data);
	blk_queue_exit(q);

	if (!rq)
		return ERR_PTR(-EWOULDBLOCK);

	rq->__data_len = 0;
	rq->__sector = (sector_t) -1;
	rq->bio = rq->biotail = NULL;
	return rq;
}
EXPORT_SYMBOL(blk_mq_alloc_request);

static struct request *blk_mq_get_request(struct request_queue *q,
					  struct bio *bio,
					  struct blk_mq_alloc_data *data)
{
	struct elevator_queue *e = q->elevator;
	struct request *rq;
	unsigned int tag;
	bool clear_ctx_on_error = false;
	u64 alloc_time_ns = 0;

	blk_queue_enter_live(q);

	/* alloc_time includes depth and tag waits */
	if (blk_queue_rq_alloc_time(q))
		alloc_time_ns = ktime_get_ns();

	data->q = q;
	if (likely(!data->ctx)) {
		data->ctx = blk_mq_get_ctx(q);
		clear_ctx_on_error = true;
	}
	if (likely(!data->hctx))
		data->hctx = blk_mq_map_queue(q, data->cmd_flags,
						data->ctx);
	if (data->cmd_flags & REQ_NOWAIT)
		data->flags |= BLK_MQ_REQ_NOWAIT;

	if (e) {
		data->flags |= BLK_MQ_REQ_INTERNAL;

		/*
		 * Flush requests are special and go directly to the
		 * dispatch list. Don't include reserved tags in the
		 * limiting, as it isn't useful.
		 */
		if (!op_is_flush(data->cmd_flags) &&
		    e->type->ops.limit_depth &&
		    !(data->flags & BLK_MQ_REQ_RESERVED))
			e->type->ops.limit_depth(data->cmd_flags, data);
	} else {
		blk_mq_tag_busy(data->hctx);
	}

	tag = blk_mq_get_tag(data);
	if (tag == BLK_MQ_TAG_FAIL) {
		if (clear_ctx_on_error)
			data->ctx = NULL;
		blk_queue_exit(q);
		return NULL;
	}

	rq = blk_mq_rq_ctx_init(data, tag, data->cmd_flags, alloc_time_ns);
	if (!op_is_flush(data->cmd_flags)) {
		rq->elv.icq = NULL;
		if (e && e->type->ops.prepare_request) {
			if (e->type->icq_cache)
				blk_mq_sched_assign_ioc(rq);

			e->type->ops.prepare_request(rq, bio);
			rq->rq_flags |= RQF_ELVPRIV;
		}
	}
	data->hctx->queued++;
	return rq;
}

unsigned int blk_mq_get_tag(struct blk_mq_alloc_data *data)
{
	struct blk_mq_tags *tags = blk_mq_tags_from_data(data);
	struct sbitmap_queue *bt;
	struct sbq_wait_state *ws;
	DEFINE_SBQ_WAIT(wait);
	unsigned int tag_offset;
	int tag;

	if (data->flags & BLK_MQ_REQ_RESERVED) {
		if (unlikely(!tags->nr_reserved_tags)) {
			WARN_ON_ONCE(1);
			return BLK_MQ_TAG_FAIL;
		}
		bt = &tags->breserved_tags;
		tag_offset = 0;
	} else {
		bt = &tags->bitmap_tags;
		tag_offset = tags->nr_reserved_tags;
	}

	tag = __blk_mq_get_tag(data, bt);
	if (tag != -1)
		goto found_tag;

	if (data->flags & BLK_MQ_REQ_NOWAIT)
		return BLK_MQ_TAG_FAIL;

	ws = bt_wait_ptr(bt, data->hctx);
	do {
		struct sbitmap_queue *bt_prev;

		/*
		 * We're out of tags on this hardware queue, kick any
		 * pending IO submits before going to sleep waiting for
		 * some to complete.
		 */
		blk_mq_run_hw_queue(data->hctx, false);

		/*
		 * Retry tag allocation after running the hardware queue,
		 * as running the queue may also have found completions.
		 */
		tag = __blk_mq_get_tag(data, bt);
		if (tag != -1)
			break;

		sbitmap_prepare_to_wait(bt, ws, &wait, TASK_UNINTERRUPTIBLE);

		tag = __blk_mq_get_tag(data, bt);
		if (tag != -1)
			break;

		bt_prev = bt;
		io_schedule();

		sbitmap_finish_wait(bt, ws, &wait);

		data->ctx = blk_mq_get_ctx(data->q);
		data->hctx = blk_mq_map_queue(data->q, data->cmd_flags,
						data->ctx);
		tags = blk_mq_tags_from_data(data);
		if (data->flags & BLK_MQ_REQ_RESERVED)
			bt = &tags->breserved_tags;
		else
			bt = &tags->bitmap_tags;

		/*
		 * If destination hw queue is changed, fake wake up on
		 * previous queue for compensating the wake up miss, so
		 * other allocations on previous queue won't be starved.
		 */
		if (bt != bt_prev)
			sbitmap_queue_wake_up(bt_prev);

		ws = bt_wait_ptr(bt, data->hctx);
	} while (1);

	sbitmap_finish_wait(bt, ws, &wait);

found_tag:
	return tag + tag_offset;
}

static int __blk_mq_get_tag(struct blk_mq_alloc_data *data,
			    struct sbitmap_queue *bt)
{
	if (!(data->flags & BLK_MQ_REQ_INTERNAL) &&
	    !hctx_may_queue(data->hctx, bt))
		return -1;
	if (data->shallow_depth)
		return __sbitmap_queue_get_shallow(bt, data->shallow_depth);
	else
		return __sbitmap_queue_get(bt);
}
















