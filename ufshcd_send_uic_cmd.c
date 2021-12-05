
/**
 * struct uic_command - UIC command structure
 * @command: UIC command
 * @argument1: UIC command argument 1
 * @argument2: UIC command argument 2
 * @argument3: UIC command argument 3
 * @cmd_active: Indicate if UIC command is outstanding
 * @result: UIC command result
 * @done: UIC command completion
 */
struct uic_command {
	u32 command;
	u32 argument1;
	u32 argument2;
	u32 argument3;
	int cmd_active;
	int result;
	struct completion done;
};

/**
 * ufshcd_send_uic_cmd - Send UIC commands and retrieve the result
 * @hba: per adapter instance
 * @uic_cmd: UIC command
 *
 * Returns 0 only if success.
 */
int ufshcd_send_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	int ret;
	unsigned long flags;

	ufshcd_hold(hba, false);                                                             // enable clocks
	mutex_lock(&hba->uic_cmd_mutex);                                                     // 获取锁hba->uic_cmd_mutex
	ufshcd_add_delay_before_dme_cmd(hba);                                                // 保证本次和上次之间的时间差不小于1000微妙

	spin_lock_irqsave(hba->host->host_lock, flags);                                      // 获取锁hba->host->host_lock
	ret = __ufshcd_send_uic_cmd(hba, uic_cmd, true);                                     // Send UIC commands and retrieve the result，成功返回0
	spin_unlock_irqrestore(hba->host->host_lock, flags);                                 // 释放锁hba->host->host_lock
	if (!ret)                                                                            // 若ret为0，即__ufshcd_send_uic_cmd执行成功
		ret = ufshcd_wait_for_uic_cmd(hba, uic_cmd);                                     // Wait complectioin of UIC command

	mutex_unlock(&hba->uic_cmd_mutex);                                                   // 释放锁hba->uic_cmd_mutex

	ufshcd_release(hba);                                                                 // disable clocks
	return ret;                                                                          // 返回ret
}

static inline void ufshcd_add_delay_before_dme_cmd(struct ufs_hba *hba)
{
	#define MIN_DELAY_BEFORE_DME_CMDS_US	1000                                         // 保证本次和上次之间的时间差不小于1000微妙
	unsigned long min_sleep_time_us;

	if (!(hba->quirks & UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS))                             // 若hba->quirks不包含UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS
		return;                                                                          // 返回

	/*
	 * last_dme_cmd_tstamp will be 0 only for 1st call to
	 * this function
	 */
	if (unlikely(!ktime_to_us(hba->last_dme_cmd_tstamp))) {                              // 参考注释理解代码逻辑
		min_sleep_time_us = MIN_DELAY_BEFORE_DME_CMDS_US;                                // 设置min_sleep_time_us为MIN_DELAY_BEFORE_DME_CMDS_US
	} else {
		unsigned long delta =
			(unsigned long) ktime_to_us(
				ktime_sub(ktime_get(),
				hba->last_dme_cmd_tstamp));                                              // 计算本次和上次之间的时间差

		if (delta < MIN_DELAY_BEFORE_DME_CMDS_US)                                        // 若本次和上次之间的时间差delta小于MIN_DELAY_BEFORE_DME_CMDS_US
			min_sleep_time_us =
				MIN_DELAY_BEFORE_DME_CMDS_US - delta;                                    // 设置min_sleep_time_us为MIN_DELAY_BEFORE_DME_CMDS_US和delta的差值
		else
			return; /* no more delay required */                                         // 返回
	}

	/* allow sleep for extra 50us if needed */
	usleep_range(min_sleep_time_us, min_sleep_time_us + 50);                             // 睡眠min_sleep_time_us到min_sleep_time_us + 50微妙，即确保本次和上次之间的时间差不小于1000微妙
}

/**
 * __ufshcd_send_uic_cmd - Send UIC commands and retrieve the result
 * @hba: per adapter instance
 * @uic_cmd: UIC command
 * @completion: initialize the completion only if this is set to true
 *
 * Identical to ufshcd_send_uic_cmd() expect mutex. Must be called
 * with mutex held and host_lock locked.
 * Returns 0 only if success.
 */
static int
__ufshcd_send_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd,
		      bool completion)
{
	if (!ufshcd_ready_for_uic_cmd(hba)) {                                                // Check if controller is ready
		dev_err(hba->dev,
			"Controller not ready to accept UIC commands\n");                            // 打印提示信息
		return -EIO;                                                                     // 返回-EIO
	}

	if (completion)                                                                      // 若completion为true
		init_completion(&uic_cmd->done);                                                 // 初始化uic_cmd->done

	ufshcd_dispatch_uic_cmd(hba, uic_cmd);                                               // Dispatch UIC commands to unipro layers

	return 0;                                                                            // 返回0
}

/**
 * ufshcd_ready_for_uic_cmd - Check if controller is ready
 *                            to accept UIC commands
 * @hba: per adapter instance
 * Return true on success, else false
 */
static inline bool ufshcd_ready_for_uic_cmd(struct ufs_hba *hba)
{
	if (ufshcd_readl(hba, REG_CONTROLLER_STATUS) & UIC_COMMAND_READY)                    // 若ufshcd_readl(hba, REG_CONTROLLER_STATUS)包含UIC_COMMAND_READY
		return true;                                                                     // 返回true
	else
		return false;                                                                    // 返回false
}

/**
 * ufshcd_dispatch_uic_cmd - Dispatch UIC commands to unipro layers
 * @hba: per adapter instance
 * @uic_cmd: UIC command
 *
 * Mutex must be held.
 */
static inline void
ufshcd_dispatch_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	WARN_ON(hba->active_uic_cmd);                                                        // 重点：hba->active_uic_cmd为true表示有uic命令正在执行

	hba->active_uic_cmd = uic_cmd;                                                       // 重点：设置hba->active_uic_cmd为uic_cmd

	/* Write Args */
	ufshcd_writel(hba, uic_cmd->argument1, REG_UIC_COMMAND_ARG_1);                       // Write Args
	ufshcd_writel(hba, uic_cmd->argument2, REG_UIC_COMMAND_ARG_2);                       // Write Args
	ufshcd_writel(hba, uic_cmd->argument3, REG_UIC_COMMAND_ARG_3);                       // Write Args

	/* Write UIC Cmd */
	ufshcd_writel(hba, uic_cmd->command & COMMAND_OPCODE_MASK,
		      REG_UIC_COMMAND);                                                          // Write UIC Cmd
}

/* UIC command timeout, unit: ms */
#define UIC_CMD_TIMEOUT	500
#define MASK_UIC_COMMAND_RESULT			0xFF

/**
 * ufshcd_wait_for_uic_cmd - Wait complectioin of UIC command
 * @hba: per adapter instance
 * @uic_cmd: UIC command
 *
 * Must be called with mutex held.
 * Returns 0 only if success.
 */
static int
(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	int ret;
	unsigned long flags;

	if (wait_for_completion_timeout(&uic_cmd->done,
					msecs_to_jiffies(UIC_CMD_TIMEOUT)))                                  // 等待uic_cmd->done，超时时间UIC_CMD_TIMEOUT毫秒，返回非0表示未超时
		ret = uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT;                              // 重点：在ufshcd_uic_cmd_compl中会设置hba->active_uic_cmd->argument2
	else
		ret = -ETIMEDOUT;                                                                // 返回-ETIMEDOUT

	spin_lock_irqsave(hba->host->host_lock, flags);                                      // 获取锁hba->host->host_lock
	hba->active_uic_cmd = NULL;                                                          // 重点：设置hba->active_uic_cmd为NULL
	spin_unlock_irqrestore(hba->host->host_lock, flags);                                 // 释放锁hba->host->host_lock

	return ret;                                                                          // 返回ret
}

/**
 * ufshcd_intr - Main interrupt service routine
 * @irq: irq number
 * @__hba: pointer to adapter instance
 *
 * Returns IRQ_HANDLED - If interrupt is valid
 *		IRQ_NONE - If invalid interrupt
 */
static irqreturn_t ufshcd_intr(int irq, void *__hba)
{
	u32 intr_status, enabled_intr_status;
	irqreturn_t retval = IRQ_NONE;
	struct ufs_hba *hba = __hba;
	int retries = hba->nutrs;

	spin_lock(hba->host->host_lock);                                                     // 获取锁hba->host->host_lock
	intr_status = ufshcd_readl(hba, REG_INTERRUPT_STATUS);                               // 读取intr_status

	/*
	 * There could be max of hba->nutrs reqs in flight and in worst case
	 * if the reqs get finished 1 by 1 after the interrupt status is
	 * read, make sure we handle them by checking the interrupt status
	 * again in a loop until we process all of the reqs before returning.
	 */
	do {
		enabled_intr_status =
			intr_status & ufshcd_readl(hba, REG_INTERRUPT_ENABLE);                       // 确定中断是否使能
		if (intr_status)                                                                 // 若intr_status为true
			ufshcd_writel(hba, intr_status, REG_INTERRUPT_STATUS);                       // 清除中断状态
		if (enabled_intr_status) {                                                       // 若enabled_intr_status为true
			ufshcd_sl_intr(hba, enabled_intr_status);                                    // Interrupt service routine
			retval = IRQ_HANDLED;                                                        // 设置retval为IRQ_HANDLED
		}

		intr_status = ufshcd_readl(hba, REG_INTERRUPT_STATUS);                           // 获取intr_status
	} while (intr_status && --retries);                                                  // 若intr_status && --retries为true

	spin_unlock(hba->host->host_lock);                                                   // 释放锁hba->host->host_lock
	return retval;                                                                       // 返回retval
}

/**
 * ufshcd_sl_intr - Interrupt service routine
 * @hba: per adapter instance
 * @intr_status: contains interrupts generated by the controller
 */
static void ufshcd_sl_intr(struct ufs_hba *hba, u32 intr_status)
{
	hba->errors = UFSHCD_ERROR_MASK & intr_status;                                       // 确认是否存在errors

	if (ufshcd_is_auto_hibern8_error(hba, intr_status))                                  // 
		hba->errors |= (UFSHCD_UIC_HIBERN8_MASK & intr_status);                          // 

	if (hba->errors)                                                                     // 若hba->errors为true
		ufshcd_check_errors(hba);                                                        // 

	if (intr_status & UFSHCD_UIC_MASK)                                                   // 若intr_status & UFSHCD_UIC_MASK为true
		ufshcd_uic_cmd_compl(hba, intr_status);                                          // 

	if (intr_status & UTP_TASK_REQ_COMPL)                                                // 若intr_status & UTP_TASK_REQ_COMPL为true
		ufshcd_tmc_handler(hba);                                                         // 

	if (intr_status & UTP_TRANSFER_REQ_COMPL)                                            // 若intr_status & UTP_TRANSFER_REQ_COMPL为true
		ufshcd_transfer_req_compl(hba);                                                  // 
}

/**
 * ufshcd_uic_cmd_compl - handle completion of uic command
 * @hba: per adapter instance
 * @intr_status: interrupt status generated by the controller
 */
static void ufshcd_uic_cmd_compl(struct ufs_hba *hba, u32 intr_status)
{
	if ((intr_status & UIC_COMMAND_COMPL) && hba->active_uic_cmd) {                      // 若intr_status包含UIC_COMMAND_COMPL，并且hba->active_uic_cmd为true（有uic命令正在执行）
		hba->active_uic_cmd->argument2 |=
			ufshcd_get_uic_cmd_result(hba);                                              // 重点：hba->active_uic_cmd->argument2 |= ufshcd_readl(hba, REG_UIC_COMMAND_ARG_2) & MASK_UIC_COMMAND_RESULT
		hba->active_uic_cmd->argument3 =
			ufshcd_get_dme_attr_val(hba);                                                // 重点：hba->active_uic_cmd->argument3 = ufshcd_readl(hba, REG_UIC_COMMAND_ARG_3)
		complete(&hba->active_uic_cmd->done);                                            // 重点：完成hba->active_uic_cmd->done
	}

	if ((intr_status & UFSHCD_UIC_PWR_MASK) && hba->uic_async_done)                      // 若intr_status & UFSHCD_UIC_PWR_MASK为true，并且hba->uic_async_done为true
		complete(hba->uic_async_done);                                                   // 完成hba->uic_async_done
}

/**
 * ufshcd_get_uic_cmd_result - Get the UIC command result
 * @hba: Pointer to adapter instance
 *
 * This function gets the result of UIC command completion
 * Returns 0 on success, non zero value on error
 */
static inline int ufshcd_get_uic_cmd_result(struct ufs_hba *hba)
{
	return ufshcd_readl(hba, REG_UIC_COMMAND_ARG_2) &
	       MASK_UIC_COMMAND_RESULT;                                                      // 
}

/**
 * ufshcd_get_dme_attr_val - Get the value of attribute returned by UIC command
 * @hba: Pointer to adapter instance
 *
 * This function gets UIC command argument3
 * Returns 0 on success, non zero value on error
 */
static inline u32 ufshcd_get_dme_attr_val(struct ufs_hba *hba)
{
	return ufshcd_readl(hba, REG_UIC_COMMAND_ARG_3);                                     // 
}






















































