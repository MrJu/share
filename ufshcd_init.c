
/**
 * ufshcd_init - Driver initialization routine
 * @hba: per-adapter instance
 * @mmio_base: base register address
 * @irq: Interrupt line of device
 * Returns 0 on success, non-zero value on failure
 */
int ufshcd_init(struct ufs_hba *hba, void __iomem *mmio_base, unsigned int irq)
{
	int err;
	struct Scsi_Host *host = hba->host;                                                  // 获取host
	struct device *dev = hba->dev;

	if (!mmio_base) {                                                                    // 若mmio_base为NULL
		dev_err(hba->dev,
		"Invalid memory reference for mmio_base is NULL\n");                             // 打印提示信息
		err = -ENODEV;                                                                   // 设置err为-ENODEV
		goto out_error;                                                                  // 跳转至out_error
	}

	hba->mmio_base = mmio_base;                                                          // 设置hba->mmio_base为mmio_base
	hba->irq = irq;                                                                      // 设置hba->irq为irq

	/* Set descriptor lengths to specification defaults */
	ufshcd_def_desc_sizes(hba);                                                          // 初始化hba->desc_size下成员的默认值

	err = ufshcd_hba_init(hba);                                                          // 主要初始化和设置regulators和clocks
	if (err)                                                                             // 若err不为0
		goto out_error;                                                                  // 跳转至out_error

	/* Read capabilities registers */
	ufshcd_hba_capabilities(hba);                                                        // 设置hba->capabilities、hba->nutrs和hba->nutmrs

	/* Get UFS version supported by the controller */
	hba->ufs_version = ufshcd_get_ufs_version(hba);                                      // get the UFS version supported by the HBA

	if ((hba->ufs_version != UFSHCI_VERSION_10) &&
	    (hba->ufs_version != UFSHCI_VERSION_11) &&
	    (hba->ufs_version != UFSHCI_VERSION_20) &&
	    (hba->ufs_version != UFSHCI_VERSION_21))                                         // 重点：注意这里没有UFSHCI_VERSION_3x，因此实际可能会打印err信息，但并不影响执行
		dev_err(hba->dev, "invalid UFS version 0x%x\n",
			hba->ufs_version);                                                           // 打印提示信息

	/* Get Interrupt bit mask per version */
	hba->intr_mask = ufshcd_get_intr_mask(hba);                                          // 根据hba->ufs_version确定hba->intr_mask

	err = ufshcd_set_dma_mask(hba);                                                      // set dma mask based on the controller addressing capability
	if (err) {                                                                           // 若err不为0
		dev_err(hba->dev, "set dma mask failed\n");                                      // 打印提示信息
		goto out_disable;                                                                // 跳转至out_disable
	}

	/* Allocate memory for host memory space */
	err = ufshcd_memory_alloc(hba);                                                      // 重点：该接口将分配非常核心的数据结构的内存空间，后续单独分析
	if (err) {                                                                           // 若err不为0
		dev_err(hba->dev, "Memory allocation failed\n");                                 // 打印提示信息
		goto out_disable;
	}

	/* Configure LRB */
	ufshcd_host_memory_configure(hba);                                                   // 重点：该接口将配置ufshcd_memory_alloc分配的数据结构

	host->can_queue = hba->nutrs;                                                        // 设置host->can_queue为hba->nutrs
	host->cmd_per_lun = hba->nutrs;                                                      // 设置host->cmd_per_lun为hba->nutrs
	host->max_id = UFSHCD_MAX_ID;                                                        // 重点：scsi框架引入的概念，在scan时会用到
	host->max_lun = UFS_MAX_LUNS;                                                        // 重点：scsi框架引入的概念，在scan时会用到
	host->max_channel = UFSHCD_MAX_CHANNEL;                                              // 重点：scsi框架引入的概念，在scan时会用到
	host->unique_id = host->host_no;                                                     // 这是每个host的唯一的id
	host->max_cmd_len = UFS_CDB_SIZE;                                                    // 重点：设置host->max_cmd_len为UFS_CDB_SIZE

	hba->max_pwr_info.is_valid = false;                                                  // 设置hba->max_pwr_info.is_valid为false

	/* Initailize wait queue for task management */
	init_waitqueue_head(&hba->tm_wq);                                                    // 初始化hba->tm_wq
	init_waitqueue_head(&hba->tm_tag_wq);                                                // 初始化hba->tm_tag_wq

	/* Initialize work queues */
	INIT_WORK(&hba->eh_work, ufshcd_err_handler);                                        // 初始化hba->eh_work
	INIT_WORK(&hba->eeh_work, ufshcd_exception_event_handler);                           // 初始化hba->eeh_work

	/* Initialize UIC command mutex */
	mutex_init(&hba->uic_cmd_mutex);                                                     // 初始化hba->uic_cmd_mutex

	/* Initialize mutex for device management commands */
	mutex_init(&hba->dev_cmd.lock);                                                      // 初始化hba->dev_cmd.lock

	init_rwsem(&hba->clk_scaling_lock);                                                  // 初始化hba->clk_scaling_lock

	/* Initialize device management tag acquire wait queue */
	init_waitqueue_head(&hba->dev_cmd.tag_wq);                                           // 初始化hba->dev_cmd.tag_wq

	ufshcd_init_clk_gating(hba);                                                         // 

	ufshcd_init_clk_scaling(hba);                                                        // 

	/*
	 * In order to avoid any spurious interrupt immediately after
	 * registering UFS controller interrupt handler, clear any pending UFS
	 * interrupt status and disable all the UFS interrupts.
	 */
	ufshcd_writel(hba, ufshcd_readl(hba, REG_INTERRUPT_STATUS),
		      REG_INTERRUPT_STATUS);                                                     // 参考注释理解代码意图，清除中断状态
	ufshcd_writel(hba, 0, REG_INTERRUPT_ENABLE);                                         // 使能中断
	/*
	 * Make sure that UFS interrupts are disabled and any pending interrupt
	 * status is cleared before registering UFS interrupt handler.
	 */
	mb();                                                                                // 参考注释理解代码意图

	/* IRQ registration */
	err = devm_request_irq(dev, irq, ufshcd_intr, IRQF_SHARED, UFSHCD, hba);             // 注册中断，即由ufshcd触发的重点
	if (err) {                                                                           // 若err不为0
		dev_err(hba->dev, "request irq failed\n");                                       // 打印提示信息
		goto exit_gating;                                                                // 跳转至exit_gating
	} else {
		hba->is_irq_enabled = true;                                                      // 设置hba->is_irq_enabled为true
	}

	err = scsi_add_host(host, hba->dev);                                                 // 
	if (err) {                                                                           // 若err不为0
		dev_err(hba->dev, "scsi_add_host failed\n");                                     // 打印提示信息
		goto exit_gating;                                                                // 跳转至exit_gating
	}

	/* Reset the attached device */
	ufshcd_vops_device_reset(hba);                                                       // 重点：

	/* Host controller enable */
	err = ufshcd_hba_enable(hba);                                                        // 
	if (err) {                                                                           // 若err不为0
		dev_err(hba->dev, "Host controller enable failed\n");                            // 打印提示信息
		ufshcd_print_host_regs(hba);                                                     // 打印提示信息
		ufshcd_print_host_state(hba);                                                    // 打印提示信息
		goto out_remove_scsi_host;                                                       // 跳转至out_remove_scsi_host
	}

	/*
	 * Set the default power management level for runtime and system PM.
	 * Default power saving mode is to keep UFS link in Hibern8 state
	 * and UFS device in sleep state.
	 */
	hba->rpm_lvl = ufs_get_desired_pm_lvl_for_dev_link_state(
						UFS_SLEEP_PWR_MODE,
						UIC_LINK_HIBERN8_STATE);                                         // 
	hba->spm_lvl = ufs_get_desired_pm_lvl_for_dev_link_state(
						UFS_SLEEP_PWR_MODE,
						UIC_LINK_HIBERN8_STATE);                                         // 

	/* Set the default auto-hiberate idle timer value to 150 ms */
	if (ufshcd_is_auto_hibern8_supported(hba) && !hba->ahit) {                           // 
		hba->ahit = FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 150) |
			    FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 3);                               // 
	}

	/* Hold auto suspend until async scan completes */
	pm_runtime_get_sync(dev);                                                            // 
	atomic_set(&hba->scsi_block_reqs_cnt, 0);                                            // 
	/*
	 * We are assuming that device wasn't put in sleep/power-down
	 * state exclusively during the boot stage before kernel.
	 * This assumption helps avoid doing link startup twice during
	 * ufshcd_probe_hba().
	 */
	ufshcd_set_ufs_dev_active(hba);                                                      // 

	async_schedule(ufshcd_async_scan, hba);                                              // 重点：在ufshcd_async_scan中将进行scan
	ufs_sysfs_add_nodes(hba->dev);                                                       // 

	return 0;                                                                            // 返回0

out_remove_scsi_host:
	scsi_remove_host(hba->host);
exit_gating:
	ufshcd_exit_clk_scaling(hba);
	ufshcd_exit_clk_gating(hba);
out_disable:
	hba->is_irq_enabled = false;
	ufshcd_hba_exit(hba);
out_error:
	return err;
}
EXPORT_SYMBOL_GPL(ufshcd_init);

static void ufshcd_def_desc_sizes(struct ufs_hba *hba)
{
	hba->desc_size.dev_desc = QUERY_DESC_DEVICE_DEF_SIZE;
	hba->desc_size.pwr_desc = QUERY_DESC_POWER_DEF_SIZE;
	hba->desc_size.interc_desc = QUERY_DESC_INTERCONNECT_DEF_SIZE;
	hba->desc_size.conf_desc = QUERY_DESC_CONFIGURATION_DEF_SIZE;
	hba->desc_size.unit_desc = QUERY_DESC_UNIT_DEF_SIZE;
	hba->desc_size.geom_desc = QUERY_DESC_GEOMETRY_DEF_SIZE;
	hba->desc_size.hlth_desc = QUERY_DESC_HEALTH_DEF_SIZE;
}

static int ufshcd_hba_init(struct ufs_hba *hba)
{
	int err;

	/*
	 * Handle host controller power separately from the UFS device power
	 * rails as it will help controlling the UFS host controller power
	 * collapse easily which is different than UFS device power collapse.
	 * Also, enable the host controller power before we go ahead with rest
	 * of the initialization here.
	 */
	err = ufshcd_init_hba_vreg(hba);
	if (err)
		goto out;

	err = ufshcd_setup_hba_vreg(hba, true);
	if (err)
		goto out;

	err = ufshcd_init_clocks(hba);
	if (err)
		goto out_disable_hba_vreg;

	err = ufshcd_setup_clocks(hba, true);
	if (err)
		goto out_disable_hba_vreg;

	err = ufshcd_init_vreg(hba);
	if (err)
		goto out_disable_clks;

	err = ufshcd_setup_vreg(hba, true);
	if (err)
		goto out_disable_clks;

	err = ufshcd_variant_hba_init(hba);
	if (err)
		goto out_disable_vreg;

	hba->is_powered = true;
	goto out;

out_disable_vreg:
	ufshcd_setup_vreg(hba, false);
out_disable_clks:
	ufshcd_setup_clocks(hba, false);
out_disable_hba_vreg:
	ufshcd_setup_hba_vreg(hba, false);
out:
	return err;
}

static inline int __must_check scsi_add_host(struct Scsi_Host *host,
					     struct device *dev)
{
	return scsi_add_host_with_dma(host, dev, dev);
}

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
	struct scsi_host_template *sht = shost->hostt;                                       // 获取sht
	int error = -EINVAL;                                                                 // 设置error为-EINVAL

	shost_printk(KERN_INFO, shost, "%s\n",
			sht->info ? sht->info(shost) : sht->name);                                   // 打印提示信息

	if (!shost->can_queue) {                                                             // 若shost->can_queue为0
		shost_printk(KERN_ERR, shost,
			     "can_queue = 0 no longer supported\n");                                 // 打印提示信息
		goto fail;                                                                       // 跳转至fail
	}

	error = scsi_init_sense_cache(shost);                                                // 分配sense cache
	if (error)                                                                           // 若error不为0
		goto fail;                                                                       // 跳转至fail

	error = scsi_mq_setup_tags(shost);                                                   // 重点：
	if (error)                                                                           // 若error不为0
		goto fail;                                                                       // 跳转至fail

	if (!shost->shost_gendev.parent)                                                     // 
		shost->shost_gendev.parent = dev ? dev : &platform_bus;                          // 重点：
	if (!dma_dev)                                                                        // 
		dma_dev = shost->shost_gendev.parent;                                            // 

	shost->dma_dev = dma_dev;                                                            // 

	/*
	 * Increase usage count temporarily here so that calling
	 * scsi_autopm_put_host() will trigger runtime idle if there is
	 * nothing else preventing suspending the device.
	 */
	pm_runtime_get_noresume(&shost->shost_gendev);                                       // 
	pm_runtime_set_active(&shost->shost_gendev);                                         // 
	pm_runtime_enable(&shost->shost_gendev);                                             // 
	device_enable_async_suspend(&shost->shost_gendev);                                   // 

	error = device_add(&shost->shost_gendev);                                            // 
	if (error)                                                                           // 
		goto out_disable_runtime_pm;                                                     // 

	scsi_host_set_state(shost, SHOST_RUNNING);                                           // 
	get_device(shost->shost_gendev.parent);                                              // 

	device_enable_async_suspend(&shost->shost_dev);                                      // 

	error = device_add(&shost->shost_dev);                                               // 
	if (error)                                                                           // 
		goto out_del_gendev;                                                             // 

	get_device(&shost->shost_gendev);                                                    // 

	if (shost->transportt->host_size) {                                                  // 
		shost->shost_data = kzalloc(shost->transportt->host_size,
					 GFP_KERNEL);                                                        // 
		if (shost->shost_data == NULL) {                                                 // 
			error = -ENOMEM;                                                             // 
			goto out_del_dev;                                                            // 
		}
	}

	if (shost->transportt->create_work_queue) {                                          // 
		snprintf(shost->work_q_name, sizeof(shost->work_q_name),
			 "scsi_wq_%d", shost->host_no);                                              // 
		shost->work_q = create_singlethread_workqueue(
					shost->work_q_name);                                                 // 
		if (!shost->work_q) {                                                            // 
			error = -EINVAL;                                                             // 
			goto out_free_shost_data;                                                    // 
		}
	}

	error = scsi_sysfs_add_host(shost);                                                  // 
	if (error)                                                                           // 
		goto out_destroy_host;

	scsi_proc_host_add(shost);                                                           // 
	scsi_autopm_put_host(shost);
	return error;                                                                        // 返回error

 out_destroy_host:
	if (shost->work_q)
		destroy_workqueue(shost->work_q);
 out_free_shost_data:
	kfree(shost->shost_data);
 out_del_dev:
	device_del(&shost->shost_dev);
 out_del_gendev:
	device_del(&shost->shost_gendev);
 out_disable_runtime_pm:
	device_disable_async_suspend(&shost->shost_gendev);
	pm_runtime_disable(&shost->shost_gendev);
	pm_runtime_set_suspended(&shost->shost_gendev);
	pm_runtime_put_noidle(&shost->shost_gendev);
	scsi_mq_destroy_tags(shost);
 fail:
	return error;
}
EXPORT_SYMBOL(scsi_add_host_with_dma);

static struct kmem_cache *scsi_sense_cache;
static struct kmem_cache *scsi_sense_isadma_cache;
static DEFINE_MUTEX(scsi_sense_cache_mutex);

int scsi_init_sense_cache(struct Scsi_Host *shost)
{
	struct kmem_cache *cache;
	int ret = 0;

	mutex_lock(&scsi_sense_cache_mutex);                                                 // 获取scsi_sense_cache_mutex
	cache = scsi_select_sense_cache(shost->unchecked_isa_dma);                           // cache = unchecked_isa_dma ? scsi_sense_isadma_cache : scsi_sense_cache
	if (cache)                                                                           // 若cache为NULL
		goto exit;                                                                       // 跳转至exit

	if (shost->unchecked_isa_dma) {                                                      // 若shost->unchecked_isa_dma不为0
		scsi_sense_isadma_cache =
			kmem_cache_create("scsi_sense_cache(DMA)",
				SCSI_SENSE_BUFFERSIZE, 0,
				SLAB_HWCACHE_ALIGN | SLAB_CACHE_DMA, NULL);                              // 分配scsi_sense_isadma_cache
		if (!scsi_sense_isadma_cache)                                                    // 若scsi_sense_isadma_cache为NULL
			ret = -ENOMEM;                                                               // 设置ret为-ENOMEM
	} else {
		scsi_sense_cache =
			kmem_cache_create_usercopy("scsi_sense_cache",
				SCSI_SENSE_BUFFERSIZE, 0, SLAB_HWCACHE_ALIGN,
				0, SCSI_SENSE_BUFFERSIZE, NULL);                                         // 分配scsi_sense_cache
		if (!scsi_sense_cache)                                                           // 若scsi_sense_cache为NULL
			ret = -ENOMEM;                                                               // 设置ret为-ENOMEM
	}
 exit:
	mutex_unlock(&scsi_sense_cache_mutex);                                               // 释放锁scsi_sense_cache_mutex
	return ret;                                                                          // 返回ret
}

static inline struct kmem_cache *
scsi_select_sense_cache(bool unchecked_isa_dma)
{
	return unchecked_isa_dma ? scsi_sense_isadma_cache : scsi_sense_cache;
}

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

/**
 * scsi_sysfs_add_host - add scsi host to subsystem
 * @shost:     scsi host struct to add to subsystem
 **/
int scsi_sysfs_add_host(struct Scsi_Host *shost)
{
	int error, i;

	/* add host specific attributes */
	if (shost->hostt->shost_attrs) {
		for (i = 0; shost->hostt->shost_attrs[i]; i++) {
			error = device_create_file(&shost->shost_dev,
					shost->hostt->shost_attrs[i]);
			if (error)
				return error;
		}
	}

	transport_register_device(&shost->shost_gendev);
	transport_configure_device(&shost->shost_gendev);
	return 0;
}

/**
 * ufshcd_get_intr_mask - Get the interrupt bit mask
 * @hba: Pointer to adapter instance
 *
 * Returns interrupt bit mask per version
 */
static inline u32 ufshcd_get_intr_mask(struct ufs_hba *hba)
{
	u32 intr_mask = 0;                                                                   // 设置intr_mask为0

	switch (hba->ufs_version) {                                                          // 根据hba->ufs_version设置intr_mask
	case UFSHCI_VERSION_10:                                                              // 若hba->ufs_version为UFSHCI_VERSION_10
		intr_mask = INTERRUPT_MASK_ALL_VER_10;                                           // 设置intr_mask为INTERRUPT_MASK_ALL_VER_10
		break;
	case UFSHCI_VERSION_11:                                                              // 若hba->ufs_version为UFSHCI_VERSION_11
	case UFSHCI_VERSION_20:                                                              // 若hba->ufs_version为UFSHCI_VERSION_20
		intr_mask = INTERRUPT_MASK_ALL_VER_11;                                           // 设置intr_mask为INTERRUPT_MASK_ALL_VER_11
		break;
	case UFSHCI_VERSION_21:                                                              // 若hba->ufs_version为UFSHCI_VERSION_21
	default:
		intr_mask = INTERRUPT_MASK_ALL_VER_21;                                           // 设置intr_mask为INTERRUPT_MASK_ALL_VER_21
		break;
	}

	return intr_mask;                                                                    // 返回intr_mask
}

/**
 * ufshcd_set_dma_mask - Set dma mask based on the controller
 *			 addressing capability
 * @hba: per adapter instance
 *
 * Returns 0 for success, non-zero for failure
 */
static int ufshcd_set_dma_mask(struct ufs_hba *hba)
{
	if (hba->capabilities & MASK_64_ADDRESSING_SUPPORT) {                                // 若hba->capabilities包含MASK_64_ADDRESSING_SUPPORT
		if (!dma_set_mask_and_coherent(hba->dev, DMA_BIT_MASK(64)))                      // set both the DMA mask and the coherent DMA mask to the same thing
			return 0;
	}
	return dma_set_mask_and_coherent(hba->dev, DMA_BIT_MASK(32));                        // set both the DMA mask and the coherent DMA mask to the same thing
}

static void ufshcd_init_clk_gating(struct ufs_hba *hba)
{
	char wq_name[sizeof("ufs_clk_gating_00")];

	if (!ufshcd_is_clkgating_allowed(hba))
		return;

	hba->clk_gating.delay_ms = 150;
	INIT_DELAYED_WORK(&hba->clk_gating.gate_work, ufshcd_gate_work);
	INIT_WORK(&hba->clk_gating.ungate_work, ufshcd_ungate_work);

	snprintf(wq_name, ARRAY_SIZE(wq_name), "ufs_clk_gating_%d",
		 hba->host->host_no);
	hba->clk_gating.clk_gating_workq = alloc_ordered_workqueue(wq_name,
							   WQ_MEM_RECLAIM);

	hba->clk_gating.is_enabled = true;

	hba->clk_gating.delay_attr.show = ufshcd_clkgate_delay_show;
	hba->clk_gating.delay_attr.store = ufshcd_clkgate_delay_store;
	sysfs_attr_init(&hba->clk_gating.delay_attr.attr);
	hba->clk_gating.delay_attr.attr.name = "clkgate_delay_ms";
	hba->clk_gating.delay_attr.attr.mode = 0644;
	if (device_create_file(hba->dev, &hba->clk_gating.delay_attr))
		dev_err(hba->dev, "Failed to create sysfs for clkgate_delay\n");

	hba->clk_gating.enable_attr.show = ufshcd_clkgate_enable_show;
	hba->clk_gating.enable_attr.store = ufshcd_clkgate_enable_store;
	sysfs_attr_init(&hba->clk_gating.enable_attr.attr);
	hba->clk_gating.enable_attr.attr.name = "clkgate_enable";
	hba->clk_gating.enable_attr.attr.mode = 0644;
	if (device_create_file(hba->dev, &hba->clk_gating.enable_attr))
		dev_err(hba->dev, "Failed to create sysfs for clkgate_enable\n");
}

static void ufshcd_init_clk_scaling(struct ufs_hba *hba)
{
	char wq_name[sizeof("ufs_clkscaling_00")];

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	INIT_WORK(&hba->clk_scaling.suspend_work,
		  ufshcd_clk_scaling_suspend_work);
	INIT_WORK(&hba->clk_scaling.resume_work,
		  ufshcd_clk_scaling_resume_work);

	snprintf(wq_name, sizeof(wq_name), "ufs_clkscaling_%d",
		 hba->host->host_no);
	hba->clk_scaling.workq = create_singlethread_workqueue(wq_name);

	ufshcd_clkscaling_init_sysfs(hba);
}






static int ufshcd_hba_init(struct ufs_hba *hba)
{
	int err;

	/*
	 * Handle host controller power separately from the UFS device power
	 * rails as it will help controlling the UFS host controller power
	 * collapse easily which is different than UFS device power collapse.
	 * Also, enable the host controller power before we go ahead with rest
	 * of the initialization here.
	 */
	err = ufshcd_init_hba_vreg(hba);
	if (err)
		goto out;

	err = ufshcd_setup_hba_vreg(hba, true);
	if (err)
		goto out;

	err = ufshcd_init_clocks(hba);
	if (err)
		goto out_disable_hba_vreg;

	err = ufshcd_setup_clocks(hba, true);
	if (err)
		goto out_disable_hba_vreg;

	err = ufshcd_init_vreg(hba);
	if (err)
		goto out_disable_clks;

	err = ufshcd_setup_vreg(hba, true);
	if (err)
		goto out_disable_clks;

	err = ufshcd_variant_hba_init(hba);
	if (err)
		goto out_disable_vreg;

	hba->is_powered = true;
	goto out;

out_disable_vreg:
	ufshcd_setup_vreg(hba, false);
out_disable_clks:
	ufshcd_setup_clocks(hba, false);
out_disable_hba_vreg:
	ufshcd_setup_hba_vreg(hba, false);
out:
	return err;
}

/**
 * ufshcd_hba_capabilities - Read controller capabilities
 * @hba: per adapter instance
 */
static inline void ufshcd_hba_capabilities(struct ufs_hba *hba)
{
	hba->capabilities = ufshcd_readl(hba, REG_CONTROLLER_CAPABILITIES);

	/* nutrs and nutmrs are 0 based values */
	hba->nutrs = (hba->capabilities & MASK_TRANSFER_REQUESTS_SLOTS) + 1;
	hba->nutmrs =
	((hba->capabilities & MASK_TASK_MANAGEMENT_REQUEST_SLOTS) >> 16) + 1;
}

/**
 * ufshcd_get_ufs_version - Get the UFS version supported by the HBA
 * @hba: Pointer to adapter instance
 *
 * Returns UFSHCI version supported by the controller
 */
static inline u32 ufshcd_get_ufs_version(struct ufs_hba *hba)
{
	if (hba->quirks & UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION)
		return ufshcd_vops_get_ufs_hci_version(hba);

	return ufshcd_readl(hba, REG_UFS_VERSION);
}



/**
 * ufshcd_probe_hba - probe hba to detect device and initialize
 * @hba: per-adapter instance
 *
 * Execute link-startup and verify device initialization
 */
static int ufshcd_probe_hba(struct ufs_hba *hba)
{
	struct ufs_dev_desc card = {0};
	int ret;
	ktime_t start = ktime_get();

	ret = ufshcd_link_startup(hba);                                                      // 重点
	if (ret)
		goto out;

	/* set the default level for urgent bkops */
	hba->urgent_bkops_lvl = BKOPS_STATUS_PERF_IMPACT;
	hba->is_urgent_bkops_lvl_checked = false;

	/* Debug counters initialization */
	ufshcd_clear_dbg_ufs_stats(hba);

	/* UniPro link is active now */
	ufshcd_set_link_active(hba);

	/* Enable Auto-Hibernate if configured */
	ufshcd_auto_hibern8_enable(hba);

	ret = ufshcd_verify_dev_init(hba);
	if (ret)
		goto out;

	ret = ufshcd_complete_dev_init(hba);
	if (ret)
		goto out;

	/* Init check for device descriptor sizes */
	ufshcd_init_desc_sizes(hba);

	ret = ufs_get_device_desc(hba, &card);
	if (ret) {
		dev_err(hba->dev, "%s: Failed getting device info. err = %d\n",
			__func__, ret);
		goto out;
	}

	ufs_fixup_device_setup(hba, &card);
	ufs_put_device_desc(&card);

	ufshcd_tune_unipro_params(hba);

	/* UFS device is also active now */
	ufshcd_set_ufs_dev_active(hba);
	ufshcd_force_reset_auto_bkops(hba);
	hba->wlun_dev_clr_ua = true;

	if (ufshcd_get_max_pwr_mode(hba)) {
		dev_err(hba->dev,
			"%s: Failed getting max supported power mode\n",
			__func__);
	} else {
		/*
		 * Set the right value to bRefClkFreq before attempting to
		 * switch to HS gears.
		 */
		if (hba->dev_ref_clk_freq != REF_CLK_FREQ_INVAL)
			ufshcd_set_dev_ref_clk(hba);
		ret = ufshcd_config_pwr_mode(hba, &hba->max_pwr_info.info);                      // 
		if (ret) {
			dev_err(hba->dev, "%s: Failed setting power mode, err = %d\n",
					__func__, ret);
			goto out;
		}
	}

	/* set the state as operational after switching to desired gear */
	hba->ufshcd_state = UFSHCD_STATE_OPERATIONAL;

	/*
	 * If we are in error handling context or in power management callbacks
	 * context, no need to scan the host
	 */
	if (!ufshcd_eh_in_progress(hba) && !hba->pm_op_in_progress) {
		bool flag;

		/* clear any previous UFS device information */
		memset(&hba->dev_info, 0, sizeof(hba->dev_info));
		if (!ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_READ_FLAG,
				QUERY_FLAG_IDN_PWR_ON_WPE, &flag))
			hba->dev_info.f_power_on_wp_en = flag;

		if (!hba->is_init_prefetch)
			ufshcd_init_icc_levels(hba);

		/* Add required well known logical units to scsi mid layer */
		if (ufshcd_scsi_add_wlus(hba))
			goto out;

		/* Initialize devfreq after UFS device is detected */
		if (ufshcd_is_clkscaling_supported(hba)) {
			memcpy(&hba->clk_scaling.saved_pwr_info.info,
				&hba->pwr_info,
				sizeof(struct ufs_pa_layer_attr));
			hba->clk_scaling.saved_pwr_info.is_valid = true;
			if (!hba->devfreq) {
				ret = ufshcd_devfreq_init(hba);
				if (ret)
					goto out;
			}
			hba->clk_scaling.is_allowed = true;
		}

		ufs_bsg_probe(hba);

		scsi_scan_host(hba->host);                                                       // 重点
		pm_runtime_put_sync(hba->dev);
	}

	if (!hba->is_init_prefetch)
		hba->is_init_prefetch = true;

out:
	/*
	 * If we failed to initialize the device or the device is not
	 * present, turn off the power/clocks etc.
	 */
	if (ret && !ufshcd_eh_in_progress(hba) && !hba->pm_op_in_progress) {
		pm_runtime_put_sync(hba->dev);
		ufshcd_exit_clk_scaling(hba);
		ufshcd_hba_exit(hba);
	}

	trace_ufshcd_init(dev_name(hba->dev), ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	return ret;
}












