ufshcd_init
	|-- ufshcd_hba_capabilities
	|-- ufshcd_init_clk_gating
	|-- ufshcd_init_clk_scaling
	|-- ufshcd_vops_device_reset
	|-- ufshcd_probe_hba
			|-- ufshcd_link_startup
					|-- ufshcd_dme_link_startup
							|-- ufshcd_send_uic_cmd
					|-- ufshcd_update_reg_hist
					|-- ufshcd_init_pwr_info
					|-- ufshcd_make_hba_operational
							|-- ufshcd_enable_intr
							|-- ufshcd_writel
							|-- ufshcd_readl
			|-- ufshcd_set_link_active
			|-- ufshcd_auto_hibern8_enable
					|-- ufshcd_writel
			|-- ufshcd_verify_dev_init
					|-- ufshcd_exec_dev_cmd
			|-- ufshcd_complete_dev_init
					|-- ufshcd_query_flag_retry
			|-- ufs_fixup_device_setup
			|-- ufshcd_tune_unipro_params
					|-- ufshcd_tune_pa_tactivate
					|-- ufshcd_tune_pa_hibern8time
					|-- ufshcd_dme_set
					|-- ufshcd_quirk_tune_host_pa_tactivate
					|-- ufshcd_vops_apply_dev_quirks
			|-- ufshcd_force_reset_auto_bkops
					|-- ufshcd_enable_auto_bkops
			|-- ufshcd_get_max_pwr_mode
					|-- ufshcd_dme_get
					|-- ufshcd_dme_peer_get
			|-- ufshcd_set_dev_ref_clk
					|-- ufshcd_query_attr_retry
			|-- ufshcd_config_pwr_mode
					|-- ufshcd_change_power_mode

/**
 * struct ufs_hba_mtk_vops - UFS MTK specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static struct ufs_hba_variant_ops ufs_hba_mtk_vops = {
	.name                = "mediatek.ufshci",
	.init                = ufs_mtk_init,
	.setup_clocks        = ufs_mtk_setup_clocks,
	.link_startup_notify = ufs_mtk_link_startup_notify,
	.pwr_change_notify   = ufs_mtk_pwr_change_notify,
	.suspend             = ufs_mtk_suspend,
	.resume              = ufs_mtk_resume,
};

/**
 * ufshcd_link_startup - Initialize unipro link startup
 * @hba: per adapter instance
 *
 * Returns 0 for success, non-zero in case of failure
 */
static int ufshcd_link_startup(struct ufs_hba *hba)
{
	int ret;
	int retries = DME_LINKSTARTUP_RETRIES;                                               // 设置retries为DME_LINKSTARTUP_RETRIES
	bool link_startup_again = false;                                                     // 设置link_startup_again为false

	/*
	 * If UFS device isn't active then we will have to issue link startup
	 * 2 times to make sure the device state move to active.
	 */
	if (!ufshcd_is_ufs_dev_active(hba))                                                  // hba->curr_dev_pwr_mode == UFS_ACTIVE_PWR_MODE，参考注释理解接口意图
		link_startup_again = true;                                                       // 设置link_startup_again为true

link_startup:
	do {
		ufshcd_vops_link_startup_notify(hba, PRE_CHANGE);                                // 调用hba->vops->link_startup_notify方法

		ret = ufshcd_dme_link_startup(hba);                                              // 执行UIC_CMD_DME_LINK_STARTUP命令，成功返回0，该接口调用ufshcd_send_uic_cmd，可能会休眠

		/* check if device is detected by inter-connect layer */
		if (!ret && !ufshcd_is_device_present(hba)) {                                    // 若ret为0，并且!(ufshcd_readl(hba, REG_CONTROLLER_STATUS) & DEVICE_PRESENT)
			ufshcd_update_reg_hist(&hba->ufs_stats.link_startup_err,
					       0);                                                           // 记录ret信息到hba->ufs_stats.link_startup_err中
			dev_err(hba->dev, "%s: Device not present\n", __func__);                     // 打印提示信息
			ret = -ENXIO;                                                                // 设置ret为-ENXIO
			goto out;                                                                    // 跳转至out
		}

		/*
		 * DME link lost indication is only received when link is up,
		 * but we can't be sure if the link is up until link startup
		 * succeeds. So reset the local Uni-Pro and try again.
		 */
		if (ret && ufshcd_hba_enable(hba)) {                                             // 若ret不为0，并且
			ufshcd_update_reg_hist(&hba->ufs_stats.link_startup_err,
					       (u32)ret);                                                    // 记录ret信息到hba->ufs_stats.link_startup_err中
			goto out;                                                                    // 跳转至out
		}
	} while (ret && retries--);                                                          // 若ret不为0，并且retries--不为0

	if (ret) {                                                                           // 若ret不为0
		/* failed to get the link up... retire */
		ufshcd_update_reg_hist(&hba->ufs_stats.link_startup_err,
				       (u32)ret);                                                        // 记录ret信息到hba->ufs_stats.link_startup_err中
		goto out;                                                                        // 跳转至out
	}

	if (link_startup_again) {                                                            // 若link_startup_again为true
		link_startup_again = false;                                                      // 设置link_startup_again为false
		retries = DME_LINKSTARTUP_RETRIES;                                               // 设置retries为DME_LINKSTARTUP_RETRIES
		goto link_startup;                                                               // 跳转至link_startup
	}

	/* Mark that link is up in PWM-G1, 1-lane, SLOW-AUTO mode */
	ufshcd_init_pwr_info(hba);                                                           // 参考注释理解代码意图
	ufshcd_print_pwr_info(hba);                                                          // 打印提示信息

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_LCC) {                                         // 若hba->quirks包含UFSHCD_QUIRK_BROKEN_LCC
		ret = ufshcd_disable_device_tx_lcc(hba);                                         // 
		if (ret)                                                                         // 若ret不为0
			goto out;                                                                    // 跳转至out
	}

	/* Include any host controller configuration via UIC commands */
	ret = ufshcd_vops_link_startup_notify(hba, POST_CHANGE);                             // 调用hba->vops->link_startup_notify方法
	if (ret)                                                                             // 若ret不为0
		goto out;                                                                        // 跳转至out

	ret = ufshcd_make_hba_operational(hba);                                              // 
out:
	if (ret) {                                                                           // 若ret不为0
		dev_err(hba->dev, "link startup failed %d\n", ret);                              // 打印提示信息
		ufshcd_print_host_state(hba);                                                    // 打印提示信息
		ufshcd_print_pwr_info(hba);                                                      // 打印提示信息
		ufshcd_print_host_regs(hba);                                                     // 打印提示信息
	}
	return ret;                                                                          // 返回ret
}

static int ufshcd_hba_enable(struct ufs_hba *hba)
{
	int ret;

	if (hba->quirks & UFSHCI_QUIRK_BROKEN_HCE) {                                         // 若hba->quirks包含UFSHCI_QUIRK_BROKEN_HCE
		ufshcd_set_link_off(hba);                                                        // 
		ufshcd_vops_hce_enable_notify(hba, PRE_CHANGE);                                  // 

		/* enable UIC related interrupts */
		ufshcd_enable_intr(hba, UFSHCD_UIC_MASK);                                        // 
		ret = ufshcd_dme_reset(hba);                                                     // 
		if (!ret) {                                                                      // 
			ret = ufshcd_dme_enable(hba);                                                // 
			if (!ret)                                                                    // 
				ufshcd_vops_hce_enable_notify(hba, POST_CHANGE);                         // 
			if (ret)                                                                     // 
				dev_err(hba->dev,
					"Host controller enable failed with non-hce\n");                     // 打印提示信息
		}
	} else {
		ret = ufshcd_hba_execute_hce(hba);                                               // 
	}

	return ret;                                                                          // 返回ret
}

static inline int ufshcd_vops_link_startup_notify(struct ufs_hba *hba,
						bool status)
{
	if (hba->vops && hba->vops->link_startup_notify)                                     // 
		return hba->vops->link_startup_notify(hba, status);                              // 

	return 0;                                                                            // 
}

/**
 * ufshcd_dme_link_startup - Notify Unipro to perform link startup
 * @hba: per adapter instance
 *
 * UIC_CMD_DME_LINK_STARTUP command must be issued to Unipro layer,
 * in order to initialize the Unipro link startup procedure.
 * Once the Unipro links are up, the device connected to the controller
 * is detected.
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_dme_link_startup(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {0};
	int ret;

	uic_cmd.command = UIC_CMD_DME_LINK_STARTUP;                                          // 构造uic命令               

	ret = ufshcd_send_uic_cmd(hba, &uic_cmd);                                            // 发送uic命令
	if (ret)                                                                             // 若ret不为0
		dev_dbg(hba->dev,
			"dme-link-startup: error code %d\n", ret);                                   // 打印提示信息
	return ret;                                                                          // 返回ret
}

static void ufshcd_update_reg_hist(struct ufs_err_reg_hist *reg_hist,
				   u32 reg)
{
	reg_hist->reg[reg_hist->pos] = reg;
	reg_hist->tstamp[reg_hist->pos] = ktime_get();
	reg_hist->pos = (reg_hist->pos + 1) % UFS_ERR_REG_HIST_LENGTH;
}

 /**
 * ufshcd_init_pwr_info - setting the POR (power on reset)
 * values in hba power info
 * @hba: per-adapter instance
 */
static void ufshcd_init_pwr_info(struct ufs_hba *hba)
{
	hba->pwr_info.gear_rx = UFS_PWM_G1;
	hba->pwr_info.gear_tx = UFS_PWM_G1;
	hba->pwr_info.lane_rx = 1;
	hba->pwr_info.lane_tx = 1;
	hba->pwr_info.pwr_rx = SLOWAUTO_MODE;
	hba->pwr_info.pwr_tx = SLOWAUTO_MODE;
	hba->pwr_info.hs_rate = 0;
}


#define ufshcd_set_link_active(hba) ((hba)->uic_link_state = \
				    UIC_LINK_ACTIVE_STATE)
					
static void ufshcd_auto_hibern8_enable(struct ufs_hba *hba)
{
	unsigned long flags;

	if (!ufshcd_is_auto_hibern8_supported(hba) || !hba->ahit)
		return;

	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_writel(hba, hba->ahit, REG_AUTO_HIBERNATE_IDLE_TIMER);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

/**
 * ufshcd_verify_dev_init() - Verify device initialization
 * @hba: per-adapter instance
 *
 * Send NOP OUT UPIU and wait for NOP IN response to check whether the
 * device Transport Protocol (UTP) layer is ready after a reset.
 * If the UTP layer at the device side is not initialized, it may
 * not respond with NOP IN UPIU within timeout of %NOP_OUT_TIMEOUT
 * and we retry sending NOP OUT for %NOP_OUT_RETRIES iterations.
 */
static int ufshcd_verify_dev_init(struct ufs_hba *hba)
{
	int err = 0;
	int retries;

	ufshcd_hold(hba, false);
	mutex_lock(&hba->dev_cmd.lock);
	for (retries = NOP_OUT_RETRIES; retries > 0; retries--) {
		err = ufshcd_exec_dev_cmd(hba, DEV_CMD_TYPE_NOP,
					       NOP_OUT_TIMEOUT);

		if (!err || err == -ETIMEDOUT)
			break;

		dev_dbg(hba->dev, "%s: error %d retrying\n", __func__, err);
	}
	mutex_unlock(&hba->dev_cmd.lock);
	ufshcd_release(hba);

	if (err)
		dev_err(hba->dev, "%s: NOP OUT failed %d\n", __func__, err);
	return err;
}

/**
 * ufshcd_complete_dev_init() - checks device readiness
 * @hba: per-adapter instance
 *
 * Set fDeviceInit flag and poll until device toggles it.
 */
static int ufshcd_complete_dev_init(struct ufs_hba *hba)
{
	int i;
	int err;
	bool flag_res = 1;

	err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG,
		QUERY_FLAG_IDN_FDEVICEINIT, NULL);
	if (err) {
		dev_err(hba->dev,
			"%s setting fDeviceInit flag failed with error %d\n",
			__func__, err);
		goto out;
	}

	/* poll for max. 1000 iterations for fDeviceInit flag to clear */
	for (i = 0; i < 1000 && !err && flag_res; i++)
		err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_READ_FLAG,
			QUERY_FLAG_IDN_FDEVICEINIT, &flag_res);

	if (err)
		dev_err(hba->dev,
			"%s reading fDeviceInit flag failed with error %d\n",
			__func__, err);
	else if (flag_res)
		dev_err(hba->dev,
			"%s fDeviceInit was not cleared by the device\n",
			__func__);

out:
	return err;
}

static void ufshcd_init_desc_sizes(struct ufs_hba *hba)
{
	int err;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_DEVICE, 0,
		&hba->desc_size.dev_desc);
	if (err)
		hba->desc_size.dev_desc = QUERY_DESC_DEVICE_DEF_SIZE;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_POWER, 0,
		&hba->desc_size.pwr_desc);
	if (err)
		hba->desc_size.pwr_desc = QUERY_DESC_POWER_DEF_SIZE;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_INTERCONNECT, 0,
		&hba->desc_size.interc_desc);
	if (err)
		hba->desc_size.interc_desc = QUERY_DESC_INTERCONNECT_DEF_SIZE;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_CONFIGURATION, 0,
		&hba->desc_size.conf_desc);
	if (err)
		hba->desc_size.conf_desc = QUERY_DESC_CONFIGURATION_DEF_SIZE;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_UNIT, 0,
		&hba->desc_size.unit_desc);
	if (err)
		hba->desc_size.unit_desc = QUERY_DESC_UNIT_DEF_SIZE;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_GEOMETRY, 0,
		&hba->desc_size.geom_desc);
	if (err)
		hba->desc_size.geom_desc = QUERY_DESC_GEOMETRY_DEF_SIZE;
	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_HEALTH, 0,
		&hba->desc_size.hlth_desc);
	if (err)
		hba->desc_size.hlth_desc = QUERY_DESC_HEALTH_DEF_SIZE;
}


static int ufs_get_device_desc(struct ufs_hba *hba,
			       struct ufs_dev_desc *dev_desc)
{
	int err;
	size_t buff_len;
	u8 model_index;
	u8 *desc_buf;

	if (!dev_desc)
		return -EINVAL;

	buff_len = max_t(size_t, hba->desc_size.dev_desc,
			 QUERY_DESC_MAX_SIZE + 1);
	desc_buf = kmalloc(buff_len, GFP_KERNEL);
	if (!desc_buf) {
		err = -ENOMEM;
		goto out;
	}

	err = ufshcd_read_device_desc(hba, desc_buf, hba->desc_size.dev_desc);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading Device Desc. err = %d\n",
			__func__, err);
		goto out;
	}

	/*
	 * getting vendor (manufacturerID) and Bank Index in big endian
	 * format
	 */
	dev_desc->wmanufacturerid = desc_buf[DEVICE_DESC_PARAM_MANF_ID] << 8 |
				     desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];

	model_index = desc_buf[DEVICE_DESC_PARAM_PRDCT_NAME];
	err = ufshcd_read_string_desc(hba, model_index,
				      &dev_desc->model, SD_ASCII_STD);
	if (err < 0) {
		dev_err(hba->dev, "%s: Failed reading Product Name. err = %d\n",
			__func__, err);
		goto out;
	}

	/*
	 * ufshcd_read_string_desc returns size of the string
	 * reset the error value
	 */
	err = 0;

out:
	kfree(desc_buf);
	return err;
}

static void ufs_fixup_device_setup(struct ufs_hba *hba,
				   struct ufs_dev_desc *dev_desc)
{
	struct ufs_dev_fix *f;

	for (f = ufs_fixups; f->quirk; f++) {
		if ((f->card.wmanufacturerid == dev_desc->wmanufacturerid ||
		     f->card.wmanufacturerid == UFS_ANY_VENDOR) &&
		     ((dev_desc->model &&
		       STR_PRFX_EQUAL(f->card.model, dev_desc->model)) ||
		      !strcmp(f->card.model, UFS_ANY_MODEL)))
			hba->dev_quirks |= f->quirk;
	}
}

static void ufs_put_device_desc(struct ufs_dev_desc *dev_desc)
{
	kfree(dev_desc->model);
	dev_desc->model = NULL;
}

static void ufshcd_tune_unipro_params(struct ufs_hba *hba)
{
	if (ufshcd_is_unipro_pa_params_tuning_req(hba)) {
		ufshcd_tune_pa_tactivate(hba);
		ufshcd_tune_pa_hibern8time(hba);
	}

	if (hba->dev_quirks & UFS_DEVICE_QUIRK_PA_TACTIVATE)
		/* set 1ms timeout for PA_TACTIVATE */
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE), 10);

	if (hba->dev_quirks & UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE)
		ufshcd_quirk_tune_host_pa_tactivate(hba);

	ufshcd_vops_apply_dev_quirks(hba);
}

#define ufshcd_set_ufs_dev_active(h) \
	((h)->curr_dev_pwr_mode = UFS_ACTIVE_PWR_MODE)

/**
 * ufshcd_force_reset_auto_bkops - force reset auto bkops state
 * @hba: per adapter instance
 *
 * After a device reset the device may toggle the BKOPS_EN flag
 * to default value. The s/w tracking variables should be updated
 * as well. This function would change the auto-bkops state based on
 * UFSHCD_CAP_KEEP_AUTO_BKOPS_ENABLED_EXCEPT_SUSPEND.
 */
static void ufshcd_force_reset_auto_bkops(struct ufs_hba *hba)
{
	if (ufshcd_keep_autobkops_enabled_except_suspend(hba)) {
		hba->auto_bkops_enabled = false;
		hba->ee_ctrl_mask |= MASK_EE_URGENT_BKOPS;
		ufshcd_enable_auto_bkops(hba);
	} else {
		hba->auto_bkops_enabled = true;
		hba->ee_ctrl_mask &= ~MASK_EE_URGENT_BKOPS;
		ufshcd_disable_auto_bkops(hba);
	}
}

/**
 * ufshcd_get_max_pwr_mode - reads the max power mode negotiated with device
 * @hba: per-adapter instance
 */
static int ufshcd_get_max_pwr_mode(struct ufs_hba *hba)
{
	struct ufs_pa_layer_attr *pwr_info = &hba->max_pwr_info.info;

	if (hba->max_pwr_info.is_valid)
		return 0;

	pwr_info->pwr_tx = FAST_MODE;
	pwr_info->pwr_rx = FAST_MODE;
	pwr_info->hs_rate = PA_HS_MODE_B;

	/* Get the connected lane count */
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDRXDATALANES),
			&pwr_info->lane_rx);
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
			&pwr_info->lane_tx);

	if (!pwr_info->lane_rx || !pwr_info->lane_tx) {
		dev_err(hba->dev, "%s: invalid connected lanes value. rx=%d, tx=%d\n",
				__func__,
				pwr_info->lane_rx,
				pwr_info->lane_tx);
		return -EINVAL;
	}

	/*
	 * First, get the maximum gears of HS speed.
	 * If a zero value, it means there is no HSGEAR capability.
	 * Then, get the maximum gears of PWM speed.
	 */
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR), &pwr_info->gear_rx);
	if (!pwr_info->gear_rx) {
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&pwr_info->gear_rx);
		if (!pwr_info->gear_rx) {
			dev_err(hba->dev, "%s: invalid max pwm rx gear read = %d\n",
				__func__, pwr_info->gear_rx);
			return -EINVAL;
		}
		pwr_info->pwr_rx = SLOW_MODE;
	}

	ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
			&pwr_info->gear_tx);
	if (!pwr_info->gear_tx) {
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&pwr_info->gear_tx);
		if (!pwr_info->gear_tx) {
			dev_err(hba->dev, "%s: invalid max pwm tx gear read = %d\n",
				__func__, pwr_info->gear_tx);
			return -EINVAL;
		}
		pwr_info->pwr_tx = SLOW_MODE;
	}

	hba->max_pwr_info.is_valid = true;
	return 0;
}

/**
 * ufshcd_config_pwr_mode - configure a new power mode
 * @hba: per-adapter instance
 * @desired_pwr_mode: desired power configuration
 */
int ufshcd_config_pwr_mode(struct ufs_hba *hba,
		struct ufs_pa_layer_attr *desired_pwr_mode)
{
	struct ufs_pa_layer_attr final_params = { 0 };
	int ret;

	ret = ufshcd_vops_pwr_change_notify(hba, PRE_CHANGE,
					desired_pwr_mode, &final_params);

	if (ret)
		memcpy(&final_params, desired_pwr_mode, sizeof(final_params));

	ret = ufshcd_change_power_mode(hba, &final_params);
	if (!ret)
		ufshcd_print_pwr_info(hba);

	return ret;
}
EXPORT_SYMBOL_GPL(ufshcd_config_pwr_mode);

static inline int ufshcd_vops_pwr_change_notify(struct ufs_hba *hba,
				  bool status,
				  struct ufs_pa_layer_attr *dev_max_params,
				  struct ufs_pa_layer_attr *dev_req_params)
{
	if (hba->vops && hba->vops->pwr_change_notify)
		return hba->vops->pwr_change_notify(hba, status,
					dev_max_params, dev_req_params);

	return -ENOTSUPP;
}

static int ufshcd_change_power_mode(struct ufs_hba *hba,
			     struct ufs_pa_layer_attr *pwr_mode)
{
	int ret;

	/* if already configured to the requested pwr_mode */
	if (pwr_mode->gear_rx == hba->pwr_info.gear_rx &&
	    pwr_mode->gear_tx == hba->pwr_info.gear_tx &&
	    pwr_mode->lane_rx == hba->pwr_info.lane_rx &&
	    pwr_mode->lane_tx == hba->pwr_info.lane_tx &&
	    pwr_mode->pwr_rx == hba->pwr_info.pwr_rx &&
	    pwr_mode->pwr_tx == hba->pwr_info.pwr_tx &&
	    pwr_mode->hs_rate == hba->pwr_info.hs_rate) {
		dev_dbg(hba->dev, "%s: power already configured\n", __func__);
		return 0;
	}

	/*
	 * Configure attributes for power mode change with below.
	 * - PA_RXGEAR, PA_ACTIVERXDATALANES, PA_RXTERMINATION,
	 * - PA_TXGEAR, PA_ACTIVETXDATALANES, PA_TXTERMINATION,
	 * - PA_HSSERIES
	 */
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_RXGEAR), pwr_mode->gear_rx);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_ACTIVERXDATALANES),
			pwr_mode->lane_rx);
	if (pwr_mode->pwr_rx == FASTAUTO_MODE ||
			pwr_mode->pwr_rx == FAST_MODE)
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_RXTERMINATION), TRUE);
	else
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_RXTERMINATION), FALSE);

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXGEAR), pwr_mode->gear_tx);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_ACTIVETXDATALANES),
			pwr_mode->lane_tx);
	if (pwr_mode->pwr_tx == FASTAUTO_MODE ||
			pwr_mode->pwr_tx == FAST_MODE)
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXTERMINATION), TRUE);
	else
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXTERMINATION), FALSE);

	if (pwr_mode->pwr_rx == FASTAUTO_MODE ||
	    pwr_mode->pwr_tx == FASTAUTO_MODE ||
	    pwr_mode->pwr_rx == FAST_MODE ||
	    pwr_mode->pwr_tx == FAST_MODE)
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_HSSERIES),
						pwr_mode->hs_rate);

	ret = ufshcd_uic_change_pwr_mode(hba, pwr_mode->pwr_rx << 4
			| pwr_mode->pwr_tx);

	if (ret) {
		dev_err(hba->dev,
			"%s: power mode change failed %d\n", __func__, ret);
	} else {
		ufshcd_vops_pwr_change_notify(hba, POST_CHANGE, NULL,
								pwr_mode);

		memcpy(&hba->pwr_info, pwr_mode,
			sizeof(struct ufs_pa_layer_attr));
	}

	return ret;
}

static int ufshcd_hba_enable(struct ufs_hba *hba)
{
	int ret;

	if (hba->quirks & UFSHCI_QUIRK_BROKEN_HCE) {
		ufshcd_set_link_off(hba);
		ufshcd_vops_hce_enable_notify(hba, PRE_CHANGE);

		/* enable UIC related interrupts */
		ufshcd_enable_intr(hba, UFSHCD_UIC_MASK);
		ret = ufshcd_dme_reset(hba);
		if (!ret) {
			ret = ufshcd_dme_enable(hba);
			if (!ret)
				ufshcd_vops_hce_enable_notify(hba, POST_CHANGE);
			if (ret)
				dev_err(hba->dev,
					"Host controller enable failed with non-hce\n");
		}
	} else {
		ret = ufshcd_hba_execute_hce(hba);
	}

	return ret;
}





















