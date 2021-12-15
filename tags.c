
static const struct blk_mq_ops scsi_mq_ops = {
	.get_budget	= scsi_mq_get_budget,
	.put_budget	= scsi_mq_put_budget,
	.queue_rq	= scsi_queue_rq,
	.commit_rqs	= scsi_commit_rqs,
	.complete	= scsi_softirq_done,
	.timeout	= scsi_timeout,
#ifdef CONFIG_BLK_DEBUG_FS
	.show_rq	= scsi_show_rq,
#endif
	.init_request	= scsi_mq_init_request,
	.exit_request	= scsi_mq_exit_request,
	.initialize_rq_fn = scsi_initialize_rq,
	.cleanup_rq	= scsi_cleanup_rq,
	.busy		= scsi_mq_lld_busy,
	.map_queues	= scsi_map_queues,
};

static const struct blk_mq_ops scsi_mq_ops_no_commit = {
	.get_budget	= scsi_mq_get_budget,
	.put_budget	= scsi_mq_put_budget,
	.queue_rq	= scsi_queue_rq,
	.complete	= scsi_softirq_done,
	.timeout	= scsi_timeout,
#ifdef CONFIG_BLK_DEBUG_FS
	.show_rq	= scsi_show_rq,
#endif
	.init_request	= scsi_mq_init_request,
	.exit_request	= scsi_mq_exit_request,
	.initialize_rq_fn = scsi_initialize_rq,
	.cleanup_rq	= scsi_cleanup_rq,
	.busy		= scsi_mq_lld_busy,
	.map_queues	= scsi_map_queues,
};




ufs_mtk_probe 
	|-- ufshcd_pltfrm_init
			|-- ufshcd_init
					|-- scsi_add_host
					|		|-- scsi_add_host_with_dma
					|				|-- scsi_mq_setup_tags
					|						|-- blk_mq_alloc_tag_set  // driver tag
					|-- async_schedule(ufshcd_async_scan, hba)
							|-- ufshcd_async_scan
									|-- ufshcd_probe_hba
											|-- scsi_scan_host
													|-- async_schedule(do_scan_async, data)
															|-- do_scan_async
																	|-- do_scsi_scan_host
																			|-- scsi_scan_host_selected
																					|-- scsi_scan_channel
																							|-- __scsi_scan_target
																									|-- scsi_report_lun_scan
																											|-- scsi_probe_and_add_lun / scsi_sequential_lun_scan
																													|-- scsi_alloc_sdev
																															|-- scsi_mq_alloc_queue
																																	|-- blk_mq_init_queue
																																			|-- blk_mq_init_allocated_queue
																																					|-- blk_mq_realloc_hw_ctxs
																																							|-- blk_mq_alloc_and_init_hctx
																																									|-- hctx = blk_mq_alloc_hctx(q, set, node)
																																									|-- blk_mq_init_hctx(q, set, hctx, hctx_idx)
																																											|-- hctx->tags = set->tags[hctx_idx]
																																					|-- blk_mq_add_queue_tag_set
																																					|-- elevator_init_mq
																																							|-- blk_mq_init_sched
																																									|-- blk_mq_sched_alloc_tags
																																											|-- blk_mq_alloc_rq_map





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
	.can_queue		= UFSHCD_CAN_QUEUE,                                                  // UFSHCD_CAN_QUEUE为32
	.max_segment_size	= PRDT_DATA_BYTE_COUNT_MAX,
	.max_host_blocked	= 1,
	.track_queue_depth	= 1,
	.sdev_groups		= ufshcd_driver_groups,
	.dma_boundary		= PAGE_SIZE - 1,
};

static const struct blk_mq_ops scsi_mq_ops = {
	.get_budget	= scsi_mq_get_budget,
	.put_budget	= scsi_mq_put_budget,
	.queue_rq	= scsi_queue_rq,
	.commit_rqs	= scsi_commit_rqs,
	.complete	= scsi_softirq_done,
	.timeout	= scsi_timeout,
#ifdef CONFIG_BLK_DEBUG_FS
	.show_rq	= scsi_show_rq,
#endif
	.init_request	= scsi_mq_init_request,
	.exit_request	= scsi_mq_exit_request,
	.initialize_rq_fn = scsi_initialize_rq,
	.cleanup_rq	= scsi_cleanup_rq,
	.busy		= scsi_mq_lld_busy,
	.map_queues	= scsi_map_queues,
};

static const struct blk_mq_ops scsi_mq_ops_no_commit = {
	.get_budget	= scsi_mq_get_budget,
	.put_budget	= scsi_mq_put_budget,
	.queue_rq	= scsi_queue_rq,
	.complete	= scsi_softirq_done,
	.timeout	= scsi_timeout,
#ifdef CONFIG_BLK_DEBUG_FS
	.show_rq	= scsi_show_rq,
#endif
	.init_request	= scsi_mq_init_request,
	.exit_request	= scsi_mq_exit_request,
	.initialize_rq_fn = scsi_initialize_rq,
	.cleanup_rq	= scsi_cleanup_rq,
	.busy		= scsi_mq_lld_busy,
	.map_queues	= scsi_map_queues,
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
	unsigned int		nr_hw_queues;	/* nr hw queues across maps */                   // 重点：实际使用的hctx数量
	unsigned int		queue_depth;	/* max hw supported */
	unsigned int		reserved_tags;                                                   // 重点：支持的reserved tags数量，使用举例，在使用blk_get_request时flags带有BLK_MQ_REQ_RESERVED则会从tags->breserved_tags中分配tag，linux-5.4版本ufshcd没有使用到reserved tags
	unsigned int		cmd_size;	/* per-request extra data */
	int			numa_node;
	unsigned int		timeout;
	unsigned int		flags;		/* BLK_MQ_F_* */
	void			*driver_data;

	struct blk_mq_tags	**tags;                                                          // 每一个hctx都有一个struct blk_mq_tags实例

	struct mutex		tag_list_lock;
	struct list_head	tag_list;
};

struct Scsi_Host {
	...
	/* Area to keep a shared tag map */
	struct blk_mq_tag_set	tag_set;                                                     // 一个host一个struct blk_mq_tag_set实例
	...
	int can_queue;
	short cmd_per_lun;
	...
	/*
	 * In scsi-mq mode, the number of hardware queues supported by the LLD.
	 *
	 * Note: it is assumed that each hardware queue has a queue depth of
	 * can_queue. In other words, the total queue depth per host
	 * is nr_hw_queues * can_queue.
	 */
	unsigned nr_hw_queues;
	...
};

/**
 * scsi_add_host_with_dma - add a scsi host with dma device
 * @shost:	scsi host pointer to add
 * @dev:	a struct device of type scsi class
 * @dma_dev:	dma device for the host
 *
 * Note: You rarely need to worry about this unless you're in a
 * virtualised host environments, so use the simpler scsi_add_host()
 * function instead.
 *
 * Return value: 
 * 	0 on success / != 0 for error
 **/
int scsi_add_host_with_dma(struct Scsi_Host *shost, struct device *dev,
			   struct device *dma_dev)
{
	struct scsi_host_template *sht = shost->hostt;
	...

	error = scsi_mq_setup_tags(shost);                                                   // 重点：初始化及分配shost->tag_set下成员，注意一个host一个struct blk_mq_tag_set实例
	if (error)
		goto fail;

	...
}
EXPORT_SYMBOL(scsi_add_host_with_dma);

int scsi_mq_setup_tags(struct Scsi_Host *shost)
{
	unsigned int cmd_size, sgl_size;

	sgl_size = max_t(unsigned int, sizeof(struct scatterlist),
				scsi_mq_inline_sgl_size(shost));                                         // 计算sgl_size
	cmd_size = sizeof(struct scsi_cmnd) + shost->hostt->cmd_size + sgl_size;             // 计算cmd_size
	if (scsi_host_get_prot(shost))                                                       // shost->prot_capabilities
		cmd_size += sizeof(struct scsi_data_buffer) +
			sizeof(struct scatterlist) * SCSI_INLINE_PROT_SG_CNT;                        // 计算cmd_size

	memset(&shost->tag_set, 0, sizeof(shost->tag_set));                                  // 重点：清空shost->tag_set空间，一个host一个struct blk_mq_tag_set实例
	if (shost->hostt->commit_rqs)                                                        // 若shost->hostt->commit_rqs不为NULL
		shost->tag_set.ops = &scsi_mq_ops;                                               // 设置shost->tag_set.ops为&scsi_mq_ops
	else
		shost->tag_set.ops = &scsi_mq_ops_no_commit;                                     // 设置shost->tag_set.ops为&scsi_mq_ops_no_commit
	shost->tag_set.nr_hw_queues = shost->nr_hw_queues ? : 1;                             // 重点：对于ufshcd而言shost->nr_hw_queues没有显示初始化，也即初始化为0，因此shost->tag_set.nr_hw_queues会被设置为1，表示实际在使用的hctx数量，对于其他host驱动shost->nr_hw_queues可能会初始化非0值
	shost->tag_set.queue_depth = shost->can_queue;                                       // 设置shost->tag_set.queue_depth为shost->can_queue
	shost->tag_set.cmd_size = cmd_size;                                                  // 设置shost->tag_set.cmd_size为cmd_size
	shost->tag_set.numa_node = NUMA_NO_NODE;                                             // 设置shost->tag_set.numa_node为NUMA_NO_NODE
	shost->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;                                        // 设置shost->tag_set.flags为BLK_MQ_F_SHOULD_MERGE
	shost->tag_set.flags |=
		BLK_ALLOC_POLICY_TO_MQ_FLAG(shost->hostt->tag_alloc_policy);                     // 添加BLK_ALLOC_POLICY_TO_MQ_FLAG(shost->hostt->tag_alloc_policy)到shost->tag_set.flags
	shost->tag_set.driver_data = shost;                                                  // 设置shost->tag_set.driver_data为shost

	return blk_mq_alloc_tag_set(&shost->tag_set);                                        // 分配及初始化shost->tag_set下的成员
}

/*
 * Alloc a tag set to be associated with one or more request queues.
 * May fail with EINVAL for various error conditions. May adjust the
 * requested depth down, if it's too large. In that case, the set
 * value will be stored in set->queue_depth.
 */
int blk_mq_alloc_tag_set(struct blk_mq_tag_set *set)
{
	int i, ret;

	BUILD_BUG_ON(BLK_MQ_MAX_DEPTH > 1 << BLK_MQ_UNIQUE_TAG_BITS);                        // 自检

	if (!set->nr_hw_queues)                                                              // 在scsi_mq_setup_tags中设置set->nr_hw_queues为1
		return -EINVAL;                                                                  // 返回-EINVAL
	if (!set->queue_depth)                                                               // 若set->queue_depth为0
		return -EINVAL;                                                                  // 返回-EINVAL
	if (set->queue_depth < set->reserved_tags + BLK_MQ_TAG_MIN)                          // 在ufshcd驱动中没有使用到reserved tags，reserved tags可以给特殊的需求使用
		return -EINVAL;                                                                  // 返回-EINVAL

	if (!set->ops->queue_rq)                                                             // 若set->ops->queue_rq为NULL
		return -EINVAL;                                                                  // 返回-EINVAL

	if (!set->ops->get_budget ^ !set->ops->put_budget)                                   // set->ops->get_budget和set->ops->put_budget必须成对定义
		return -EINVAL;                                                                  // 返回-EINVAL

	if (set->queue_depth > BLK_MQ_MAX_DEPTH) {                                           // 若set->queue_depth > BLK_MQ_MAX_DEPTH为true
		pr_info("blk-mq: reduced tag depth to %u\n",
			BLK_MQ_MAX_DEPTH);                                                           // 打印提示信息
		set->queue_depth = BLK_MQ_MAX_DEPTH;                                             // 设置set->queue_depth为BLK_MQ_MAX_DEPTH
	}

	if (!set->nr_maps)                                                                   // scsi_mq_setup_tags中没有设定set->nr_maps，即初始化为0
		set->nr_maps = 1;                                                                // 设置set->nr_maps为1，即scsi框架初始化set->nr_maps为1
	else if (set->nr_maps > HCTX_MAX_TYPES)                                              // 若set->nr_maps > HCTX_MAX_TYPES为true
		return -EINVAL;                                                                  // 返回-EINVAL

	/*
	 * If a crashdump is active, then we are potentially in a very
	 * memory constrained environment. Limit us to 1 queue and
	 * 64 tags to prevent using too much memory.
	 */
	if (is_kdump_kernel()) {                                                             // 若当前处于crashdump kernel中
		set->nr_hw_queues = 1;                                                           // 设置set->nr_hw_queues为1，实际使用的hctx数量
		set->nr_maps = 1;                                                                // 设置set->nr_maps为1
		set->queue_depth = min(64U, set->queue_depth);                                   // 设置set->queue_depth为64与set->queue_depth最小者
	}
	/*
	 * There is no use for more h/w queues than cpus if we just have
	 * a single map
	 */
	if (set->nr_maps == 1 && set->nr_hw_queues > nr_cpu_ids)                             // 参考注释理解代码意图，若set->nr_maps为1，并且set->nr_hw_queues > nr_cpu_ids为true
		set->nr_hw_queues = nr_cpu_ids;                                                  // 设置set->nr_hw_queues为nr_cpu_ids

	set->tags = kcalloc_node(nr_hw_queues(set), sizeof(struct blk_mq_tags *),
				 GFP_KERNEL, set->numa_node);                                            // 申请set->nr_maps == 1? nr_cpu_ids : max(set->nr_hw_queues, nr_cpu_ids)对应的struct blk_mq_tags实例指针的数组，set->tags作为数组首元素地址
	if (!set->tags)                                                                      // 若set->tags为NULL
		return -ENOMEM;                                                                  // 返回-ENOMEM

	ret = -ENOMEM;                                                                       // 设置ret为-ENOMEM
	for (i = 0; i < set->nr_maps; i++) {                                                 // 循环[0, set->nr_maps)，对于ufshcd而言为[0, 1)
		set->map[i].mq_map = kcalloc_node(nr_cpu_ids,
						  sizeof(set->map[i].mq_map[0]),
						  GFP_KERNEL, set->numa_node);                                   // 分配所支持的hctx的数量对应的映射关系
		if (!set->map[i].mq_map)                                                         // 若set->map[i].mq_map为NULL
			goto out_free_mq_map;                                                        // 跳转至out_free_mq_map
		set->map[i].nr_queues = is_kdump_kernel() ? 1 : set->nr_hw_queues;               // 若当前处于crashdump kernel中则设置set->map[i].nr_queues为1，否则设置为set->nr_hw_queues，因为set->map[i]中存放的是所支持hctx对应的映射关系，而set->map[i].nr_queues表示的是实际会用到的hctx数量
	}

	ret = blk_mq_update_queue_map(set);                                                  // 建立ctx和hctx的映射表
	if (ret)                                                                             // 若ret为true
		goto out_free_mq_map;                                                            // 跳转至out_free_mq_map

	ret = blk_mq_alloc_rq_maps(set);                                                     // 重点：主要为set中正是用的hctx的tags分配sbitmap，tags->rqs，tags->static_rqs及tags->static_rqs对应的struct request实例的内存，并进行相应初始化
	if (ret)                                                                             // 若ret为true
		goto out_free_mq_map;                                                            // 跳转至out_free_mq_map

	mutex_init(&set->tag_list_lock);                                                     // 初始化set->tag_list_lock
	INIT_LIST_HEAD(&set->tag_list);                                                      // 初始化set->tag_list

	return 0;                                                                            // 返回0

out_free_mq_map:
	for (i = 0; i < set->nr_maps; i++) {
		kfree(set->map[i].mq_map);
		set->map[i].mq_map = NULL;
	}
	kfree(set->tags);
	set->tags = NULL;
	return ret;
}
EXPORT_SYMBOL(blk_mq_alloc_tag_set);

static int blk_mq_update_queue_map(struct blk_mq_tag_set *set)
{
	if (set->ops->map_queues && !is_kdump_kernel()) {                                    // 若set->ops->map_queues不为NULL，并且当前不处于crashdump kernel中
		int i;

		/*
		 * transport .map_queues is usually done in the following
		 * way:
		 *
		 * for (queue = 0; queue < set->nr_hw_queues; queue++) {
		 * 	mask = get_cpu_mask(queue)
		 * 	for_each_cpu(cpu, mask)
		 * 		set->map[x].mq_map[cpu] = queue;
		 * }
		 *
		 * When we need to remap, the table has to be cleared for
		 * killing stale mapping since one CPU may not be mapped
		 * to any hw queue.
		 */
		for (i = 0; i < set->nr_maps; i++)                                               // 循环[0, set->nr_maps)
			blk_mq_clear_mq_map(&set->map[i]);                                           // 清除原有ctx和hctx的映射表

		return set->ops->map_queues(set);                                                // 调用set->ops->map_queues建立ctx和hctx的映射表
	} else {
		BUG_ON(set->nr_maps > 1);                                                        // 处于crashdump kernel中set->nr_maps应当为1
		return blk_mq_map_queues(&set->map[HCTX_TYPE_DEFAULT]);                          // 建立ctx和hctx的映射表
	}
}

/*
 * Allocate the request maps associated with this tag_set. Note that this
 * may reduce the depth asked for, if memory is tight. set->queue_depth
 * will be updated to reflect the allocated depth.
 */
static int blk_mq_alloc_rq_maps(struct blk_mq_tag_set *set)
{
	unsigned int depth;
	int err;

	depth = set->queue_depth;                                                            // 获取depth
	do {
		err = __blk_mq_alloc_rq_maps(set);                                               // 主要为set中正是用的hctx的tags分配sbitmap，tags->rqs，tags->static_rqs及tags->static_rqs对应的struct request实例的内存，并进行相应初始化
		if (!err)                                                                        // 若err为0
			break;                                                                       // 退出循环

		set->queue_depth >>= 1;                                                          // 减少set->queue_depth后再尝试申请
		if (set->queue_depth < set->reserved_tags + BLK_MQ_TAG_MIN) {                    // 若set->queue_depth < set->reserved_tags + BLK_MQ_TAG_MIN为true
			err = -ENOMEM;                                                               // 设置err为-ENOMEM
			break;                                                                       // 退出循环
		}
	} while (set->queue_depth);                                                          // 若set->queue_depth不为0

	if (!set->queue_depth || err) {                                                      // 若set->queue_depth为0，或err不为0
		pr_err("blk-mq: failed to allocate request map\n");                              // 打印提示信息
		return -ENOMEM;                                                                  // 返回-ENOMEM
	}

	if (depth != set->queue_depth)                                                       // 若depth不为set->queue_depth
		pr_info("blk-mq: reduced tag depth (%u -> %u)\n",
						depth, set->queue_depth);                                        // 打印提示信息

	return 0;                                                                            // 返回0
}

static int __blk_mq_alloc_rq_maps(struct blk_mq_tag_set *set)
{
	int i;

	for (i = 0; i < set->nr_hw_queues; i++)                                              // 循环[0, set->nr_hw_queues)，set->nr_hw_queues为实际使用的hctx数量，对于ufshcd为1
		if (!__blk_mq_alloc_rq_map(set, i))                                              // 主要分配sbitmap，tags->rqs，tags->static_rqs及tags->static_rqs对应的struct request实例的内存，并进行相应初始化
			goto out_unwind;                                                             // 跳转至out_unwind

	return 0;                                                                            // 返回0

out_unwind:
	while (--i >= 0)
		blk_mq_free_rq_map(set->tags[i]);

	return -ENOMEM;
}

static bool __blk_mq_alloc_rq_map(struct blk_mq_tag_set *set, int hctx_idx)
{
	int ret = 0;

	set->tags[hctx_idx] = blk_mq_alloc_rq_map(set, hctx_idx,
					set->queue_depth, set->reserved_tags);                               // 重点：主要分配sbitmap，tags->rqs和tags->static_rqs，并进行相应初始化
	if (!set->tags[hctx_idx])                                                            // 若set->tags[hctx_idx]为NULL
		return false;                                                                    // 返回false

	ret = blk_mq_alloc_rqs(set, set->tags[hctx_idx], hctx_idx,
				set->queue_depth);                                                       // 为set->tags[hctx_idx]->static_rqs中的struct request指针分配struct request实例空间，并对分配的struct request实例初始化
	if (!ret)                                                                            // 若ret为0
		return true;                                                                     // 返回true

	blk_mq_free_rq_map(set->tags[hctx_idx]);
	set->tags[hctx_idx] = NULL;
	return false;
}

struct blk_mq_tags *blk_mq_alloc_rq_map(struct blk_mq_tag_set *set,
					unsigned int hctx_idx,
					unsigned int nr_tags,
					unsigned int reserved_tags)
{
	struct blk_mq_tags *tags;
	int node;

	node = blk_mq_hw_queue_to_node(&set->map[HCTX_TYPE_DEFAULT], hctx_idx);              // 获取hctx所在cpu的node
	if (node == NUMA_NO_NODE)                                                            // 若node为NUMA_NO_NODE
		node = set->numa_node;                                                           // 设置node为set->numa_node

	tags = blk_mq_init_tags(nr_tags, reserved_tags, node,
				BLK_MQ_FLAG_TO_ALLOC_POLICY(set->flags));                                // 分配struct blk_mq_tags实例，并初始化其下成员
	if (!tags)                                                                           // 若tags为NULL
		return NULL;                                                                     // 返回NULL

	tags->rqs = kcalloc_node(nr_tags, sizeof(struct request *),
				 GFP_NOIO | __GFP_NOWARN | __GFP_NORETRY,
				 node);                                                                  // 重点：分配包含nr_tags个struct request实例的指针的数组，tags->rqs作为数组首元素地址，相当于一个request map，该数组中的成员将在blk_mq_rq_ctx_init中初始化
	if (!tags->rqs) {                                                                    // 若tags->rqs为NULL
		blk_mq_free_tags(tags);                                                          // 释放tags->bitmap_tags、tags->breserved_tags及tags
		return NULL;                                                                     // 返回NULL
	}

	tags->static_rqs = kcalloc_node(nr_tags, sizeof(struct request *),
					GFP_NOIO | __GFP_NOWARN | __GFP_NORETRY,
					node);                                                               // 重点：分配包含nr_tags个struct request实例的指针的数组，tags->static_rqs作为数组首元素地址，相当于一个request map，该数组中成员将在blk_mq_alloc_rqs中初始化
	if (!tags->static_rqs) {                                                             // 若tags->static_rqs为NULL
		kfree(tags->rqs);                                                                // 释放tags->rqs
		blk_mq_free_tags(tags);                                                          // 释放tags->bitmap_tags、tags->breserved_tags及tags
		return NULL;                                                                     // 返回NULL
	}

	return tags;                                                                         // 返回tags
}

struct blk_mq_tags *blk_mq_init_tags(unsigned int total_tags,
				     unsigned int reserved_tags,
				     int node, int alloc_policy)
{
	struct blk_mq_tags *tags;

	if (total_tags > BLK_MQ_TAG_MAX) {                                                   // 若total_tags > BLK_MQ_TAG_MAX为true
		pr_err("blk-mq: tag depth too large\n");                                         // 打印提示信息
		return NULL;                                                                     // 返回NULL
	}

	tags = kzalloc_node(sizeof(*tags), GFP_KERNEL, node);                                // 分配tags
	if (!tags)                                                                           // 若tags为NULL
		return NULL;                                                                     // 返回NULL

	tags->nr_tags = total_tags;                                                          // 设置tags->nr_tags为total_tags
	tags->nr_reserved_tags = reserved_tags;                                              // 设置tags->nr_reserved_tags为reserved_tags

	return blk_mq_init_bitmap_tags(tags, node, alloc_policy);                            // 分配及初始化tags->bitmap_tags和tags->breserved_tags
}

static struct blk_mq_tags *blk_mq_init_bitmap_tags(struct blk_mq_tags *tags,
						   int node, int alloc_policy)
{
	unsigned int depth = tags->nr_tags - tags->nr_reserved_tags;                         // 计算depth
	bool round_robin = alloc_policy == BLK_TAG_ALLOC_RR;                                 // 设置round_robin和alloc_policy为BLK_TAG_ALLOC_RR

	if (bt_alloc(&tags->bitmap_tags, depth, round_robin, node))                          // 分配并初始化tags->bitmap_tags
		goto free_tags;                                                                  // 跳转至free_tags
	if (bt_alloc(&tags->breserved_tags, tags->nr_reserved_tags, round_robin,
		     node))                                                                      // 分配并初始化tags->breserved_tags
		goto free_bitmap_tags;                                                           // 跳转至free_bitmap_tags

	return tags;                                                                         // 返回tags
free_bitmap_tags:
	sbitmap_queue_free(&tags->bitmap_tags);
free_tags:
	kfree(tags);
	return NULL;
}

int blk_mq_alloc_rqs(struct blk_mq_tag_set *set, struct blk_mq_tags *tags,
		     unsigned int hctx_idx, unsigned int depth)
{
	unsigned int i, j, entries_per_page, max_order = 4;                                  // 设置max_order为4
	size_t rq_size, left;
	int node;

	node = blk_mq_hw_queue_to_node(&set->map[HCTX_TYPE_DEFAULT], hctx_idx);              // 确定hctx所在cpu的memory node
	if (node == NUMA_NO_NODE)                                                            // 若node为NUMA_NO_NODE
		node = set->numa_node;                                                           // 设置node为set->numa_node

	INIT_LIST_HEAD(&tags->page_list);                                                    // 初始化tags->page_list

	/*
	 * rq_size is the size of the request plus driver payload, rounded
	 * to the cacheline size
	 */
	rq_size = round_up(sizeof(struct request) + set->cmd_size,
				cache_line_size());                                                      // 重点：计算rq_size，分配struct request实例时还会额外分配set->cmd_size个bytes空间
	left = rq_size * depth;                                                              // 计算left

	for (i = 0; i < depth; ) {                                                           // [0, depth)
		int this_order = max_order;                                                      // 设置this_order为max_order
		struct page *page;
		int to_do;
		void *p;

		while (this_order && left < order_to_size(this_order - 1))                       // 计算最小符合要求的this_order
			this_order--;                                                                // this_order自减

		do {
			page = alloc_pages_node(node,
				GFP_NOIO | __GFP_NOWARN | __GFP_NORETRY | __GFP_ZERO,
				this_order);                                                             // 分配this_order阶page
			if (page)                                                                    // 若page不为NULL
				break;                                                                   // 退出循环
			if (!this_order--)                                                           // 若this_order为0，this_order自减
				break;                                                                   // 退出循环
			if (order_to_size(this_order) < rq_size)                                     // 若order_to_size(this_order) < rq_size为true
				break;                                                                   // 退出循环
		} while (1);

		if (!page)                                                                       // 若page为NULL
			goto fail;                                                                   // 跳转至fail

		page->private = this_order;                                                      // 设置page->private为this_order
		list_add_tail(&page->lru, &tags->page_list);                                     // 添加page->lru到tags->page_list

		p = page_address(page);                                                          // 获取page表达的虚拟地址p
		/*
		 * Allow kmemleak to scan these pages as they contain pointers
		 * to additional allocations like via ops->init_request().
		 */
		kmemleak_alloc(p, order_to_size(this_order), 1, GFP_NOIO);                       // register a newly allocated object
		entries_per_page = order_to_size(this_order) / rq_size;                          // 获取this_order阶page所能表达的request数量，即为entries_per_page
		to_do = min(entries_per_page, depth - i);                                        // 计算下本次分配的request数量
		left -= to_do * rq_size;                                                         // 更新left
		for (j = 0; j < to_do; j++) {                                                    // 循环[0, to_do)
			struct request *rq = p;                                                      // 设置rq为p

			tags->static_rqs[i] = rq;                                                    // 重点：设置tags->static_rqs[i]为rq
			if (blk_mq_init_request(set, rq, hctx_idx, node)) {                          // 初始化rq
				tags->static_rqs[i] = NULL;                                              // 设置tags->static_rqs[i]为NULL
				goto fail;                                                               // 跳转至fail
			}

			p += rq_size;                                                                // 重点：p自加rq_size，表示下一个rq的地址
			i++;                                                                         // 重点：i自加1
		}
	}
	return 0;                                                                            // 返回0

fail:
	blk_mq_free_rqs(set, tags, hctx_idx);
	return -ENOMEM;
}

static int blk_mq_init_request(struct blk_mq_tag_set *set, struct request *rq,
			       unsigned int hctx_idx, int node)
{
	int ret;

	if (set->ops->init_request) {                                                        // 若set->ops->init_request不为NULL
		ret = set->ops->init_request(set, rq, hctx_idx, node);                           // 调用set->ops->init_request
		if (ret)                                                                         // 若ret不为0
			return ret;                                                                  // 返回ret
	}

	WRITE_ONCE(rq->state, MQ_RQ_IDLE);                                                   // 设置rq->state为MQ_RQ_IDLE
	return 0;                                                                            // 返回0
}

static int scsi_mq_init_request(struct blk_mq_tag_set *set, struct request *rq,
				unsigned int hctx_idx, unsigned int numa_node)
{
	struct Scsi_Host *shost = set->driver_data;                                          // 取出shost
	const bool unchecked_isa_dma = shost->unchecked_isa_dma;                             // 取出unchecked_isa_dma
	struct scsi_cmnd *cmd = blk_mq_rq_to_pdu(rq);                                        // 获取cmd
	struct scatterlist *sg;

	if (unchecked_isa_dma)                                                               // unchecked_isa_dma为true
		cmd->flags |= SCMD_UNCHECKED_ISA_DMA;                                            // 包含SCMD_UNCHECKED_ISA_DMA到cmd->flags中
	cmd->sense_buffer = scsi_alloc_sense_buffer(unchecked_isa_dma,
						    GFP_KERNEL, numa_node);                                      // 分配cmd->sense_buffer空间
	if (!cmd->sense_buffer)                                                              // 若cmd->sense_buffer为NULL
		return -ENOMEM;                                                                  // 返回-ENOMEM
	cmd->req.sense = cmd->sense_buffer;                                                  // 设置cmd->req.sense为cmd->sense_buffer

	if (scsi_host_get_prot(shost)) {                                                     // shost->prot_capabilities
		sg = (void *)cmd + sizeof(struct scsi_cmnd) +
			shost->hostt->cmd_size;                                                      // 获取sg
		cmd->prot_sdb = (void *)sg + scsi_mq_inline_sgl_size(shost);                     // 设置cmd->prot_sdb为(void *)sg + scsi_mq_inline_sgl_size(shost)
	}

	return 0;                                                                            // 返回0
}








static struct request *blk_mq_get_request(struct request_queue *q,
					  struct bio *bio,
					  struct blk_mq_alloc_data *data)
{
	struct elevator_queue *e = q->elevator;                                              // 取出e
	struct request *rq;
	unsigned int tag;
	bool clear_ctx_on_error = false;
	u64 alloc_time_ns = 0;

	blk_queue_enter_live(q);

	/* alloc_time includes depth and tag waits */
	if (blk_queue_rq_alloc_time(q))
		alloc_time_ns = ktime_get_ns();

	data->q = q;                                                                         // 设置data->q为q
	if (likely(!data->ctx)) {                                                            // 若data->ctx为NULL
		data->ctx = blk_mq_get_ctx(q);                                                   // 
		clear_ctx_on_error = true;                                                       // 设置clear_ctx_on_error为true
	}
	if (likely(!data->hctx))                                                             // 若data->hctx为NULL
		data->hctx = blk_mq_map_queue(q, data->cmd_flags,
						data->ctx);                                                      // 
	if (data->cmd_flags & REQ_NOWAIT)                                                    // 若data->cmd_flags包含REQ_NOWAIT
		data->flags |= BLK_MQ_REQ_NOWAIT;                                                // 包含BLK_MQ_REQ_NOWAIT到data->flags

	if (e) {                                                                             // 若e不为NULL
		data->flags |= BLK_MQ_REQ_INTERNAL;                                              // 重点：包含BLK_MQ_REQ_INTERNAL到data->flags

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

	tag = blk_mq_get_tag(data);                                                          // 
	if (tag == BLK_MQ_TAG_FAIL) {                                                        // 若tag为BLK_MQ_TAG_FAIL
		if (clear_ctx_on_error)                                                          // 若clear_ctx_on_error为true
			data->ctx = NULL;                                                            // 设置data->ctx为NULL
		blk_queue_exit(q);                                                               // 
		return NULL;                                                                     // 返回NULL
	}

	rq = blk_mq_rq_ctx_init(data, tag, data->cmd_flags, alloc_time_ns);                  // 主要是再次初始化rq
	if (!op_is_flush(data->cmd_flags)) {                                                 // 
		rq->elv.icq = NULL;                                                              // 
		if (e && e->type->ops.prepare_request) {                                         // 
			if (e->type->icq_cache)                                                      // 
				blk_mq_sched_assign_ioc(rq);                                             // 

			e->type->ops.prepare_request(rq, bio);                                       // 
			rq->rq_flags |= RQF_ELVPRIV;                                                 // 
		}
	}
	data->hctx->queued++;                                                                // 重点：data->hctx->queued自加
	return rq;                                                                           // 返回rq
}

unsigned int blk_mq_get_tag(struct blk_mq_alloc_data *data)
{
	struct blk_mq_tags *tags = blk_mq_tags_from_data(data);                              // 获取tags
	struct sbitmap_queue *bt;
	struct sbq_wait_state *ws;
	DEFINE_SBQ_WAIT(wait);                                                               // 定义并初始化wait
	unsigned int tag_offset;
	int tag;

	if (data->flags & BLK_MQ_REQ_RESERVED) {                                             // 若data->flags包含BLK_MQ_REQ_RESERVED
		if (unlikely(!tags->nr_reserved_tags)) {                                         // 若tags->nr_reserved_tags为0
			WARN_ON_ONCE(1);                                                             // 自检警告
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
	    !hctx_may_queue(data->hctx, bt))                                                 // 
		return -1;                                                                       // 返回-1
	if (data->shallow_depth)                                                             // 若data->shallow_depth为true
		return __sbitmap_queue_get_shallow(bt, data->shallow_depth);                     // 
	else
		return __sbitmap_queue_get(bt);                                                  // 
}

/*
 * For shared tag users, we track the number of currently active users
 * and attempt to provide a fair share of the tag depth for each of them.
 */
static inline bool hctx_may_queue(struct blk_mq_hw_ctx *hctx,
				  struct sbitmap_queue *bt)
{
	unsigned int depth, users;

	if (!hctx || !(hctx->flags & BLK_MQ_F_TAG_SHARED))                                   // 重点：若hctx为NULL，或hctx->flags不包含BLK_MQ_F_TAG_SHARED
		return true;                                                                     // 返回true
	if (!test_bit(BLK_MQ_S_TAG_ACTIVE, &hctx->state))                                    // 若hctx->state不包含BLK_MQ_S_TAG_ACTIVE
		return true;                                                                     // 返回true

	/*
	 * Don't try dividing an ant
	 */
	if (bt->sb.depth == 1)                                                               // 若bt->sb.depth为1
		return true;                                                                     // 返回true

	users = atomic_read(&hctx->tags->active_queues);                                     // 获取users
	if (!users)                                                                          // 若users为0
		return true;                                                                     // 返回true

	/*
	 * Allow at least some tags
	 */
	depth = max((bt->sb.depth + users - 1) / users, 4U);                                 // 
	return atomic_read(&hctx->nr_active) < depth;                                        // 
}

static struct request *blk_mq_rq_ctx_init(struct blk_mq_alloc_data *data,
		unsigned int tag, unsigned int op, u64 alloc_time_ns)
{
	struct blk_mq_tags *tags = blk_mq_tags_from_data(data);                              // 获取tags
	struct request *rq = tags->static_rqs[tag];                                          // 重点：获取rq
	req_flags_t rq_flags = 0;

	if (data->flags & BLK_MQ_REQ_INTERNAL) {                                             // 若data->flags包含BLK_MQ_REQ_INTERNAL
		rq->tag = -1;                                                                    // 设置rq->tag为-1
		rq->internal_tag = tag;                                                          // 设置rq->internal_tag为tag
	} else {
		if (data->hctx->flags & BLK_MQ_F_TAG_SHARED) {                                   // 若data->hctx->flags包含BLK_MQ_F_TAG_SHARED
			rq_flags = RQF_MQ_INFLIGHT;                                                  // 设置rq_flags为RQF_MQ_INFLIGHT
			atomic_inc(&data->hctx->nr_active);                                          // 重点：data->hctx->nr_active自加
		}
		rq->tag = tag;                                                                   // 设置rq->tag为tag
		rq->internal_tag = -1;                                                           // 设置rq->internal_tag为-1
		data->hctx->tags->rqs[rq->tag] = rq;                                             // 重点：设置data->hctx->tags->rqs[rq->tag]为rq，即将hctx->tags->static_rqs[rq->tag]映射到hctx->tags->rqs[rq->tag]
	}

	/* csd/requeue_work/fifo_time is initialized before use */
	rq->q = data->q;                                                                     // 设置rq->q为data->q
	rq->mq_ctx = data->ctx;                                                              // 设置rq->mq_ctx为data->ctx
	rq->mq_hctx = data->hctx;                                                            // 设置rq->mq_hctx为data->hctx
	rq->rq_flags = rq_flags;                                                             // 设置rq->rq_flags为rq_flags
	rq->cmd_flags = op;                                                                  // 设置rq->cmd_flags为op
	if (data->flags & BLK_MQ_REQ_PREEMPT)                                                // 若data->flags包含BLK_MQ_REQ_PREEMPT
		rq->rq_flags |= RQF_PREEMPT;                                                     // 包含RQF_PREEMPT到rq->rq_flags
	if (blk_queue_io_stat(data->q))                                                      // 
		rq->rq_flags |= RQF_IO_STAT;                                                     // 包含RQF_IO_STAT到rq->rq_flags
	INIT_LIST_HEAD(&rq->queuelist);                                                      // 初始化rq->queuelist
	INIT_HLIST_NODE(&rq->hash);                                                          // 初始化rq->hash
	RB_CLEAR_NODE(&rq->rb_node);                                                         // 清空rq->rb_node
	rq->rq_disk = NULL;                                                                  // 设置rq->rq_disk为NULL
	rq->part = NULL;                                                                     // 设置rq->part为NULL
#ifdef CONFIG_BLK_RQ_ALLOC_TIME
	rq->alloc_time_ns = alloc_time_ns;                                                   // 设置rq->alloc_time_ns为alloc_time_ns
#endif
	if (blk_mq_need_time_stamp(rq))                                                      // 
		rq->start_time_ns = ktime_get_ns();                                              // 设置rq->start_time_ns为ktime_get_ns()
	else
		rq->start_time_ns = 0;                                                           // 设置rq->start_time_ns为0
	rq->io_start_time_ns = 0;                                                            // 设置rq->io_start_time_ns为0
	rq->stats_sectors = 0;                                                               // 设置rq->stats_sectors为0
	rq->nr_phys_segments = 0;                                                            // 设置rq->nr_phys_segments为0
#if defined(CONFIG_BLK_DEV_INTEGRITY)
	rq->nr_integrity_segments = 0;                                                       // 设置rq->nr_integrity_segments为0
#endif
	/* tag was already set */
	rq->extra_len = 0;                                                                   // 设置rq->extra_len为0
	WRITE_ONCE(rq->deadline, 0);                                                         // 设置rq->deadline为0

	rq->timeout = 0;                                                                     // 设置rq->timeout为0

	rq->end_io = NULL;                                                                   // 设置rq->end_io为NULL
	rq->end_io_data = NULL;                                                              // 设置rq->end_io_data为NULL

	data->ctx->rq_dispatched[op_is_sync(op)]++;                                          // data->ctx->rq_dispatched[op_is_sync(op)]自加
	refcount_set(&rq->ref, 1);                                                           // 设置rq->ref为1
	return rq;                                                                           // 返回
}







struct request_queue *scsi_mq_alloc_queue(struct scsi_device *sdev)
{
	sdev->request_queue = blk_mq_init_queue(&sdev->host->tag_set);                       // 
	if (IS_ERR(sdev->request_queue))                                                     // 若IS_ERR(sdev->request_queue)为true
		return NULL;                                                                     // 返回NULL

	sdev->request_queue->queuedata = sdev;                                               // 设置sdev->request_queue->queuedata为sdev
	__scsi_init_queue(sdev->host, sdev->request_queue);                                  // 
	blk_queue_flag_set(QUEUE_FLAG_SCSI_PASSTHROUGH, sdev->request_queue);                // 
	return sdev->request_queue;                                                          // 返回sdev->request_queue
}

struct request_queue *blk_mq_init_queue(struct blk_mq_tag_set *set)
{
	struct request_queue *uninit_q, *q;

	uninit_q = blk_alloc_queue_node(GFP_KERNEL, set->numa_node);                         // 分配uninit_q并初始化其部分成员
	if (!uninit_q)                                                                       // 若uninit_q为NULL
		return ERR_PTR(-ENOMEM);                                                         // 返回ERR_PTR(-ENOMEM)

	/*
	 * Initialize the queue without an elevator. device_add_disk() will do
	 * the initialization.
	 */
	q = blk_mq_init_allocated_queue(set, uninit_q, false);                               // 
	if (IS_ERR(q))                                                                       // 若IS_ERR(q)为true
		blk_cleanup_queue(uninit_q);                                                     // shutdown a request queue

	return q;                                                                            // 返回q
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
				gfp_mask | __GFP_ZERO, node_id);                                         // 申请q
	if (!q)                                                                              // 若q为NULL
		return NULL;                                                                     // 返回NULL

	q->last_merge = NULL;                                                                // 设置q->last_merge为NULL

	q->id = ida_simple_get(&blk_queue_ida, 0, 0, gfp_mask);                              // 分配q->id
	if (q->id < 0)                                                                       // 若q->id < 0为true
		goto fail_q;                                                                     // 跳转至fail_q

	ret = bioset_init(&q->bio_split, BIO_POOL_SIZE, 0, BIOSET_NEED_BVECS);               // 
	if (ret)                                                                             // 若ret不为0
		goto fail_id;                                                                    // 跳转至fail_id

	q->backing_dev_info = bdi_alloc_node(gfp_mask, node_id);                             // 分配并初始化q->backing_dev_info
	if (!q->backing_dev_info)                                                            // 若q->backing_dev_info为NULL
		goto fail_split;                                                                 // 跳转至fail_split

	q->stats = blk_alloc_queue_stats();                                                  // 分配并初始化q->stats
	if (!q->stats)                                                                       // 若q->stats为NULL
		goto fail_stats;                                                                 // 跳转至fail_stats

	q->backing_dev_info->ra_pages = VM_READAHEAD_PAGES;                                  // 重点：设置q->backing_dev_info->ra_pages为VM_READAHEAD_PAGES
	q->backing_dev_info->capabilities = BDI_CAP_CGROUP_WRITEBACK;                        // 重点：设置q->backing_dev_info->capabilities为BDI_CAP_CGROUP_WRITEBACK
	q->backing_dev_info->name = "block";                                                 // 设置q->backing_dev_info->name为"block"
	q->node = node_id;                                                                   // 设置q->node为node_id

	timer_setup(&q->backing_dev_info->laptop_mode_wb_timer,
		    laptop_mode_timer_fn, 0);                                                    // 初始化q->backing_dev_info->laptop_mode_wb_timer
	timer_setup(&q->timeout, blk_rq_timed_out_timer, 0);                                 // 初始化q->timeout
	INIT_WORK(&q->timeout_work, blk_timeout_work);                                       // 初始化q->timeout_work
	INIT_LIST_HEAD(&q->icq_list);                                                        // 初始化q->icq_list
#ifdef CONFIG_BLK_CGROUP
	INIT_LIST_HEAD(&q->blkg_list);                                                       // 初始化q->blkg_list
#endif

	kobject_init(&q->kobj, &blk_queue_ktype);                                            // 初始化q->kobj

#ifdef CONFIG_BLK_DEV_IO_TRACE
	mutex_init(&q->blk_trace_mutex);                                                     // 初始化q->blk_trace_mutex
#endif
	mutex_init(&q->sysfs_lock);                                                          // 初始化q->sysfs_lock
	mutex_init(&q->sysfs_dir_lock);                                                      // 初始化q->sysfs_dir_lock
	spin_lock_init(&q->queue_lock);                                                      // 初始化q->queue_lock

	init_waitqueue_head(&q->mq_freeze_wq);                                               // 初始化q->mq_freeze_wq
	mutex_init(&q->mq_freeze_lock);                                                      // 初始化q->mq_freeze_lock

	/*
	 * Init percpu_ref in atomic mode so that it's faster to shutdown.
	 * See blk_register_queue() for details.
	 */
	if (percpu_ref_init(&q->q_usage_counter,
				blk_queue_usage_counter_release,
				PERCPU_REF_INIT_ATOMIC, GFP_KERNEL))                                     // 
		goto fail_bdi;                                                                   // 跳转至fail_bdi

	if (blkcg_init_queue(q))                                                             // 
		goto fail_ref;                                                                   // 跳转至fail_ref

	return q;                                                                            // 返回q

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

struct backing_dev_info *bdi_alloc_node(gfp_t gfp_mask, int node_id)
{
	struct backing_dev_info *bdi;

	bdi = kmalloc_node(sizeof(struct backing_dev_info),
			   gfp_mask | __GFP_ZERO, node_id);                                          // 申请bdi
	if (!bdi)                                                                            // 若bdi为NULL
		return NULL;                                                                     // 返回NULL

	if (bdi_init(bdi)) {                                                                 // 初始化bdi
		kfree(bdi);                                                                      // 释放bdi
		return NULL;                                                                     // 返回NULL
	}
	return bdi;                                                                          // 返回NULL
}
EXPORT_SYMBOL(bdi_alloc_node);

static int bdi_init(struct backing_dev_info *bdi)
{
	int ret;

	bdi->dev = NULL;                                                                     // 设置bdi->dev为NULL

	kref_init(&bdi->refcnt);                                                             // 初始化bdi->refcnt
	bdi->min_ratio = 0;                                                                  // 设置bdi->min_ratio为0
	bdi->max_ratio = 100;                                                                // 设置bdi->max_ratio为100
	bdi->max_prop_frac = FPROP_FRAC_BASE;                                                // 设置bdi->max_prop_frac为FPROP_FRAC_BASE
	INIT_LIST_HEAD(&bdi->bdi_list);                                                      // 重点：初始化bdi->bdi_list
	INIT_LIST_HEAD(&bdi->wb_list);                                                       // 重点：初始化bdi->wb_list
	init_waitqueue_head(&bdi->wb_waitq);                                                 // 重点：初始化bdi->wb_waitq

	ret = cgwb_bdi_init(bdi);                                                            // cgroup writeback相关

	return ret;                                                                          // 返回ret
}

struct blk_queue_stats *blk_alloc_queue_stats(void)
{
	struct blk_queue_stats *stats;

	stats = kmalloc(sizeof(*stats), GFP_KERNEL);                                         // 分配stats
	if (!stats)                                                                          // 若stats为NULL
		return NULL;                                                                     // 返回NULL

	INIT_LIST_HEAD(&stats->callbacks);                                                   // 初始化stats->callbacks
	spin_lock_init(&stats->lock);                                                        // 初始化stats->lock
	stats->enable_accounting = false;                                                    // 设置stats->enable_accounting为false

	return stats;                                                                        // 返回stats
}


struct request_queue *blk_mq_init_allocated_queue(struct blk_mq_tag_set *set,
						  struct request_queue *q,
						  bool elevator_init)
{
	/* mark the queue as mq asap */
	q->mq_ops = set->ops;                                                                // 重点：设置q->mq_ops为set->ops

	q->poll_cb = blk_stat_alloc_callback(blk_mq_poll_stats_fn,
					     blk_mq_poll_stats_bkt,
					     BLK_MQ_POLL_STATS_BKTS, q);                                     // 分配q->poll_cb并初始化其部分成员
	if (!q->poll_cb)                                                                     // 若q->poll_cb为NULL
		goto err_exit;                                                                   // 跳转至err_exit

	if (blk_mq_alloc_ctxs(q))                                                            // 分配ctxs及ctxs->queue_ctx，并保存ctxs->queue_ctx到q->queue_ctx中
		goto err_poll;                                                                   // 跳转至err_poll

	/* init q->mq_kobj and sw queues' kobjects */
	blk_mq_sysfs_init(q);                                                                // 重点：init q->mq_kobj and sw queues' kobjects

	q->nr_queues = nr_hw_queues(set);                                                    // 重点：set->nr_maps == 1? nr_cpu_ids : max(set->nr_hw_queues, nr_cpu_ids)
	q->queue_hw_ctx = kcalloc_node(q->nr_queues, sizeof(*(q->queue_hw_ctx)),
						GFP_KERNEL, set->numa_node);                                     // 重点：分配q->nr_queues个hctx指针，初始化为NULL
	if (!q->queue_hw_ctx)                                                                // 若q->queue_hw_ctx为NULL
		goto err_sys_init;                                                               // 跳转至err_sys_init

	INIT_LIST_HEAD(&q->unused_hctx_list);                                                // 初始化q->unused_hctx_list
	spin_lock_init(&q->unused_hctx_lock);                                                // 初始化q->unused_hctx_lock

	blk_mq_realloc_hw_ctxs(set, q);                                                      // 重点：主要调用blk_mq_hw_queue_to_node分配并初始化q->queue_hw_ctx数组成员，重点是设置hctx->tags为set->tags[hctx_idx]
	if (!q->nr_hw_queues)                                                                // q->nr_hw_queues表示实际使用的hctx数量，在blk_mq_realloc_hw_ctxs执行之前q->nr_hw_queues为0，因为分配struct request queue实例时q->nr_hw_queues即为0
		goto err_hctxs;                                                                  // 跳转至err_hctxs

	INIT_WORK(&q->timeout_work, blk_mq_timeout_work);                                    // 初始化q->timeout_work
	blk_queue_rq_timeout(q, set->timeout ? set->timeout : 30 * HZ);                      // q->rq_timeout = set->timeout ? set->timeout : 30 * HZ

	q->tag_set = set;                                                                    // 重点：设置q->tag_set为set

	q->queue_flags |= QUEUE_FLAG_MQ_DEFAULT;                                             // 包含QUEUE_FLAG_MQ_DEFAULT到q->queue_flags
	if (set->nr_maps > HCTX_TYPE_POLL &&
	    set->map[HCTX_TYPE_POLL].nr_queues)                                              // 若set->nr_maps > HCTX_TYPE_POLL && set->map[HCTX_TYPE_POLL].nr_queues为true
		blk_queue_flag_set(QUEUE_FLAG_POLL, q);                                          // set_bit(QUEUE_FLAG_POLL, &q->queue_flags)

	q->sg_reserved_size = INT_MAX;                                                       // 设置q->sg_reserved_size为INT_MAX

	INIT_DELAYED_WORK(&q->requeue_work, blk_mq_requeue_work);                            // 初始化q->requeue_work
	INIT_LIST_HEAD(&q->requeue_list);                                                    // 初始化q->requeue_list
	spin_lock_init(&q->requeue_lock);                                                    // 初始化q->requeue_lock

	blk_queue_make_request(q, blk_mq_make_request);                                      // 重点：

	/*
	 * Do this after blk_queue_make_request() overrides it...
	 */
	q->nr_requests = set->queue_depth;                                                   // 重点：设置q->nr_requests为set->queue_depth

	/*
	 * Default to classic polling
	 */
	q->poll_nsec = BLK_MQ_POLL_CLASSIC;                                                  // 设置q->poll_nsec为BLK_MQ_POLL_CLASSIC

	blk_mq_init_cpu_queues(q, set->nr_hw_queues);                                        // 初始化ctx及hctx部分成员
	blk_mq_add_queue_tag_set(set, q);                                                    // 重点：主要是判断是否需要设置hctx->flags和set->flags的BLK_MQ_F_TAG_SHARED位，并添加q->tag_set_list到set->tag_list
	blk_mq_map_swqueue(q);                                                               // 

	if (elevator_init)                                                                   // 若elevator_init为true
		elevator_init_mq(q);                                                             // 

	return q;                                                                            // 返回q

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

/*
 * Maximum number of hardware queues we support. For single sets, we'll never
 * have more than the CPUs (software queues). For multiple sets, the tag_set
 * user may have set ->nr_hw_queues larger.
 */
static unsigned int nr_hw_queues(struct blk_mq_tag_set *set)
{
	if (set->nr_maps == 1)                                                               // 若set->nr_maps为1
		return nr_cpu_ids;                                                               // 返回nr_cpu_ids

	return max(set->nr_hw_queues, nr_cpu_ids);                                           // 返回set->nr_hw_queues于nr_cpu_ids最大者
}

struct blk_stat_callback *
blk_stat_alloc_callback(void (*timer_fn)(struct blk_stat_callback *),
			int (*bucket_fn)(const struct request *),
			unsigned int buckets, void *data)
{
	struct blk_stat_callback *cb;

	cb = kmalloc(sizeof(*cb), GFP_KERNEL);
	if (!cb)
		return NULL;

	cb->stat = kmalloc_array(buckets, sizeof(struct blk_rq_stat),
				 GFP_KERNEL);
	if (!cb->stat) {
		kfree(cb);
		return NULL;
	}
	cb->cpu_stat = __alloc_percpu(buckets * sizeof(struct blk_rq_stat),
				      __alignof__(struct blk_rq_stat));
	if (!cb->cpu_stat) {
		kfree(cb->stat);
		kfree(cb);
		return NULL;
	}

	cb->timer_fn = timer_fn;
	cb->bucket_fn = bucket_fn;
	cb->data = data;
	cb->buckets = buckets;
	timer_setup(&cb->timer, blk_stat_timer_fn, 0);

	return cb;
}

/* All allocations will be freed in release handler of q->mq_kobj */
static int blk_mq_alloc_ctxs(struct request_queue *q)
{
	struct blk_mq_ctxs *ctxs;
	int cpu;

	ctxs = kzalloc(sizeof(*ctxs), GFP_KERNEL);                                           // 分配ctxs
	if (!ctxs)                                                                           // 若ctxs为NULL
		return -ENOMEM;                                                                  // 返回-ENOMEM

	ctxs->queue_ctx = alloc_percpu(struct blk_mq_ctx);                                   // 分配per-cpu的ctxs->queue_ctx实例
	if (!ctxs->queue_ctx)                                                                // 若ctxs->queue_ctx为NULL
		goto fail;                                                                       // 跳转至fail

	for_each_possible_cpu(cpu) {                                                         // 遍历possible cpu
		struct blk_mq_ctx *ctx = per_cpu_ptr(ctxs->queue_ctx, cpu);                      // 取出ctx
		ctx->ctxs = ctxs;                                                                // 设置ctx->ctxs为ctxs
	}

	q->mq_kobj = &ctxs->kobj;                                                            // 设置q->mq_kobj为&ctxs->kobj
	q->queue_ctx = ctxs->queue_ctx;                                                      // 设置q->queue_ctx为ctxs->queue_ctx

	return 0;                                                                            // 返回0
 fail:
	kfree(ctxs);
	return -ENOMEM;
}

static void blk_mq_realloc_hw_ctxs(struct blk_mq_tag_set *set,
						struct request_queue *q)
{
	int i, j, end;
	struct blk_mq_hw_ctx **hctxs = q->queue_hw_ctx;                                      // 重点：取出struct blk_mq_hw_ctx *数组地址，这是在blk_mq_init_allocated_queue中分配的

	/* protect against switching io scheduler  */
	mutex_lock(&q->sysfs_lock);                                                          // 获取锁q->sysfs_lock，参考注释理解代码意图
	for (i = 0; i < set->nr_hw_queues; i++) {                                            // 重点：循环[0, set->nr_hw_queues)，注意set->nr_hw_queues为实际正使用的hctx数量
		int node;
		struct blk_mq_hw_ctx *hctx;

		node = blk_mq_hw_queue_to_node(&set->map[HCTX_TYPE_DEFAULT], i);                 // 确定hctx所在cpu的memory node
		/*
		 * If the hw queue has been mapped to another numa node,
		 * we need to realloc the hctx. If allocation fails, fallback
		 * to use the previous one.
		 */
		if (hctxs[i] && (hctxs[i]->numa_node == node))                                   // 参考注释理解代码意图，实际hctxs[i]为NULL
			continue;                                                                    // 继续下步循环

		hctx = blk_mq_alloc_and_init_hctx(set, q, i, node);                              // 到这里说明hctxs[i]为NULL或hctxs[i]->numa_node与node不相等，因此重新申请和初始化hctx
		if (hctx) {                                                                      // 若hctx不为NULL，表示重新申请和初始化hctx成功
			if (hctxs[i])                                                                // 若hctxs[i]不为NULL
				blk_mq_exit_hctx(q, set, hctxs[i], i);                                   // 销毁hctxs[i]
			hctxs[i] = hctx;                                                             // 重新设置hctxs[i]为hctx
		} else {
			if (hctxs[i])                                                                // 若hctxs[i]不为NULL
				pr_warn("Allocate new hctx on node %d fails,\
						fallback to previous one on node %d\n",
						node, hctxs[i]->numa_node);                                      // 打印提示信息
			else
				break;                                                                   // 退出循环
		}
	}
	/*
	 * Increasing nr_hw_queues fails. Free the newly allocated
	 * hctxs and keep the previous q->nr_hw_queues.
	 */
	if (i != set->nr_hw_queues) {                                                        // 若i不为set->nr_hw_queues（实际使用的hctx数量），表示分配hctx失败了，并且hctxs[i]为NULL
		j = q->nr_hw_queues;                                                             // 设置j为q->nr_hw_queues
		end = i;                                                                         // 设置end为i
	} else {
		j = i;                                                                           // 设置j为i，此时i为set->nr_hw_queues
		end = q->nr_hw_queues;                                                           // 设置end为q->nr_hw_queues
		q->nr_hw_queues = set->nr_hw_queues;                                             // 设置q->nr_hw_queues为set->nr_hw_queues，q->nr_hw_queues默认为0
	}

	for (; j < end; j++) {                                                               // 循环[0, end)
		struct blk_mq_hw_ctx *hctx = hctxs[j];                                           // 取出hctx

		if (hctx) {                                                                      // 若hctx不为NULL
			if (hctx->tags)                                                              // 若hctx->tags不为NULL
				blk_mq_free_map_and_requests(set, j);                                    // 
			blk_mq_exit_hctx(q, set, hctx, j);                                           // 销毁hctxs[j]
			hctxs[j] = NULL;                                                             // 设置hctxs[j]为NULL
		}
	}
	mutex_unlock(&q->sysfs_lock);                                                        // 释放锁q->sysfs_lock
}

static void blk_mq_init_cpu_queues(struct request_queue *q,
				   unsigned int nr_hw_queues)
{
	struct blk_mq_tag_set *set = q->tag_set;                                             // 获取set
	unsigned int i, j;

	for_each_possible_cpu(i) {                                                           // 遍历possible的cpu
		struct blk_mq_ctx *__ctx = per_cpu_ptr(q->queue_ctx, i);                         // 获取__ctx
		struct blk_mq_hw_ctx *hctx;
		int k;

		__ctx->cpu = i;                                                                  // 设置__ctx->cpu为i
		spin_lock_init(&__ctx->lock);                                                    // 初始化锁__ctx->lock
		for (k = HCTX_TYPE_DEFAULT; k < HCTX_MAX_TYPES; k++)                             // 循环[HCTX_TYPE_DEFAULT, HCTX_MAX_TYPES)
			INIT_LIST_HEAD(&__ctx->rq_lists[k]);                                         // 初始化__ctx->rq_lists[k]

		__ctx->queue = q;                                                                // 设置__ctx->queue为q

		/*
		 * Set local node, IFF we have more than one hw queue. If
		 * not, we remain on the home node of the device
		 */
		for (j = 0; j < set->nr_maps; j++) {                                             // 循环[0, set->nr_maps)
			hctx = blk_mq_map_queue_type(q, j, i);                                       // map (hctx_type,cpu) to hardware queue
			if (nr_hw_queues > 1 && hctx->numa_node == NUMA_NO_NODE)                     // nr_hw_queues > 1为true，并且hctx->numa_node为NUMA_NO_NODE
				hctx->numa_node = local_memory_node(cpu_to_node(i));                     // 设置hctx->numa_node为local_memory_node(cpu_to_node(i))
		}
	}
}

/* hctx->ctxs will be freed in queue's release handler */
static void blk_mq_exit_hctx(struct request_queue *q,
		struct blk_mq_tag_set *set,
		struct blk_mq_hw_ctx *hctx, unsigned int hctx_idx)
{
	if (blk_mq_hw_queue_mapped(hctx))
		blk_mq_tag_idle(hctx);

	if (set->ops->exit_request)
		set->ops->exit_request(set, hctx->fq->flush_rq, hctx_idx);

	if (set->ops->exit_hctx)
		set->ops->exit_hctx(hctx, hctx_idx);

	blk_mq_remove_cpuhp(hctx);

	spin_lock(&q->unused_hctx_lock);
	list_add(&hctx->hctx_list, &q->unused_hctx_list);
	spin_unlock(&q->unused_hctx_lock);
}


static void blk_mq_add_queue_tag_set(struct blk_mq_tag_set *set,
				     struct request_queue *q)
{
	mutex_lock(&set->tag_list_lock);                                                     // 获取锁set->tag_list_lock

	/*
	 * Check to see if we're transitioning to shared (from 1 to 2 queues).
	 */
	if (!list_empty(&set->tag_list) &&
	    !(set->flags & BLK_MQ_F_TAG_SHARED)) {                                           // 重点：若set->tag_list不为empty，说明不止一个queue使用set->tags，并且set->flags不包含BLK_MQ_F_TAG_SHARED
		set->flags |= BLK_MQ_F_TAG_SHARED;                                               // 重点：添加BLK_MQ_F_TAG_SHARED到set->flags
		/* update existing queue */
		blk_mq_update_tag_set_depth(set, true);                                          // 重点：遍历set->tag_list，取出queue，并设置queue中hctx的flags的BLK_MQ_F_TAG_SHARED位
	}
	if (set->flags & BLK_MQ_F_TAG_SHARED)                                                // 若set->flags包含BLK_MQ_F_TAG_SHARED
		queue_set_hctx_shared(q, true);                                                  // 重点：包含BLK_MQ_F_TAG_SHARED到hctx->flags
	list_add_tail_rcu(&q->tag_set_list, &set->tag_list);                                 // 重点：添加q->tag_set_list到set->tag_list，这是判断是否要包含BLK_MQ_F_TAG_SHARED到set->flags的依据

	mutex_unlock(&set->tag_list_lock);                                                   // 释放锁set->tag_list_lock
}

/*
 * Caller needs to ensure that we're either frozen/quiesced, or that
 * the queue isn't live yet.
 */
static void queue_set_hctx_shared(struct request_queue *q, bool shared)
{
	struct blk_mq_hw_ctx *hctx;
	int i;

	queue_for_each_hw_ctx(q, hctx, i) {                                                  // 遍历q->queue_hw_ctx[0, q->nr_hw_queues)
		if (shared)                                                                      // 若shared位true
			hctx->flags |= BLK_MQ_F_TAG_SHARED;                                          // 包含BLK_MQ_F_TAG_SHARED到hctx->flags
		else
			hctx->flags &= ~BLK_MQ_F_TAG_SHARED;                                         // 清除hctx->flags的BLK_MQ_F_TAG_SHARED位
	}
}

static void blk_mq_update_tag_set_depth(struct blk_mq_tag_set *set,
					bool shared)
{
	struct request_queue *q;

	lockdep_assert_held(&set->tag_list_lock);

	list_for_each_entry(q, &set->tag_list, tag_set_list) {                               // 遍历set->tag_list
		blk_mq_freeze_queue(q);                                                          // freeze queue
		queue_set_hctx_shared(q, shared);                                                // 设置或清除hctx->flags的BLK_MQ_F_TAG_SHARED位
		blk_mq_unfreeze_queue(q);                                                        // unfreeze queue
	}
}

static void blk_mq_map_swqueue(struct request_queue *q)
{
	unsigned int i, j, hctx_idx;
	struct blk_mq_hw_ctx *hctx;
	struct blk_mq_ctx *ctx;
	struct blk_mq_tag_set *set = q->tag_set;                                             // 取出set

	queue_for_each_hw_ctx(q, hctx, i) {                                                  // 遍历q->queue_hw_ctx[0, q->nr_hw_queues)
		cpumask_clear(hctx->cpumask);                                                    // 清除hctx->cpumask
		hctx->nr_ctx = 0;                                                                // 设置hctx->nr_ctx位0
		hctx->dispatch_from = NULL;                                                      // 设置hctx->dispatch_from为NULL
	}

	/*
	 * Map software to hardware queues.
	 *
	 * If the cpu isn't present, the cpu is mapped to first hctx.
	 */
	for_each_possible_cpu(i) {                                                           // 遍历possible的cpu
		hctx_idx = set->map[HCTX_TYPE_DEFAULT].mq_map[i];                                // 获取i对应的hctx_idx
		/* unmapped hw queue can be remapped after CPU topo changed */
		if (!set->tags[hctx_idx] &&
		    !__blk_mq_alloc_rq_map(set, hctx_idx)) {                                     // 
			/*
			 * If tags initialization fail for some hctx,
			 * that hctx won't be brought online.  In this
			 * case, remap the current ctx to hctx[0] which
			 * is guaranteed to always have tags allocated
			 */
			set->map[HCTX_TYPE_DEFAULT].mq_map[i] = 0;                                   // 
		}

		ctx = per_cpu_ptr(q->queue_ctx, i);                                              // 获取ctx
		for (j = 0; j < set->nr_maps; j++) {                                             // 循环[0, set->nr_maps)
			if (!set->map[j].nr_queues) {                                                // 若set->map[j].nr_queues为0
				ctx->hctxs[j] = blk_mq_map_queue_type(q,
						HCTX_TYPE_DEFAULT, i);                                           // 
				continue;                                                                // 继续下步循环
			}

			hctx = blk_mq_map_queue_type(q, j, i);                                       // 
			ctx->hctxs[j] = hctx;                                                        // 设置ctx->hctxs[j]为hctx
			/*
			 * If the CPU is already set in the mask, then we've
			 * mapped this one already. This can happen if
			 * devices share queues across queue maps.
			 */
			if (cpumask_test_cpu(i, hctx->cpumask))                                      // 
				continue;                                                                // 继续下步循环

			cpumask_set_cpu(i, hctx->cpumask);                                           // 
			hctx->type = j;                                                              // 设置hctx->type为j
			ctx->index_hw[hctx->type] = hctx->nr_ctx;                                    // 
			hctx->ctxs[hctx->nr_ctx++] = ctx;                                            // 

			/*
			 * If the nr_ctx type overflows, we have exceeded the
			 * amount of sw queues we can support.
			 */
			BUG_ON(!hctx->nr_ctx);                                                       // 
		}

		for (; j < HCTX_MAX_TYPES; j++)                                                  // 
			ctx->hctxs[j] = blk_mq_map_queue_type(q,
					HCTX_TYPE_DEFAULT, i);                                               // 
	}

	queue_for_each_hw_ctx(q, hctx, i) {                                                  // 遍历q->queue_hw_ctx[0, q->nr_hw_queues)
		/*
		 * If no software queues are mapped to this hardware queue,
		 * disable it and free the request entries.
		 */
		if (!hctx->nr_ctx) {                                                             // 
			/* Never unmap queue 0.  We need it as a
			 * fallback in case of a new remap fails
			 * allocation
			 */
			if (i && set->tags[i])                                                       // 
				blk_mq_free_map_and_requests(set, i);                                    // 

			hctx->tags = NULL;                                                           // 
			continue;                                                                    // 
		}

		hctx->tags = set->tags[i];                                                       // 
		WARN_ON(!hctx->tags);                                                            // 

		/*
		 * Set the map size to the number of mapped software queues.
		 * This is more accurate and more efficient than looping
		 * over all possibly mapped software queues.
		 */
		sbitmap_resize(&hctx->ctx_map, hctx->nr_ctx);                                    // 

		/*
		 * Initialize batch roundrobin counts
		 */
		hctx->next_cpu = blk_mq_first_mapped_cpu(hctx);                                  // 
		hctx->next_cpu_batch = BLK_MQ_CPU_WORK_BATCH;                                    // 
	}
}

static void blk_mq_free_map_and_requests(struct blk_mq_tag_set *set,
					 unsigned int hctx_idx)
{
	if (set->tags && set->tags[hctx_idx]) {
		blk_mq_free_rqs(set, set->tags[hctx_idx], hctx_idx);
		blk_mq_free_rq_map(set->tags[hctx_idx]);
		set->tags[hctx_idx] = NULL;
	}
}

static struct blk_mq_hw_ctx *blk_mq_alloc_and_init_hctx(
		struct blk_mq_tag_set *set, struct request_queue *q,
		int hctx_idx, int node)
{
	struct blk_mq_hw_ctx *hctx = NULL, *tmp;

	/* reuse dead hctx first */
	spin_lock(&q->unused_hctx_lock);                                                     // 获取锁q->unused_hctx_lock
	list_for_each_entry(tmp, &q->unused_hctx_list, hctx_list) {                          // 遍历q->unused_hctx_list
		if (tmp->numa_node == node) {                                                    // 若tmp->numa_node为node
			hctx = tmp;                                                                  // 设置hctx为tmp
			break;                                                                       // 退出遍历
		}
	}
	if (hctx)                                                                            // 若hctx不为NULL
		list_del_init(&hctx->hctx_list);                                                 // 将hctx->hctx_list从所在链表移除并初始化
	spin_unlock(&q->unused_hctx_lock);                                                   // 释放锁q->unused_hctx_lock

	if (!hctx)                                                                           // 若hctx为NULL
		hctx = blk_mq_alloc_hctx(q, set, node);                                          // 分配hctx，并初始化其部分成员
	if (!hctx)                                                                           // 若hctx为NULL
		goto fail;                                                                       // 跳转至fail

	if (blk_mq_init_hctx(q, set, hctx, hctx_idx))                                        // 初始化hctx
		goto free_hctx;                                                                  // 跳转至free_hctx

	return hctx;                                                                         // 返回hctx

 free_hctx:
	kobject_put(&hctx->kobj);
 fail:
	return NULL;
}

static struct blk_mq_hw_ctx *
blk_mq_alloc_hctx(struct request_queue *q, struct blk_mq_tag_set *set,
		int node)
{
	struct blk_mq_hw_ctx *hctx;
	gfp_t gfp = GFP_NOIO | __GFP_NOWARN | __GFP_NORETRY;

	hctx = kzalloc_node(blk_mq_hw_ctx_size(set), gfp, node);                             // 分配hctx
	if (!hctx)                                                                           // 若hctx为NULL
		goto fail_alloc_hctx;                                                            // 跳转至fail_alloc_hctx

	if (!zalloc_cpumask_var_node(&hctx->cpumask, gfp, node))                             // 分配cpumask保存到hctx->cpumask中
		goto free_hctx;                                                                  // 跳转至free_hctx

	atomic_set(&hctx->nr_active, 0);                                                     // 设置hctx->nr_active为0
	if (node == NUMA_NO_NODE)                                                            // 若node为NUMA_NO_NODE
		node = set->numa_node;                                                           // 设置node为set->numa_node
	hctx->numa_node = node;                                                              // 设置hctx->numa_node为node

	INIT_DELAYED_WORK(&hctx->run_work, blk_mq_run_work_fn);                              // 初始化hctx->run_work
	spin_lock_init(&hctx->lock);                                                         // 初始化hctx->lock
	INIT_LIST_HEAD(&hctx->dispatch);                                                     // 初始化hctx->dispatch
	hctx->queue = q;                                                                     // 设置hctx->queue为q
	hctx->flags = set->flags & ~BLK_MQ_F_TAG_SHARED;                                     // 重点：从hctx->flags = set->flags中清除BLK_MQ_F_TAG_SHARED

	INIT_LIST_HEAD(&hctx->hctx_list);                                                    // 初始化hctx->hctx_list

	/*
	 * Allocate space for all possible cpus to avoid allocation at
	 * runtime
	 */
	hctx->ctxs = kmalloc_array_node(nr_cpu_ids, sizeof(void *),
			gfp, node);                                                                  // 分配nr_cpu_ids个指针
	if (!hctx->ctxs)                                                                     // 若hctx->ctxs为NULL
		goto free_cpumask;                                                               // 跳转至free_cpumask

	if (sbitmap_init_node(&hctx->ctx_map, nr_cpu_ids, ilog2(8),
				gfp, node))                                                              // 初始化hctx->ctx_map
		goto free_ctxs;                                                                  // 跳转至free_ctxs
	hctx->nr_ctx = 0;                                                                    // 设置hctx->nr_ctx为0

	spin_lock_init(&hctx->dispatch_wait_lock);                                           // 初始化hctx->dispatch_wait_lock
	init_waitqueue_func_entry(&hctx->dispatch_wait, blk_mq_dispatch_wake);               // 初始化hctx->dispatch_wait
	INIT_LIST_HEAD(&hctx->dispatch_wait.entry);                                          // 初始化hctx->dispatch_wait.entry

	hctx->fq = blk_alloc_flush_queue(q, hctx->numa_node, set->cmd_size,
			gfp);                                                                        // 分配并初始化hctx->fq
	if (!hctx->fq)                                                                       // 若hctx->fq为NULL
		goto free_bitmap;                                                                // 跳转至free_bitmap

	if (hctx->flags & BLK_MQ_F_BLOCKING)                                                 // 若hctx->flags包含BLK_MQ_F_BLOCKING
		init_srcu_struct(hctx->srcu);                                                    // 初始化hctx->srcu
	blk_mq_hctx_kobj_init(hctx);                                                         // kobject_init(&hctx->kobj, &blk_mq_hw_ktype)

	return hctx;                                                                         // 返回hctx

 free_bitmap:
	sbitmap_free(&hctx->ctx_map);
 free_ctxs:
	kfree(hctx->ctxs);
 free_cpumask:
	free_cpumask_var(hctx->cpumask);
 free_hctx:
	kfree(hctx);
 fail_alloc_hctx:
	return NULL;
}

struct blk_flush_queue *blk_alloc_flush_queue(struct request_queue *q,
		int node, int cmd_size, gfp_t flags)
{
	struct blk_flush_queue *fq;
	int rq_sz = sizeof(struct request);

	fq = kzalloc_node(sizeof(*fq), flags, node);
	if (!fq)
		goto fail;

	spin_lock_init(&fq->mq_flush_lock);

	rq_sz = round_up(rq_sz + cmd_size, cache_line_size());
	fq->flush_rq = kzalloc_node(rq_sz, flags, node);
	if (!fq->flush_rq)
		goto fail_rq;

	INIT_LIST_HEAD(&fq->flush_queue[0]);
	INIT_LIST_HEAD(&fq->flush_queue[1]);
	INIT_LIST_HEAD(&fq->flush_data_in_flight);

	return fq;

 fail_rq:
	kfree(fq);
 fail:
	return NULL;
}

static int blk_mq_hw_ctx_size(struct blk_mq_tag_set *tag_set)
{
	int hw_ctx_size = sizeof(struct blk_mq_hw_ctx);

	BUILD_BUG_ON(ALIGN(offsetof(struct blk_mq_hw_ctx, srcu),
			   __alignof__(struct blk_mq_hw_ctx)) !=
		     sizeof(struct blk_mq_hw_ctx));

	if (tag_set->flags & BLK_MQ_F_BLOCKING)
		hw_ctx_size += sizeof(struct srcu_struct);

	return hw_ctx_size;
}

static int blk_mq_init_hctx(struct request_queue *q,
		struct blk_mq_tag_set *set,
		struct blk_mq_hw_ctx *hctx, unsigned hctx_idx)
{
	hctx->queue_num = hctx_idx;                                                          // 设置hctx->queue_num为hctx_idx

	cpuhp_state_add_instance_nocalls(CPUHP_BLK_MQ_DEAD, &hctx->cpuhp_dead);              // Add an instance for a state without invoking the startup callback

	hctx->tags = set->tags[hctx_idx];                                                    // 重点：设置hctx->tags为set->tags[hctx_idx]，可以看出host上不同的queue的hctx->tags可能是相同的

	if (set->ops->init_hctx &&
	    set->ops->init_hctx(hctx, set->driver_data, hctx_idx))                           // 若set->ops->init_hctx为true，则调用set->ops->init_hctx
		goto unregister_cpu_notifier;                                                    // 跳转至unregister_cpu_notifier

	if (blk_mq_init_request(set, hctx->fq->flush_rq, hctx_idx,
				hctx->numa_node))                                                        // 初始化hctx->fq->flush_rq
		goto exit_hctx;                                                                  // 跳转至exit_hctx
	return 0;                                                                            // 返回0

 exit_hctx:
	if (set->ops->exit_hctx)
		set->ops->exit_hctx(hctx, hctx_idx);
 unregister_cpu_notifier:
	blk_mq_remove_cpuhp(hctx);
	return -1;
}

static int blk_mq_init_request(struct blk_mq_tag_set *set, struct request *rq,
			       unsigned int hctx_idx, int node)
{
	int ret;

	if (set->ops->init_request) {                                                        // 若set->ops->init_request为true
		ret = set->ops->init_request(set, rq, hctx_idx, node);                           // 调用set->ops->init_request
		if (ret)                                                                         // 若ret不为0
			return ret;                                                                  // 返回ret
	}

	WRITE_ONCE(rq->state, MQ_RQ_IDLE);                                                   // 设置rq->state为MQ_RQ_IDLE
	return 0;                                                                            // 返回0
}

static int scsi_mq_init_request(struct blk_mq_tag_set *set, struct request *rq,
				unsigned int hctx_idx, unsigned int numa_node)
{
	struct Scsi_Host *shost = set->driver_data;
	const bool unchecked_isa_dma = shost->unchecked_isa_dma;
	struct scsi_cmnd *cmd = blk_mq_rq_to_pdu(rq);
	struct scatterlist *sg;

	if (unchecked_isa_dma)
		cmd->flags |= SCMD_UNCHECKED_ISA_DMA;
	cmd->sense_buffer = scsi_alloc_sense_buffer(unchecked_isa_dma,
						    GFP_KERNEL, numa_node);
	if (!cmd->sense_buffer)
		return -ENOMEM;
	cmd->req.sense = cmd->sense_buffer;

	if (scsi_host_get_prot(shost)) {
		sg = (void *)cmd + sizeof(struct scsi_cmnd) +
			shost->hostt->cmd_size;
		cmd->prot_sdb = (void *)sg + scsi_mq_inline_sgl_size(shost);
	}

	return 0;
}

static int blk_mq_sched_alloc_tags(struct request_queue *q,
				   struct blk_mq_hw_ctx *hctx,
				   unsigned int hctx_idx)
{
	struct blk_mq_tag_set *set = q->tag_set;
	int ret;

	hctx->sched_tags = blk_mq_alloc_rq_map(set, hctx_idx, q->nr_requests,
					       set->reserved_tags);                                          // 重点：分配hctx->sched_tags
	if (!hctx->sched_tags)                                                               // 
		return -ENOMEM;                                                                  // 

	ret = blk_mq_alloc_rqs(set, hctx->sched_tags, hctx_idx, q->nr_requests);             // 
	if (ret)                                                                             // 
		blk_mq_sched_free_tags(set, hctx, hctx_idx);                                     // 

	return ret;                                                                          // 返回ret
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

	tags->rqs = kcalloc_node(nr_tags, sizeof(struct request *),
				 GFP_NOIO | __GFP_NOWARN | __GFP_NORETRY,
				 node);
	if (!tags->rqs) {
		blk_mq_free_tags(tags);
		return NULL;
	}

	tags->static_rqs = kcalloc_node(nr_tags, sizeof(struct request *),
					GFP_NOIO | __GFP_NOWARN | __GFP_NORETRY,
					node);
	if (!tags->static_rqs) {
		kfree(tags->rqs);
		blk_mq_free_tags(tags);
		return NULL;
	}

	return tags;
}

/*
 * For a device queue that has no required features, use the default elevator
 * settings. Otherwise, use the first elevator available matching the required
 * features. If no suitable elevator is find or if the chosen elevator
 * initialization fails, fall back to the "none" elevator (no elevator).
 */
void elevator_init_mq(struct request_queue *q)
{
	struct elevator_type *e;
	int err;

	if (!elv_support_iosched(q))                                                         // 若q->mq_ops为NULL，或q->tag_set->flags & BLK_MQ_F_NO_SCHED为true
		return;                                                                          // 返回

	WARN_ON_ONCE(test_bit(QUEUE_FLAG_REGISTERED, &q->queue_flags));                      // 自检警告

	if (unlikely(q->elevator))                                                           // 若q->elevator不为NULL
		return;                                                                          // 返回

	if (!q->required_elevator_features)                                                  // 若q->required_elevator_features为0
		e = elevator_get_default(q);                                                     // for single queue devices, default to using mq-deadline
	else
		e = elevator_get_by_features(q);                                                 // 
	if (!e)                                                                              // 若e为NULL
		return;                                                                          // 返回

	blk_mq_freeze_queue(q);                                                              // freeze queue
	blk_mq_quiesce_queue(q);                                                             // quiesce queue

	err = blk_mq_init_sched(q, e);                                                       // 

	blk_mq_unquiesce_queue(q);                                                           // unquiesce queue
	blk_mq_unfreeze_queue(q);                                                            // unfreeze queue

	if (err) {                                                                           // 若err不为0
		pr_warn("\"%s\" elevator initialization failed, "
			"falling back to \"none\"\n", e->elevator_name);                             // 打印提示信息
		elevator_put(e);                                                                 // 
	}
}

/*
 * For single queue devices, default to using mq-deadline. If we have multiple
 * queues or mq-deadline is not available, default to "none".
 */
static struct elevator_type *elevator_get_default(struct request_queue *q)
{
	if (q->nr_hw_queues != 1)                                                            // 若q->nr_hw_queues不为1
		return NULL;                                                                     // 返回NULL

	return elevator_get(q, "mq-deadline", false);                                        // 
}

static struct elevator_type *elevator_get(struct request_queue *q,
					  const char *name, bool try_loading)
{
	struct elevator_type *e;

	spin_lock(&elv_list_lock);                                                           // 获取锁elv_list_lock

	e = elevator_find(name, q->required_elevator_features);                              // find an elevator
	if (!e && try_loading) {                                                             // 若e为NULL，并且try_loading为true
		spin_unlock(&elv_list_lock);                                                     // 释放锁elv_list_lock
		request_module("%s-iosched", name);                                              // try to load a kernel module
		spin_lock(&elv_list_lock);                                                       // 获取锁elv_list_lock
		e = elevator_find(name, q->required_elevator_features);                          // find an elevator
	}

	if (e && !try_module_get(e->elevator_owner))                                         // 
		e = NULL;                                                                        // 设置e为NULL

	spin_unlock(&elv_list_lock);                                                         // 释放锁elv_list_lock
	return e;                                                                            // 返回e
}

/**
 * elevator_find - Find an elevator
 * @name: Name of the elevator to find
 * @required_features: Features that the elevator must provide
 *
 * Return the first registered scheduler with name @name and supporting the
 * features @required_features and NULL otherwise.
 */
static struct elevator_type *elevator_find(const char *name,
					   unsigned int required_features)
{
	struct elevator_type *e;

	list_for_each_entry(e, &elv_list, list) {                                            // 遍历elv_list
		if (elevator_match(e, name, required_features))                                  // test an elevator name and features
			return e;                                                                    // 返回e
	}

	return NULL;                                                                         // 返回NULL
}

/**
 * elevator_match - Test an elevator name and features
 * @e: Scheduler to test
 * @name: Elevator name to test
 * @required_features: Features that the elevator must provide
 *
 * Return true is the elevator @e name matches @name and if @e provides all the
 * the feratures spcified by @required_features.
 */
static bool elevator_match(const struct elevator_type *e, const char *name,
			   unsigned int required_features)
{
	if (!elv_support_features(e->elevator_features, required_features))                  // required_features & elv_features不为required_features
		return false;                                                                    // 返回false
	if (!strcmp(e->elevator_name, name))                                                 // 若e->elevator_name与name相同
		return true;                                                                     // 返回true
	if (e->elevator_alias && !strcmp(e->elevator_alias, name))                           // 若e->elevator_alias存在，并且e->elevator_alias与name相同
		return true;                                                                     // 返回true

	return false;                                                                        // 返回false
}

/*
 * Get the first elevator providing the features required by the request queue.
 * Default to "none" if no matching elevator is found.
 */
static struct elevator_type *elevator_get_by_features(struct request_queue *q)
{
	struct elevator_type *e, *found = NULL;

	spin_lock(&elv_list_lock);

	list_for_each_entry(e, &elv_list, list) {
		if (elv_support_features(e->elevator_features,
					 q->required_elevator_features)) {
			found = e;
			break;
		}
	}

	if (found && !try_module_get(found->elevator_owner))
		found = NULL;

	spin_unlock(&elv_list_lock);
	return found;
}

int blk_mq_init_sched(struct request_queue *q, struct elevator_type *e)
{
	struct blk_mq_hw_ctx *hctx;
	struct elevator_queue *eq;
	unsigned int i;
	int ret;

	if (!e) {
		q->elevator = NULL;
		q->nr_requests = q->tag_set->queue_depth;
		return 0;
	}

	/*
	 * Default to double of smaller one between hw queue_depth and 128,
	 * since we don't split into sync/async like the old code did.
	 * Additionally, this is a per-hw queue depth.
	 */
	q->nr_requests = 2 * min_t(unsigned int, q->tag_set->queue_depth,
				   BLKDEV_MAX_RQ);

	queue_for_each_hw_ctx(q, hctx, i) {
		ret = blk_mq_sched_alloc_tags(q, hctx, i);
		if (ret)
			goto err;
	}

	ret = e->ops.init_sched(q, e);
	if (ret)
		goto err;

	blk_mq_debugfs_register_sched(q);

	queue_for_each_hw_ctx(q, hctx, i) {
		if (e->ops.init_hctx) {
			ret = e->ops.init_hctx(hctx, i);
			if (ret) {
				eq = q->elevator;
				blk_mq_sched_free_requests(q);
				blk_mq_exit_sched(q, eq);
				kobject_put(&eq->kobj);
				return ret;
			}
		}
		blk_mq_debugfs_register_sched_hctx(q, hctx);
	}

	return 0;

err:
	blk_mq_sched_free_requests(q);
	blk_mq_sched_tags_teardown(q);
	q->elevator = NULL;
	return ret;
}






