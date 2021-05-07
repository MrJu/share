https://blog.csdn.net/u012414189/article/details/88358648
https://blog.csdn.net/weixin_30855761/article/details/95924136
https://www.codenong.com/cs106302820/
https://blog.csdn.net/hu1610552336/article/details/111464548
http://www.361way.com/kernel-hung-task-analysis/4326.html // 一次内核hung task分析
https://zhuanlan.zhihu.com/p/345592034                    // 由 OOM 引发的 ext4 文件系统卡死
https://www.sohu.com/a/384319777_261288                   // Linux IO Block layer 解析 
https://www.sohu.com/a/395091887_467784                   // Multi-queue 架构分析 



void __sched io_schedule(void)
{
	int token;

	token = io_schedule_prepare();
	schedule();
	io_schedule_finish(token);
}
EXPORT_SYMBOL(io_schedule);

int io_schedule_prepare(void)
{
	int old_iowait = current->in_iowait;

	current->in_iowait = 1;
	blk_schedule_flush_plug(current);

	return old_iowait;
}

static inline void blk_schedule_flush_plug(struct task_struct *tsk)
{
	struct blk_plug *plug = tsk->plug;

	if (plug)
		blk_flush_plug_list(plug, true);
}

void blk_flush_plug_list(struct blk_plug *plug, bool from_schedule)
{
	flush_plug_callbacks(plug, from_schedule);

	if (!list_empty(&plug->mq_list))
		blk_mq_flush_plug_list(plug, from_schedule);
}

static void flush_plug_callbacks(struct blk_plug *plug, bool from_schedule)
{
	LIST_HEAD(callbacks);

	while (!list_empty(&plug->cb_list)) {
		list_splice_init(&plug->cb_list, &callbacks);

		while (!list_empty(&callbacks)) {
			struct blk_plug_cb *cb = list_first_entry(&callbacks,
							  struct blk_plug_cb,
							  list);
			list_del(&cb->list);
			cb->callback(cb, from_schedule);
		}
	}
}

/*
 * Check whether this bio carries any data or not. A NULL bio is allowed.
 */
static inline bool bio_has_data(struct bio *bio)
{
	if (bio &&
	    bio->bi_iter.bi_size &&
	    bio_op(bio) != REQ_OP_DISCARD &&
	    bio_op(bio) != REQ_OP_SECURE_ERASE &&
	    bio_op(bio) != REQ_OP_WRITE_ZEROES)
		return true;

	return false;
}

static inline void count_vm_events(enum vm_event_item item, long delta)
{
	this_cpu_add(vm_event_states.event[item], delta);
}

/*
 * Flag that makes the machine dump writes/reads and block dirtyings.
 */
int block_dump;

static struct ctl_table kern_table[] = {
	...
	{
		.procname	= "block_dump",
		.data		= &block_dump,
		.maxlen		= sizeof(block_dump),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
		.extra1		= SYSCTL_ZERO,
	},
	...
}

/**
 * submit_bio - submit a bio to the block device layer for I/O
 * @rw: whether to %READ or %WRITE, or maybe to %READA (read ahead)
 * @bio: The &struct bio which describes the I/O
 *
 * submit_bio() is very similar in purpose to generic_make_request(), and
 * uses that function to do most of the work. Both are fairly rough
 * interfaces; @bio must be presetup and ready for I/O.
 *
 */
blk_qc_t submit_bio(int rw, struct bio *bio)
{
	bio->bi_rw |= rw;

	/*
	 * If it's a regular read/write or a barrier with data attached,
	 * go through the normal accounting stuff before submission.
	 */
	if (bio_has_data(bio)) {                                          // Check whether this bio carries any data or not. A NULL bio is allowed.
		unsigned int count;

		if (unlikely(rw & REQ_WRITE_SAME))
			count = bdev_logical_block_size(bio->bi_bdev) >> 9;       // ？
		else
			count = bio_sectors(bio);

		if (rw & WRITE) {                                             // 如果是写
			count_vm_events(PGPGOUT, count);                          // 更新vm统计数据REQ_WRITE_SAME
		} else {
			task_io_account_read(bio->bi_iter.bi_size);               // 记录读的量
			count_vm_events(PGPGIN, count);                           // 更新vm统计数据
		}

		if (unlikely(block_dump)) {                                   // 调试打印
			char b[BDEVNAME_SIZE];
			printk(KERN_DEBUG "%s(%d): %s block %Lu on %s (%u sectors)\n",
			current->comm, task_pid_nr(current),
				(rw & WRITE) ? "WRITE" : "READ",
				(unsigned long long)bio->bi_iter.bi_sector,
				bdevname(bio->bi_bdev, b),
				count);
		}
	}

	return generic_make_request(bio);
}
EXPORT_SYMBOL(submit_bio);

static noinline_for_stack bool
generic_make_request_checks(struct bio *bio)
{
	struct request_queue *q;
	int nr_sectors = bio_sectors(bio);
	blk_status_t status = BLK_STS_IOERR;
	char b[BDEVNAME_SIZE];

	might_sleep();

	q = bio->bi_disk->queue;
	if (unlikely(!q)) {
		printk(KERN_ERR
		       "generic_make_request: Trying to access "
			"nonexistent block-device %s (%Lu)\n",
			bio_devname(bio, b), (long long)bio->bi_iter.bi_sector);
		goto end_io;
	}

	/*
	 * For a REQ_NOWAIT based request, return -EOPNOTSUPP
	 * if queue is not a request based queue.
	 */
	if ((bio->bi_opf & REQ_NOWAIT) && !queue_is_mq(q))
		goto not_supported;

	if (should_fail_bio(bio))
		goto end_io;

	if (bio->bi_partno) {
		if (unlikely(blk_partition_remap(bio)))
			goto end_io;
	} else {
		if (unlikely(bio_check_ro(bio, &bio->bi_disk->part0)))
			goto end_io;
		if (unlikely(bio_check_eod(bio, get_capacity(bio->bi_disk))))
			goto end_io;
	}

	/*
	 * Filter flush bio's early so that make_request based
	 * drivers without flush support don't have to worry
	 * about them.
	 */
	if (op_is_flush(bio->bi_opf) &&
	    !test_bit(QUEUE_FLAG_WC, &q->queue_flags)) {
		bio->bi_opf &= ~(REQ_PREFLUSH | REQ_FUA);
		if (!nr_sectors) {
			status = BLK_STS_OK;
			goto end_io;
		}
	}

	if (!test_bit(QUEUE_FLAG_POLL, &q->queue_flags))
		bio->bi_opf &= ~REQ_HIPRI;

	switch (bio_op(bio)) {
	case REQ_OP_DISCARD:
		if (!blk_queue_discard(q))
			goto not_supported;
		break;
	case REQ_OP_SECURE_ERASE:
		if (!blk_queue_secure_erase(q))
			goto not_supported;
		break;
	case REQ_OP_WRITE_SAME:
		if (!q->limits.max_write_same_sectors)
			goto not_supported;
		break;
	case REQ_OP_ZONE_RESET:
		if (!blk_queue_is_zoned(q))
			goto not_supported;
		break;
	case REQ_OP_ZONE_RESET_ALL:
		if (!blk_queue_is_zoned(q) || !blk_queue_zone_resetall(q))
			goto not_supported;
		break;
	case REQ_OP_WRITE_ZEROES:
		if (!q->limits.max_write_zeroes_sectors)
			goto not_supported;
		break;
	default:
		break;
	}

	/*
	 * Various block parts want %current->io_context and lazy ioc
	 * allocation ends up trading a lot of pain for a small amount of
	 * memory.  Just allocate it upfront.  This may fail and block
	 * layer knows how to live with it.
	 */
	create_io_context(GFP_ATOMIC, q->node);

	if (!blkcg_bio_issue_check(q, bio))
		return false;

	if (!bio_flagged(bio, BIO_TRACE_COMPLETION)) {
		trace_block_bio_queue(q, bio);
		/* Now that enqueuing has been traced, we need to trace
		 * completion as well.
		 */
		bio_set_flag(bio, BIO_TRACE_COMPLETION);
	}
	return true;

not_supported:
	status = BLK_STS_NOTSUPP;
end_io:
	bio->bi_status = status;
	bio_endio(bio);
	return false;
}

static inline void bio_list_add(struct bio_list *bl, struct bio *bio)
{
	bio->bi_next = NULL;

	if (bl->tail)
		bl->tail->bi_next = bio;
	else
		bl->head = bio;

	bl->tail = bio;
}

/*
 * main unit of I/O for the block layer and lower layers (ie drivers and
 * stacking drivers)
 */
struct bio {
	struct bio		*bi_next;	/* request queue link */
	struct gendisk		*bi_disk;
	unsigned int		bi_opf;		/* bottom bits req flags,
						 * top bits REQ_OP. Use
						 * accessors.
						 */
	unsigned short		bi_flags;	/* status, etc and bvec pool number */
	unsigned short		bi_ioprio;
	unsigned short		bi_write_hint;
	blk_status_t		bi_status;
	u8			bi_partno;

	struct bvec_iter	bi_iter;

	atomic_t		__bi_remaining;
	bio_end_io_t		*bi_end_io;

	void			*bi_private;
#ifdef CONFIG_BLK_CGROUP
	/*
	 * Represents the association of the css and request_queue for the bio.
	 * If a bio goes direct to device, it will not have a blkg as it will
	 * not have a request_queue associated with it.  The reference is put
	 * on release of the bio.
	 */
	struct blkcg_gq		*bi_blkg;
	struct bio_issue	bi_issue;
#ifdef CONFIG_BLK_CGROUP_IOCOST
	u64			bi_iocost_cost;
#endif
#endif
	union {
#if defined(CONFIG_BLK_DEV_INTEGRITY)
		struct bio_integrity_payload *bi_integrity; /* data integrity */
#endif
	};

	unsigned short		bi_vcnt;	/* how many bio_vec's */

	/*
	 * Everything starting with bi_max_vecs will be preserved by bio_reset()
	 */

	unsigned short		bi_max_vecs;	/* max bvl_vecs we can hold */

	atomic_t		__bi_cnt;	/* pin count */

	struct bio_vec		*bi_io_vec;	/* the actual vec list */

	struct bio_set		*bi_pool;

	/*
	 * We can inline a number of vecs at the end of the bio, to avoid
	 * double allocations for a small number of bio_vecs. This member
	 * MUST obviously be kept at the very end of the bio.
	 */
	struct bio_vec		bi_inline_vecs[0];
};



struct rq_wb {
	/*
	 * Settings that govern how we throttle
	 */
	unsigned int wb_background;		/* background writeback */
	unsigned int wb_normal;			/* normal writeback */

	short enable_state;			/* WBT_STATE_* */

	/*
	 * Number of consecutive periods where we don't have enough
	 * information to make a firm scale up/down decision.
	 */
	unsigned int unknown_cnt;

	u64 win_nsec;				/* default window size */
	u64 cur_win_nsec;			/* current window size */

	struct blk_stat_callback *cb;

	u64 sync_issue;
	void *sync_cookie;

	unsigned int wc;

	unsigned long last_issue;		/* last non-throttled issue */
	unsigned long last_comp;		/* last non-throttled comp */
	unsigned long min_lat_nsec;
	struct rq_qos rqos;                                                    // struct rq_wb *rwb = RQWB(rqos)
	struct rq_wait rq_wait[WBT_NUM_RWQ];
	struct rq_depth rq_depth;
};


/**
 * generic_make_request - hand a buffer to its device driver for I/O
 * @bio:  The bio describing the location in memory and on the device.
 *
 * generic_make_request() is used to make I/O requests of block
 * devices. It is passed a &struct bio, which describes the I/O that needs
 * to be done.
 *
 * generic_make_request() does not return any status.  The
 * success/failure status of the request, along with notification of
 * completion, is delivered asynchronously through the bio->bi_end_io
 * function described (one day) else where.
 *
 * The caller of generic_make_request must make sure that bi_io_vec
 * are set to describe the memory buffer, and that bi_dev and bi_sector are
 * set to describe the device address, and the
 * bi_end_io and optionally bi_private are set to describe how
 * completion notification should be signaled.
 *
 * generic_make_request and the drivers it calls may use bi_next if this
 * bio happens to be merged with someone else, and may resubmit the bio to
 * a lower device by calling into generic_make_request recursively, which
 * means the bio should NOT be touched after the call to ->make_request_fn.
 */
blk_qc_t generic_make_request(struct bio *bio)
{
	/*
	 * bio_list_on_stack[0] contains bios submitted by the current
	 * make_request_fn.
	 * bio_list_on_stack[1] contains bios that were submitted before
	 * the current make_request_fn, but that haven't been processed
	 * yet.
	 */
	struct bio_list bio_list_on_stack[2];                                  // 参考注释理解
	blk_qc_t ret = BLK_QC_T_NONE;

	if (!generic_make_request_checks(bio))
		goto out;

	/*
	 * We only want one ->make_request_fn to be active at a time, else
	 * stack usage with stacked devices could be a problem.  So use
	 * current->bio_list to keep a list of requests submited by a
	 * make_request_fn function.  current->bio_list is also used as a
	 * flag to say if generic_make_request is currently active in this
	 * task or not.  If it is NULL, then no make_request is active.  If
	 * it is non-NULL, then a make_request is active, and new requests
	 * should be added at the tail
	 */
	if (current->bio_list) {                                               // 
		bio_list_add(&current->bio_list[0], bio);
		goto out;
	}

	/* following loop may be a bit non-obvious, and so deserves some
	 * explanation.
	 * Before entering the loop, bio->bi_next is NULL (as all callers
	 * ensure that) so we have a list with a single bio.
	 * We pretend that we have just taken it off a longer list, so
	 * we assign bio_list to a pointer to the bio_list_on_stack,
	 * thus initialising the bio_list of new bios to be
	 * added.  ->make_request() may indeed add some more bios
	 * through a recursive call to generic_make_request.  If it
	 * did, we find a non-NULL value in bio_list and re-enter the loop
	 * from the top.  In this case we really did just take the bio
	 * of the top of the list (no pretending) and so remove it from
	 * bio_list, and call into ->make_request() again.
	 */
	BUG_ON(bio->bi_next);
	bio_list_init(&bio_list_on_stack[0]);                                  // 
	current->bio_list = bio_list_on_stack;
	do {
		struct request_queue *q = bio->bi_disk->queue;
		blk_mq_req_flags_t flags = bio->bi_opf & REQ_NOWAIT ?
			BLK_MQ_REQ_NOWAIT : 0;

		if (likely(blk_queue_enter(q, flags) == 0)) {
			struct bio_list lower, same;

			/* Create a fresh bio_list for all subordinate requests */
			bio_list_on_stack[1] = bio_list_on_stack[0];
			bio_list_init(&bio_list_on_stack[0]);
			ret = q->make_request_fn(q, bio);

			blk_queue_exit(q);

			/* sort new bios into those for a lower level
			 * and those for the same level
			 */
			bio_list_init(&lower);
			bio_list_init(&same);
			while ((bio = bio_list_pop(&bio_list_on_stack[0])) != NULL)
				if (q == bio->bi_disk->queue)
					bio_list_add(&same, bio);
				else
					bio_list_add(&lower, bio);
			/* now assemble so we handle the lowest level first */
			bio_list_merge(&bio_list_on_stack[0], &lower);
			bio_list_merge(&bio_list_on_stack[0], &same);
			bio_list_merge(&bio_list_on_stack[0], &bio_list_on_stack[1]);
		} else {
			if (unlikely(!blk_queue_dying(q) &&
					(bio->bi_opf & REQ_NOWAIT)))
				bio_wouldblock_error(bio);
			else
				bio_io_error(bio);
		}
		bio = bio_list_pop(&bio_list_on_stack[0]);
	} while (bio);
	current->bio_list = NULL; /* deactivate */

out:
	return ret;
}
EXPORT_SYMBOL(generic_make_request);

void __sched io_schedule(void)
{
	int token;

	token = io_schedule_prepare();
	schedule();
	io_schedule_finish(token);
}
EXPORT_SYMBOL(io_schedule);

int io_schedule_prepare(void)
{
	int old_iowait = current->in_iowait;

	current->in_iowait = 1;
	blk_schedule_flush_plug(current);

	return old_iowait;
}

void io_schedule_finish(int token)
{
	current->in_iowait = token;
}

#ifdef CONFIG_BLOCK
static inline void blk_schedule_flush_plug(struct task_struct *tsk)
{
	struct blk_plug *plug = tsk->plug;

	if (plug)
		blk_flush_plug_list(plug, true);
}
#else /* CONFIG_BLOCK */
static inline void blk_schedule_flush_plug(struct task_struct *task)
{
}
#endif /* CONFIG_BLOCK */

void blk_flush_plug_list(struct blk_plug *plug, bool from_schedule)   // 接口名已经表明接口的功能了
{
	flush_plug_callbacks(plug, from_schedule);

	if (!list_empty(&plug->mq_list))
		blk_mq_flush_plug_list(plug, from_schedule);
}

static void flush_plug_callbacks(struct blk_plug *plug, bool from_schedule)
{                                                                     // 从接口名称可以看出来接口的功能，flush plug中的cb_list并调用
	LIST_HEAD(callbacks);                                             // 创建并初始化callbacks

	while (!list_empty(&plug->cb_list)) {                             // 当plug->cb_list非空
		list_splice_init(&plug->cb_list, &callbacks);                 // 将plug->cb_list合并到callbacks中，并重新初始化plug->cb_list

		while (!list_empty(&callbacks)) {                             // 当callbacks非空
			struct blk_plug_cb *cb = list_first_entry(&callbacks,     // 取出cb
							  struct blk_plug_cb,
							  list);
			list_del(&cb->list);                                      // 将cb->list从callbacks中删除
			cb->callback(cb, from_schedule);                          // 调用cb->callback
		}
	}
}

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	WRITE_ONCE(list->next, list);
	list->prev = list;
}

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

static inline void __list_splice(const struct list_head *list,
				 struct list_head *prev,
				 struct list_head *next)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

/**
 * list_splice - join two lists, this is designed for stacks
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice(const struct list_head *list,
				struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head, head->next);
}

/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 *
 * The list at @list is reinitialised
 */
static inline void list_splice_init(struct list_head *list,
				    struct list_head *head)
{
	if (!list_empty(list)) {
		__list_splice(list, head, head->next);
		INIT_LIST_HEAD(list);
	}
}

#define list_entry_rq(ptr)	list_entry((ptr), struct request, queuelist)

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static inline void list_del_init(struct list_head *entry)
{
	__list_del_entry(entry);
	INIT_LIST_HEAD(entry);
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void __list_del_entry(struct list_head *entry)
{
	if (!__list_del_entry_valid(entry))
		return;

	__list_del(entry->prev, entry->next);
}

/*
 * blk_plug permits building a queue of related requests by holding the I/O
 * fragments for a short period. This allows merging of sequential requests
 * into single larger request. As the requests are moved from a per-task list to
 * the device's request_queue in a batch, this results in improved scalability
 * as the lock contention for request_queue lock is reduced.
 *
 * It is ok not to disable preemption when adding the request to the plug list
 * or when attempting a merge, because blk_schedule_flush_list() will only flush
 * the plug list when the task sleeps by itself. For details, please see
 * schedule() where blk_schedule_flush_plug() is called.
 */
struct blk_plug {
	struct list_head mq_list; /* blk-mq requests */
	struct list_head cb_list; /* md requires an unplug callback */
	unsigned short rq_count;                                          // 用于对取出的mq_list进行排序的判断
	bool multiple_queues;                                             // 用于对取出的mq_list进行排序的判断
};

void blk_mq_flush_plug_list(struct blk_plug *plug, bool from_schedule)
{
	struct blk_mq_hw_ctx *this_hctx;
	struct blk_mq_ctx *this_ctx;
	struct request_queue *this_q;
	struct request *rq;
	LIST_HEAD(list);                                                  // 定义并初始化list
	LIST_HEAD(rq_list);                                               // 定义并初始化rq_list
	unsigned int depth;

	list_splice_init(&plug->mq_list, &list);                          // 将plug->mq_list添加到list，并初始化plug->mq_list

	if (plug->rq_count > 2 && plug->multiple_queues)
		list_sort(NULL, &list, plug_rq_cmp);                          // 对list进行排序

	plug->rq_count = 0;                                               // 重置plug->rq_count

	this_q = NULL;
	this_hctx = NULL;
	this_ctx = NULL;
	depth = 0;

	while (!list_empty(&list)) {                                      // 如果list不为空
		rq = list_entry_rq(list.next);
		list_del_init(&rq->queuelist);                                // 从list中delete并重新初始化rq->queuelist
		BUG_ON(!rq->q);
		if (rq->mq_hctx != this_hctx || rq->mq_ctx != this_ctx) {
			if (this_hctx) {
				trace_block_unplug(this_q, depth, !from_schedule);
				blk_mq_sched_insert_requests(this_hctx, this_ctx,
								&rq_list,
								from_schedule);
			}

			this_q = rq->q;
			this_ctx = rq->mq_ctx;
			this_hctx = rq->mq_hctx;
			depth = 0;
		}

		depth++;
		list_add_tail(&rq->queuelist, &rq_list);
	}

	/*
	 * If 'this_hctx' is set, we know we have entries to complete
	 * on 'rq_list'. Do those.
	 */
	if (this_hctx) {
		trace_block_unplug(this_q, depth, !from_schedule);
		blk_mq_sched_insert_requests(this_hctx, this_ctx, &rq_list,
						from_schedule);
	}
}

void blk_mq_sched_insert_requests(struct blk_mq_hw_ctx *hctx,
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

/**
 * blk_start_plug - initialize blk_plug and track it inside the task_struct
 * @plug:	The &struct blk_plug that needs to be initialized
 *
 * Description:
 *   blk_start_plug() indicates to the block layer an intent by the caller
 *   to submit multiple I/O requests in a batch.  The block layer may use
 *   this hint to defer submitting I/Os from the caller until blk_finish_plug()
 *   is called.  However, the block layer may choose to submit requests
 *   before a call to blk_finish_plug() if the number of queued I/Os
 *   exceeds %BLK_MAX_REQUEST_COUNT, or if the size of the I/O is larger than
 *   %BLK_PLUG_FLUSH_SIZE.  The queued I/Os may also be submitted early if
 *   the task schedules (see below).
 *
 *   Tracking blk_plug inside the task_struct will help with auto-flushing the
 *   pending I/O should the task end up blocking between blk_start_plug() and
 *   blk_finish_plug(). This is important from a performance perspective, but
 *   also ensures that we don't deadlock. For instance, if the task is blocking
 *   for a memory allocation, memory reclaim could end up wanting to free a
 *   page belonging to that request that is currently residing in our private
 *   plug. By flushing the pending I/O when the process goes to sleep, we avoid
 *   this kind of deadlock.
 */
void blk_start_plug(struct blk_plug *plug)                            // 初始化plug
{
	struct task_struct *tsk = current;

	/*
	 * If this is a nested plug, don't actually assign it.
	 */
	if (tsk->plug)
		return;

	INIT_LIST_HEAD(&plug->mq_list);
	INIT_LIST_HEAD(&plug->cb_list);
	plug->rq_count = 0;
	plug->multiple_queues = false;

	/*
	 * Store ordering should not be needed here, since a potential
	 * preempt will imply a full memory barrier
	 */
	tsk->plug = plug;
}
EXPORT_SYMBOL(blk_start_plug);

/**
 * blk_finish_plug - mark the end of a batch of submitted I/O
 * @plug:	The &struct blk_plug passed to blk_start_plug()
 *
 * Description:
 * Indicate that a batch of I/O submissions is complete.  This function
 * must be paired with an initial call to blk_start_plug().  The intent
 * is to allow the block layer to optimize I/O submission.  See the
 * documentation for blk_start_plug() for more information.
 */
void blk_finish_plug(struct blk_plug *plug)
{
	if (plug != current->plug)
		return;
	blk_flush_plug_list(plug, false);                                // flush plug中的list

	current->plug = NULL;
}
EXPORT_SYMBOL(blk_finish_plug);

static int __f2fs_write_data_pages(struct address_space *mapping,
						struct writeback_control *wbc,
						enum iostat_type io_type)
{
	struct inode *inode = mapping->host;
	struct f2fs_sb_info *sbi = F2FS_I_SB(inode);
	struct blk_plug plug;
	int ret;
	bool locked = false;

	/* deal with chardevs and other special file */
	if (!mapping->a_ops->writepage)
		return 0;

	/* skip writing if there is no dirty page in this inode */
	if (!get_dirty_pages(inode) && wbc->sync_mode == WB_SYNC_NONE)
		return 0;

	/* during POR, we don't need to trigger writepage at all. */
	if (unlikely(is_sbi_flag_set(sbi, SBI_POR_DOING)))
		goto skip_write;

	if ((S_ISDIR(inode->i_mode) || IS_NOQUOTA(inode)) &&
			wbc->sync_mode == WB_SYNC_NONE &&
			get_dirty_pages(inode) < nr_pages_to_skip(sbi, DATA) &&
			f2fs_available_free_memory(sbi, DIRTY_DENTS))
		goto skip_write;

	/* skip writing during file defragment */
	if (is_inode_flag_set(inode, FI_DO_DEFRAG))
		goto skip_write;

	trace_f2fs_writepages(mapping->host, wbc, DATA);

	/* to avoid spliting IOs due to mixed WB_SYNC_ALL and WB_SYNC_NONE */
	if (wbc->sync_mode == WB_SYNC_ALL)
		atomic_inc(&sbi->wb_sync_req[DATA]);
	else if (atomic_read(&sbi->wb_sync_req[DATA]))
		goto skip_write;

	if (__should_serialize_io(inode, wbc)) {
		mutex_lock(&sbi->writepages);
		locked = true;
	}

	blk_start_plug(&plug);                                            // 初始化plug并赋值给current->plug
	ret = f2fs_write_cache_pages(mapping, wbc, io_type);
	blk_finish_plug(&plug);

	if (locked)
		mutex_unlock(&sbi->writepages);

	if (wbc->sync_mode == WB_SYNC_ALL)
		atomic_dec(&sbi->wb_sync_req[DATA]);
	/*
	 * if some pages were truncated, we cannot guarantee its mapping->host
	 * to detect pending bios.
	 */

	f2fs_remove_dirty_inode(inode);
	return ret;

skip_write:
	wbc->pages_skipped += get_dirty_pages(inode);
	trace_f2fs_writepages(mapping->host, wbc, DATA);
	return 0;
}

static int f2fs_write_data_pages(struct address_space *mapping,
			    struct writeback_control *wbc)
{
	struct inode *inode = mapping->host;

	return __f2fs_write_data_pages(mapping, wbc,
			F2FS_I(inode)->cp_task == current ?
			FS_CP_DATA_IO : FS_DATA_IO);
}

const struct address_space_operations f2fs_dblock_aops = {
	.readpage	= f2fs_read_data_page,
	.readpages	= f2fs_read_data_pages,
	.writepage	= f2fs_write_data_page,
	.writepages	= f2fs_write_data_pages,
	.write_begin	= f2fs_write_begin,
	.write_end	= f2fs_write_end,
	.set_page_dirty	= f2fs_set_data_page_dirty,
	.invalidatepage	= f2fs_invalidate_page,
	.releasepage	= f2fs_release_page,
	.direct_IO	= f2fs_direct_IO,
	.bmap		= f2fs_bmap,
	.swap_activate  = f2fs_swap_activate,
	.swap_deactivate = f2fs_swap_deactivate,
#ifdef CONFIG_MIGRATION
	.migratepage    = f2fs_migrate_page,
#endif
};

struct inode *f2fs_iget(struct super_block *sb, unsigned long ino)
{
	struct f2fs_sb_info *sbi = F2FS_SB(sb);
	struct inode *inode;
	int ret = 0;

	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	if (!(inode->i_state & I_NEW)) {
		trace_f2fs_iget(inode);
		return inode;
	}
	if (ino == F2FS_NODE_INO(sbi) || ino == F2FS_META_INO(sbi))
		goto make_now;

	ret = do_read_inode(inode);
	if (ret)
		goto bad_inode;
make_now:
	if (ino == F2FS_NODE_INO(sbi)) {
		inode->i_mapping->a_ops = &f2fs_node_aops;
		mapping_set_gfp_mask(inode->i_mapping, GFP_NOFS);
	} else if (ino == F2FS_META_INO(sbi)) {
		inode->i_mapping->a_ops = &f2fs_meta_aops;
		mapping_set_gfp_mask(inode->i_mapping, GFP_NOFS);
	} else if (S_ISREG(inode->i_mode)) {
		inode->i_op = &f2fs_file_inode_operations;
		inode->i_fop = &f2fs_file_operations;
		inode->i_mapping->a_ops = &f2fs_dblock_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &f2fs_dir_inode_operations;
		inode->i_fop = &f2fs_dir_operations;
		inode->i_mapping->a_ops = &f2fs_dblock_aops;
		inode_nohighmem(inode);
	} else if (S_ISLNK(inode->i_mode)) {
		if (file_is_encrypt(inode))
			inode->i_op = &f2fs_encrypted_symlink_inode_operations;
		else
			inode->i_op = &f2fs_symlink_inode_operations;
		inode_nohighmem(inode);
		inode->i_mapping->a_ops = &f2fs_dblock_aops;
	} else if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode) ||
			S_ISFIFO(inode->i_mode) || S_ISSOCK(inode->i_mode)) {
		inode->i_op = &f2fs_special_inode_operations;
		init_special_inode(inode, inode->i_mode, inode->i_rdev);
	} else {
		ret = -EIO;
		goto bad_inode;
	}
	f2fs_set_inode_flags(inode);
	unlock_new_inode(inode);
	trace_f2fs_iget(inode);
	return inode;

bad_inode:
	f2fs_inode_synced(inode);
	iget_failed(inode);
	trace_f2fs_iget_exit(inode, ret);
	return ERR_PTR(ret);
}

static int f2fs_create(struct inode *dir, struct dentry *dentry, umode_t mode,
						bool excl)
{
	struct f2fs_sb_info *sbi = F2FS_I_SB(dir);
	struct inode *inode;
	nid_t ino = 0;
	int err;

	if (unlikely(f2fs_cp_error(sbi)))
		return -EIO;
	if (!f2fs_is_checkpoint_ready(sbi))
		return -ENOSPC;

	err = dquot_initialize(dir);
	if (err)
		return err;

	inode = f2fs_new_inode(dir, mode);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	if (!test_opt(sbi, DISABLE_EXT_IDENTIFY))
		set_file_temperature(sbi, inode, dentry->d_name.name);

	inode->i_op = &f2fs_file_inode_operations;
	inode->i_fop = &f2fs_file_operations;
	inode->i_mapping->a_ops = &f2fs_dblock_aops;
	ino = inode->i_ino;

	f2fs_lock_op(sbi);
	err = f2fs_add_link(dentry, inode);
	if (err)
		goto out;
	f2fs_unlock_op(sbi);

	f2fs_alloc_nid_done(sbi, ino);

	d_instantiate_new(dentry, inode);

	if (IS_DIRSYNC(dir))
		f2fs_sync_fs(sbi->sb, 1);

	f2fs_balance_fs(sbi, true);
	return 0;
out:
	f2fs_handle_failed_inode(inode);
	return err;
}

/*
 * A control structure which tells the writeback code what to do.  These are
 * always on the stack, and hence need no locking.  They are always initialised
 * in a manner such that unspecified fields are set to zero.
 */
struct writeback_control {
	long nr_to_write;		/* Write this many pages, and decrement
					   this for each page written */
	long pages_skipped;		/* Pages which were not written */

	/*
	 * For a_ops->writepages(): if start or end are non-zero then this is
	 * a hint that the filesystem need only write out the pages inside that
	 * byterange.  The byte at `end' is included in the writeout request.
	 */
	loff_t range_start;
	loff_t range_end;

	enum writeback_sync_modes sync_mode;

	unsigned for_kupdate:1;		/* A kupdate writeback */
	unsigned for_background:1;	/* A background writeback */
	unsigned tagged_writepages:1;	/* tag-and-write to avoid livelock */
	unsigned for_reclaim:1;		/* Invoked from the page allocator */
	unsigned range_cyclic:1;	/* range_start is cyclic */
	unsigned for_sync:1;		/* sync(2) WB_SYNC_ALL writeback */

	/*
	 * When writeback IOs are bounced through async layers, only the
	 * initial synchronous phase should be accounted towards inode
	 * cgroup ownership arbitration to avoid confusion.  Later stages
	 * can set the following flag to disable the accounting.
	 */
	unsigned no_cgroup_owner:1;

	unsigned punt_to_cgroup:1;	/* cgrp punting, see __REQ_CGROUP_PUNT */

#ifdef CONFIG_CGROUP_WRITEBACK
	struct bdi_writeback *wb;	/* wb this writeback is issued under */
	struct inode *inode;		/* inode being written out */

	/* foreign inode detection, see wbc_detach_inode() */
	int wb_id;			/* current wb id */
	int wb_lcand_id;		/* last foreign candidate wb id */
	int wb_tcand_id;		/* this foreign candidate wb id */
	size_t wb_bytes;		/* bytes written by current wb */
	size_t wb_lcand_bytes;		/* bytes written by last candidate */
	size_t wb_tcand_bytes;		/* bytes written by this candidate */
#endif
};

void blk_mq_sched_insert_request(struct request *rq, bool at_head,
				 bool run_queue, bool async)
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

	WARN_ON(e && (rq->tag != -1));

	if (blk_mq_sched_bypass_insert(hctx, !!e, rq))
		goto run;

	if (e && e->type->ops.insert_requests) {
		LIST_HEAD(list);

		list_add(&rq->queuelist, &list);
		e->type->ops.insert_requests(hctx, &list, at_head);
	} else {
		spin_lock(&ctx->lock);
		__blk_mq_insert_request(hctx, rq, at_head);
		spin_unlock(&ctx->lock);
	}

run:
	if (run_queue)
		blk_mq_run_hw_queue(hctx, async);
}

static struct mmc_driver mmc_driver = {
	.drv		= {
		.name	= "mmcblk",
		.pm	= &mmc_blk_pm_ops,
	},
	.probe		= mmc_blk_probe,
	.remove		= mmc_blk_remove,
	.shutdown	= mmc_blk_shutdown,
};

static int mmc_blk_probe(struct mmc_card *card)
{
	struct mmc_blk_data *md, *part_md;
	char cap_str[10];

	/*
	 * Check that the card supports the command class(es) we need.
	 */
	if (!(card->csd.cmdclass & CCC_BLOCK_READ))
		return -ENODEV;

	mmc_fixup_device(card, mmc_blk_fixups);

	card->complete_wq = alloc_workqueue("mmc_complete",
					WQ_MEM_RECLAIM | WQ_HIGHPRI, 0);
	if (unlikely(!card->complete_wq)) {
		pr_err("Failed to create mmc completion workqueue");
		return -ENOMEM;
	}

	md = mmc_blk_alloc(card);
	if (IS_ERR(md))
		return PTR_ERR(md);

	string_get_size((u64)get_capacity(md->disk), 512, STRING_UNITS_2,
			cap_str, sizeof(cap_str));
	pr_info("%s: %s %s %s %s\n",
		md->disk->disk_name, mmc_card_id(card), mmc_card_name(card),
		cap_str, md->read_only ? "(ro)" : "");

	if (mmc_blk_alloc_parts(card, md))
		goto out;

	dev_set_drvdata(&card->dev, md);

	if (mmc_add_disk(md))
		goto out;

	list_for_each_entry(part_md, &md->part, part) {
		if (mmc_add_disk(part_md))
			goto out;
	}

	/* Add two debugfs entries */
	mmc_blk_add_debugfs(card, md);

	pm_runtime_set_autosuspend_delay(&card->dev, 3000);
	pm_runtime_use_autosuspend(&card->dev);

	/*
	 * Don't enable runtime PM for SD-combo cards here. Leave that
	 * decision to be taken during the SDIO init sequence instead.
	 */
	if (card->type != MMC_TYPE_SD_COMBO) {
		pm_runtime_set_active(&card->dev);
		pm_runtime_enable(&card->dev);
	}

	return 0;

 out:
	mmc_blk_remove_parts(card, md);
	mmc_blk_remove_req(md);
	return 0;
}

static int mmc_add_disk(struct mmc_blk_data *md)
{
	int ret;
	struct mmc_card *card = md->queue.card;

	device_add_disk(md->parent, md->disk, NULL);
	md->force_ro.show = force_ro_show;
	md->force_ro.store = force_ro_store;
	sysfs_attr_init(&md->force_ro.attr);
	md->force_ro.attr.name = "force_ro";
	md->force_ro.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(disk_to_dev(md->disk), &md->force_ro);
	if (ret)
		goto force_ro_fail;

	if ((md->area_type & MMC_BLK_DATA_AREA_BOOT) &&
	     card->ext_csd.boot_ro_lockable) {
		umode_t mode;

		if (card->ext_csd.boot_ro_lock & EXT_CSD_BOOT_WP_B_PWR_WP_DIS)
			mode = S_IRUGO;
		else
			mode = S_IRUGO | S_IWUSR;

		md->power_ro_lock.show = power_ro_lock_show;
		md->power_ro_lock.store = power_ro_lock_store;
		sysfs_attr_init(&md->power_ro_lock.attr);
		md->power_ro_lock.attr.mode = mode;
		md->power_ro_lock.attr.name =
					"ro_lock_until_next_power_on";
		ret = device_create_file(disk_to_dev(md->disk),
				&md->power_ro_lock);
		if (ret)
			goto power_ro_lock_fail;
	}
	return ret;

power_ro_lock_fail:
	device_remove_file(disk_to_dev(md->disk), &md->force_ro);
force_ro_fail:
	del_gendisk(md->disk);

	return ret;
}

static struct mmc_blk_data *mmc_blk_alloc(struct mmc_card *card)
{
	sector_t size;

	if (!mmc_card_sd(card) && mmc_card_blockaddr(card)) {
		/*
		 * The EXT_CSD sector count is in number or 512 byte
		 * sectors.
		 */
		size = card->ext_csd.sectors;
	} else {
		/*
		 * The CSD capacity field is in units of read_blkbits.
		 * set_capacity takes units of 512 bytes.
		 */
		size = (typeof(sector_t))card->csd.capacity
			<< (card->csd.read_blkbits - 9);
	}

	return mmc_blk_alloc_req(card, &card->dev, size, false, NULL,
					MMC_BLK_DATA_AREA_MAIN);
}

/*
 * There is one mmc_blk_data per slot.
 */
struct mmc_blk_data {
	struct device	*parent;
	struct gendisk	*disk;
	struct mmc_queue queue;
	struct list_head part;
	struct list_head rpmbs;

	unsigned int	flags;
#define MMC_BLK_CMD23	(1 << 0)	/* Can do SET_BLOCK_COUNT for multiblock */
#define MMC_BLK_REL_WR	(1 << 1)	/* MMC Reliable write support */

	unsigned int	usage;
	unsigned int	read_only;
	unsigned int	part_type;
	unsigned int	reset_done;
#define MMC_BLK_READ		BIT(0)
#define MMC_BLK_WRITE		BIT(1)
#define MMC_BLK_DISCARD		BIT(2)
#define MMC_BLK_SECDISCARD	BIT(3)
#define MMC_BLK_CQE_RECOVERY	BIT(4)

	/*
	 * Only set in main mmc_blk_data associated
	 * with mmc_card with dev_set_drvdata, and keeps
	 * track of the current selected device partition.
	 */
	unsigned int	part_curr;
	struct device_attribute force_ro;
	struct device_attribute power_ro_lock;
	int	area_type;

	/* debugfs files (only in main mmc_blk_data) */
	struct dentry *status_dentry;
	struct dentry *ext_csd_dentry;
};

static struct mmc_blk_data *mmc_blk_alloc_req(struct mmc_card *card,
					      struct device *parent,
					      sector_t size,
					      bool default_ro,
					      const char *subname,
					      int area_type)
{
	struct mmc_blk_data *md;
	int devidx, ret;

	devidx = ida_simple_get(&mmc_blk_ida, 0, max_devices, GFP_KERNEL);
	if (devidx < 0) {
		/*
		 * We get -ENOSPC because there are no more any available
		 * devidx. The reason may be that, either userspace haven't yet
		 * unmounted the partitions, which postpones mmc_blk_release()
		 * from being called, or the device has more partitions than
		 * what we support.
		 */
		if (devidx == -ENOSPC)
			dev_err(mmc_dev(card->host),
				"no more device IDs available\n");

		return ERR_PTR(devidx);
	}

	md = kzalloc(sizeof(struct mmc_blk_data), GFP_KERNEL);
	if (!md) {
		ret = -ENOMEM;
		goto out;
	}

	md->area_type = area_type;

	/*
	 * Set the read-only status based on the supported commands
	 * and the write protect switch.
	 */
	md->read_only = mmc_blk_readonly(card);

	md->disk = alloc_disk(perdev_minors);
	if (md->disk == NULL) {
		ret = -ENOMEM;
		goto err_kfree;
	}

	INIT_LIST_HEAD(&md->part);
	INIT_LIST_HEAD(&md->rpmbs);
	md->usage = 1;

	ret = mmc_init_queue(&md->queue, card);
	if (ret)
		goto err_putdisk;

	md->queue.blkdata = md;

	/*
	 * Keep an extra reference to the queue so that we can shutdown the
	 * queue (i.e. call blk_cleanup_queue()) while there are still
	 * references to the 'md'. The corresponding blk_put_queue() is in
	 * mmc_blk_put().
	 */
	if (!blk_get_queue(md->queue.queue)) {
		mmc_cleanup_queue(&md->queue);
		ret = -ENODEV;
		goto err_putdisk;
	}

	md->disk->major	= MMC_BLOCK_MAJOR;
	md->disk->first_minor = devidx * perdev_minors;
	md->disk->fops = &mmc_bdops;
	md->disk->private_data = md;
	md->disk->queue = md->queue.queue;
	md->parent = parent;
	set_disk_ro(md->disk, md->read_only || default_ro);
	md->disk->flags = GENHD_FL_EXT_DEVT;
	if (area_type & (MMC_BLK_DATA_AREA_RPMB | MMC_BLK_DATA_AREA_BOOT))
		md->disk->flags |= GENHD_FL_NO_PART_SCAN
				   | GENHD_FL_SUPPRESS_PARTITION_INFO;

	/*
	 * As discussed on lkml, GENHD_FL_REMOVABLE should:
	 *
	 * - be set for removable media with permanent block devices
	 * - be unset for removable block devices with permanent media
	 *
	 * Since MMC block devices clearly fall under the second
	 * case, we do not set GENHD_FL_REMOVABLE.  Userspace
	 * should use the block device creation/destruction hotplug
	 * messages to tell when the card is present.
	 */

	snprintf(md->disk->disk_name, sizeof(md->disk->disk_name),
		 "mmcblk%u%s", card->host->index, subname ? subname : "");

	set_capacity(md->disk, size);

	if (mmc_host_cmd23(card->host)) {
		if ((mmc_card_mmc(card) &&
		     card->csd.mmca_vsn >= CSD_SPEC_VER_3) ||
		    (mmc_card_sd(card) &&
		     card->scr.cmds & SD_SCR_CMD23_SUPPORT))
			md->flags |= MMC_BLK_CMD23;
	}

	if (mmc_card_mmc(card) &&
	    md->flags & MMC_BLK_CMD23 &&
	    ((card->ext_csd.rel_param & EXT_CSD_WR_REL_PARAM_EN) ||
	     card->ext_csd.rel_sectors)) {
		md->flags |= MMC_BLK_REL_WR;
		blk_queue_write_cache(md->queue.queue, true, true);
	}

	return md;

 err_putdisk:
	put_disk(md->disk);
 err_kfree:
	kfree(md);
 out:
	ida_simple_remove(&mmc_blk_ida, devidx);
	return ERR_PTR(ret);
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

	WARN_ON_ONCE(op & REQ_NOWAIT);
	WARN_ON_ONCE(flags & ~(BLK_MQ_REQ_NOWAIT | BLK_MQ_REQ_PREEMPT));

	req = blk_mq_alloc_request(q, op, flags);
	if (!IS_ERR(req) && q->mq_ops->initialize_rq_fn)
		q->mq_ops->initialize_rq_fn(req);

	return req;
}
EXPORT_SYMBOL(blk_get_request);

void blk_put_request(struct request *req)
{
	blk_mq_free_request(req);
}
EXPORT_SYMBOL(blk_put_request);

/**
 * blk_execute_rq - insert a request into queue for execution
 * @q:		queue to insert the request in
 * @bd_disk:	matching gendisk
 * @rq:		request to insert
 * @at_head:    insert request at head or tail of queue
 *
 * Description:
 *    Insert a fully prepared request at the back of the I/O scheduler queue
 *    for execution and wait for completion.
 */
void blk_execute_rq(struct request_queue *q, struct gendisk *bd_disk,
		   struct request *rq, int at_head)
{
	DECLARE_COMPLETION_ONSTACK(wait);
	unsigned long hang_check;

	rq->end_io_data = &wait;
	blk_execute_rq_nowait(q, bd_disk, rq, at_head, blk_end_sync_rq);

	/* Prevent hang_check timer from firing at us during very long I/O */
	hang_check = sysctl_hung_task_timeout_secs;
	if (hang_check)
		while (!wait_for_completion_io_timeout(&wait, hang_check * (HZ/2)));
	else
		wait_for_completion_io(&wait);
}
EXPORT_SYMBOL(blk_execute_rq);


blk_mq_make_request

static blk_qc_t blk_mq_make_request(struct request_queue *q, struct bio *bio)
{
	const int is_sync = op_is_sync(bio->bi_opf);
	const int is_flush_fua = op_is_flush(bio->bi_opf);
	struct blk_mq_alloc_data data = { .flags = 0};
	struct request *rq;
	struct blk_plug *plug;
	struct request *same_queue_rq = NULL;
	unsigned int nr_segs;
	blk_qc_t cookie;

	blk_queue_bounce(q, &bio);
	__blk_queue_split(q, &bio, &nr_segs);

	if (!bio_integrity_prep(bio))
		return BLK_QC_T_NONE;

	if (!is_flush_fua && !blk_queue_nomerges(q) &&
	    blk_attempt_plug_merge(q, bio, nr_segs, &same_queue_rq))
		return BLK_QC_T_NONE;

	if (blk_mq_sched_bio_merge(q, bio, nr_segs))
		return BLK_QC_T_NONE;

	rq_qos_throttle(q, bio);

	data.cmd_flags = bio->bi_opf;
	rq = blk_mq_get_request(q, bio, &data);
	if (unlikely(!rq)) {
		rq_qos_cleanup(q, bio);
		if (bio->bi_opf & REQ_NOWAIT)
			bio_wouldblock_error(bio);
		return BLK_QC_T_NONE;
	}

	trace_block_getrq(q, bio, bio->bi_opf);

	rq_qos_track(q, rq, bio);

	cookie = request_to_qc_t(data.hctx, rq);

	blk_mq_bio_to_request(rq, bio, nr_segs);

	plug = blk_mq_plug(q, bio);
	if (unlikely(is_flush_fua)) {
		/* bypass scheduler for flush rq */
		blk_insert_flush(rq);
		blk_mq_run_hw_queue(data.hctx, true);
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
	} else if (q->elevator) {
		blk_mq_sched_insert_request(rq, false, true, true);
	} else if (plug && !blk_queue_nomerges(q)) {
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

[   42.960719] CPU: 1 PID: 68 Comm: sh Not tainted 5.4.0+ #174
[   42.961063] Hardware name: linux,dummy-virt (DT)
[   42.961394] Call trace:
[   42.961546]  dump_backtrace+0x0/0x108
[   42.961718]  show_stack+0x14/0x20
[   42.961883]  dump_stack+0xb0/0xd8
[   42.962039]  virtio_queue_rq+0x38/0x3e4
[   42.962236]  blk_mq_dispatch_rq_list+0x70/0x588
[   42.962594]  blk_mq_do_dispatch_sched+0x48/0xd0
[   42.962820]  blk_mq_sched_dispatch_requests+0x128/0x1a0
[   42.963262]  __blk_mq_run_hw_queue+0x8c/0x108
[   42.963749]  __blk_mq_delay_run_hw_queue+0x1d0/0x1d8
[   42.964128]  blk_mq_run_hw_queue+0x94/0x110
[   42.964411]  blk_mq_sched_insert_requests+0x84/0x150
[   42.964672]  blk_mq_flush_plug_list+0x13c/0x168
[   42.964900]  blk_flush_plug_list+0xbc/0xd0
[   42.965125]  blk_finish_plug+0x30/0x40
[   42.965485]  ext4_writepages+0x390/0xa28
[   42.966004]  do_writepages+0x34/0xd8
[   42.966325]  __filemap_fdatawrite_range+0x74/0x98
[   42.966536]  filemap_flush+0x18/0x20
[   42.966698]  ext4_alloc_da_blocks+0x20/0x28
[   42.966878]  ext4_release_file+0x70/0xc8
[   42.967057]  __fput+0x48/0x128
[   42.967240]  ____fput+0xc/0x18
[   42.967385]  task_work_run+0xc4/0xf8
[   42.967559]  do_notify_resume+0x1f0/0x278
[   42.967763]  work_pending+0x8/0x10

[   45.168559] CPU: 2 PID: 55 Comm: jbd2/vda-8 Not tainted 5.4.0+ #174
[   45.169056] Hardware name: linux,dummy-virt (DT)
[   45.169507] Call trace:
[   45.169803]  dump_backtrace+0x0/0x108
[   45.170312]  show_stack+0x14/0x20
[   45.170678]  dump_stack+0xb0/0xd8
[   45.170929]  virtio_queue_rq+0x38/0x3e4
[   45.171288]  blk_mq_dispatch_rq_list+0x70/0x588
[   45.172085]  blk_mq_do_dispatch_sched+0x48/0xd0
[   45.172470]  blk_mq_sched_dispatch_requests+0x128/0x1a0
[   45.173071]  __blk_mq_run_hw_queue+0x8c/0x108
[   45.173541]  __blk_mq_delay_run_hw_queue+0x1d0/0x1d8
[   45.173941]  blk_mq_run_hw_queue+0x94/0x110
[   45.174324]  blk_mq_sched_insert_requests+0x84/0x150
[   45.174671]  blk_mq_flush_plug_list+0x13c/0x168
[   45.175176]  blk_flush_plug_list+0xbc/0xd0
[   45.175751]  blk_finish_plug+0x30/0x40
[   45.176166]  jbd2_journal_commit_transaction+0x9bc/0x1788
[   45.176619]  kjournald2+0xd4/0x248
[   45.176927]  kthread+0x114/0x118
[   45.177197]  ret_from_fork+0x10/0x18

[   45.180325] CPU: 2 PID: 58 Comm: kworker/2:1H Not tainted 5.4.0+ #174
[   45.181138] Hardware name: linux,dummy-virt (DT)
[   45.181782] Workqueue: kblockd blk_mq_requeue_work
[   45.182346] Call trace:
[   45.182715]  dump_backtrace+0x0/0x108
[   45.183195]  show_stack+0x14/0x20
[   45.183763]  dump_stack+0xb0/0xd8
[   45.184758]  virtio_queue_rq+0x38/0x3e4
[   45.185530]  blk_mq_dispatch_rq_list+0x70/0x588
[   45.185976]  blk_mq_sched_dispatch_requests+0x114/0x1a0
[   45.186387]  __blk_mq_run_hw_queue+0x8c/0x108
[   45.187117]  __blk_mq_delay_run_hw_queue+0x1d0/0x1d8
[   45.188171]  blk_mq_run_hw_queue+0x94/0x110
[   45.188653]  blk_mq_run_hw_queues+0x40/0x68
[   45.189118]  blk_mq_requeue_work+0x168/0x180
[   45.189444]  process_one_work+0x1d0/0x330
[   45.190042]  worker_thread+0x4c/0x458
[   45.190404]  kthread+0x114/0x118
[   45.190823]  ret_from_fork+0x10/0x18

/*
 * Softirq action handler - move entries to local list and loop over them
 * while passing them to the queue registered handler.
 */
static __latent_entropy void blk_done_softirq(struct softirq_action *h)
{
	struct list_head *cpu_list, local_list;

	local_irq_disable();
	cpu_list = this_cpu_ptr(&blk_cpu_done);
	list_replace_init(cpu_list, &local_list);
	local_irq_enable();

	while (!list_empty(&local_list)) {
		struct request *rq;

		rq = list_entry(local_list.next, struct request, ipi_list);
		list_del_init(&rq->ipi_list);
		rq->q->mq_ops->complete(rq);
	}
}

/*
 * Setup and invoke a run of 'trigger_softirq' on the given cpu.
 */
static int raise_blk_irq(int cpu, struct request *rq)
{
	if (cpu_online(cpu)) {
		call_single_data_t *data = &rq->csd;

		data->func = trigger_softirq;
		data->info = rq;
		data->flags = 0;

		smp_call_function_single_async(cpu, data);
		return 0;
	}

	return 1;
}
#else /* CONFIG_SMP */
static int raise_blk_irq(int cpu, struct request *rq)
{
	return 1;
}
#endif

void __blk_complete_request(struct request *req)
{
	struct request_queue *q = req->q;
	int cpu, ccpu = req->mq_ctx->cpu;
	unsigned long flags;
	bool shared = false;

	BUG_ON(!q->mq_ops->complete);

	local_irq_save(flags);
	cpu = smp_processor_id();

	/*
	 * Select completion CPU
	 */
	if (test_bit(QUEUE_FLAG_SAME_COMP, &q->queue_flags) && ccpu != -1) {
		if (!test_bit(QUEUE_FLAG_SAME_FORCE, &q->queue_flags))
			shared = cpus_share_cache(cpu, ccpu);
	} else
		ccpu = cpu;

	/*
	 * If current CPU and requested CPU share a cache, run the softirq on
	 * the current CPU. One might concern this is just like
	 * QUEUE_FLAG_SAME_FORCE, but actually not. blk_complete_request() is
	 * running in interrupt handler, and currently I/O controller doesn't
	 * support multiple interrupts, so current CPU is unique actually. This
	 * avoids IPI sending from current CPU to the first CPU of a group.
	 */
	if (ccpu == cpu || shared) {
		struct list_head *list;
do_local:
		list = this_cpu_ptr(&blk_cpu_done);
		list_add_tail(&req->ipi_list, list);

		/*
		 * if the list only contains our just added request,
		 * signal a raise of the softirq. If there are already
		 * entries there, someone already raised the irq but it
		 * hasn't run yet.
		 */
		if (list->next == &req->ipi_list)
			raise_softirq_irqoff(BLOCK_SOFTIRQ);
	} else if (raise_blk_irq(ccpu, req))
		goto do_local;

	local_irq_restore(flags);
}

static void __blk_mq_complete_request(struct request *rq)
{
	struct blk_mq_ctx *ctx = rq->mq_ctx;
	struct request_queue *q = rq->q;
	bool shared = false;
	int cpu;

	WRITE_ONCE(rq->state, MQ_RQ_COMPLETE);
	/*
	 * Most of single queue controllers, there is only one irq vector
	 * for handling IO completion, and the only irq's affinity is set
	 * as all possible CPUs. On most of ARCHs, this affinity means the
	 * irq is handled on one specific CPU.
	 *
	 * So complete IO reqeust in softirq context in case of single queue
	 * for not degrading IO performance by irqsoff latency.
	 */
	if (q->nr_hw_queues == 1) {
		__blk_complete_request(rq);
		return;
	}

	/*
	 * For a polled request, always complete locallly, it's pointless
	 * to redirect the completion.
	 */
	if ((rq->cmd_flags & REQ_HIPRI) ||
	    !test_bit(QUEUE_FLAG_SAME_COMP, &q->queue_flags)) {
		q->mq_ops->complete(rq);
		return;
	}

	cpu = get_cpu();
	if (!test_bit(QUEUE_FLAG_SAME_FORCE, &q->queue_flags))
		shared = cpus_share_cache(cpu, ctx->cpu);

	if (cpu != ctx->cpu && !shared && cpu_online(ctx->cpu)) {
		rq->csd.func = __blk_mq_complete_request_remote;
		rq->csd.info = rq;
		rq->csd.flags = 0;
		smp_call_function_single_async(ctx->cpu, &rq->csd);
	} else {
		q->mq_ops->complete(rq);
	}
	put_cpu();
}

/**
 * blk_mq_complete_request - end I/O on a request
 * @rq:		the request being processed
 *
 * Description:
 *	Ends all I/O on a request. It does not handle partial completions.
 *	The actual completion happens out-of-order, through a IPI handler.
 **/
bool blk_mq_complete_request(struct request *rq)
{
	if (unlikely(blk_should_fake_timeout(rq->q)))
		return false;
	__blk_mq_complete_request(rq);
	return true;
}
EXPORT_SYMBOL(blk_mq_complete_request);

static void virtblk_done(struct virtqueue *vq)
{
	struct virtio_blk *vblk = vq->vdev->priv;
	bool req_done = false;
	int qid = vq->index;
	struct virtblk_req *vbr;
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&vblk->vqs[qid].lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((vbr = virtqueue_get_buf(vblk->vqs[qid].vq, &len)) != NULL) {
			struct request *req = blk_mq_rq_from_pdu(vbr);

			blk_mq_complete_request(req);
			req_done = true;
		}
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));

	/* In case queue is stopped waiting for more buffers. */
	if (req_done)
		blk_mq_start_stopped_hw_queues(vblk->disk->queue, true);
	spin_unlock_irqrestore(&vblk->vqs[qid].lock, flags);
}

irqreturn_t vring_interrupt(int irq, void *_vq)
{
	struct vring_virtqueue *vq = to_vvq(_vq);

	if (!more_used(vq)) {
		pr_debug("virtqueue interrupt with no work for %p\n", vq);
		return IRQ_NONE;
	}

	if (unlikely(vq->broken))
		return IRQ_HANDLED;

	pr_debug("virtqueue callback for %p (%p)\n", vq, vq->vq.callback);
	if (vq->vq.callback)
		vq->vq.callback(&vq->vq);

	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(vring_interrupt);

/* Notify all virtqueues on an interrupt. */
static irqreturn_t vm_interrupt(int irq, void *opaque)
{
	struct virtio_mmio_device *vm_dev = opaque;
	struct virtio_mmio_vq_info *info;
	unsigned long status;
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;

	/* Read and acknowledge interrupts */
	status = readl(vm_dev->base + VIRTIO_MMIO_INTERRUPT_STATUS);
	writel(status, vm_dev->base + VIRTIO_MMIO_INTERRUPT_ACK);

	if (unlikely(status & VIRTIO_MMIO_INT_CONFIG)) {
		virtio_config_changed(&vm_dev->vdev);
		ret = IRQ_HANDLED;
	}

	if (likely(status & VIRTIO_MMIO_INT_VRING)) {
		spin_lock_irqsave(&vm_dev->lock, flags);
		list_for_each_entry(info, &vm_dev->virtqueues, node)
			ret |= vring_interrupt(irq, info->vq);
		spin_unlock_irqrestore(&vm_dev->lock, flags);
	}

	return ret;
}

static int vm_find_vqs(struct virtio_device *vdev, unsigned nvqs,
		       struct virtqueue *vqs[],
		       vq_callback_t *callbacks[],
		       const char * const names[],
		       const bool *ctx,
		       struct irq_affinity *desc)
{
	struct virtio_mmio_device *vm_dev = to_virtio_mmio_device(vdev);
	int irq = platform_get_irq(vm_dev->pdev, 0);
	int i, err, queue_idx = 0;

	if (irq < 0) {
		dev_err(&vdev->dev, "Cannot get IRQ resource\n");
		return irq;
	}

	err = request_irq(irq, vm_interrupt, IRQF_SHARED,
			dev_name(&vdev->dev), vm_dev);
	if (err)
		return err;

	for (i = 0; i < nvqs; ++i) {
		if (!names[i]) {
			vqs[i] = NULL;
			continue;
		}

		vqs[i] = vm_setup_vq(vdev, queue_idx++, callbacks[i], names[i],
				     ctx ? ctx[i] : false);
		if (IS_ERR(vqs[i])) {
			vm_del_vqs(vdev);
			return PTR_ERR(vqs[i]);
		}
	}

	return 0;
}

static inline void virtblk_request_done(struct request *req)
{
	struct virtblk_req *vbr = blk_mq_rq_to_pdu(req);

	if (req->rq_flags & RQF_SPECIAL_PAYLOAD) {
		kfree(page_address(req->special_vec.bv_page) +
		      req->special_vec.bv_offset);
	}

	switch (req_op(req)) {
	case REQ_OP_SCSI_IN:
	case REQ_OP_SCSI_OUT:
		virtblk_scsi_request_done(req);
		break;
	}

	blk_mq_end_request(req, virtblk_result(vbr));
}

void blk_mq_end_request(struct request *rq, blk_status_t error)
{
	if (blk_update_request(rq, error, blk_rq_bytes(rq)))
		BUG();
	__blk_mq_end_request(rq, error);
}
EXPORT_SYMBOL(blk_mq_end_request);

inline void __blk_mq_end_request(struct request *rq, blk_status_t error)
{
	u64 now = 0;

	if (blk_mq_need_time_stamp(rq))
		now = ktime_get_ns();

	if (rq->rq_flags & RQF_STATS) {
		blk_mq_poll_stats_start(rq->q);
		blk_stat_add(rq, now);
	}

	if (rq->internal_tag != -1)
		blk_mq_sched_completed_request(rq, now);

	blk_account_io_done(rq, now);

	if (rq->end_io) {
		rq_qos_done(rq->q, rq);
		rq->end_io(rq, error);
	} else {
		blk_mq_free_request(rq);
	}
}
EXPORT_SYMBOL(__blk_mq_end_request);


static inline struct request_queue *bdev_get_queue(struct block_device *bdev)
{
	return bdev->bd_disk->queue;	/* this is never NULL */
}

/*
 * main unit of I/O for the block layer and lower layers (ie drivers and
 * stacking drivers)
 */
struct bio {
	struct bio		*bi_next;	/* request queue link */
	struct gendisk		*bi_disk;
	unsigned int		bi_opf;		/* bottom bits req flags,
						 * top bits REQ_OP. Use
						 * accessors.
						 */
	unsigned short		bi_flags;	/* status, etc and bvec pool number */
	unsigned short		bi_ioprio;
	unsigned short		bi_write_hint;
	blk_status_t		bi_status;
	u8			bi_partno;

	struct bvec_iter	bi_iter;

	atomic_t		__bi_remaining;
	bio_end_io_t		*bi_end_io;

	void			*bi_private;
#ifdef CONFIG_BLK_CGROUP
	/*
	 * Represents the association of the css and request_queue for the bio.
	 * If a bio goes direct to device, it will not have a blkg as it will
	 * not have a request_queue associated with it.  The reference is put
	 * on release of the bio.
	 */
	struct blkcg_gq		*bi_blkg;
	struct bio_issue	bi_issue;
#ifdef CONFIG_BLK_CGROUP_IOCOST
	u64			bi_iocost_cost;
#endif
#endif
	union {
#if defined(CONFIG_BLK_DEV_INTEGRITY)
		struct bio_integrity_payload *bi_integrity; /* data integrity */
#endif
	};

	unsigned short		bi_vcnt;	/* how many bio_vec's */

	/*
	 * Everything starting with bi_max_vecs will be preserved by bio_reset()
	 */

	unsigned short		bi_max_vecs;	/* max bvl_vecs we can hold */

	atomic_t		__bi_cnt;	/* pin count */

	struct bio_vec		*bi_io_vec;	/* the actual vec list */

	struct bio_set		*bi_pool;

	/*
	 * We can inline a number of vecs at the end of the bio, to avoid
	 * double allocations for a small number of bio_vecs. This member
	 * MUST obviously be kept at the very end of the bio.
	 */
	struct bio_vec		bi_inline_vecs[0];
};

/**
 * generic_make_request - hand a buffer to its device driver for I/O
 * @bio:  The bio describing the location in memory and on the device.
 *
 * generic_make_request() is used to make I/O requests of block
 * devices. It is passed a &struct bio, which describes the I/O that needs
 * to be done.
 *
 * generic_make_request() does not return any status.  The
 * success/failure status of the request, along with notification of
 * completion, is delivered asynchronously through the bio->bi_end_io
 * function described (one day) else where.
 *
 * The caller of generic_make_request must make sure that bi_io_vec
 * are set to describe the memory buffer, and that bi_dev and bi_sector are
 * set to describe the device address, and the
 * bi_end_io and optionally bi_private are set to describe how
 * completion notification should be signaled.
 *
 * generic_make_request and the drivers it calls may use bi_next if this
 * bio happens to be merged with someone else, and may resubmit the bio to
 * a lower device by calling into generic_make_request recursively, which
 * means the bio should NOT be touched after the call to ->make_request_fn.
 */
blk_qc_t generic_make_request(struct bio *bio)
{
	/*
	 * bio_list_on_stack[0] contains bios submitted by the current
	 * make_request_fn.
	 * bio_list_on_stack[1] contains bios that were submitted before
	 * the current make_request_fn, but that haven't been processed
	 * yet.
	 */
	struct bio_list bio_list_on_stack[2];
	blk_qc_t ret = BLK_QC_T_NONE;

	if (!generic_make_request_checks(bio))
		goto out;

	/*
	 * We only want one ->make_request_fn to be active at a time, else
	 * stack usage with stacked devices could be a problem.  So use
	 * current->bio_list to keep a list of requests submited by a
	 * make_request_fn function.  current->bio_list is also used as a
	 * flag to say if generic_make_request is currently active in this
	 * task or not.  If it is NULL, then no make_request is active.  If
	 * it is non-NULL, then a make_request is active, and new requests
	 * should be added at the tail
	 */
	if (current->bio_list) {
		bio_list_add(&current->bio_list[0], bio);
		goto out;
	}

	/* following loop may be a bit non-obvious, and so deserves some
	 * explanation.
	 * Before entering the loop, bio->bi_next is NULL (as all callers
	 * ensure that) so we have a list with a single bio.
	 * We pretend that we have just taken it off a longer list, so
	 * we assign bio_list to a pointer to the bio_list_on_stack,
	 * thus initialising the bio_list of new bios to be
	 * added.  ->make_request() may indeed add some more bios
	 * through a recursive call to generic_make_request.  If it
	 * did, we find a non-NULL value in bio_list and re-enter the loop
	 * from the top.  In this case we really did just take the bio
	 * of the top of the list (no pretending) and so remove it from
	 * bio_list, and call into ->make_request() again.
	 */
	BUG_ON(bio->bi_next);
	bio_list_init(&bio_list_on_stack[0]);
	current->bio_list = bio_list_on_stack;
	do {
		struct request_queue *q = bio->bi_disk->queue;                   // struct gendisk *bi_disk;
		blk_mq_req_flags_t flags = bio->bi_opf & REQ_NOWAIT ?
			BLK_MQ_REQ_NOWAIT : 0;

		if (likely(blk_queue_enter(q, flags) == 0)) {
			struct bio_list lower, same;

			/* Create a fresh bio_list for all subordinate requests */
			bio_list_on_stack[1] = bio_list_on_stack[0];
			bio_list_init(&bio_list_on_stack[0]);
			ret = q->make_request_fn(q, bio);

			blk_queue_exit(q);

			/* sort new bios into those for a lower level
			 * and those for the same level
			 */
			bio_list_init(&lower);
			bio_list_init(&same);
			while ((bio = bio_list_pop(&bio_list_on_stack[0])) != NULL)
				if (q == bio->bi_disk->queue)
					bio_list_add(&same, bio);
				else
					bio_list_add(&lower, bio);
			/* now assemble so we handle the lowest level first */
			bio_list_merge(&bio_list_on_stack[0], &lower);
			bio_list_merge(&bio_list_on_stack[0], &same);
			bio_list_merge(&bio_list_on_stack[0], &bio_list_on_stack[1]);
		} else {
			if (unlikely(!blk_queue_dying(q) &&
					(bio->bi_opf & REQ_NOWAIT)))
				bio_wouldblock_error(bio);
			else
				bio_io_error(bio);
		}
		bio = bio_list_pop(&bio_list_on_stack[0]);
	} while (bio);
	current->bio_list = NULL; /* deactivate */

out:
	return ret;
}
EXPORT_SYMBOL(generic_make_request);

void
prepare_to_wait_exclusive(struct wait_queue_head *wq_head, struct wait_queue_entry *wq_entry, int state)
{
	unsigned long flags;

	wq_entry->flags |= WQ_FLAG_EXCLUSIVE;
	spin_lock_irqsave(&wq_head->lock, flags);
	if (list_empty(&wq_entry->entry))
		__add_wait_queue_entry_tail(wq_head, wq_entry);
	set_current_state(state);
	spin_unlock_irqrestore(&wq_head->lock, flags);
}
EXPORT_SYMBOL(prepare_to_wait_exclusive);

static bool wbt_inflight_cb(struct rq_wait *rqw, void *private_data)
{
	struct wbt_wait_data *data = private_data;
	return rq_wait_inc_below(rqw, get_limit(data->rwb, data->rw));
}

/**
 * finish_wait - clean up after waiting in a queue
 * @wq_head: waitqueue waited on
 * @wq_entry: wait descriptor
 *
 * Sets current thread back to running state and removes
 * the wait descriptor from the given waitqueue if still
 * queued.
 */
void finish_wait(struct wait_queue_head *wq_head, struct wait_queue_entry *wq_entry)
{
	unsigned long flags;

	__set_current_state(TASK_RUNNING);
	/*
	 * We can check for list emptiness outside the lock
	 * IFF:
	 *  - we use the "careful" check that verifies both
	 *    the next and prev pointers, so that there cannot
	 *    be any half-pending updates in progress on other
	 *    CPU's that we haven't seen yet (and that might
	 *    still change the stack area.
	 * and
	 *  - all other users take the lock (ie we can only
	 *    have _one_ other CPU that looks at or modifies
	 *    the list).
	 */
	if (!list_empty_careful(&wq_entry->entry)) {
		spin_lock_irqsave(&wq_head->lock, flags);
		list_del_init(&wq_entry->entry);
		spin_unlock_irqrestore(&wq_head->lock, flags);
	}
}

/**
 * rq_qos_wait - throttle on a rqw if we need to
 * @rqw: rqw to throttle on
 * @private_data: caller provided specific data
 * @acquire_inflight_cb: inc the rqw->inflight counter if we can
 * @cleanup_cb: the callback to cleanup in case we race with a waker
 *
 * This provides a uniform place for the rq_qos users to do their throttling.
 * Since you can end up with a lot of things sleeping at once, this manages the
 * waking up based on the resources available.  The acquire_inflight_cb should
 * inc the rqw->inflight if we have the ability to do so, or return false if not
 * and then we will sleep until the room becomes available.
 *
 * cleanup_cb is in case that we race with a waker and need to cleanup the
 * inflight count accordingly.
 */
void rq_qos_wait(struct rq_wait *rqw, void *private_data,
		 acquire_inflight_cb_t *acquire_inflight_cb,
		 cleanup_cb_t *cleanup_cb)
{
	struct rq_qos_wait_data data = {
		.wq = {
			.func	= rq_qos_wake_function,
			.entry	= LIST_HEAD_INIT(data.wq.entry),
		},
		.task = current,
		.rqw = rqw,
		.cb = acquire_inflight_cb,
		.private_data = private_data,
	};
	bool has_sleeper;

	has_sleeper = wq_has_sleeper(&rqw->wait);
	if (!has_sleeper && acquire_inflight_cb(rqw, private_data))
		return;

	prepare_to_wait_exclusive(&rqw->wait, &data.wq, TASK_UNINTERRUPTIBLE);
	has_sleeper = !wq_has_single_sleeper(&rqw->wait);
	do {
		/* The memory barrier in set_task_state saves us here. */
		if (data.got_token)
			break;
		if (!has_sleeper && acquire_inflight_cb(rqw, private_data)) {
			finish_wait(&rqw->wait, &data.wq);

			/*
			 * We raced with wbt_wake_function() getting a token,
			 * which means we now have two. Put our local token
			 * and wake anyone else potentially waiting for one.
			 */
			smp_rmb();
			if (data.got_token)
				cleanup_cb(rqw, private_data);
			break;
		}
		io_schedule();
		has_sleeper = true;
		set_current_state(TASK_UNINTERRUPTIBLE);
	} while (1);
	finish_wait(&rqw->wait, &data.wq);
}

/*
 * Block if we will exceed our limit, or if we are currently waiting for
 * the timer to kick off queuing again.
 */
static void __wbt_wait(struct rq_wb *rwb, enum wbt_flags wb_acct,
		       unsigned long rw)
{
	struct rq_wait *rqw = get_rq_wait(rwb, wb_acct);
	struct wbt_wait_data data = {
		.rwb = rwb,
		.wb_acct = wb_acct,
		.rw = rw,
	};

	rq_qos_wait(rqw, &data, wbt_inflight_cb, wbt_cleanup_cb);
}



/* linux-4.19 */
/*
 * Block if we will exceed our limit, or if we are currently waiting for
 * the timer to kick off queuing again.
 */
static void __wbt_wait(struct rq_wb *rwb, enum wbt_flags wb_acct,
		       unsigned long rw, spinlock_t *lock)
	__releases(lock)
	__acquires(lock)
{
	struct rq_wait *rqw = get_rq_wait(rwb, wb_acct);
	struct wbt_wait_data data = {
		.wq = {
			.func	= wbt_wake_function,
			.entry	= LIST_HEAD_INIT(data.wq.entry),
		},
		.task = current,
		.rwb = rwb,
		.rqw = rqw,
		.rw = rw,
	};
	bool has_sleeper;

	has_sleeper = wq_has_sleeper(&rqw->wait);
	if (!has_sleeper && rq_wait_inc_below(rqw, get_limit(rwb, rw)))
		return;

	prepare_to_wait_exclusive(&rqw->wait, &data.wq, TASK_UNINTERRUPTIBLE);
	do {
		if (data.got_token)
			break;

		if (!has_sleeper &&
		    rq_wait_inc_below(rqw, get_limit(rwb, rw))) {
			finish_wait(&rqw->wait, &data.wq);

			/*
			 * We raced with wbt_wake_function() getting a token,
			 * which means we now have two. Put our local token
			 * and wake anyone else potentially waiting for one.
			 */
			if (data.got_token)
				wbt_rqw_done(rwb, rqw, wb_acct);
			break;
		}

		if (lock) {
			spin_unlock_irq(lock);
			io_schedule();
			spin_lock_irq(lock);
		} else
			io_schedule();

		has_sleeper = false;
	} while (1);

	finish_wait(&rqw->wait, &data.wq);
}


/*
 * Returns true if the IO request should be accounted, false if not.
 * May sleep, if we have exceeded the writeback limits. Caller can pass
 * in an irq held spinlock, if it holds one when calling this function.
 * If we do sleep, we'll release and re-grab it.
 */
static void wbt_wait(struct rq_qos *rqos, struct bio *bio)
{
	struct rq_wb *rwb = RQWB(rqos);
	enum wbt_flags flags;

	flags = bio_to_wbt_flags(rwb, bio);
	if (!(flags & WBT_TRACKED)) {
		if (flags & WBT_READ)
			wb_timestamp(rwb, &rwb->last_issue);
		return;
	}

	__wbt_wait(rwb, flags, bio->bi_opf);

	if (!blk_stat_is_active(rwb->cb))
		rwb_arm_timer(rwb);
}

static struct rq_qos_ops wbt_rqos_ops = {
	.throttle = wbt_wait,
	.issue = wbt_issue,
	.track = wbt_track,
	.requeue = wbt_requeue,
	.done = wbt_done,
	.cleanup = wbt_cleanup,
	.queue_depth_changed = wbt_queue_depth_changed,
	.exit = wbt_exit,
#ifdef CONFIG_BLK_DEBUG_FS
	.debugfs_attrs = wbt_debugfs_attrs,
#endif
};

static struct rq_qos_ops ioc_rqos_ops = {
	.throttle = ioc_rqos_throttle,
	.merge = ioc_rqos_merge,
	.done_bio = ioc_rqos_done_bio,
	.done = ioc_rqos_done,
	.queue_depth_changed = ioc_rqos_queue_depth_changed,
	.exit = ioc_rqos_exit,
};

static struct rq_qos_ops blkcg_iolatency_ops = {
	.throttle = blkcg_iolatency_throttle,
	.done_bio = blkcg_iolatency_done_bio,
	.exit = blkcg_iolatency_exit,
};

void __rq_qos_throttle(struct rq_qos *rqos, struct bio *bio)        // rqos也是一个链表
{
	do {
		if (rqos->ops->throttle)
			rqos->ops->throttle(rqos, bio);
		rqos = rqos->next;
	} while (rqos);
}

static inline void bio_set_flag(struct bio *bio, unsigned int bit)
{
	bio->bi_flags |= (1U << bit);
}

static inline void rq_qos_throttle(struct request_queue *q, struct bio *bio)
{
	/*
	 * BIO_TRACKED lets controllers know that a bio went through the
	 * normal rq_qos path.
	 */
	bio_set_flag(bio, BIO_TRACKED);
	if (q->rq_qos)
		__rq_qos_throttle(q->rq_qos, bio);
}

static blk_qc_t blk_mq_make_request(struct request_queue *q, struct bio *bio)
{
	const int is_sync = op_is_sync(bio->bi_opf);
	const int is_flush_fua = op_is_flush(bio->bi_opf);
	struct blk_mq_alloc_data data = { .flags = 0};
	struct request *rq;
	struct blk_plug *plug;
	struct request *same_queue_rq = NULL;
	unsigned int nr_segs;
	blk_qc_t cookie;

	blk_queue_bounce(q, &bio);
	__blk_queue_split(q, &bio, &nr_segs);

	if (!bio_integrity_prep(bio))
		return BLK_QC_T_NONE;

	if (!is_flush_fua && !blk_queue_nomerges(q) &&
	    blk_attempt_plug_merge(q, bio, nr_segs, &same_queue_rq))
		return BLK_QC_T_NONE;

	if (blk_mq_sched_bio_merge(q, bio, nr_segs))
		return BLK_QC_T_NONE;

	rq_qos_throttle(q, bio);

	data.cmd_flags = bio->bi_opf;
	rq = blk_mq_get_request(q, bio, &data);
	if (unlikely(!rq)) {
		rq_qos_cleanup(q, bio);
		if (bio->bi_opf & REQ_NOWAIT)
			bio_wouldblock_error(bio);
		return BLK_QC_T_NONE;
	}

	trace_block_getrq(q, bio, bio->bi_opf);

	rq_qos_track(q, rq, bio);

	cookie = request_to_qc_t(data.hctx, rq);

	blk_mq_bio_to_request(rq, bio, nr_segs);

	plug = blk_mq_plug(q, bio);
	if (unlikely(is_flush_fua)) {
		/* bypass scheduler for flush rq */
		blk_insert_flush(rq);
		blk_mq_run_hw_queue(data.hctx, true);
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
	} else if (q->elevator) {
		blk_mq_sched_insert_request(rq, false, true, true);
	} else if (plug && !blk_queue_nomerges(q)) {
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


static inline struct request_queue *bdev_get_queue(struct block_device *bdev)
{
	return bdev->bd_disk->queue;	/* this is never NULL */
}

struct ext4_io_submit {
	struct writeback_control *io_wbc;
	struct bio		*io_bio;
	ext4_io_end_t		*io_end;
	sector_t		io_next_block;
};


void ext4_io_submit_init(struct ext4_io_submit *io,
			 struct writeback_control *wbc)
{
	io->io_wbc = wbc;
	io->io_bio = NULL;
	io->io_end = NULL;
}

ext4_io_end_t *ext4_init_io_end(struct inode *inode, gfp_t flags)
{
	ext4_io_end_t *io = kmem_cache_zalloc(io_end_cachep, flags);
	if (io) {
		io->inode = inode;
		INIT_LIST_HEAD(&io->list);
		atomic_set(&io->count, 1);
	}
	return io;
}

#define page_private(page)		((page)->private)

/* If we *know* page->private refers to buffer_heads */
#define page_buffers(page)					\
	({							\
		BUG_ON(!PagePrivate(page));			\
		((struct buffer_head *)page_private(page));	\
	})

int ext4_bio_write_page(struct ext4_io_submit *io,
			struct page *page,
			int len,
			struct writeback_control *wbc,
			bool keep_towrite)
{
	struct page *bounce_page = NULL;
	struct inode *inode = page->mapping->host;
	unsigned block_start;
	struct buffer_head *bh, *head;
	int ret = 0;
	int nr_submitted = 0;
	int nr_to_submit = 0;

	BUG_ON(!PageLocked(page));
	BUG_ON(PageWriteback(page));

	if (keep_towrite)
		set_page_writeback_keepwrite(page);
	else
		set_page_writeback(page);
	ClearPageError(page);

	/*
	 * Comments copied from block_write_full_page:
	 *
	 * The page straddles i_size.  It must be zeroed out on each and every
	 * writepage invocation because it may be mmapped.  "A file is mapped
	 * in multiples of the page size.  For a file that is not a multiple of
	 * the page size, the remaining memory is zeroed when mapped, and
	 * writes to that region are not written out to the file."
	 */
	if (len < PAGE_SIZE)
		zero_user_segment(page, len, PAGE_SIZE);
	/*
	 * In the first loop we prepare and mark buffers to submit. We have to
	 * mark all buffers in the page before submitting so that
	 * end_page_writeback() cannot be called from ext4_bio_end_io() when IO
	 * on the first buffer finishes and we are still working on submitting
	 * the second buffer.
	 */
	bh = head = page_buffers(page);
	do {
		block_start = bh_offset(bh);
		if (block_start >= len) {
			clear_buffer_dirty(bh);
			set_buffer_uptodate(bh);
			continue;
		}
		if (!buffer_dirty(bh) || buffer_delay(bh) ||
		    !buffer_mapped(bh) || buffer_unwritten(bh)) {
			/* A hole? We can safely clear the dirty bit */
			if (!buffer_mapped(bh))
				clear_buffer_dirty(bh);
			if (io->io_bio)
				ext4_io_submit(io);
			continue;
		}
		if (buffer_new(bh))
			clear_buffer_new(bh);
		set_buffer_async_write(bh);
		nr_to_submit++;
	} while ((bh = bh->b_this_page) != head);

	bh = head = page_buffers(page);

	/*
	 * If any blocks are being written to an encrypted file, encrypt them
	 * into a bounce page.  For simplicity, just encrypt until the last
	 * block which might be needed.  This may cause some unneeded blocks
	 * (e.g. holes) to be unnecessarily encrypted, but this is rare and
	 * can't happen in the common case of blocksize == PAGE_SIZE.
	 */
	if (IS_ENCRYPTED(inode) && S_ISREG(inode->i_mode) && nr_to_submit) {
		gfp_t gfp_flags = GFP_NOFS;
		unsigned int enc_bytes = round_up(len, i_blocksize(inode));

	retry_encrypt:
		bounce_page = fscrypt_encrypt_pagecache_blocks(page, enc_bytes,
							       0, gfp_flags);
		if (IS_ERR(bounce_page)) {
			ret = PTR_ERR(bounce_page);
			if (ret == -ENOMEM && wbc->sync_mode == WB_SYNC_ALL) {
				if (io->io_bio) {
					ext4_io_submit(io);
					congestion_wait(BLK_RW_ASYNC, HZ/50);
				}
				gfp_flags |= __GFP_NOFAIL;
				goto retry_encrypt;
			}
			bounce_page = NULL;
			goto out;
		}
	}

	/* Now submit buffers to write */
	do {
		if (!buffer_async_write(bh))
			continue;
		ret = io_submit_add_bh(io, inode, bounce_page ?: page, bh);
		if (ret) {
			/*
			 * We only get here on ENOMEM.  Not much else
			 * we can do but mark the page as dirty, and
			 * better luck next time.
			 */
			break;
		}
		nr_submitted++;
		clear_buffer_dirty(bh);
	} while ((bh = bh->b_this_page) != head);

	/* Error stopped previous loop? Clean up buffers... */
	if (ret) {
	out:
		fscrypt_free_bounce_page(bounce_page);
		printk_ratelimited(KERN_ERR "%s: ret = %d\n", __func__, ret);
		redirty_page_for_writepage(wbc, page);
		do {
			clear_buffer_async_write(bh);
			bh = bh->b_this_page;
		} while (bh != head);
	}
	unlock_page(page);
	/* Nothing submitted - we have to end page writeback */
	if (!nr_submitted)
		end_page_writeback(page);
	return ret;
}

static int io_submit_add_bh(struct ext4_io_submit *io,
			    struct inode *inode,
			    struct page *page,
			    struct buffer_head *bh)
{
	int ret;

	if (io->io_bio && bh->b_blocknr != io->io_next_block) {
submit_and_retry:
		ext4_io_submit(io);
	}
	if (io->io_bio == NULL) {
		ret = io_submit_init_bio(io, bh);
		if (ret)
			return ret;
		io->io_bio->bi_write_hint = inode->i_write_hint;
	}
	ret = bio_add_page(io->io_bio, page, bh->b_size, bh_offset(bh));
	if (ret != bh->b_size)
		goto submit_and_retry;
	wbc_account_cgroup_owner(io->io_wbc, page, bh->b_size);
	io->io_next_block++;
	return 0;
}

static inline struct bio *bio_alloc(gfp_t gfp_mask, unsigned int nr_iovecs)
{
	return bio_alloc_bioset(gfp_mask, nr_iovecs, &fs_bio_set);
}

#define bio_set_dev(bio, bdev)                  \
do {                                            \
        if ((bio)->bi_disk != (bdev)->bd_disk)  \
                bio_clear_flag(bio, BIO_THROTTLED);\
        (bio)->bi_disk = (bdev)->bd_disk;       \
        (bio)->bi_partno = (bdev)->bd_partno;   \
        bio_associate_blkg(bio);                \
} while (0)

static int io_submit_init_bio(struct ext4_io_submit *io,
			      struct buffer_head *bh)
{
	struct bio *bio;

	bio = bio_alloc(GFP_NOIO, BIO_MAX_PAGES);
	if (!bio)
		return -ENOMEM;
	bio->bi_iter.bi_sector = bh->b_blocknr * (bh->b_size >> 9);
	bio_set_dev(bio, bh->b_bdev);
	bio->bi_end_io = ext4_end_bio;
	bio->bi_private = ext4_get_io_end(io->io_end);
	io->io_bio = bio;
	io->io_next_block = bh->b_blocknr;
	wbc_init_bio(io->io_wbc, bio);
	return 0;
}



/**
 * __bio_add_page - add page(s) to a bio in a new segment
 * @bio: destination bio
 * @page: start page to add
 * @len: length of the data to add, may cross pages
 * @off: offset of the data relative to @page, may cross pages
 *
 * Add the data at @page + @off to @bio as a new bvec.  The caller must ensure
 * that @bio has space for another bvec.
 */
void __bio_add_page(struct bio *bio, struct page *page,
		unsigned int len, unsigned int off)
{
	struct bio_vec *bv = &bio->bi_io_vec[bio->bi_vcnt];          // bio->bi_io_vec在bio_alloc_bioset中初始化的

	WARN_ON_ONCE(bio_flagged(bio, BIO_CLONED));
	WARN_ON_ONCE(bio_full(bio, len));

	bv->bv_page = page;
	bv->bv_offset = off;
	bv->bv_len = len;

	bio->bi_iter.bi_size += len;
	bio->bi_vcnt++;

	if (!bio_flagged(bio, BIO_WORKINGSET) && unlikely(PageWorkingset(page)))
		bio_set_flag(bio, BIO_WORKINGSET);
}
EXPORT_SYMBOL_GPL(__bio_add_page);

/**
 *	bio_add_page	-	attempt to add page(s) to bio
 *	@bio: destination bio
 *	@page: start page to add
 *	@len: vec entry length, may cross pages
 *	@offset: vec entry offset relative to @page, may cross pages
 *
 *	Attempt to add page(s) to the bio_vec maplist. This will only fail
 *	if either bio->bi_vcnt == bio->bi_max_vecs or it's a cloned bio.
 */
int bio_add_page(struct bio *bio, struct page *page,
		 unsigned int len, unsigned int offset)
{
	bool same_page = false;

	if (!__bio_try_merge_page(bio, page, len, offset, &same_page)) {
		if (bio_full(bio, len))
			return 0;
		__bio_add_page(bio, page, len, offset);
	}
	return len;
}
EXPORT_SYMBOL(bio_add_page);

static int io_submit_add_bh(struct ext4_io_submit *io,
			    struct inode *inode,
			    struct page *page,
			    struct buffer_head *bh)
{
	int ret;

	if (io->io_bio && bh->b_blocknr != io->io_next_block) {
submit_and_retry:
		ext4_io_submit(io);
	}
	if (io->io_bio == NULL) {
		ret = io_submit_init_bio(io, bh);
		if (ret)
			return ret;
		io->io_bio->bi_write_hint = inode->i_write_hint;
	}
	ret = bio_add_page(io->io_bio, page, bh->b_size, bh_offset(bh));
	if (ret != bh->b_size)
		goto submit_and_retry;
	wbc_account_cgroup_owner(io->io_wbc, page, bh->b_size);
	io->io_next_block++;
	return 0;
}

/*
 * Historically, a buffer_head was used to map a single block
 * within a page, and of course as the unit of I/O through the
 * filesystem and block layers.  Nowadays the basic I/O unit
 * is the bio, and buffer_heads are used for extracting block
 * mappings (via a get_block_t call), for tracking state within
 * a page (via a page_mapping) and for wrapping bio submission
 * for backward compatibility reasons (e.g. submit_bh).
 */
struct buffer_head {
	unsigned long b_state;		/* buffer state bitmap (see above) */
	struct buffer_head *b_this_page;/* circular list of page's buffers */
	struct page *b_page;		/* the page this bh is mapped to */

	sector_t b_blocknr;		/* start block number */
	size_t b_size;			/* size of mapping */
	char *b_data;			/* pointer to data within the page */

	struct block_device *b_bdev;
	bh_end_io_t *b_end_io;		/* I/O completion */
 	void *b_private;		/* reserved for b_end_io */
	struct list_head b_assoc_buffers; /* associated with another mapping */
	struct address_space *b_assoc_map;	/* mapping this buffer is
						   associated with */
	atomic_t b_count;		/* users using this buffer_head */
};

/*
 * Function used by generic_writepages to call the real writepage
 * function and set the mapping flags on error
 */
static int __writepage(struct page *page, struct writeback_control *wbc,
		       void *data)
{
	struct address_space *mapping = data;
	int ret = mapping->a_ops->writepage(page, wbc);
	mapping_set_error(mapping, ret);
	return ret;
}

/**
 * generic_writepages - walk the list of dirty pages of the given address space and writepage() all of them.
 * @mapping: address space structure to write
 * @wbc: subtract the number of written pages from *@wbc->nr_to_write
 *
 * This is a library function, which implements the writepages()
 * address_space_operation.
 *
 * Return: %0 on success, negative error code otherwise
 */
int generic_writepages(struct address_space *mapping,
		       struct writeback_control *wbc)
{
	struct blk_plug plug;
	int ret;

	/* deal with chardevs and other special file */
	if (!mapping->a_ops->writepage)
		return 0;

	blk_start_plug(&plug);
	ret = write_cache_pages(mapping, wbc, __writepage, mapping);
	blk_finish_plug(&plug);
	return ret;
}

EXPORT_SYMBOL(generic_writepages);

void set_bh_page(struct buffer_head *bh,
		struct page *page, unsigned long offset)
{
	bh->b_page = page;
	BUG_ON(offset >= PAGE_SIZE);
	if (PageHighMem(page))
		/*
		 * This catches illegal uses and preserves the offset:
		 */
		bh->b_data = (char *)(0 + offset);
	else
		bh->b_data = page_address(page) + offset;
}
EXPORT_SYMBOL(set_bh_page);

static void
__clear_page_buffers(struct page *page)
{
	ClearPagePrivate(page);
	set_page_private(page, 0);
	put_page(page);
}

#define page_has_buffers(page)	PagePrivate(page)

/*
 * inline definitions
 */

static inline void attach_page_buffers(struct page *page,
		struct buffer_head *head)
{
	get_page(page);
	SetPagePrivate(page);
	set_page_private(page, (unsigned long)head);
}

static struct buffer_head *create_page_buffers(struct page *page, struct inode *inode, unsigned int b_state)
{
	BUG_ON(!PageLocked(page));

	if (!page_has_buffers(page))
		create_empty_buffers(page, 1 << READ_ONCE(inode->i_blkbits),
				     b_state);
	return page_buffers(page);
}

[  245.832445] CPU: 3 PID: 69 Comm: sh Not tainted 5.4.0+ #177
[  245.833355] Hardware name: linux,dummy-virt (DT)
[  245.833838] Call trace:
[  245.834085]  dump_backtrace+0x0/0x108
[  245.834590]  show_stack+0x14/0x20
[  245.834923]  dump_stack+0xb0/0xd8
[  245.835237]  attach_page_buffers+0x18/0x64
[  245.836656]  create_empty_buffers+0x124/0x170
[  245.837494]  create_page_buffers+0x68/0x70
[  245.837842]  __block_write_begin_int+0x74/0x6e8
[  245.838320]  __block_write_begin+0x10/0x18
[  245.838901]  ext4_da_write_begin+0x104/0x418
[  245.839389]  generic_perform_write+0xc0/0x178
[  245.840279]  __generic_file_write_iter+0x12c/0x198
[  245.840687]  ext4_file_write_iter+0xbc/0x350
[  245.841206]  new_sync_write+0xe8/0x150
[  245.841580]  __vfs_write+0x2c/0x40
[  245.841920]  vfs_write+0xa0/0x118
[  245.842260]  ksys_write+0x4c/0xc8
[  245.842528]  __arm64_sys_write+0x18/0x20
[  245.842856]  el0_svc_handler+0x7c/0xd0
[  245.843182]  el0_svc+0x8/0xc


[  251.040513] CPU: 1 PID: 55 Comm: jbd2/vda-8 Not tainted 5.4.0+ #177
[  251.043585] Hardware name: linux,dummy-virt (DT)
[  251.045706] Call trace:
[  251.046825]  dump_backtrace+0x0/0x108
[  251.047911]  show_stack+0x14/0x20
[  251.048522]  dump_stack+0xb0/0xd8
[  251.049107]  attach_page_buffers+0x18/0x64
[  251.050122]  __getblk_gfp+0x198/0x2a8
[  251.051664]  jbd2_journal_get_descriptor_buffer+0x40/0xf8
[  251.053085]  jbd2_journal_commit_transaction+0xda0/0x1788
[  251.053509]  kjournald2+0xd4/0x248
[  251.053875]  kthread+0x114/0x118
[  251.054246]  ret_from_fork+0x10/0x18

static inline void
map_bh(struct buffer_head *bh, struct super_block *sb, sector_t block)
{
        set_buffer_mapped(bh);
        bh->b_bdev = sb->s_bdev;
        bh->b_blocknr = block;
        bh->b_size = sb->s_blocksize;
}

[   31.866916] CPU: 3 PID: 68 Comm: sh Not tainted 5.4.0+ #178
[   31.867377] Hardware name: linux,dummy-virt (DT)
[   31.868159] Call trace:
[   31.869463]  dump_backtrace+0x0/0x108
[   31.869640]  show_stack+0x14/0x20
[   31.869761]  dump_stack+0xb0/0xd8
[   31.869880]  map_bh.isra.104+0x24/0x68
[   31.870001]  ext4_da_get_block_prep+0x188/0x3e0
[   31.870137]  __block_write_begin_int+0x10c/0x6e8
[   31.870454]  __block_write_begin+0x10/0x18
[   31.870626]  ext4_da_write_begin+0x104/0x418
[   31.870860]  generic_perform_write+0xc0/0x178
[   31.871056]  __generic_file_write_iter+0x12c/0x198
[   31.871417]  ext4_file_write_iter+0xbc/0x350
[   31.871693]  new_sync_write+0xe8/0x150
[   31.871905]  __vfs_write+0x2c/0x40
[   31.872578]  vfs_write+0xa0/0x118
[   31.872880]  ksys_write+0x4c/0xc8
[   31.873214]  __arm64_sys_write+0x18/0x20
[   31.873426]  el0_svc_handler+0x7c/0xd0
[   31.873633]  el0_svc+0x8/0xc

/*
 * Keep mostly read-only and often accessed (especially for
 * the RCU path lookup and 'stat' data) fields at the beginning
 * of the 'struct inode'
 */
struct inode {
	umode_t			i_mode;
	unsigned short		i_opflags;
	kuid_t			i_uid;
	kgid_t			i_gid;
	unsigned int		i_flags;

#ifdef CONFIG_FS_POSIX_ACL
	struct posix_acl	*i_acl;
	struct posix_acl	*i_default_acl;
#endif

	const struct inode_operations	*i_op;
	struct super_block	*i_sb;
	struct address_space	*i_mapping;

#ifdef CONFIG_SECURITY
	void			*i_security;
#endif

	/* Stat data, not accessed from path walking */
	unsigned long		i_ino;
	/*
	 * Filesystems may only read i_nlink directly.  They shall use the
	 * following functions for modification:
	 *
	 *    (set|clear|inc|drop)_nlink
	 *    inode_(inc|dec)_link_count
	 */
	union {
		const unsigned int i_nlink;
		unsigned int __i_nlink;
	};
	dev_t			i_rdev;
	loff_t			i_size;
	struct timespec64	i_atime;
	struct timespec64	i_mtime;
	struct timespec64	i_ctime;
	spinlock_t		i_lock;	/* i_blocks, i_bytes, maybe i_size */
	unsigned short          i_bytes;
	u8			i_blkbits;
	u8			i_write_hint;
	blkcnt_t		i_blocks;

#ifdef __NEED_I_SIZE_ORDERED
	seqcount_t		i_size_seqcount;
#endif

	/* Misc */
	unsigned long		i_state;
	struct rw_semaphore	i_rwsem;

	unsigned long		dirtied_when;	/* jiffies of first dirtying */
	unsigned long		dirtied_time_when;

	struct hlist_node	i_hash;
	struct list_head	i_io_list;	/* backing dev IO list */
#ifdef CONFIG_CGROUP_WRITEBACK
	struct bdi_writeback	*i_wb;		/* the associated cgroup wb */

	/* foreign inode detection, see wbc_detach_inode() */
	int			i_wb_frn_winner;
	u16			i_wb_frn_avg_time;
	u16			i_wb_frn_history;
#endif
	struct list_head	i_lru;		/* inode LRU list */
	struct list_head	i_sb_list;
	struct list_head	i_wb_list;	/* backing dev writeback list */
	union {
		struct hlist_head	i_dentry;
		struct rcu_head		i_rcu;
	};
	atomic64_t		i_version;
	atomic_t		i_count;
	atomic_t		i_dio_count;
	atomic_t		i_writecount;
#if defined(CONFIG_IMA) || defined(CONFIG_FILE_LOCKING)
	atomic_t		i_readcount; /* struct files open RO */
#endif
	union {
		const struct file_operations	*i_fop;	/* former ->i_op->default_file_ops */
		void (*free_inode)(struct inode *);
	};
	struct file_lock_context	*i_flctx;
	struct address_space	i_data;
	struct list_head	i_devices;
	union {
		struct pipe_inode_info	*i_pipe;
		struct block_device	*i_bdev;
		struct cdev		*i_cdev;
		char			*i_link;
		unsigned		i_dir_seq;
	};

	__u32			i_generation;

#ifdef CONFIG_FSNOTIFY
	__u32			i_fsnotify_mask; /* all events this inode cares about */
	struct fsnotify_mark_connector __rcu	*i_fsnotify_marks;
#endif

#ifdef CONFIG_FS_ENCRYPTION
	struct fscrypt_info	*i_crypt_info;
#endif

#ifdef CONFIG_FS_VERITY
	struct fsverity_info	*i_verity_info;
#endif

	void			*i_private; /* fs or device private pointer */
} __randomize_layout;

struct super_block {
	struct list_head	s_list;		/* Keep this first */
	dev_t			s_dev;		/* search index; _not_ kdev_t */
	unsigned char		s_blocksize_bits;
	unsigned long		s_blocksize;
	loff_t			s_maxbytes;	/* Max file size */
	struct file_system_type	*s_type;
	const struct super_operations	*s_op;
	const struct dquot_operations	*dq_op;
	const struct quotactl_ops	*s_qcop;
	const struct export_operations *s_export_op;
	unsigned long		s_flags;
	unsigned long		s_iflags;	/* internal SB_I_* flags */
	unsigned long		s_magic;
	struct dentry		*s_root;
	struct rw_semaphore	s_umount;
	int			s_count;
	atomic_t		s_active;
#ifdef CONFIG_SECURITY
	void                    *s_security;
#endif
	const struct xattr_handler **s_xattr;
#ifdef CONFIG_FS_ENCRYPTION
	const struct fscrypt_operations	*s_cop;
	struct key		*s_master_keys; /* master crypto keys in use */
#endif
#ifdef CONFIG_FS_VERITY
	const struct fsverity_operations *s_vop;
#endif
	struct hlist_bl_head	s_roots;	/* alternate root dentries for NFS */
	struct list_head	s_mounts;	/* list of mounts; _not_ for fs use */
	struct block_device	*s_bdev;
	struct backing_dev_info *s_bdi;
	struct mtd_info		*s_mtd;
	struct hlist_node	s_instances;
	unsigned int		s_quota_types;	/* Bitmask of supported quota types */
	struct quota_info	s_dquot;	/* Diskquota specific options */

	struct sb_writers	s_writers;

	/*
	 * Keep s_fs_info, s_time_gran, s_fsnotify_mask, and
	 * s_fsnotify_marks together for cache efficiency. They are frequently
	 * accessed and rarely modified.
	 */
	void			*s_fs_info;	/* Filesystem private info */

	/* Granularity of c/m/atime in ns (cannot be worse than a second) */
	u32			s_time_gran;
	/* Time limits for c/m/atime in seconds */
	time64_t		   s_time_min;
	time64_t		   s_time_max;
#ifdef CONFIG_FSNOTIFY
	__u32			s_fsnotify_mask;
	struct fsnotify_mark_connector __rcu	*s_fsnotify_marks;
#endif

	char			s_id[32];	/* Informational name */
	uuid_t			s_uuid;		/* UUID */

	unsigned int		s_max_links;
	fmode_t			s_mode;

	/*
	 * The next field is for VFS *only*. No filesystems have any business
	 * even looking at it. You had been warned.
	 */
	struct mutex s_vfs_rename_mutex;	/* Kludge */

	/*
	 * Filesystem subtype.  If non-empty the filesystem type field
	 * in /proc/mounts will be "type.subtype"
	 */
	const char *s_subtype;

	const struct dentry_operations *s_d_op; /* default d_op for dentries */

	/*
	 * Saved pool identifier for cleancache (-1 means none)
	 */
	int cleancache_poolid;

	struct shrinker s_shrink;	/* per-sb shrinker handle */

	/* Number of inodes with nlink == 0 but still referenced */
	atomic_long_t s_remove_count;

	/* Pending fsnotify inode refs */
	atomic_long_t s_fsnotify_inode_refs;

	/* Being remounted read-only */
	int s_readonly_remount;

	/* AIO completions deferred from interrupt context */
	struct workqueue_struct *s_dio_done_wq;
	struct hlist_head s_pins;

	/*
	 * Owning user namespace and default context in which to
	 * interpret filesystem uids, gids, quotas, device nodes,
	 * xattrs and security labels.
	 */
	struct user_namespace *s_user_ns;

	/*
	 * The list_lru structure is essentially just a pointer to a table
	 * of per-node lru lists, each of which has its own spinlock.
	 * There is no need to put them into separate cachelines.
	 */
	struct list_lru		s_dentry_lru;
	struct list_lru		s_inode_lru;
	struct rcu_head		rcu;
	struct work_struct	destroy_work;

	struct mutex		s_sync_lock;	/* sync serialisation lock */

	/*
	 * Indicates how deep in a filesystem stack this SB is
	 */
	int s_stack_depth;

	/* s_inode_list_lock protects s_inodes */
	spinlock_t		s_inode_list_lock ____cacheline_aligned_in_smp;
	struct list_head	s_inodes;	/* all inodes */

	spinlock_t		s_inode_wblist_lock;
	struct list_head	s_inodes_wb;	/* writeback inodes */
} __randomize_layout;

struct block_device {
	dev_t			bd_dev;  /* not a kdev_t - it's a search key */
	int			bd_openers;
	struct inode *		bd_inode;	/* will die */
	struct super_block *	bd_super;
	struct mutex		bd_mutex;	/* open/close mutex */
	void *			bd_claiming;
	void *			bd_holder;
	int			bd_holders;
	bool			bd_write_holder;
#ifdef CONFIG_SYSFS
	struct list_head	bd_holder_disks;
#endif
	struct block_device *	bd_contains;
	unsigned		bd_block_size;
	u8			bd_partno;
	struct hd_struct *	bd_part;
	/* number of times partitions within this device have been opened. */
	unsigned		bd_part_count;
	int			bd_invalidated;
	struct gendisk *	bd_disk;
	struct request_queue *  bd_queue;
	struct backing_dev_info *bd_bdi;
	struct list_head	bd_list;
	/*
	 * Private data.  You must have bd_claim'ed the block_device
	 * to use this.  NOTE:  bd_claim allows an owner to claim
	 * the same device multiple times, the owner must take special
	 * care to not mess up bd_private for that case.
	 */
	unsigned long		bd_private;

	/* The counter of freeze processes */
	int			bd_fsfreeze_count;
	/* Mutex for freeze */
	struct mutex		bd_fsfreeze_mutex;
} __randomize_layout;

struct gendisk {
	/* major, first_minor and minors are input parameters only,
	 * don't use directly.  Use disk_devt() and disk_max_parts().
	 */
	int major;			/* major number of driver */
	int first_minor;
	int minors;                     /* maximum number of minors, =1 for
                                         * disks that can't be partitioned. */

	char disk_name[DISK_NAME_LEN];	/* name of major driver */
	char *(*devnode)(struct gendisk *gd, umode_t *mode);

	unsigned short events;		/* supported events */
	unsigned short event_flags;	/* flags related to event processing */

	/* Array of pointers to partitions indexed by partno.
	 * Protected with matching bdev lock but stat and other
	 * non-critical accesses use RCU.  Always access through
	 * helpers.
	 */
	struct disk_part_tbl __rcu *part_tbl;
	struct hd_struct part0;

	const struct block_device_operations *fops;
	struct request_queue *queue;
	void *private_data;

	int flags;
	struct rw_semaphore lookup_sem;
	struct kobject *slave_dir;

	struct timer_rand_state *random;
	atomic_t sync_io;		/* RAID */
	struct disk_events *ev;
#ifdef  CONFIG_BLK_DEV_INTEGRITY
	struct kobject integrity_kobj;
#endif	/* CONFIG_BLK_DEV_INTEGRITY */
	int node_id;
	struct badblocks *bb;
	struct lockdep_map lockdep_map;
};

static int virtblk_probe(struct virtio_device *vdev)
{
	struct virtio_blk *vblk;
	struct request_queue *q;
	int err, index;

	u32 v, blk_size, max_size, sg_elems, opt_io_size;
	u16 min_io_size;
	u8 physical_block_exp, alignment_offset;

	if (!vdev->config->get) {
		dev_err(&vdev->dev, "%s failure: config access disabled\n",
			__func__);
		return -EINVAL;
	}

	err = ida_simple_get(&vd_index_ida, 0, minor_to_index(1 << MINORBITS),
			     GFP_KERNEL);
	if (err < 0)
		goto out;
	index = err;

	/* We need to know how many segments before we allocate. */
	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_SEG_MAX,
				   struct virtio_blk_config, seg_max,
				   &sg_elems);

	/* We need at least one SG element, whatever they say. */
	if (err || !sg_elems)
		sg_elems = 1;

	/* We need an extra sg elements at head and tail. */
	sg_elems += 2;
	vdev->priv = vblk = kmalloc(sizeof(*vblk), GFP_KERNEL);
	if (!vblk) {
		err = -ENOMEM;
		goto out_free_index;
	}

	vblk->vdev = vdev;
	vblk->sg_elems = sg_elems;

	INIT_WORK(&vblk->config_work, virtblk_config_changed_work);

	err = init_vq(vblk);
	if (err)
		goto out_free_vblk;

	/* FIXME: How many partitions?  How long is a piece of string? */
	vblk->disk = alloc_disk(1 << PART_BITS);
	if (!vblk->disk) {
		err = -ENOMEM;
		goto out_free_vq;
	}

	/* Default queue sizing is to fill the ring. */
	if (!virtblk_queue_depth) {
		virtblk_queue_depth = vblk->vqs[0].vq->num_free;
		/* ... but without indirect descs, we use 2 descs per req */
		if (!virtio_has_feature(vdev, VIRTIO_RING_F_INDIRECT_DESC))
			virtblk_queue_depth /= 2;
	}

	memset(&vblk->tag_set, 0, sizeof(vblk->tag_set));
	vblk->tag_set.ops = &virtio_mq_ops;
	vblk->tag_set.queue_depth = virtblk_queue_depth;
	vblk->tag_set.numa_node = NUMA_NO_NODE;
	vblk->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	vblk->tag_set.cmd_size =
		sizeof(struct virtblk_req) +
		sizeof(struct scatterlist) * sg_elems;
	vblk->tag_set.driver_data = vblk;
	vblk->tag_set.nr_hw_queues = vblk->num_vqs;

	err = blk_mq_alloc_tag_set(&vblk->tag_set);
	if (err)
		goto out_put_disk;

	q = blk_mq_init_queue(&vblk->tag_set);                                        // 创建request queue
	if (IS_ERR(q)) {
		err = -ENOMEM;
		goto out_free_tags;
	}
	vblk->disk->queue = q;                                                         // 这个成员非常重要

	q->queuedata = vblk;

	virtblk_name_format("vd", index, vblk->disk->disk_name, DISK_NAME_LEN);

	vblk->disk->major = major;
	vblk->disk->first_minor = index_to_minor(index);
	vblk->disk->private_data = vblk;
	vblk->disk->fops = &virtblk_fops;
	vblk->disk->flags |= GENHD_FL_EXT_DEVT;
	vblk->index = index;

	/* configure queue flush support */
	virtblk_update_cache_mode(vdev);

	/* If disk is read-only in the host, the guest should obey */
	if (virtio_has_feature(vdev, VIRTIO_BLK_F_RO))
		set_disk_ro(vblk->disk, 1);

	/* We can handle whatever the host told us to handle. */
	blk_queue_max_segments(q, vblk->sg_elems-2);

	/* No real sector limit. */
	blk_queue_max_hw_sectors(q, -1U);

	max_size = virtio_max_dma_size(vdev);

	/* Host can optionally specify maximum segment size and number of
	 * segments. */
	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_SIZE_MAX,
				   struct virtio_blk_config, size_max, &v);
	if (!err)
		max_size = min(max_size, v);

	blk_queue_max_segment_size(q, max_size);

	/* Host can optionally specify the block size of the device */
	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_BLK_SIZE,
				   struct virtio_blk_config, blk_size,
				   &blk_size);
	if (!err)
		blk_queue_logical_block_size(q, blk_size);
	else
		blk_size = queue_logical_block_size(q);

	/* Use topology information if available */
	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_TOPOLOGY,
				   struct virtio_blk_config, physical_block_exp,
				   &physical_block_exp);
	if (!err && physical_block_exp)
		blk_queue_physical_block_size(q,
				blk_size * (1 << physical_block_exp));

	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_TOPOLOGY,
				   struct virtio_blk_config, alignment_offset,
				   &alignment_offset);
	if (!err && alignment_offset)
		blk_queue_alignment_offset(q, blk_size * alignment_offset);

	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_TOPOLOGY,
				   struct virtio_blk_config, min_io_size,
				   &min_io_size);
	if (!err && min_io_size)
		blk_queue_io_min(q, blk_size * min_io_size);

	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_TOPOLOGY,
				   struct virtio_blk_config, opt_io_size,
				   &opt_io_size);
	if (!err && opt_io_size)
		blk_queue_io_opt(q, blk_size * opt_io_size);

	if (virtio_has_feature(vdev, VIRTIO_BLK_F_DISCARD)) {
		q->limits.discard_granularity = blk_size;

		virtio_cread(vdev, struct virtio_blk_config,
			     discard_sector_alignment, &v);
		q->limits.discard_alignment = v ? v << SECTOR_SHIFT : 0;

		virtio_cread(vdev, struct virtio_blk_config,
			     max_discard_sectors, &v);
		blk_queue_max_discard_sectors(q, v ? v : UINT_MAX);

		virtio_cread(vdev, struct virtio_blk_config, max_discard_seg,
			     &v);
		blk_queue_max_discard_segments(q,
					       min_not_zero(v,
							    MAX_DISCARD_SEGMENTS));

		blk_queue_flag_set(QUEUE_FLAG_DISCARD, q);
	}

	if (virtio_has_feature(vdev, VIRTIO_BLK_F_WRITE_ZEROES)) {
		virtio_cread(vdev, struct virtio_blk_config,
			     max_write_zeroes_sectors, &v);
		blk_queue_max_write_zeroes_sectors(q, v ? v : UINT_MAX);
	}

	virtblk_update_capacity(vblk, false);
	virtio_device_ready(vdev);

	device_add_disk(&vdev->dev, vblk->disk, virtblk_attr_groups);
	return 0;

out_free_tags:
	blk_mq_free_tag_set(&vblk->tag_set);
out_put_disk:
	put_disk(vblk->disk);
out_free_vq:
	vdev->config->del_vqs(vdev);
out_free_vblk:
	kfree(vblk);
out_free_index:
	ida_simple_remove(&vd_index_ida, index);
out:
	return err;
}

struct request_queue *blk_mq_init_queue(struct blk_mq_tag_set *set)
{
	struct request_queue *uninit_q, *q;

	uninit_q = blk_alloc_queue_node(GFP_KERNEL, set->numa_node);
	if (!uninit_q)
		return ERR_PTR(-ENOMEM);

	/*
	 * Initialize the queue without an elevator. device_add_disk() will do
	 * the initialization.
	 */
	q = blk_mq_init_allocated_queue(set, uninit_q, false);
	if (IS_ERR(q))
		blk_cleanup_queue(uninit_q);

	return q;
}
EXPORT_SYMBOL(blk_mq_init_queue);

/**
 * blk_alloc_queue_node - allocate a request queue
 * @gfp_mask: memory allocation flags
 * @node_id: NUMA node to allocate memory from
 */
struct request_queue *blk_alloc_queue_node(gfp_t gfp_mask, int node_id)
{
	struct request_queue *q;
	int ret;

	q = kmem_cache_alloc_node(blk_requestq_cachep,
				gfp_mask | __GFP_ZERO, node_id);
	if (!q)
		return NULL;

	q->last_merge = NULL;

	q->id = ida_simple_get(&blk_queue_ida, 0, 0, gfp_mask);
	if (q->id < 0)
		goto fail_q;

	ret = bioset_init(&q->bio_split, BIO_POOL_SIZE, 0, BIOSET_NEED_BVECS);
	if (ret)
		goto fail_id;

	q->backing_dev_info = bdi_alloc_node(gfp_mask, node_id);
	if (!q->backing_dev_info)
		goto fail_split;

	q->stats = blk_alloc_queue_stats();
	if (!q->stats)
		goto fail_stats;

	q->backing_dev_info->ra_pages = VM_READAHEAD_PAGES;
	q->backing_dev_info->capabilities = BDI_CAP_CGROUP_WRITEBACK;
	q->backing_dev_info->name = "block";
	q->node = node_id;

	timer_setup(&q->backing_dev_info->laptop_mode_wb_timer,
		    laptop_mode_timer_fn, 0);
	timer_setup(&q->timeout, blk_rq_timed_out_timer, 0);
	INIT_WORK(&q->timeout_work, blk_timeout_work);
	INIT_LIST_HEAD(&q->icq_list);
#ifdef CONFIG_BLK_CGROUP
	INIT_LIST_HEAD(&q->blkg_list);
#endif

	kobject_init(&q->kobj, &blk_queue_ktype);

#ifdef CONFIG_BLK_DEV_IO_TRACE
	mutex_init(&q->blk_trace_mutex);
#endif
	mutex_init(&q->sysfs_lock);
	mutex_init(&q->sysfs_dir_lock);
	spin_lock_init(&q->queue_lock);

	init_waitqueue_head(&q->mq_freeze_wq);
	mutex_init(&q->mq_freeze_lock);

	/*
	 * Init percpu_ref in atomic mode so that it's faster to shutdown.
	 * See blk_register_queue() for details.
	 */
	if (percpu_ref_init(&q->q_usage_counter,
				blk_queue_usage_counter_release,
				PERCPU_REF_INIT_ATOMIC, GFP_KERNEL))
		goto fail_bdi;

	if (blkcg_init_queue(q))
		goto fail_ref;

	return q;

fail_ref:
	percpu_ref_exit(&q->q_usage_counter);
fail_bdi:
	blk_free_queue_stats(q->stats);
fail_stats:
	bdi_put(q->backing_dev_info);
fail_split:
	bioset_exit(&q->bio_split);
fail_id:
	ida_simple_remove(&blk_queue_ida, q->id);
fail_q:
	kmem_cache_free(blk_requestq_cachep, q);
	return NULL;
}
EXPORT_SYMBOL(blk_alloc_queue_node);

struct request_queue *blk_mq_init_allocated_queue(struct blk_mq_tag_set *set,
						  struct request_queue *q,
						  bool elevator_init)
{
	/* mark the queue as mq asap */
	q->mq_ops = set->ops;

	q->poll_cb = blk_stat_alloc_callback(blk_mq_poll_stats_fn,
					     blk_mq_poll_stats_bkt,
					     BLK_MQ_POLL_STATS_BKTS, q);
	if (!q->poll_cb)
		goto err_exit;

	if (blk_mq_alloc_ctxs(q))
		goto err_poll;

	/* init q->mq_kobj and sw queues' kobjects */
	blk_mq_sysfs_init(q);

	q->nr_queues = nr_hw_queues(set);
	q->queue_hw_ctx = kcalloc_node(q->nr_queues, sizeof(*(q->queue_hw_ctx)),
						GFP_KERNEL, set->numa_node);
	if (!q->queue_hw_ctx)
		goto err_sys_init;

	INIT_LIST_HEAD(&q->unused_hctx_list);
	spin_lock_init(&q->unused_hctx_lock);

	blk_mq_realloc_hw_ctxs(set, q);
	if (!q->nr_hw_queues)
		goto err_hctxs;

	INIT_WORK(&q->timeout_work, blk_mq_timeout_work);
	blk_queue_rq_timeout(q, set->timeout ? set->timeout : 30 * HZ);

	q->tag_set = set;

	q->queue_flags |= QUEUE_FLAG_MQ_DEFAULT;
	if (set->nr_maps > HCTX_TYPE_POLL &&
	    set->map[HCTX_TYPE_POLL].nr_queues)
		blk_queue_flag_set(QUEUE_FLAG_POLL, q);

	q->sg_reserved_size = INT_MAX;

	INIT_DELAYED_WORK(&q->requeue_work, blk_mq_requeue_work);
	INIT_LIST_HEAD(&q->requeue_list);
	spin_lock_init(&q->requeue_lock);

	blk_queue_make_request(q, blk_mq_make_request);                                // 这个接口用于设置make_request_fn，在generic_make_request被调用

	/*
	 * Do this after blk_queue_make_request() overrides it...
	 */
	q->nr_requests = set->queue_depth;

	/*
	 * Default to classic polling
	 */
	q->poll_nsec = BLK_MQ_POLL_CLASSIC;

	blk_mq_init_cpu_queues(q, set->nr_hw_queues);
	blk_mq_add_queue_tag_set(set, q);
	blk_mq_map_swqueue(q);

	if (elevator_init)
		elevator_init_mq(q);

	return q;

err_hctxs:
	kfree(q->queue_hw_ctx);
	q->nr_hw_queues = 0;
err_sys_init:
	blk_mq_sysfs_deinit(q);
err_poll:
	blk_stat_free_callback(q->poll_cb);
	q->poll_cb = NULL;
err_exit:
	q->mq_ops = NULL;
	return ERR_PTR(-ENOMEM);
}
EXPORT_SYMBOL(blk_mq_init_allocated_queue);

/**
 * blk_queue_make_request - define an alternate make_request function for a device
 * @q:  the request queue for the device to be affected
 * @mfn: the alternate make_request function
 *
 * Description:
 *    The normal way for &struct bios to be passed to a device
 *    driver is for them to be collected into requests on a request
 *    queue, and then to allow the device driver to select requests
 *    off that queue when it is ready.  This works well for many block
 *    devices. However some block devices (typically virtual devices
 *    such as md or lvm) do not benefit from the processing on the
 *    request queue, and are served best by having the requests passed
 *    directly to them.  This can be achieved by providing a function
 *    to blk_queue_make_request().
 *
 * Caveat:
 *    The driver that does this *must* be able to deal appropriately
 *    with buffers in "highmemory". This can be accomplished by either calling
 *    kmap_atomic() to get a temporary kernel mapping, or by calling
 *    blk_queue_bounce() to create a buffer in normal memory.
 **/
void blk_queue_make_request(struct request_queue *q, make_request_fn *mfn)         // 这个接口用于设置make_request_fn，在generic_make_request被调用
{
	/*
	 * set defaults
	 */
	q->nr_requests = BLKDEV_MAX_RQ;

	q->make_request_fn = mfn;
	blk_queue_dma_alignment(q, 511);

	blk_set_default_limits(&q->limits);
}
EXPORT_SYMBOL(blk_queue_make_request);

struct request_queue {
	struct request		*last_merge;
	struct elevator_queue	*elevator;

	struct blk_queue_stats	*stats;
	struct rq_qos		*rq_qos;

	make_request_fn		*make_request_fn;
	dma_drain_needed_fn	*dma_drain_needed;

	const struct blk_mq_ops	*mq_ops;

	/* sw queues */
	struct blk_mq_ctx __percpu	*queue_ctx;
	unsigned int		nr_queues;

	unsigned int		queue_depth;

	/* hw dispatch queues */
	struct blk_mq_hw_ctx	**queue_hw_ctx;
	unsigned int		nr_hw_queues;

	struct backing_dev_info	*backing_dev_info;

	/*
	 * The queue owner gets to use this for whatever they like.
	 * ll_rw_blk doesn't touch it.
	 */
	void			*queuedata;

	/*
	 * various queue flags, see QUEUE_* below
	 */
	unsigned long		queue_flags;
	/*
	 * Number of contexts that have called blk_set_pm_only(). If this
	 * counter is above zero then only RQF_PM and RQF_PREEMPT requests are
	 * processed.
	 */
	atomic_t		pm_only;

	/*
	 * ida allocated id for this queue.  Used to index queues from
	 * ioctx.
	 */
	int			id;

	/*
	 * queue needs bounce pages for pages above this limit
	 */
	gfp_t			bounce_gfp;

	spinlock_t		queue_lock;

	/*
	 * queue kobject
	 */
	struct kobject kobj;

	/*
	 * mq queue kobject
	 */
	struct kobject *mq_kobj;

#ifdef  CONFIG_BLK_DEV_INTEGRITY
	struct blk_integrity integrity;
#endif	/* CONFIG_BLK_DEV_INTEGRITY */

#ifdef CONFIG_PM
	struct device		*dev;
	int			rpm_status;
	unsigned int		nr_pending;
#endif

	/*
	 * queue settings
	 */
	unsigned long		nr_requests;	/* Max # of requests */

	unsigned int		dma_drain_size;
	void			*dma_drain_buffer;
	unsigned int		dma_pad_mask;
	unsigned int		dma_alignment;

	unsigned int		rq_timeout;
	int			poll_nsec;

	struct blk_stat_callback	*poll_cb;
	struct blk_rq_stat	poll_stat[BLK_MQ_POLL_STATS_BKTS];

	struct timer_list	timeout;
	struct work_struct	timeout_work;

	struct list_head	icq_list;
#ifdef CONFIG_BLK_CGROUP
	DECLARE_BITMAP		(blkcg_pols, BLKCG_MAX_POLS);
	struct blkcg_gq		*root_blkg;
	struct list_head	blkg_list;
#endif

	struct queue_limits	limits;

	unsigned int		required_elevator_features;

#ifdef CONFIG_BLK_DEV_ZONED
	/*
	 * Zoned block device information for request dispatch control.
	 * nr_zones is the total number of zones of the device. This is always
	 * 0 for regular block devices. seq_zones_bitmap is a bitmap of nr_zones
	 * bits which indicates if a zone is conventional (bit clear) or
	 * sequential (bit set). seq_zones_wlock is a bitmap of nr_zones
	 * bits which indicates if a zone is write locked, that is, if a write
	 * request targeting the zone was dispatched. All three fields are
	 * initialized by the low level device driver (e.g. scsi/sd.c).
	 * Stacking drivers (device mappers) may or may not initialize
	 * these fields.
	 *
	 * Reads of this information must be protected with blk_queue_enter() /
	 * blk_queue_exit(). Modifying this information is only allowed while
	 * no requests are being processed. See also blk_mq_freeze_queue() and
	 * blk_mq_unfreeze_queue().
	 */
	unsigned int		nr_zones;
	unsigned long		*seq_zones_bitmap;
	unsigned long		*seq_zones_wlock;
#endif /* CONFIG_BLK_DEV_ZONED */

	/*
	 * sg stuff
	 */
	unsigned int		sg_timeout;
	unsigned int		sg_reserved_size;
	int			node;
#ifdef CONFIG_BLK_DEV_IO_TRACE
	struct blk_trace	*blk_trace;
	struct mutex		blk_trace_mutex;
#endif
	/*
	 * for flush operations
	 */
	struct blk_flush_queue	*fq;

	struct list_head	requeue_list;
	spinlock_t		requeue_lock;
	struct delayed_work	requeue_work;

	struct mutex		sysfs_lock;
	struct mutex		sysfs_dir_lock;

	/*
	 * for reusing dead hctx instance in case of updating
	 * nr_hw_queues
	 */
	struct list_head	unused_hctx_list;
	spinlock_t		unused_hctx_lock;

	int			mq_freeze_depth;

#if defined(CONFIG_BLK_DEV_BSG)
	struct bsg_class_device bsg_dev;
#endif

#ifdef CONFIG_BLK_DEV_THROTTLING
	/* Throttle data */
	struct throtl_data *td;
#endif
	struct rcu_head		rcu_head;
	wait_queue_head_t	mq_freeze_wq;
	/*
	 * Protect concurrent access to q_usage_counter by
	 * percpu_ref_kill() and percpu_ref_reinit().
	 */
	struct mutex		mq_freeze_lock;
	struct percpu_ref	q_usage_counter;

	struct blk_mq_tag_set	*tag_set;
	struct list_head	tag_set_list;
	struct bio_set		bio_split;

#ifdef CONFIG_BLK_DEBUG_FS
	struct dentry		*debugfs_dir;
	struct dentry		*sched_debugfs_dir;
	struct dentry		*rqos_debugfs_dir;
#endif

	bool			mq_sysfs_init_done;

	size_t			cmd_size;

	struct work_struct	release_work;

#define BLK_MAX_WRITE_HINTS	5
	u64			write_hints[BLK_MAX_WRITE_HINTS];
};

static blk_qc_t blk_mq_make_request(struct request_queue *q, struct bio *bio)
{
	const int is_sync = op_is_sync(bio->bi_opf);
	const int is_flush_fua = op_is_flush(bio->bi_opf);
	struct blk_mq_alloc_data data = { .flags = 0};
	struct request *rq;
	struct blk_plug *plug;
	struct request *same_queue_rq = NULL;
	unsigned int nr_segs;
	blk_qc_t cookie;

	blk_queue_bounce(q, &bio);
	__blk_queue_split(q, &bio, &nr_segs);

	if (!bio_integrity_prep(bio))
		return BLK_QC_T_NONE;

	if (!is_flush_fua && !blk_queue_nomerges(q) &&
	    blk_attempt_plug_merge(q, bio, nr_segs, &same_queue_rq))
		return BLK_QC_T_NONE;

	if (blk_mq_sched_bio_merge(q, bio, nr_segs))
		return BLK_QC_T_NONE;

	rq_qos_throttle(q, bio);

	data.cmd_flags = bio->bi_opf;
	rq = blk_mq_get_request(q, bio, &data);
	if (unlikely(!rq)) {
		rq_qos_cleanup(q, bio);
		if (bio->bi_opf & REQ_NOWAIT)
			bio_wouldblock_error(bio);
		return BLK_QC_T_NONE;
	}

	trace_block_getrq(q, bio, bio->bi_opf);

	rq_qos_track(q, rq, bio);

	cookie = request_to_qc_t(data.hctx, rq);

	blk_mq_bio_to_request(rq, bio, nr_segs);

	plug = blk_mq_plug(q, bio);
	if (unlikely(is_flush_fua)) {
		/* bypass scheduler for flush rq */
		blk_insert_flush(rq);
		blk_mq_run_hw_queue(data.hctx, true);
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
	} else if (q->elevator) {
		blk_mq_sched_insert_request(rq, false, true, true);
	} else if (plug && !blk_queue_nomerges(q)) {
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

/**
 * struct blk_mq_hw_ctx - State for a hardware queue facing the hardware block device
 */
struct blk_mq_hw_ctx {
	struct {
		spinlock_t		lock;
		struct list_head	dispatch;
		unsigned long		state;		/* BLK_MQ_S_* flags */
	} ____cacheline_aligned_in_smp;

	struct delayed_work	run_work;
	cpumask_var_t		cpumask;
	int			next_cpu;
	int			next_cpu_batch;

	unsigned long		flags;		/* BLK_MQ_F_* flags */

	void			*sched_data;
	struct request_queue	*queue;
	struct blk_flush_queue	*fq;

	void			*driver_data;

	struct sbitmap		ctx_map;

	struct blk_mq_ctx	*dispatch_from;
	unsigned int		dispatch_busy;

	unsigned short		type;
	unsigned short		nr_ctx;
	struct blk_mq_ctx	**ctxs;

	spinlock_t		dispatch_wait_lock;
	wait_queue_entry_t	dispatch_wait;
	atomic_t		wait_index;

	struct blk_mq_tags	*tags;
	struct blk_mq_tags	*sched_tags;

	unsigned long		queued;
	unsigned long		run;
#define BLK_MQ_MAX_DISPATCH_ORDER	7
	unsigned long		dispatched[BLK_MQ_MAX_DISPATCH_ORDER];

	unsigned int		numa_node;
	unsigned int		queue_num;

	atomic_t		nr_active;

	struct hlist_node	cpuhp_dead;
	struct kobject		kobj;

	unsigned long		poll_considered;
	unsigned long		poll_invoked;
	unsigned long		poll_success;

#ifdef CONFIG_BLK_DEBUG_FS
	struct dentry		*debugfs_dir;
	struct dentry		*sched_debugfs_dir;
#endif

	struct list_head	hctx_list;

	/* Must be the last member - see also blk_mq_hw_ctx_size(). */
	struct srcu_struct	srcu[0];
};

机制

plug/unplug
Linux块设备层早期就引入了plug/unplug机制，为每个task分配一个plug list，
当task提交IO时不立马处理，而是积攒一定量再统一加入下一级派发队列，减少对派发队列的竞争，
从而提高性能，block multi-queue也继承了这一机制，与单队列的实现基本一致，网络上已有大量分析，这里就不再赘述。

IO 合并
request由bio转化而来，最初一个request内只包含一个bio，若有新提交的bio与已经在排队等待处理的request物理上连续，
就直接将该bio合入request。推荐这篇文章https://blog.csdn.net/juS3Ve/article/details/80576965，
已经分析的十分透彻了。block multi-queue 新增的部分是，当不使用调度器时request会在软件队列的rq_list上等待派发，
提交IO时也会尝试与rq_list中的request进行合并。
对于block mulit-queue架构，可能发生IO合并的队列有三个：plug list ，调度器内部队列(使用调度器)，软件队列的rq_list(不使用调度器)。

调度器
block multi-queue最开始是不支持调度器的，调度器只在单队列中生效，随着单队列代码逐步移出内核，
而调度器对于hdd这种慢速设备又很有必要，调度器框架也加入了block multi-queue。
从代码也能看出multi-queue框架中是否使用调度器，处理流程差异很大。
Linux5.x后支持mq-deadline，bfq，kyber三种调度器，其中只有kyber有针对multi-queue框架中的多软硬队列做了处理，
也被称为是真正意义上的多队列调度器。

并发管理
block multi-queue通过控制request的获取，来限制一个硬件队列在block层最大并发request数量。由之前的数据结构分析可知，request是预先分配好的保存在struct blk_mq_tags结构中。运行时根据是否使用调度器，决定从硬件队列的tags或是sched_tags中获取request，所以最大并发request数也要分是否使用调度器来讨论。获取request的函数为blk_mq_get_request()。

使用调度器流程：

找到当前软件队列映射的硬件队列
调用调度器limit_depth方法(若有实现)，设置shallow_depth
sched_tags有空闲的request且分配的request总数
若分配不成功，调用blk_mq_run_hw_queue()启动request派发，再重试获取request，期间可能进入iowait
不使用调度器流程：

找到当前软件队列映射的硬件队列
若硬件blk_mq_tag_set支持共享，增加tags的active_queues
若硬件blk_mq_tag_set支持共享，且当前硬件队列已分配的request超过均分上限，分配失败
tags有空闲的request，分配成功
若分配不成功，调用blk_mq_run_hw_queue()启动request派发，再重试获取request，期间可能进入iowait
总结一下，每个硬件队列的最大并发request数是：

使用调度器：request_queue的nr_request，与调度器limit_depth方法返回值的较小值。

不使用调度器且不支持共享tag_set：硬件队列queue_depth。

不使用调度器支持共享tag_set：queue_depth个request，各active_queue均分。






Multi queue多队列核心IO传输函数是blk_mq_make_request
Multi queue多队列架构引入了struct blk_mq_tag_set、struct blk_mq_tag、struct blk_mq_hw_ctx、struct blk_mq_ctx等数据结构
1 struct blk_mq_ctx代表每个CPU独有的软件队列；
2 struct blk_mq_hw_ctx代表硬件队列，块设备至少有一个；
3 struct blk_mq_tag每个硬件队列结构struct blk_mq_hw_ctx对应一个；
4 struct blk_mq_tag主要是管理struct request(下文简称req)的分配。struct request大家应该都比较熟悉了，单队列时代就存在，IO传输的最后都要把bio转换成request；
5 struct blk_mq_tag_set包含了块设备的硬件配置信息，比如支持的硬件队列数nr_hw_queues、队列深度queue_depth等，在块设备驱动初始化时多处使用blk_mq_tag_set初始化其他成员；

每个CPU对应唯一的软件队列blk_mq_ctx，blk_mq_ctx对应唯一的硬件队列blk_mq_hw_ctx，blk_mq_hw_ctx对应唯一的blk_mq_tag，三者的关系在后续代码分析中多次出现。


/ # echo 2 > /mnt/hello.txt
[  117.886499] sblkdev: request start from sector 24  pos = 12288  dev_size = 536870912
[  117.887161] CPU: 3 PID: 212 Comm: kworker/3:1H Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  117.888006] Hardware name: linux,dummy-virt (DT)
[  117.888634] Workqueue: kblockd blk_mq_run_work_fn
[  117.889387] Call trace:
[  117.889756]  dump_backtrace+0x0/0x140
[  117.890131]  show_stack+0x14/0x20
[  117.890661]  dump_stack+0xbc/0x100
[  117.891015]  queue_rq+0x5c/0x234 [gendisk]
[  117.891566]  blk_mq_dispatch_rq_list+0xa8/0x580
[  117.892294]  blk_mq_do_dispatch_sched+0x68/0x108
[  117.892774]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  117.893423]  __blk_mq_run_hw_queue+0xac/0x120
[  117.894220]  blk_mq_run_work_fn+0x1c/0x28
[  117.894706]  process_one_work+0x1e0/0x358
[  117.895088]  worker_thread+0x40/0x488
[  117.895438]  kthread+0x118/0x120
[  117.895934]  ret_from_fork+0x10/0x18
/ # 
/ # sync
[  126.137309] sblkdev: request start from sector 16  pos = 8192  dev_size = 536870912
[  126.144651] CPU: 0 PID: 201 Comm: kworker/0:1H Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.145113] Hardware name: linux,dummy-virt (DT)
[  126.145386] Workqueue: kblockd blk_mq_run_work_fn
[  126.145715] Call trace:
[  126.153113]  dump_backtrace+0x0/0x140
[  126.153739]  show_stack+0x14/0x20
[  126.154215]  dump_stack+0xbc/0x100
[  126.154685]  queue_rq+0x5c/0x234 [gendisk]
[  126.155244]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.156319]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.156811]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.157311]  __blk_mq_run_hw_queue+0xac/0x120
[  126.157852]  blk_mq_run_work_fn+0x1c/0x28
[  126.158297]  process_one_work+0x1e0/0x358
[  126.158766]  worker_thread+0x40/0x488
[  126.159366]  kthread+0x118/0x120
[  126.159944]  ret_from_fork+0x10/0x18
[  126.161219] sblkdev: request start from sector 0  pos = 0  dev_size = 536870912
[  126.161742] CPU: 0 PID: 201 Comm: kworker/0:1H Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.162606] Hardware name: linux,dummy-virt (DT)
[  126.163079] Workqueue: kblockd blk_mq_run_work_fn
[  126.163345] Call trace:
[  126.163679]  dump_backtrace+0x0/0x140
[  126.163863]  show_stack+0x14/0x20
[  126.164069]  dump_stack+0xbc/0x100
[  126.164362]  queue_rq+0x5c/0x234 [gendisk]
[  126.165313]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.165751]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.166019]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.166312]  __blk_mq_run_hw_queue+0xac/0x120
[  126.166565]  blk_mq_run_work_fn+0x1c/0x28
[  126.166887]  process_one_work+0x1e0/0x358
[  126.167139]  worker_thread+0x40/0x488
[  126.167419]  kthread+0x118/0x120
[  126.167645]  ret_from_fork+0x10/0x18
[  126.168685] sblkdev: request start from sector 24  pos = 12288  dev_size = 536870912
[  126.169072] CPU: 0 PID: 201 Comm: kworker/0:1H Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.169562] Hardware name: linux,dummy-virt (DT)
[  126.169850] Workqueue: kblockd blk_mq_run_work_fn
[  126.170163] Call trace:
[  126.170332]  dump_backtrace+0x0/0x140
[  126.170526]  show_stack+0x14/0x20
[  126.170707]  dump_stack+0xbc/0x100
[  126.170903]  queue_rq+0x5c/0x234 [gendisk]
[  126.171088]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.171297]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.171490]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.171706]  __blk_mq_run_hw_queue+0xac/0x120
[  126.171924]  blk_mq_run_work_fn+0x1c/0x28
[  126.172122]  process_one_work+0x1e0/0x358
[  126.172316]  worker_thread+0x40/0x488
[  126.172403] sblkdev: request start from sector 4128  pos = 2113536  dev_size = 536870912
[  126.172641]  kthread+0x118/0x120
[  126.172666]  ret_from_fork+0x10/0x18
[  126.172853] sblkdev: request start from sector 4168  pos = 2134016  dev_size = 536870912
[  126.172899] CPU: 0 PID: 201 Comm: kworker/0:1H Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.172926] Hardware name: linux,dummy-virt (DT)
[  126.174700] Workqueue: kblockd blk_mq_run_work_fn
[  126.174947] Call trace:
[  126.175090]  dump_backtrace+0x0/0x140
[  126.175443]  show_stack+0x14/0x20
[  126.175730]  dump_stack+0xbc/0x100
[  126.175996]  queue_rq+0x5c/0x234 [gendisk]
[  126.176476]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.176770]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.177061]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.177421]  __blk_mq_run_hw_queue+0xac/0x120
[  126.177751]  blk_mq_run_work_fn+0x1c/0x28
[  126.177964]  process_one_work+0x1e0/0x358
[  126.178201]  worker_thread+0x40/0x488
[  126.178363]  kthread+0x118/0x120
[  126.178510]  ret_from_fork+0x10/0x18
[  126.178810] CPU: 1 PID: 190 Comm: kworker/u16:5 Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.179230] Hardware name: linux,dummy-virt (DT)
[  126.179463] Workqueue: writeback wb_workfn (flush-253:0)
[  126.179740] Call trace:
[  126.179886]  dump_backtrace+0x0/0x140
[  126.180085]  show_stack+0x14/0x20
[  126.180241]  dump_stack+0xbc/0x100
[  126.180418]  queue_rq+0x5c/0x234 [gendisk]
[  126.180666]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.180863]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.181053]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.181259]  __blk_mq_run_hw_queue+0xac/0x120
[  126.181439]  __blk_mq_delay_run_hw_queue+0x1f4/0x220
[  126.181688]  blk_mq_run_hw_queue+0xa0/0xf8
[  126.181863]  blk_mq_sched_insert_requests+0xe4/0x248
[  126.182059]  blk_mq_flush_plug_list+0x14c/0x190
[  126.182244]  blk_flush_plug_list+0xd4/0x100
[  126.182417]  blk_finish_plug+0x30/0x21c
[  126.198886]  wb_writeback+0x14c/0x1e8
[  126.199934]  wb_workfn+0x184/0x310
[  126.201133]  process_one_work+0x1e0/0x358
[  126.201745]  worker_thread+0x40/0x488
[  126.202163]  kthread+0x118/0x120
[  126.202631]  ret_from_fork+0x10/0x18
[  126.238845] sblkdev: request start from sector 8  pos = 4096  dev_size = 536870912
[  126.241012] CPU: 2 PID: 215 Comm: sync Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.242697] Hardware name: linux,dummy-virt (DT)
[  126.243064] Call trace:
[  126.243290]  dump_backtrace+0x0/0x140
[  126.243557]  show_stack+0x14/0x20
[  126.244386]  dump_stack+0xbc/0x100
[  126.245224]  queue_rq+0x5c/0x234 [gendisk]
[  126.246055]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.246950]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.248411]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.248840]  __blk_mq_run_hw_queue+0xac/0x120
[  126.249953]  __blk_mq_delay_run_hw_queue+0x1f4/0x220
[  126.251175]  blk_mq_run_hw_queue+0xa0/0xf8
[  126.251674]  blk_mq_sched_insert_requests+0xe4/0x248
[  126.252963]  blk_mq_flush_plug_list+0x14c/0x190
[  126.253871]  blk_flush_plug_list+0xd4/0x100
[  126.254497]  blk_finish_plug+0x30/0x21c
[  126.254877]  generic_writepages+0x68/0x90
[  126.255472]  blkdev_writepages+0xc/0x18
[  126.256143]  do_writepages+0x54/0x100
[  126.257314]  __filemap_fdatawrite_range+0xdc/0x118
[  126.259288]  filemap_fdatawrite+0x18/0x20
[  126.259937]  fdatawrite_one_bdev+0x14/0x20
[  126.261070]  iterate_bdevs+0x98/0x230
[  126.262749]  ksys_sync+0x70/0xb8
[  126.263709]  __arm64_sys_sync+0xc/0x18
[  126.265700]  el0_svc_common.constprop.2+0x88/0x150
[  126.266623]  el0_svc_handler+0x20/0x80
[  126.277226]  el0_svc+0x8/0xc
[  126.280129] sblkdev: request start from sector 32  pos = 16384  dev_size = 536870912
[  126.281419] CPU: 2 PID: 215 Comm: sync Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.282253] Hardware name: linux,dummy-virt (DT)
[  126.282659] Call trace:
[  126.283544]  dump_backtrace+0x0/0x140
[  126.284075]  show_stack+0x14/0x20
[  126.284725]  dump_stack+0xbc/0x100
[  126.285980]  queue_rq+0x5c/0x234 [gendisk]
[  126.287178]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.288268]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.288669]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.289035]  __blk_mq_run_hw_queue+0xac/0x120
[  126.289353]  __blk_mq_delay_run_hw_queue+0x1f4/0x220
[  126.289825]  blk_mq_run_hw_queue+0xa0/0xf8
[  126.290195]  blk_mq_sched_insert_requests+0xe4/0x248
[  126.290638]  blk_mq_flush_plug_list+0x14c/0x190
[  126.291093]  blk_flush_plug_list+0xd4/0x100
[  126.291422]  blk_finish_plug+0x30/0x21c
[  126.291780]  generic_writepages+0x68/0x90
[  126.292178]  blkdev_writepages+0xc/0x18
[  126.292528]  do_writepages+0x54/0x100
[  126.292863]  __filemap_fdatawrite_range+0xdc/0x118
[  126.293277]  filemap_fdatawrite+0x18/0x20
[  126.293724]  fdatawrite_one_bdev+0x14/0x20
[  126.294078]  iterate_bdevs+0x98/0x230
[  126.294427]  ksys_sync+0x70/0xb8
[  126.294770]  __arm64_sys_sync+0xc/0x18
[  126.295119]  el0_svc_common.constprop.2+0x88/0x150
[  126.295581]  el0_svc_handler+0x20/0x80
[  126.295921]  el0_svc+0x8/0xc

/**
 * Serve requests
 */
static int do_request(struct request *rq, unsigned int *nr_bytes)
{
    int ret = 0;
    struct bio_vec bvec;
    struct req_iterator iter;
    struct block_dev *dev = rq->q->queuedata;
    loff_t pos = blk_rq_pos(rq) << SECTOR_SHIFT;
    loff_t dev_size = (loff_t)(dev->capacity << SECTOR_SHIFT);

    printk(KERN_WARNING "sblkdev: request start from sector %lld  pos = %lld  dev_size = %lld\n",
					blk_rq_pos(rq), pos, dev_size);

    /* Iterate over all requests segments */
    rq_for_each_segment(bvec, rq, iter)
    {
        unsigned long b_len = bvec.bv_len;

        /* Get pointer to the data */
        void* b_buf = page_address(bvec.bv_page) + bvec.bv_offset;

        /* Simple check that we are not out of the memory bounds */
        if ((pos + b_len) > dev_size) {
            b_len = (unsigned long)(dev_size - pos);
        }

        if (rq_data_dir(rq) == WRITE) {
            /* Copy data to the buffer in to required position */
            memcpy(dev->data + pos, b_buf, b_len);
        } else {
            /* Read data from the buffer's position */
            memcpy(b_buf, dev->data + pos, b_len);
        }

        /* Increment counters */
        pos += b_len;
        *nr_bytes += b_len;
    }

    return ret;
}

/**
 * queue callback function
 */
static blk_status_t queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data* bd)
{
    unsigned int nr_bytes = 0;
    blk_status_t status = BLK_STS_OK;
    struct request *rq = bd->rq;

    /* Start request serving procedure */
    blk_mq_start_request(rq);

    if (do_request(rq, &nr_bytes) != 0) {
        status = BLK_STS_IOERR;
    }

    /* Notify kernel about processed nr_bytes */
    if (blk_update_request(rq, status, nr_bytes)) {
        /* Shouldn't fail */
        BUG();
    }

    /* Stop request serving procedure */
    __blk_mq_end_request(rq, status);

    return status;
}


/*
 * Only SCSI implements .get_budget and .put_budget, and SCSI restarts
 * its queue by itself in its completion handler, so we don't need to
 * restart queue if .get_budget() returns BLK_STS_NO_RESOURCE.
 */
static void blk_mq_do_dispatch_sched(struct blk_mq_hw_ctx *hctx)
{
	struct request_queue *q = hctx->queue;                                       // 取出hw request queue
	struct elevator_queue *e = q->elevator;                                      // 取出io调度器
	LIST_HEAD(rq_list);                                                          // 初始化临时链表

	do {
		struct request *rq;

		if (e->type->ops.has_work && !e->type->ops.has_work(hctx))               // no work, break
			break;

		if (!blk_mq_get_dispatch_budget(hctx))                                   // 参考注释
			break;

		rq = e->type->ops.dispatch_request(hctx);                                // 取出一个request
		if (!rq) {
			blk_mq_put_dispatch_budget(hctx);                                    // 参考注释
			break;
		}

		/*
		 * Now this rq owns the budget which has to be released
		 * if this rq won't be queued to driver via .queue_rq()                  // 通过注释可以看出在driver中是通过queue_rq来处理的
		 * in blk_mq_dispatch_rq_list().
		 */
		list_add(&rq->queuelist, &rq_list);                                      // 添加requet到临时链表
	} while (blk_mq_dispatch_rq_list(q, &rq_list, true));                        // 处理request
}


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

	if (list_empty(list))                                                        // 如果链表为空
		return false;                                                            // 返回false

	WARN_ON(!list_is_singular(list) && got_budget);

	/*
	 * Now process all the entries, sending them to the driver.
	 */
	errors = queued = 0;                                                         // 初始化用于返回值判断
	do {
		struct blk_mq_queue_data bd;

		rq = list_first_entry(list, struct request, queuelist);                  // 根据链表头取出request

		hctx = rq->mq_hctx;                                                      // 从rq中取出mq_hctx
		if (!got_budget && !blk_mq_get_dispatch_budget(hctx))                    // blk_mq_do_dispatch_sched调用时传递的参数got_budget为true
			break;

		if (!blk_mq_get_driver_tag(rq)) {
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

		list_del_init(&rq->queuelist);                                           // 将request从链表中移除，并重新初始化

		bd.rq = rq;

		/*
		 * Flag last if we have no more requests, or if we have more
		 * but can't assign a driver tag to it.
		 */
		if (list_empty(list))
			bd.last = true;
		else {
			nxt = list_first_entry(list, struct request, queuelist);
			bd.last = !blk_mq_get_driver_tag(nxt);
		}

		ret = q->mq_ops->queue_rq(hctx, &bd);                                    // 请求在这里被处理，queue_rq是驱动中定义的
		if (ret == BLK_STS_RESOURCE || ret == BLK_STS_DEV_RESOURCE) {
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
			__blk_mq_requeue_request(rq);
			break;
		}

		if (unlikely(ret != BLK_STS_OK)) {
			errors++;
			blk_mq_end_request(rq, BLK_STS_IOERR);
			continue;
		}

		queued++;
	} while (!list_empty(list));

	hctx->dispatched[queued_to_index(queued)]++;

	/*
	 * Any items that need requeuing? Stuff them into hctx->dispatch,
	 * that is where we will continue on next queue run.
	 */
	if (!list_empty(list)) {
		bool needs_restart;

		/*
		 * If we didn't flush the entire list, we could have told
		 * the driver there was more coming, but that turned out to
		 * be a lie.
		 */
		if (q->mq_ops->commit_rqs)
			q->mq_ops->commit_rqs(hctx);

		spin_lock(&hctx->lock);
		list_splice_init(list, &hctx->dispatch);
		spin_unlock(&hctx->lock);

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
		needs_restart = blk_mq_sched_needs_restart(hctx);
		if (!needs_restart ||
		    (no_tag && list_empty_careful(&hctx->dispatch_wait.entry)))
			blk_mq_run_hw_queue(hctx, true);
		else if (needs_restart && (ret == BLK_STS_RESOURCE))
			blk_mq_delay_run_hw_queue(hctx, BLK_MQ_RESOURCE_DELAY);

		blk_mq_update_dispatch_busy(hctx, true);
		return false;
	} else
		blk_mq_update_dispatch_busy(hctx, false);

	/*
	 * If the host/device is unable to accept more work, inform the
	 * caller of that.
	 */
	if (ret == BLK_STS_RESOURCE || ret == BLK_STS_DEV_RESOURCE)
		return false;

	return (queued + errors) != 0;
}

static void blk_mq_run_work_fn(struct work_struct *work)
{
	struct blk_mq_hw_ctx *hctx;

	hctx = container_of(work, struct blk_mq_hw_ctx, run_work.work);

	/*
	 * If we are stopped, don't run the queue.
	 */
	if (test_bit(BLK_MQ_S_STOPPED, &hctx->state))
		return;

	__blk_mq_run_hw_queue(hctx);
}






#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/blk-mq.h>
#include <linux/hdreg.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512
#endif

#define ENTRY_NAME "foo"
#define PARENT_ENTRY_NAME "debug"

static struct proc_dir_entry *parent;
static unsigned int enable = 1;

static int foo_proc_show(struct seq_file *m, void *v) {
        seq_printf(m, "%s\n", enable? "enable" : "disable");
        return 0;
}

static int foo_proc_open(struct inode *inode, struct  file *file) {
        return single_open(file, foo_proc_show, NULL);
}

static ssize_t foo_proc_write(struct file *filp, const char __user *buf,
                        size_t size, loff_t *offset)
{
        int err, val;
        char *temp = kzalloc(size, GFP_KERNEL);
        if (!temp)
                return -ENOMEM;

        if (copy_from_user(temp, buf, size)) {
                kfree(temp);
                return -EFAULT;
        }

        err = kstrtouint(temp, 10, &val);
        if (err) {
                kfree(temp);
                return err;
        }

        if (val)
                enable = 1;
        else
                enable = 0;

        kfree(temp);

        return size;
}

static struct file_operations foo_proc_fops = {
        .owner = THIS_MODULE,
        .open = foo_proc_open,
        .read = seq_read,
        .write = foo_proc_write,
        .llseek = seq_lseek,
        .release = single_release,
};

static int dev_major = 0;

/* Just internal representation of the our block device
 * can hold any useful data */
struct block_dev {
    sector_t capacity;
    u8 *data;   /* Data buffer to emulate real storage device */
    struct blk_mq_tag_set tag_set;
    struct request_queue *queue;
    struct gendisk *gdisk;
};

/* Device instance */
static struct block_dev *block_device = NULL;

static int blockdev_open(struct block_device *dev, fmode_t mode)
{
    printk(">>> blockdev_open\n");

    return 0;
}

static void blockdev_release(struct gendisk *gdisk, fmode_t mode)
{
    printk(">>> blockdev_release\n");
}

int blockdev_ioctl(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)
{
    printk("ioctl cmd 0x%08x\n", cmd);

    return -ENOTTY;
}

/* Set block device file I/O */
static struct block_device_operations blockdev_ops = {
    .owner = THIS_MODULE,
    .open = blockdev_open,
    .release = blockdev_release,
    .ioctl = blockdev_ioctl
};

/* Serve requests */
static int do_request(struct request *rq, unsigned int *nr_bytes)
{
    int ret = 0;
    struct bio_vec bvec;
    struct req_iterator iter;
    struct block_dev *dev = rq->q->queuedata;
    loff_t pos = blk_rq_pos(rq) << SECTOR_SHIFT;
    loff_t dev_size = (loff_t)(dev->capacity << SECTOR_SHIFT);

    printk(KERN_WARNING "sblkdev: request start from sector %lld  pos = %lld  dev_size = %lld\n", blk_rq_pos(rq), pos, dev_size);
	while (!enable) {
		mdelay(1000);
	}

    /* Iterate over all requests segments */
    rq_for_each_segment(bvec, rq, iter)
    {
        unsigned long b_len = bvec.bv_len;

        /* Get pointer to the data */
        void* b_buf = page_address(bvec.bv_page) + bvec.bv_offset;

        /* Simple check that we are not out of the memory bounds */
        if ((pos + b_len) > dev_size) {
            b_len = (unsigned long)(dev_size - pos);
        }

        if (rq_data_dir(rq) == WRITE) {
            /* Copy data to the buffer in to required position */
            memcpy(dev->data + pos, b_buf, b_len);
        } else {
            /* Read data from the buffer's position */
            memcpy(b_buf, dev->data + pos, b_len);
        }

        /* Increment counters */
        pos += b_len;
        *nr_bytes += b_len;
    }

    return ret;
}

/* queue callback function */
static blk_status_t queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data* bd)
{
    unsigned int nr_bytes = 0;
    blk_status_t status = BLK_STS_OK;
    struct request *rq = bd->rq;

    /* Start request serving procedure */
    blk_mq_start_request(rq);

    if (do_request(rq, &nr_bytes) != 0) {
        status = BLK_STS_IOERR;
    }

    /* Notify kernel about processed nr_bytes */
    if (blk_update_request(rq, status, nr_bytes)) {
        /* Shouldn't fail */
        BUG();
    }

    /* Stop request serving procedure */
    __blk_mq_end_request(rq, status);

    return status;
}

static struct blk_mq_ops mq_ops = {
    .queue_rq = queue_rq,
};

static int __init myblock_driver_init(void)
{
        struct proc_dir_entry *foo;

        parent = proc_mkdir(PARENT_ENTRY_NAME, NULL);
        if (!parent)
                return -ENOMEM;

        foo = proc_create(ENTRY_NAME,
                        S_IRUSR | S_IRGRP | S_IROTH,
                        parent, &foo_proc_fops);
        if (!foo) {
                remove_proc_entry(PARENT_ENTRY_NAME, NULL);
                return -ENOMEM;
        }

    /* Register new block device and get device major number */
    dev_major = register_blkdev(dev_major, "testblk");

    block_device = kmalloc(sizeof (struct block_dev), GFP_KERNEL);
    if (block_device == NULL) {
        printk("Failed to allocate struct block_dev\n");
        unregister_blkdev(dev_major, "testblk");

        return -ENOMEM;
    }

    /* Set some random capacity of the device */
    block_device->capacity = (512 * 1024 * 1024UL) >> 9; /* nsectors * SECTOR_SIZE; */
    /* Allocate corresponding data buffer */
    block_device->data = vmalloc(block_device->capacity << 9);

    if (block_device->data == NULL) {
        printk("Failed to allocate device IO buffer\n");
        unregister_blkdev(dev_major, "testblk");
        kfree(block_device);

        return -ENOMEM;
    }

    printk("Initializing queue\n");

    block_device->queue = blk_mq_init_sq_queue(&block_device->tag_set, &mq_ops, 128, BLK_MQ_F_SHOULD_MERGE);

    if (block_device->queue == NULL) {
        printk("Failed to allocate device queue\n");
        vfree(block_device->data);

        unregister_blkdev(dev_major, "testblk");
        kfree(block_device);

        return -ENOMEM;
    }

    /* Set driver's structure as user data of the queue */
    block_device->queue->queuedata = block_device;

    /* Allocate new disk */
    block_device->gdisk = alloc_disk(1);

    /* Set all required flags and data */
    block_device->gdisk->flags = GENHD_FL_NO_PART_SCAN;
    block_device->gdisk->major = dev_major;
    block_device->gdisk->first_minor = 0;

    block_device->gdisk->fops = &blockdev_ops;
    block_device->gdisk->queue = block_device->queue;
    block_device->gdisk->private_data = block_device;

    /* Set device name as it will be represented in /dev */
    strncpy(block_device->gdisk->disk_name, "blockdev\0", 9);

    printk("Adding disk %s\n", block_device->gdisk->disk_name);

    /* Set device capacity */
    set_capacity(block_device->gdisk, block_device->capacity);

    /* Notify kernel about new disk device */
    add_disk(block_device->gdisk);

    return 0;
}

static void __exit myblock_driver_exit(void)
{
    /* Don't forget to cleanup everything */
    if (block_device->gdisk) {
        del_gendisk(block_device->gdisk);
        put_disk(block_device->gdisk);
    }

    if (block_device->queue) {
        blk_cleanup_queue(block_device->queue);
    }

    vfree(block_device->data);

    unregister_blkdev(dev_major, "testblk");
    kfree(block_device);

        remove_proc_entry(ENTRY_NAME, parent);
        remove_proc_entry(PARENT_ENTRY_NAME, NULL);

}

module_init(myblock_driver_init);
module_exit(myblock_driver_exit);
MODULE_LICENSE("GPL");
































































































