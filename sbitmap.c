/*
 * Tag address space map.
 */
struct blk_mq_tags {
	unsigned int nr_tags;
	unsigned int nr_reserved_tags;

	atomic_t active_queues;

	struct sbitmap_queue bitmap_tags;
	struct sbitmap_queue breserved_tags;

	struct request **rqs;
	struct request **static_rqs;
	struct list_head page_list;
};

struct blk_mq_tag_set {
	/*
	 * map[] holds ctx -> hctx mappings, one map exists for each type
	 * that the driver wishes to support. There are no restrictions
	 * on maps being of the same size, and it's perfectly legal to
	 * share maps between types.
	 */
	struct blk_mq_queue_map	map[HCTX_MAX_TYPES];
	unsigned int		nr_maps;	/* nr entries in map[] */
	const struct blk_mq_ops	*ops;
	unsigned int		nr_hw_queues;	/* nr hw queues across maps */
	unsigned int		queue_depth;	/* max hw supported */
	unsigned int		reserved_tags;
	unsigned int		cmd_size;	/* per-request extra data */
	int			numa_node;
	unsigned int		timeout;
	unsigned int		flags;		/* BLK_MQ_F_* */
	void			*driver_data;

	struct blk_mq_tags	**tags;

	struct mutex		tag_list_lock;
	struct list_head	tag_list;
};


/**
 * struct sbitmap_queue - Scalable bitmap with the added ability to wait on free
 * bits.
 *
 * A &struct sbitmap_queue uses multiple wait queues and rolling wakeups to
 * avoid contention on the wait queue spinlock. This ensures that we don't hit a
 * scalability wall when we run out of free bits and have to start putting tasks
 * to sleep.
 */
struct sbitmap_queue {
	/**
	 * @sb: Scalable bitmap.
	 */
	struct sbitmap sb;

	/*
	 * @alloc_hint: Cache of last successfully allocated or freed bit.
	 *
	 * This is per-cpu, which allows multiple users to stick to different
	 * cachelines until the map is exhausted.
	 */
	unsigned int __percpu *alloc_hint;

	/**
	 * @wake_batch: Number of bits which must be freed before we wake up any
	 * waiters.
	 */
	unsigned int wake_batch;

	/**
	 * @wake_index: Next wait queue in @ws to wake up.
	 */
	atomic_t wake_index;

	/**
	 * @ws: Wait queues.
	 */
	struct sbq_wait_state *ws;

	/*
	 * @ws_active: count of currently active ws waitqueues
	 */
	atomic_t ws_active;

	/**
	 * @round_robin: Allocate bits in strict round-robin order.
	 */
	bool round_robin;

	/**
	 * @min_shallow_depth: The minimum shallow depth which may be passed to
	 * sbitmap_queue_get_shallow() or __sbitmap_queue_get_shallow().
	 */
	unsigned int min_shallow_depth;
};

/**
 * struct sbitmap - Scalable bitmap.
 *
 * A &struct sbitmap is spread over multiple cachelines to avoid ping-pong. This
 * trades off higher memory usage for better scalability.
 */
struct sbitmap {
	/**
	 * @depth: Number of bits used in the whole bitmap.
	 */
	unsigned int depth;

	/**
	 * @shift: log2(number of bits used per word)
	 */
	unsigned int shift;

	/**
	 * @map_nr: Number of words (cachelines) being used for the bitmap.
	 */
	unsigned int map_nr;

	/**
	 * @map: Allocated bitmap.
	 */
	struct sbitmap_word *map;                                                            // 注意：这是一个数组的地址
};

/**
 * struct sbitmap_word - Word in a &struct sbitmap.
 */
struct sbitmap_word {
	/**
	 * @depth: Number of bits being used in @word/@cleared
	 */
	unsigned long depth;

	/**
	 * @word: word holding free bits
	 */
	unsigned long word ____cacheline_aligned_in_smp;                                     // 注意：这里word是long类型

	/**
	 * @cleared: word holding cleared bits
	 */
	unsigned long cleared ____cacheline_aligned_in_smp;

	/**
	 * @swap_lock: Held while swapping word <-> cleared
	 */
	spinlock_t swap_lock;
} ____cacheline_aligned_in_smp;

static struct scsi_host_template ufshcd_driver_template = {
	.module			= THIS_MODULE,
	.name			= UFSHCD,
	.proc_name		= UFSHCD,
	.queuecommand		= ufshcd_queuecommand,
	.slave_alloc		= ufshcd_slave_alloc,
	.slave_configure	= ufshcd_slave_configure,
	.slave_destroy		= ufshcd_slave_destroy,
	.change_queue_depth	= ufshcd_change_queue_depth,
	.eh_abort_handler	= ufshcd_abort,
	.eh_device_reset_handler = ufshcd_eh_device_reset_handler,
	.eh_host_reset_handler   = ufshcd_eh_host_reset_handler,
	.eh_timed_out		= ufshcd_eh_timed_out,
	.this_id		= -1,
	.sg_tablesize		= SG_ALL,
	.cmd_per_lun		= UFSHCD_CMD_PER_LUN,
	.can_queue		= UFSHCD_CAN_QUEUE,
	.max_segment_size	= PRDT_DATA_BYTE_COUNT_MAX,
	.max_host_blocked	= 1,
	.track_queue_depth	= 1,
	.sdev_groups		= ufshcd_driver_groups,
	.dma_boundary		= PAGE_SIZE - 1,
};

#define BLK_MQ_FLAG_TO_ALLOC_POLICY(flags) \
	((flags >> BLK_MQ_F_ALLOC_POLICY_START_BIT) & \
		((1 << BLK_MQ_F_ALLOC_POLICY_BITS) - 1))
#define BLK_ALLOC_POLICY_TO_MQ_FLAG(policy) \
	((policy & ((1 << BLK_MQ_F_ALLOC_POLICY_BITS) - 1)) \
		<< BLK_MQ_F_ALLOC_POLICY_START_BIT)

int scsi_mq_setup_tags(struct Scsi_Host *shost)
{
	unsigned int cmd_size, sgl_size;

	sgl_size = max_t(unsigned int, sizeof(struct scatterlist),
				scsi_mq_inline_sgl_size(shost));
	cmd_size = sizeof(struct scsi_cmnd) + shost->hostt->cmd_size + sgl_size;
	if (scsi_host_get_prot(shost))
		cmd_size += sizeof(struct scsi_data_buffer) +
			sizeof(struct scatterlist) * SCSI_INLINE_PROT_SG_CNT;

	memset(&shost->tag_set, 0, sizeof(shost->tag_set));
	if (shost->hostt->commit_rqs)
		shost->tag_set.ops = &scsi_mq_ops;
	else
		shost->tag_set.ops = &scsi_mq_ops_no_commit;
	shost->tag_set.nr_hw_queues = shost->nr_hw_queues ? : 1;
	shost->tag_set.queue_depth = shost->can_queue;
	shost->tag_set.cmd_size = cmd_size;
	shost->tag_set.numa_node = NUMA_NO_NODE;
	shost->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	shost->tag_set.flags |=
		BLK_ALLOC_POLICY_TO_MQ_FLAG(shost->hostt->tag_alloc_policy);
	shost->tag_set.driver_data = shost;

	return blk_mq_alloc_tag_set(&shost->tag_set);
}

struct blk_mq_tags *blk_mq_alloc_rq_map(struct blk_mq_tag_set *set,
					unsigned int hctx_idx,
					unsigned int nr_tags,
					unsigned int reserved_tags)
{
	struct blk_mq_tags *tags;
	int node;

	node = blk_mq_hw_queue_to_node(&set->map[HCTX_TYPE_DEFAULT], hctx_idx);
	if (node == NUMA_NO_NODE)
		node = set->numa_node;

	tags = blk_mq_init_tags(nr_tags, reserved_tags, node,
				BLK_MQ_FLAG_TO_ALLOC_POLICY(set->flags));
	if (!tags)
		return NULL;
	...
}

struct blk_mq_tags *blk_mq_init_tags(unsigned int total_tags,
				     unsigned int reserved_tags,
				     int node, int alloc_policy)
{
	struct blk_mq_tags *tags;

	if (total_tags > BLK_MQ_TAG_MAX) {
		pr_err("blk-mq: tag depth too large\n");
		return NULL;
	}

	tags = kzalloc_node(sizeof(*tags), GFP_KERNEL, node);
	if (!tags)
		return NULL;

	tags->nr_tags = total_tags;
	tags->nr_reserved_tags = reserved_tags;

	return blk_mq_init_bitmap_tags(tags, node, alloc_policy);
}

static struct blk_mq_tags *blk_mq_init_bitmap_tags(struct blk_mq_tags *tags,
						   int node, int alloc_policy)
{
	unsigned int depth = tags->nr_tags - tags->nr_reserved_tags;
	bool round_robin = alloc_policy == BLK_TAG_ALLOC_RR;

	if (bt_alloc(&tags->bitmap_tags, depth, round_robin, node))
		goto free_tags;
	if (bt_alloc(&tags->breserved_tags, tags->nr_reserved_tags, round_robin,
		     node))
		goto free_bitmap_tags;

	return tags;
free_bitmap_tags:
	sbitmap_queue_free(&tags->bitmap_tags);
free_tags:
	kfree(tags);
	return NULL;
}

static int bt_alloc(struct sbitmap_queue *bt, unsigned int depth,
		    bool round_robin, int node)
{
	return sbitmap_queue_init_node(bt, depth, -1, round_robin, GFP_KERNEL,
				       node);
}

int sbitmap_queue_init_node(struct sbitmap_queue *sbq, unsigned int depth,
			    int shift, bool round_robin, gfp_t flags, int node)
{
	int ret;
	int i;

	ret = sbitmap_init_node(&sbq->sb, depth, shift, flags, node);                        // 分配sbq->sb.map并初始化
	if (ret)                                                                             // 若ret不为0
		return ret;                                                                      // 返回ret

	sbq->alloc_hint = alloc_percpu_gfp(unsigned int, flags);                             // 申请sbq->alloc_hint的per-cpu实例
	if (!sbq->alloc_hint) {                                                              // 若sbq->alloc_hint为NULL
		sbitmap_free(&sbq->sb);                                                          // 释放sbq->sb.map
		return -ENOMEM;                                                                  // 返回-ENOMEM
	}

	if (depth && !round_robin) {                                                         // 
		for_each_possible_cpu(i)                                                         // 遍历possible的cpu
			*per_cpu_ptr(sbq->alloc_hint, i) = prandom_u32() % depth;                    // 初始化sbq->alloc_hint在各个cpu上的实例为小于depth的随机值
	}

	sbq->min_shallow_depth = UINT_MAX;                                                   // 
	sbq->wake_batch = sbq_calc_wake_batch(sbq, depth);                                   // 
	atomic_set(&sbq->wake_index, 0);                                                     // 
	atomic_set(&sbq->ws_active, 0);                                                      // 

	sbq->ws = kzalloc_node(SBQ_WAIT_QUEUES * sizeof(*sbq->ws), flags, node);             // 
	if (!sbq->ws) {                                                                      // 
		free_percpu(sbq->alloc_hint);                                                    // 
		sbitmap_free(&sbq->sb);                                                          // 
		return -ENOMEM;                                                                  // 返回-ENOMEM
	}

	for (i = 0; i < SBQ_WAIT_QUEUES; i++) {                                              // 
		init_waitqueue_head(&sbq->ws[i].wait);                                           // 
		atomic_set(&sbq->ws[i].wait_cnt, sbq->wake_batch);                               // 
	}

	sbq->round_robin = round_robin;                                                      // 
	return 0;                                                                            // 返回0
}
EXPORT_SYMBOL_GPL(sbitmap_queue_init_node);

int sbitmap_init_node(struct sbitmap *sb, unsigned int depth, int shift,
		      gfp_t flags, int node)
{
	unsigned int bits_per_word;
	unsigned int i;

	if (shift < 0) {                                                                     // 若shift < 0为true，shift表示bits在word中的密度，shift越大密度越大
		shift = ilog2(BITS_PER_LONG);                                                    // 若BITS_PER_LONG为64，则shift为6
		/*
		 * If the bitmap is small, shrink the number of bits per word so
		 * we spread over a few cachelines, at least. If less than 4
		 * bits, just forget about it, it's not going to work optimally
		 * anyway.
		 */
		if (depth >= 4) {                                                                // 若depth太小则没有必要优化
			while ((4U << shift) > depth)                                                // 假设depth为32，则shift为3
				shift--;                                                                 // 
		}
	}
	bits_per_word = 1U << shift;                                                         // 若shift为3，则bits_per_word为8
	if (bits_per_word > BITS_PER_LONG)                                                   // 若bits_per_word > BITS_PER_LONG为true
		return -EINVAL;                                                                  // 返回-EINVAL

	sb->shift = shift;                                                                   // 设置sb->shift为shift
	sb->depth = depth;                                                                   // 设置sb->depth为depth
	sb->map_nr = DIV_ROUND_UP(sb->depth, bits_per_word);                                 // 将sb->depth对bits_per_word向上取整

	if (depth == 0) {                                                                    // 若depth为0
		sb->map = NULL;                                                                  // 设置sb->map为NULL
		return 0;                                                                        // 返回0
	}

	sb->map = kcalloc_node(sb->map_nr, sizeof(*sb->map), flags, node);                   // 申请sb->map
	if (!sb->map)                                                                        // 若sb->map为NULL
		return -ENOMEM;                                                                  // 返回-ENOMEM

	for (i = 0; i < sb->map_nr; i++) {                                                   // 
		sb->map[i].depth = min(depth, bits_per_word);                                    // 设置sb->map[i].depth为depth和bits_per_word的最小者
		depth -= sb->map[i].depth;                                                       // 更细depth
		spin_lock_init(&sb->map[i].swap_lock);                                           // 初始化sb->map[i].swap_lock
	}
	return 0;                                                                            // 返回0
}
EXPORT_SYMBOL_GPL(sbitmap_init_node);

int __sbitmap_queue_get(struct sbitmap_queue *sbq)
{
	unsigned int hint, depth;
	int nr;

	hint = this_cpu_read(*sbq->alloc_hint);                                              // 获取当前cpu的hint
	depth = READ_ONCE(sbq->sb.depth);                                                    // 获取depth
	if (unlikely(hint >= depth)) {
		hint = depth ? prandom_u32() % depth : 0;                                        //   
		this_cpu_write(*sbq->alloc_hint, hint);                                          // 
	}
	nr = sbitmap_get(&sbq->sb, hint, sbq->round_robin);                                  // 

	if (nr == -1) {                                                                      // 若nr为-1
		/* If the map is full, a hint won't do us much good. */
		this_cpu_write(*sbq->alloc_hint, 0);                                             // 
	} else if (nr == hint || unlikely(sbq->round_robin)) {                               // 
		/* Only update the hint if we used it. */
		hint = nr + 1;                                                                   // 
		if (hint >= depth - 1)                                                           // 
			hint = 0;                                                                    // 
		this_cpu_write(*sbq->alloc_hint, hint);                                          // 
	}

	return nr;                                                                           // 返回nr
}
EXPORT_SYMBOL_GPL(__sbitmap_queue_get);

#define SB_NR_TO_INDEX(sb, bitnr) ((bitnr) >> (sb)->shift)
#define SB_NR_TO_BIT(sb, bitnr) ((bitnr) & ((1U << (sb)->shift) - 1U))

int sbitmap_get(struct sbitmap *sb, unsigned int alloc_hint, bool round_robin)
{
	unsigned int i, index;
	int nr = -1;

	index = SB_NR_TO_INDEX(sb, alloc_hint);                                              // index = alloc_hint >> sb->shift

	/*
	 * Unless we're doing round robin tag allocation, just use the
	 * alloc_hint to find the right word index. No point in looping
	 * twice in find_next_zero_bit() for that case.
	 */
	if (round_robin)                                                                     // 若round_robin为true
		alloc_hint = SB_NR_TO_BIT(sb, alloc_hint);                                       // alloc_hint = alloc_hint & (1U << sb->shift - 1U)
	else
		alloc_hint = 0;                                                                  // 设置alloc_hint为0

	for (i = 0; i < sb->map_nr; i++) {                                                   // 循环[0, sb->map_nr)
		nr = sbitmap_find_bit_in_index(sb, index, alloc_hint,
						round_robin);                                                    // 在sb->map[index].word中查找为0的bit位，alloc_hint查找的起点，!round_robin表示是否会wrap
		if (nr != -1) {                                                                  // 若nr不为-1
			nr += index << sb->shift;                                                    // 确定最终的nr，将index之前的bits也考虑进来
			break;                                                                       // 退出循环
		}

		/* Jump to next index. */
		alloc_hint = 0;                                                                  // 设置alloc_hint为0
		if (++index >= sb->map_nr)                                                       // 更新index
			index = 0;                                                                   // 更新index
	}

	return nr;                                                                           // 返回nr
}
EXPORT_SYMBOL_GPL(sbitmap_get);

static int sbitmap_find_bit_in_index(struct sbitmap *sb, int index,
				     unsigned int alloc_hint, bool round_robin)
{
	int nr;

	do {
		nr = __sbitmap_get_word(&sb->map[index].word,
					sb->map[index].depth, alloc_hint,
					!round_robin);                                                       // 在word中查找为0的bit位，alloc_hint查找的起点，!round_robin表示是否会wrap
		if (nr != -1)                                                                    // 若nr不为-1
			break;                                                                       // 退出循环
		if (!sbitmap_deferred_clear(sb, index))                                          // 
			break;                                                                       // 退出循环
	} while (1);

	return nr;                                                                           // 返回nr
}

/*
 * See if we have deferred clears that we can batch move
 */
static inline bool sbitmap_deferred_clear(struct sbitmap *sb, int index)
{
	unsigned long mask, val;
	bool ret = false;
	unsigned long flags;

	spin_lock_irqsave(&sb->map[index].swap_lock, flags);

	if (!sb->map[index].cleared)
		goto out_unlock;

	/*
	 * First get a stable cleared mask, setting the old mask to 0.
	 */
	mask = xchg(&sb->map[index].cleared, 0);

	/*
	 * Now clear the masked bits in our free word
	 */
	do {
		val = sb->map[index].word;
	} while (cmpxchg(&sb->map[index].word, val, val & ~mask) != val);

	ret = true;
out_unlock:
	spin_unlock_irqrestore(&sb->map[index].swap_lock, flags);
	return ret;
}

static int __sbitmap_get_word(unsigned long *word, unsigned long depth,
			      unsigned int hint, bool wrap)
{
	unsigned int orig_hint = hint;
	int nr;

	while (1) {
		nr = find_next_zero_bit(word, depth, hint);                                      // find the next cleared bit in a memory region, hint is the bitnumber to start searching at
		if (unlikely(nr >= depth)) {                                                     // no bits are zero, returns depth
			/*
			 * We started with an offset, and we didn't reset the
			 * offset to 0 in a failure case, so start from 0 to
			 * exhaust the map.
			 */
			if (orig_hint && hint && wrap) {                                             // 若orig_hint、hint &&、wrap不为0
				hint = orig_hint = 0;                                                    // hint和orig_hint为0，表示从0开始查找
				continue;                                                                // 下步循环
			}
			return -1;                                                                   // 返回-1
		}

		if (!test_and_set_bit_lock(nr, word))                                            // set a bit and return its old value, for lock
			break;                                                                       // 退出循环

		hint = nr + 1;                                                                   // 更新hint
		if (hint >= depth - 1)                                                           // 若hint >= depth - 1为true
			hint = 0;                                                                    // 设置hint为0
	}

	return nr;                                                                           // 返回nr
}














