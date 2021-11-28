/*
 * Universal Flash Storage Host controller driver
 *
 * This code is based on drivers/scsi/ufs/ufshcd.h
 * Copyright (C) 2011-2013 Samsung India Software Operations
 * Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
 *
 * Authors:
 *	Santosh Yaraganavi <santosh.sy@samsung.com>
 *	Vinayak Holikatti <h.vinayak@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 */

#ifndef _UFSHCD_H
#define _UFSHCD_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/regulator/consumer.h>
#include "unipro.h"

#include <asm/irq.h>
#include <asm/byteorder.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_eh.h>

#include "ufs.h"
#include "ufshci.h"

#define UFSHCD "ufshcd"
#define UFSHCD_DRIVER_VERSION "0.2"

struct ufs_hba;

enum dev_cmd_type {
	DEV_CMD_TYPE_NOP		= 0x0,
	DEV_CMD_TYPE_QUERY		= 0x1,
};

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

/* Used to differentiate the power management options */
enum ufs_pm_op {
	UFS_RUNTIME_PM,
	UFS_SYSTEM_PM,
	UFS_SHUTDOWN_PM,
};

#define ufshcd_is_runtime_pm(op) ((op) == UFS_RUNTIME_PM)
#define ufshcd_is_system_pm(op) ((op) == UFS_SYSTEM_PM)
#define ufshcd_is_shutdown_pm(op) ((op) == UFS_SHUTDOWN_PM)

/* Host <-> Device UniPro Link state */
enum uic_link_state {
	UIC_LINK_OFF_STATE	= 0, /* Link powered down or disabled */
	UIC_LINK_ACTIVE_STATE	= 1, /* Link is in Fast/Slow/Sleep state */
	UIC_LINK_HIBERN8_STATE	= 2, /* Link is in Hibernate state */
};

#define ufshcd_is_link_off(hba) ((hba)->uic_link_state == UIC_LINK_OFF_STATE)
#define ufshcd_is_link_active(hba) ((hba)->uic_link_state == \
				    UIC_LINK_ACTIVE_STATE)
#define ufshcd_is_link_hibern8(hba) ((hba)->uic_link_state == \
				    UIC_LINK_HIBERN8_STATE)
#define ufshcd_set_link_off(hba) ((hba)->uic_link_state = UIC_LINK_OFF_STATE)
#define ufshcd_set_link_active(hba) ((hba)->uic_link_state = \
				    UIC_LINK_ACTIVE_STATE)
#define ufshcd_set_link_hibern8(hba) ((hba)->uic_link_state = \
				    UIC_LINK_HIBERN8_STATE)

/*
 * UFS Power management levels.
 * Each level is in increasing order of power savings.
 */
enum ufs_pm_level {
	UFS_PM_LVL_0, /* UFS_ACTIVE_PWR_MODE, UIC_LINK_ACTIVE_STATE */
	UFS_PM_LVL_1, /* UFS_ACTIVE_PWR_MODE, UIC_LINK_HIBERN8_STATE */
	UFS_PM_LVL_2, /* UFS_SLEEP_PWR_MODE, UIC_LINK_ACTIVE_STATE */
	UFS_PM_LVL_3, /* UFS_SLEEP_PWR_MODE, UIC_LINK_HIBERN8_STATE */
	UFS_PM_LVL_4, /* UFS_POWERDOWN_PWR_MODE, UIC_LINK_HIBERN8_STATE */
	UFS_PM_LVL_5, /* UFS_POWERDOWN_PWR_MODE, UIC_LINK_OFF_STATE */
	UFS_PM_LVL_MAX
};

struct ufs_pm_lvl_states {
	enum ufs_dev_pwr_mode dev_state;
	enum uic_link_state link_state;
};

/**
 * struct ufshcd_lrb - local reference block
 * @utr_descriptor_ptr: UTRD address of the command
 * @ucd_req_ptr: UCD address of the command
 * @ucd_rsp_ptr: Response UPIU address for this command
 * @ucd_prdt_ptr: PRDT address of the command
 * @utrd_dma_addr: UTRD dma address for debug
 * @ucd_prdt_dma_addr: PRDT dma address for debug
 * @ucd_rsp_dma_addr: UPIU response dma address for debug
 * @ucd_req_dma_addr: UPIU request dma address for debug
 * @cmd: pointer to SCSI command
 * @sense_buffer: pointer to sense buffer address of the SCSI command
 * @sense_bufflen: Length of the sense buffer
 * @scsi_status: SCSI status of the command
 * @command_type: SCSI, UFS, Query.
 * @task_tag: Task tag of the command
 * @lun: LUN of the command
 * @intr_cmd: Interrupt command (doesn't participate in interrupt aggregation)
 * @issue_time_stamp: time stamp for debug purposes
 * @compl_time_stamp: time stamp for statistics
 * @req_abort_skip: skip request abort task flag
 */
struct ufshcd_lrb {
	struct utp_transfer_req_desc *utr_descriptor_ptr;
	struct utp_upiu_req *ucd_req_ptr;
	struct utp_upiu_rsp *ucd_rsp_ptr;
	struct ufshcd_sg_entry *ucd_prdt_ptr;

	dma_addr_t utrd_dma_addr;
	dma_addr_t ucd_req_dma_addr;
	dma_addr_t ucd_rsp_dma_addr;
	dma_addr_t ucd_prdt_dma_addr;

	struct scsi_cmnd *cmd;
	u8 *sense_buffer;
	unsigned int sense_bufflen;
	int scsi_status;

	int command_type;
	int task_tag;
	u8 lun; /* UPIU LUN id field is only 8-bit wide */
	bool intr_cmd;
	ktime_t issue_time_stamp;
	ktime_t compl_time_stamp;

	bool req_abort_skip;
};

/**
 * struct ufs_query - holds relevant data structures for query request
 * @request: request upiu and function
 * @descriptor: buffer for sending/receiving descriptor
 * @response: response upiu and response
 */
struct ufs_query {
	struct ufs_query_req request;
	u8 *descriptor;
	struct ufs_query_res response;
};

/**
 * struct ufs_dev_cmd - all assosiated fields with device management commands
 * @type: device management command type - Query, NOP OUT
 * @lock: lock to allow one command at a time
 * @complete: internal commands completion
 * @tag_wq: wait queue until free command slot is available
 */
struct ufs_dev_cmd {
	enum dev_cmd_type type;
	struct mutex lock;
	struct completion *complete;
	wait_queue_head_t tag_wq;
	struct ufs_query query;
};

struct ufs_desc_size {
	int dev_desc;
	int pwr_desc;
	int geom_desc;
	int interc_desc;
	int unit_desc;
	int conf_desc;
	int hlth_desc;
};

/**
 * struct ufs_clk_info - UFS clock related info
 * @list: list headed by hba->clk_list_head
 * @clk: clock node
 * @name: clock name
 * @max_freq: maximum frequency supported by the clock
 * @min_freq: min frequency that can be used for clock scaling
 * @curr_freq: indicates the current frequency that it is set to
 * @enabled: variable to check against multiple enable/disable
 */
struct ufs_clk_info {
	struct list_head list;
	struct clk *clk;
	const char *name;
	u32 max_freq;
	u32 min_freq;
	u32 curr_freq;
	bool enabled;
};

enum ufs_notify_change_status {
	PRE_CHANGE,
	POST_CHANGE,
};

struct ufs_pa_layer_attr {
	u32 gear_rx;
	u32 gear_tx;
	u32 lane_rx;
	u32 lane_tx;
	u32 pwr_rx;
	u32 pwr_tx;
	u32 hs_rate;
};

struct ufs_pwr_mode_info {
	bool is_valid;
	struct ufs_pa_layer_attr info;
};

/**
 * struct ufs_hba_variant_ops - variant specific callbacks
 * @name: variant name
 * @init: called when the driver is initialized
 * @exit: called to cleanup everything done in init
 * @get_ufs_hci_version: called to get UFS HCI version
 * @clk_scale_notify: notifies that clks are scaled up/down
 * @setup_clocks: called before touching any of the controller registers
 * @setup_regulators: called before accessing the host controller
 * @hce_enable_notify: called before and after HCE enable bit is set to allow
 *                     variant specific Uni-Pro initialization.
 * @link_startup_notify: called before and after Link startup is carried out
 *                       to allow variant specific Uni-Pro initialization.
 * @pwr_change_notify: called before and after a power mode change
 *			is carried out to allow vendor spesific capabilities
 *			to be set.
 * @setup_xfer_req: called before any transfer request is issued
 *                  to set some things
 * @setup_task_mgmt: called before any task management request is issued
 *                  to set some things
 * @hibern8_notify: called around hibern8 enter/exit
 * @apply_dev_quirks: called to apply device specific quirks
 * @suspend: called during host controller PM callback
 * @resume: called during host controller PM callback
 * @dbg_register_dump: used to dump controller debug information
 * @phy_initialization: used to initialize phys
 * @device_reset: called to issue a reset pulse on the UFS device
 */
struct ufs_hba_variant_ops {                                                             // variant specific callbacks
	const char *name;                                                                    // variant name
	int	(*init)(struct ufs_hba *);                                                       // called when the driver is initialized
	void    (*exit)(struct ufs_hba *);                                                   // called to cleanup everything done in init
	u32	(*get_ufs_hci_version)(struct ufs_hba *);                                        // called to get UFS HCI version
	int	(*clk_scale_notify)(struct ufs_hba *, bool,
				    enum ufs_notify_change_status);                                      // notifies that clks are scaled up/down
	int	(*setup_clocks)(struct ufs_hba *, bool,
				enum ufs_notify_change_status);                                          // called before touching any of the controller registers
	int     (*setup_regulators)(struct ufs_hba *, bool);                                 // called before accessing the host controller
	int	(*hce_enable_notify)(struct ufs_hba *,
				     enum ufs_notify_change_status);                                     // called before and after HCE enable bit is set to allow variant specific Uni-Pro initialization
	int	(*link_startup_notify)(struct ufs_hba *,
				       enum ufs_notify_change_status);                                   // called before and after Link startup is carried out to allow variant specific Uni-Pro initialization
	int	(*pwr_change_notify)(struct ufs_hba *,
					enum ufs_notify_change_status status,
					struct ufs_pa_layer_attr *,
					struct ufs_pa_layer_attr *);                                         // called before and after a power mode change is carried out to allow vendor spesific capabilities to be set
	void	(*setup_xfer_req)(struct ufs_hba *, int, bool);                              // called before any transfer request is issued to set some things
	void	(*setup_task_mgmt)(struct ufs_hba *, int, u8);                               // called before any task management request is issued to set some things
	void    (*hibern8_notify)(struct ufs_hba *, enum uic_cmd_dme,
					enum ufs_notify_change_status);                                      // called around hibern8 enter/exit
	int	(*apply_dev_quirks)(struct ufs_hba *);                                           // called to apply device specific quirks
	int     (*suspend)(struct ufs_hba *, enum ufs_pm_op);                                // called during host controller PM callback
	int     (*resume)(struct ufs_hba *, enum ufs_pm_op);                                 // called during host controller PM callback
	void	(*dbg_register_dump)(struct ufs_hba *hba);                                   // used to dump controller debug information
	int	(*phy_initialization)(struct ufs_hba *);                                         // used to initialize phys
	void	(*device_reset)(struct ufs_hba *hba);                                        // called to issue a reset pulse on the UFS device
};

/* clock gating state  */
enum clk_gating_state {
	CLKS_OFF,
	CLKS_ON,
	REQ_CLKS_OFF,
	REQ_CLKS_ON,
};

/**
 * struct ufs_clk_gating - UFS clock gating related info
 * @gate_work: worker to turn off clocks after some delay as specified in
 * delay_ms
 * @ungate_work: worker to turn on clocks that will be used in case of
 * interrupt context
 * @state: the current clocks state
 * @delay_ms: gating delay in ms
 * @is_suspended: clk gating is suspended when set to 1 which can be used
 * during suspend/resume
 * @delay_attr: sysfs attribute to control delay_attr
 * @enable_attr: sysfs attribute to enable/disable clock gating
 * @is_enabled: Indicates the current status of clock gating
 * @active_reqs: number of requests that are pending and should be waited for
 * completion before gating clocks.
 */
struct ufs_clk_gating {
	struct delayed_work gate_work;
	struct work_struct ungate_work;
	enum clk_gating_state state;
	unsigned long delay_ms;
	bool is_suspended;
	struct device_attribute delay_attr;
	struct device_attribute enable_attr;
	bool is_enabled;
	int active_reqs;
	struct workqueue_struct *clk_gating_workq;
};

struct ufs_saved_pwr_info {
	struct ufs_pa_layer_attr info;
	bool is_valid;
};

/**
 * struct ufs_clk_scaling - UFS clock scaling related data
 * @active_reqs: number of requests that are pending. If this is zero when
 * devfreq ->target() function is called then schedule "suspend_work" to
 * suspend devfreq.
 * @tot_busy_t: Total busy time in current polling window
 * @window_start_t: Start time (in jiffies) of the current polling window
 * @busy_start_t: Start time of current busy period
 * @enable_attr: sysfs attribute to enable/disable clock scaling
 * @saved_pwr_info: UFS power mode may also be changed during scaling and this
 * one keeps track of previous power mode.
 * @workq: workqueue to schedule devfreq suspend/resume work
 * @suspend_work: worker to suspend devfreq
 * @resume_work: worker to resume devfreq
 * @is_allowed: tracks if scaling is currently allowed or not
 * @is_busy_started: tracks if busy period has started or not
 * @is_suspended: tracks if devfreq is suspended or not
 */
struct ufs_clk_scaling {
	int active_reqs;
	unsigned long tot_busy_t;
	unsigned long window_start_t;
	ktime_t busy_start_t;
	struct device_attribute enable_attr;
	struct ufs_saved_pwr_info saved_pwr_info;
	struct workqueue_struct *workq;
	struct work_struct suspend_work;
	struct work_struct resume_work;
	bool is_allowed;
	bool is_busy_started;
	bool is_suspended;
};

/**
 * struct ufs_init_prefetch - contains data that is pre-fetched once during
 * initialization
 * @icc_level: icc level which was read during initialization
 */
struct ufs_init_prefetch {
	u32 icc_level;
};

#define UFS_ERR_REG_HIST_LENGTH 8
/**
 * struct ufs_err_reg_hist - keeps history of errors
 * @pos: index to indicate cyclic buffer position
 * @reg: cyclic buffer for registers value
 * @tstamp: cyclic buffer for time stamp
 */
struct ufs_err_reg_hist {
	int pos;
	u32 reg[UFS_ERR_REG_HIST_LENGTH];
	ktime_t tstamp[UFS_ERR_REG_HIST_LENGTH];
};

/**
 * struct ufs_stats - keeps usage/err statistics
 * @hibern8_exit_cnt: Counter to keep track of number of exits,
 *		reset this after link-startup.
 * @last_hibern8_exit_tstamp: Set time after the hibern8 exit.
 *		Clear after the first successful command completion.
 * @pa_err: tracks pa-uic errors
 * @dl_err: tracks dl-uic errors
 * @nl_err: tracks nl-uic errors
 * @tl_err: tracks tl-uic errors
 * @dme_err: tracks dme errors
 * @auto_hibern8_err: tracks auto-hibernate errors
 * @fatal_err: tracks fatal errors
 * @linkup_err: tracks link-startup errors
 * @resume_err: tracks resume errors
 * @suspend_err: tracks suspend errors
 * @dev_reset: tracks device reset events
 * @host_reset: tracks host reset events
 * @tsk_abort: tracks task abort events
 */
struct ufs_stats {
	u32 hibern8_exit_cnt;
	ktime_t last_hibern8_exit_tstamp;

	/* uic specific errors */
	struct ufs_err_reg_hist pa_err;
	struct ufs_err_reg_hist dl_err;
	struct ufs_err_reg_hist nl_err;
	struct ufs_err_reg_hist tl_err;
	struct ufs_err_reg_hist dme_err;

	/* fatal errors */
	struct ufs_err_reg_hist auto_hibern8_err;
	struct ufs_err_reg_hist fatal_err;
	struct ufs_err_reg_hist link_startup_err;
	struct ufs_err_reg_hist resume_err;
	struct ufs_err_reg_hist suspend_err;

	/* abnormal events */
	struct ufs_err_reg_hist dev_reset;
	struct ufs_err_reg_hist host_reset;
	struct ufs_err_reg_hist task_abort;
};

/**
 * struct ufs_hba - per adapter private structure
 * @mmio_base: UFSHCI base register address
 * @ucdl_base_addr: UFS Command Descriptor base address
 * @utrdl_base_addr: UTP Transfer Request Descriptor base address
 * @utmrdl_base_addr: UTP Task Management Descriptor base address
 * @ucdl_dma_addr: UFS Command Descriptor DMA address
 * @utrdl_dma_addr: UTRDL DMA address
 * @utmrdl_dma_addr: UTMRDL DMA address
 * @host: Scsi_Host instance of the driver
 * @dev: device handle
 * @lrb: local reference block
 * @lrb_in_use: lrb in use
 * @outstanding_tasks: Bits representing outstanding task requests
 * @outstanding_reqs: Bits representing outstanding transfer requests
 * @capabilities: UFS Controller Capabilities
 * @nutrs: Transfer Request Queue depth supported by controller
 * @nutmrs: Task Management Queue depth supported by controller
 * @ufs_version: UFS Version to which controller complies
 * @vops: pointer to variant specific operations
 * @priv: pointer to variant specific private data
 * @irq: Irq number of the controller
 * @active_uic_cmd: handle of active UIC command
 * @uic_cmd_mutex: mutex for uic command
 * @tm_wq: wait queue for task management
 * @tm_tag_wq: wait queue for free task management slots
 * @tm_slots_in_use: bit map of task management request slots in use
 * @pwr_done: completion for power mode change
 * @tm_condition: condition variable for task management
 * @ufshcd_state: UFSHCD states
 * @eh_flags: Error handling flags
 * @intr_mask: Interrupt Mask Bits
 * @ee_ctrl_mask: Exception event control mask
 * @is_powered: flag to check if HBA is powered
 * @is_init_prefetch: flag to check if data was pre-fetched in initialization
 * @init_prefetch_data: data pre-fetched during initialization
 * @eh_work: Worker to handle UFS errors that require s/w attention
 * @eeh_work: Worker to handle exception events
 * @errors: HBA errors
 * @uic_error: UFS interconnect layer error status
 * @saved_err: sticky error mask
 * @saved_uic_err: sticky UIC error mask
 * @dev_cmd: ufs device management command information
 * @last_dme_cmd_tstamp: time stamp of the last completed DME command
 * @auto_bkops_enabled: to track whether bkops is enabled in device
 * @vreg_info: UFS device voltage regulator information
 * @clk_list_head: UFS host controller clocks list node head
 * @pwr_info: holds current power mode
 * @max_pwr_info: keeps the device max valid pwm
 * @desc_size: descriptor sizes reported by device
 * @urgent_bkops_lvl: keeps track of urgent bkops level for device
 * @is_urgent_bkops_lvl_checked: keeps track if the urgent bkops level for
 *  device is known or not.
 * @scsi_block_reqs_cnt: reference counting for scsi block requests
 */
struct ufs_hba {
	void __iomem *mmio_base;                                                             // UFSHCI base register address

	/* Virtual memory reference */
	struct utp_transfer_cmd_desc *ucdl_base_addr;                                        // ufshcd_memory_alloc: utp_transfer_cmd_desc ucdl_base_addr   <----> ucdl_dma_addr
	struct utp_transfer_req_desc *utrdl_base_addr;                                       // ufshcd_memory_alloc: utp_transfer_req_desc utrdl_base_addr  <----> utrdl_dma_addr
	struct utp_task_req_desc *utmrdl_base_addr;                                          // ufshcd_memory_alloc: utp_task_req_desc     utmrdl_base_addr <----> utmrdl_dma_addr

	/* DMA memory reference */
	dma_addr_t ucdl_dma_addr;                                                            // ufshcd_memory_alloc: utp_transfer_cmd_desc ucdl_base_addr   <----> ucdl_dma_addr
	dma_addr_t utrdl_dma_addr;                                                           // ufshcd_memory_alloc: utp_transfer_req_desc utrdl_base_addr  <----> utrdl_dma_addr
	dma_addr_t utmrdl_dma_addr;                                                          // ufshcd_memory_alloc: utp_task_req_desc     utmrdl_base_addr <----> utmrdl_dma_addr

	struct Scsi_Host *host;
	struct device *dev;
	/*
	 * This field is to keep a reference to "scsi_device" corresponding to
	 * "UFS device" W-LU.
	 */
	struct scsi_device *sdev_ufs_device;

	enum ufs_dev_pwr_mode curr_dev_pwr_mode;
	enum uic_link_state uic_link_state;
	/* Desired UFS power management level during runtime PM */
	enum ufs_pm_level rpm_lvl;
	/* Desired UFS power management level during system PM */
	enum ufs_pm_level spm_lvl;
	struct device_attribute rpm_lvl_attr;
	struct device_attribute spm_lvl_attr;
	int pm_op_in_progress;

	/* Auto-Hibernate Idle Timer register value */
	u32 ahit;

	struct ufshcd_lrb *lrb;
	unsigned long lrb_in_use;

	unsigned long outstanding_tasks;
	unsigned long outstanding_reqs;

	u32 capabilities;
	int nutrs;
	int nutmrs;
	u32 ufs_version;
	const struct ufs_hba_variant_ops *vops;
	void *priv;
	unsigned int irq;
	bool is_irq_enabled;
	enum ufs_ref_clk_freq dev_ref_clk_freq;

	/* Interrupt aggregation support is broken */
	#define UFSHCD_QUIRK_BROKEN_INTR_AGGR			0x1

	/*
	 * delay before each dme command is required as the unipro
	 * layer has shown instabilities
	 */
	#define UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS		0x2

	/*
	 * If UFS host controller is having issue in processing LCC (Line
	 * Control Command) coming from device then enable this quirk.
	 * When this quirk is enabled, host controller driver should disable
	 * the LCC transmission on UFS device (by clearing TX_LCC_ENABLE
	 * attribute of device to 0).
	 */
	#define UFSHCD_QUIRK_BROKEN_LCC				0x4

	/*
	 * The attribute PA_RXHSUNTERMCAP specifies whether or not the
	 * inbound Link supports unterminated line in HS mode. Setting this
	 * attribute to 1 fixes moving to HS gear.
	 */
	#define UFSHCD_QUIRK_BROKEN_PA_RXHSUNTERMCAP		0x8

	/*
	 * This quirk needs to be enabled if the host contoller only allows
	 * accessing the peer dme attributes in AUTO mode (FAST AUTO or
	 * SLOW AUTO).
	 */
	#define UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE		0x10

	/*
	 * This quirk needs to be enabled if the host contoller doesn't
	 * advertise the correct version in UFS_VER register. If this quirk
	 * is enabled, standard UFS host driver will call the vendor specific
	 * ops (get_ufs_hci_version) to get the correct version.
	 */
	#define UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION		0x20

	/*
	 * This quirk needs to be enabled if the host contoller regards
	 * resolution of the values of PRDTO and PRDTL in UTRD as byte.
	 */
	#define UFSHCD_QUIRK_PRDT_BYTE_GRAN			0x80

	/*
	 * Clear handling for transfer/task request list is just opposite.
	 */
	#define UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR		0x100

	/*
	 * This quirk needs to be enabled if host controller doesn't allow
	 * that the interrupt aggregation timer and counter are reset by s/w.
	 */
	#define UFSHCI_QUIRK_SKIP_RESET_INTR_AGGR		0x200

	/*
	 * This quirks needs to be enabled if host controller cannot be
	 * enabled via HCE register.
	 */
	#define UFSHCI_QUIRK_BROKEN_HCE				0x400
	unsigned int quirks;	/* Deviations from standard UFSHCI spec. */

	/* Device deviations from standard UFS device spec. */
	unsigned int dev_quirks;

	wait_queue_head_t tm_wq;
	wait_queue_head_t tm_tag_wq;
	unsigned long tm_condition;
	unsigned long tm_slots_in_use;

	struct uic_command *active_uic_cmd;
	struct mutex uic_cmd_mutex;
	struct completion *uic_async_done;

	u32 ufshcd_state;
	u32 eh_flags;
	u32 intr_mask;
	u16 ee_ctrl_mask;
	bool is_powered;
	bool is_init_prefetch;
	struct ufs_init_prefetch init_prefetch_data;

	/* Work Queues */
	struct work_struct eh_work;
	struct work_struct eeh_work;

	/* HBA Errors */
	u32 errors;
	u32 uic_error;
	u32 saved_err;
	u32 saved_uic_err;
	struct ufs_stats ufs_stats;

	/* Device management request data */
	struct ufs_dev_cmd dev_cmd;
	ktime_t last_dme_cmd_tstamp;

	/* Keeps information of the UFS device connected to this host */
	struct ufs_dev_info dev_info;
	bool auto_bkops_enabled;
	struct ufs_vreg_info vreg_info;
	struct list_head clk_list_head;

	bool wlun_dev_clr_ua;

	/* Number of requests aborts */
	int req_abort_count;

	/* Number of lanes available (1 or 2) for Rx/Tx */
	u32 lanes_per_direction;
	struct ufs_pa_layer_attr pwr_info;
	struct ufs_pwr_mode_info max_pwr_info;

	struct ufs_clk_gating clk_gating;
	/* Control to enable/disable host capabilities */
	u32 caps;
	/* Allow dynamic clk gating */
#define UFSHCD_CAP_CLK_GATING	(1 << 0)
	/* Allow hiberb8 with clk gating */
#define UFSHCD_CAP_HIBERN8_WITH_CLK_GATING (1 << 1)
	/* Allow dynamic clk scaling */
#define UFSHCD_CAP_CLK_SCALING	(1 << 2)
	/* Allow auto bkops to enabled during runtime suspend */
#define UFSHCD_CAP_AUTO_BKOPS_SUSPEND (1 << 3)
	/*
	 * This capability allows host controller driver to use the UFS HCI's
	 * interrupt aggregation capability.
	 * CAUTION: Enabling this might reduce overall UFS throughput.
	 */
#define UFSHCD_CAP_INTR_AGGR (1 << 4)
	/*
	 * This capability allows the device auto-bkops to be always enabled
	 * except during suspend (both runtime and suspend).
	 * Enabling this capability means that device will always be allowed
	 * to do background operation when it's active but it might degrade
	 * the performance of ongoing read/write operations.
	 */
#define UFSHCD_CAP_KEEP_AUTO_BKOPS_ENABLED_EXCEPT_SUSPEND (1 << 5)

	struct devfreq *devfreq;
	struct ufs_clk_scaling clk_scaling;
	bool is_sys_suspended;

	enum bkops_status urgent_bkops_lvl;
	bool is_urgent_bkops_lvl_checked;

	struct rw_semaphore clk_scaling_lock;
	struct ufs_desc_size desc_size;
	atomic_t scsi_block_reqs_cnt;

	struct device		bsg_dev;
	struct request_queue	*bsg_queue;
};

/* Returns true if clocks can be gated. Otherwise false */
static inline bool ufshcd_is_clkgating_allowed(struct ufs_hba *hba)
{
	return hba->caps & UFSHCD_CAP_CLK_GATING;
}
static inline bool ufshcd_can_hibern8_during_gating(struct ufs_hba *hba)
{
	return hba->caps & UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;
}
static inline int ufshcd_is_clkscaling_supported(struct ufs_hba *hba)
{
	return hba->caps & UFSHCD_CAP_CLK_SCALING;
}
static inline bool ufshcd_can_autobkops_during_suspend(struct ufs_hba *hba)
{
	return hba->caps & UFSHCD_CAP_AUTO_BKOPS_SUSPEND;
}

static inline bool ufshcd_is_intr_aggr_allowed(struct ufs_hba *hba)
{
/* DWC UFS Core has the Interrupt aggregation feature but is not detectable*/
#ifndef CONFIG_SCSI_UFS_DWC
	if ((hba->caps & UFSHCD_CAP_INTR_AGGR) &&
	    !(hba->quirks & UFSHCD_QUIRK_BROKEN_INTR_AGGR))
		return true;
	else
		return false;
#else
return true;
#endif
}

static inline bool ufshcd_is_auto_hibern8_supported(struct ufs_hba *hba)
{
	return (hba->capabilities & MASK_AUTO_HIBERN8_SUPPORT);
}

#define ufshcd_writel(hba, val, reg)	\
	writel((val), (hba)->mmio_base + (reg))
#define ufshcd_readl(hba, reg)	\
	readl((hba)->mmio_base + (reg))

/**
 * ufshcd_rmwl - read modify write into a register
 * @hba - per adapter instance
 * @mask - mask to apply on read value
 * @val - actual value to write
 * @reg - register address
 */
static inline void ufshcd_rmwl(struct ufs_hba *hba, u32 mask, u32 val, u32 reg)
{
	u32 tmp;

	tmp = ufshcd_readl(hba, reg);
	tmp &= ~mask;
	tmp |= (val & mask);
	ufshcd_writel(hba, tmp, reg);
}

int ufshcd_alloc_host(struct device *, struct ufs_hba **);
void ufshcd_dealloc_host(struct ufs_hba *);
int ufshcd_init(struct ufs_hba * , void __iomem * , unsigned int);
void ufshcd_remove(struct ufs_hba *);
int ufshcd_wait_for_register(struct ufs_hba *hba, u32 reg, u32 mask,
				u32 val, unsigned long interval_us,
				unsigned long timeout_ms, bool can_sleep);
void ufshcd_parse_dev_ref_clk_freq(struct ufs_hba *hba, struct clk *refclk);

static inline void check_upiu_size(void)
{
	BUILD_BUG_ON(ALIGNED_UPIU_SIZE <
		GENERAL_UPIU_REQUEST_SIZE + QUERY_DESC_MAX_SIZE);
}

/**
 * ufshcd_set_variant - set variant specific data to the hba
 * @hba - per adapter instance
 * @variant - pointer to variant specific data
 */
static inline void ufshcd_set_variant(struct ufs_hba *hba, void *variant)
{
	BUG_ON(!hba);
	hba->priv = variant;
}

/**
 * ufshcd_get_variant - get variant specific data from the hba
 * @hba - per adapter instance
 */
static inline void *ufshcd_get_variant(struct ufs_hba *hba)
{
	BUG_ON(!hba);
	return hba->priv;
}
static inline bool ufshcd_keep_autobkops_enabled_except_suspend(
							struct ufs_hba *hba)
{
	return hba->caps & UFSHCD_CAP_KEEP_AUTO_BKOPS_ENABLED_EXCEPT_SUSPEND;
}

extern int ufshcd_runtime_suspend(struct ufs_hba *hba);
extern int ufshcd_runtime_resume(struct ufs_hba *hba);
extern int ufshcd_runtime_idle(struct ufs_hba *hba);
extern int ufshcd_system_suspend(struct ufs_hba *hba);
extern int ufshcd_system_resume(struct ufs_hba *hba);
extern int ufshcd_shutdown(struct ufs_hba *hba);
extern int ufshcd_dme_set_attr(struct ufs_hba *hba, u32 attr_sel,
			       u8 attr_set, u32 mib_val, u8 peer);
extern int ufshcd_dme_get_attr(struct ufs_hba *hba, u32 attr_sel,
			       u32 *mib_val, u8 peer);
extern int ufshcd_config_pwr_mode(struct ufs_hba *hba,
			struct ufs_pa_layer_attr *desired_pwr_mode);

/* UIC command interfaces for DME primitives */
#define DME_LOCAL	0
#define DME_PEER	1
#define ATTR_SET_NOR	0	/* NORMAL */
#define ATTR_SET_ST	1	/* STATIC */

static inline int ufshcd_dme_set(struct ufs_hba *hba, u32 attr_sel,
				 u32 mib_val)
{
	return ufshcd_dme_set_attr(hba, attr_sel, ATTR_SET_NOR,
				   mib_val, DME_LOCAL);
}

static inline int ufshcd_dme_st_set(struct ufs_hba *hba, u32 attr_sel,
				    u32 mib_val)
{
	return ufshcd_dme_set_attr(hba, attr_sel, ATTR_SET_ST,
				   mib_val, DME_LOCAL);
}

static inline int ufshcd_dme_peer_set(struct ufs_hba *hba, u32 attr_sel,
				      u32 mib_val)
{
	return ufshcd_dme_set_attr(hba, attr_sel, ATTR_SET_NOR,
				   mib_val, DME_PEER);
}

static inline int ufshcd_dme_peer_st_set(struct ufs_hba *hba, u32 attr_sel,
					 u32 mib_val)
{
	return ufshcd_dme_set_attr(hba, attr_sel, ATTR_SET_ST,
				   mib_val, DME_PEER);
}

static inline int ufshcd_dme_get(struct ufs_hba *hba,
				 u32 attr_sel, u32 *mib_val)
{
	return ufshcd_dme_get_attr(hba, attr_sel, mib_val, DME_LOCAL);
}

static inline int ufshcd_dme_peer_get(struct ufs_hba *hba,
				      u32 attr_sel, u32 *mib_val)
{
	return ufshcd_dme_get_attr(hba, attr_sel, mib_val, DME_PEER);
}

static inline bool ufshcd_is_hs_mode(struct ufs_pa_layer_attr *pwr_info)
{
	return (pwr_info->pwr_rx == FAST_MODE ||
		pwr_info->pwr_rx == FASTAUTO_MODE) &&
		(pwr_info->pwr_tx == FAST_MODE ||
		pwr_info->pwr_tx == FASTAUTO_MODE);
}

/* Expose Query-Request API */
int ufshcd_query_descriptor_retry(struct ufs_hba *hba,
				  enum query_opcode opcode,
				  enum desc_idn idn, u8 index,
				  u8 selector,
				  u8 *desc_buf, int *buf_len);
int ufshcd_read_desc_param(struct ufs_hba *hba,
			   enum desc_idn desc_id,
			   int desc_index,
			   u8 param_offset,
			   u8 *param_read_buf,
			   u8 param_size);
int ufshcd_query_attr(struct ufs_hba *hba, enum query_opcode opcode,
		      enum attr_idn idn, u8 index, u8 selector, u32 *attr_val);
int ufshcd_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
	enum flag_idn idn, bool *flag_res);

#define SD_ASCII_STD true
#define SD_RAW false
int ufshcd_read_string_desc(struct ufs_hba *hba, u8 desc_index,
			    u8 **buf, bool ascii);

int ufshcd_hold(struct ufs_hba *hba, bool async);
void ufshcd_release(struct ufs_hba *hba);

int ufshcd_map_desc_id_to_length(struct ufs_hba *hba, enum desc_idn desc_id,
	int *desc_length);

u32 ufshcd_get_local_unipro_ver(struct ufs_hba *hba);

int ufshcd_send_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd);

int ufshcd_exec_raw_upiu_cmd(struct ufs_hba *hba,
			     struct utp_upiu_req *req_upiu,
			     struct utp_upiu_req *rsp_upiu,
			     int msgcode,
			     u8 *desc_buff, int *buff_len,
			     enum query_opcode desc_op);

/* Wrapper functions for safely calling variant operations */
static inline const char *ufshcd_get_var_name(struct ufs_hba *hba)
{
	if (hba->vops)
		return hba->vops->name;
	return "";
}

static inline int ufshcd_vops_init(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->init)
		return hba->vops->init(hba);

	return 0;
}

static inline void ufshcd_vops_exit(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->exit)
		return hba->vops->exit(hba);
}

static inline u32 ufshcd_vops_get_ufs_hci_version(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->get_ufs_hci_version)
		return hba->vops->get_ufs_hci_version(hba);

	return ufshcd_readl(hba, REG_UFS_VERSION);
}

static inline int ufshcd_vops_clk_scale_notify(struct ufs_hba *hba,
			bool up, enum ufs_notify_change_status status)
{
	if (hba->vops && hba->vops->clk_scale_notify)
		return hba->vops->clk_scale_notify(hba, up, status);
	return 0;
}

static inline int ufshcd_vops_setup_clocks(struct ufs_hba *hba, bool on,
					enum ufs_notify_change_status status)
{
	if (hba->vops && hba->vops->setup_clocks)
		return hba->vops->setup_clocks(hba, on, status);
	return 0;
}

static inline int ufshcd_vops_setup_regulators(struct ufs_hba *hba, bool status)
{
	if (hba->vops && hba->vops->setup_regulators)
		return hba->vops->setup_regulators(hba, status);

	return 0;
}

static inline int ufshcd_vops_hce_enable_notify(struct ufs_hba *hba,
						bool status)
{
	if (hba->vops && hba->vops->hce_enable_notify)
		return hba->vops->hce_enable_notify(hba, status);

	return 0;
}
static inline int ufshcd_vops_link_startup_notify(struct ufs_hba *hba,
						bool status)
{
	if (hba->vops && hba->vops->link_startup_notify)
		return hba->vops->link_startup_notify(hba, status);

	return 0;
}

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

static inline void ufshcd_vops_setup_xfer_req(struct ufs_hba *hba, int tag,
					bool is_scsi_cmd)
{
	if (hba->vops && hba->vops->setup_xfer_req)                                          // 若hba->vops && hba->vops->setup_xfer_req为true
		return hba->vops->setup_xfer_req(hba, tag, is_scsi_cmd);                         // 调用hba->vops->setup_xfer_req
}

static inline void ufshcd_vops_setup_task_mgmt(struct ufs_hba *hba,
					int tag, u8 tm_function)
{
	if (hba->vops && hba->vops->setup_task_mgmt)
		return hba->vops->setup_task_mgmt(hba, tag, tm_function);
}

static inline void ufshcd_vops_hibern8_notify(struct ufs_hba *hba,
					enum uic_cmd_dme cmd,
					enum ufs_notify_change_status status)
{
	if (hba->vops && hba->vops->hibern8_notify)
		return hba->vops->hibern8_notify(hba, cmd, status);
}

static inline int ufshcd_vops_apply_dev_quirks(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->apply_dev_quirks)
		return hba->vops->apply_dev_quirks(hba);
	return 0;
}

static inline int ufshcd_vops_suspend(struct ufs_hba *hba, enum ufs_pm_op op)
{
	if (hba->vops && hba->vops->suspend)
		return hba->vops->suspend(hba, op);

	return 0;
}

static inline int ufshcd_vops_resume(struct ufs_hba *hba, enum ufs_pm_op op)
{
	if (hba->vops && hba->vops->resume)
		return hba->vops->resume(hba, op);

	return 0;
}

static inline void ufshcd_vops_dbg_register_dump(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->dbg_register_dump)
		hba->vops->dbg_register_dump(hba);
}

static inline void ufshcd_vops_device_reset(struct ufs_hba *hba)
{
	if (hba->vops && hba->vops->device_reset)
		hba->vops->device_reset(hba);
}

extern struct ufs_pm_lvl_states ufs_pm_lvl_states[];

/*
 * ufshcd_scsi_to_upiu_lun - maps scsi LUN to UPIU LUN
 * @scsi_lun: scsi LUN id
 *
 * Returns UPIU LUN id
 */
static inline u8 ufshcd_scsi_to_upiu_lun(unsigned int scsi_lun)
{
	if (scsi_is_wlun(scsi_lun))
		return (scsi_lun & UFS_UPIU_MAX_UNIT_NUM_ID)
			| UFS_UPIU_WLUN_ID;
	else
		return scsi_lun & UFS_UPIU_MAX_UNIT_NUM_ID;
}

int ufshcd_dump_regs(struct ufs_hba *hba, size_t offset, size_t len,
		     const char *prefix);

#endif /* End of Header */


/*
 * Universal Flash Storage Host controller driver Core
 *
 * This code is based on drivers/scsi/ufs/ufshcd.c
 * Copyright (C) 2011-2013 Samsung India Software Operations
 * Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
 *
 * Authors:
 *	Santosh Yaraganavi <santosh.sy@samsung.com>
 *	Vinayak Holikatti <h.vinayak@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 *
 * The Linux Foundation chooses to take subject only to the GPLv2
 * license terms, and distributes only under these terms.
 */

#include <linux/async.h>
#include <linux/devfreq.h>
#include <linux/nls.h>
#include <linux/of.h>
#include <linux/bitfield.h>
#include "ufshcd.h"
#include "ufs_quirks.h"
#include "unipro.h"
#include "ufs-sysfs.h"
#include "ufs_bsg.h"

#define CREATE_TRACE_POINTS
#include <trace/events/ufs.h>

#define UFSHCD_ENABLE_INTRS	(UTP_TRANSFER_REQ_COMPL |\
				 UTP_TASK_REQ_COMPL |\
				 UFSHCD_ERROR_MASK)
/* UIC command timeout, unit: ms */
#define UIC_CMD_TIMEOUT	500

/* NOP OUT retries waiting for NOP IN response */
#define NOP_OUT_RETRIES    10
/* Timeout after 30 msecs if NOP OUT hangs without response */
#define NOP_OUT_TIMEOUT    30 /* msecs */

/* Query request retries */
#define QUERY_REQ_RETRIES 3
/* Query request timeout */
#define QUERY_REQ_TIMEOUT 1500 /* 1.5 seconds */

/* Task management command timeout */
#define TM_CMD_TIMEOUT	100 /* msecs */

/* maximum number of retries for a general UIC command  */
#define UFS_UIC_COMMAND_RETRIES 3

/* maximum number of link-startup retries */
#define DME_LINKSTARTUP_RETRIES 3

/* Maximum retries for Hibern8 enter */
#define UIC_HIBERN8_ENTER_RETRIES 3

/* maximum number of reset retries before giving up */
#define MAX_HOST_RESET_RETRIES 5

/* Expose the flag value from utp_upiu_query.value */
#define MASK_QUERY_UPIU_FLAG_LOC 0xFF

/* Interrupt aggregation default timeout, unit: 40us */
#define INT_AGGR_DEF_TO	0x02

#define ufshcd_toggle_vreg(_dev, _vreg, _on)				\
	({                                                              \
		int _ret;                                               \
		if (_on)                                                \
			_ret = ufshcd_enable_vreg(_dev, _vreg);         \
		else                                                    \
			_ret = ufshcd_disable_vreg(_dev, _vreg);        \
		_ret;                                                   \
	})

#define ufshcd_hex_dump(prefix_str, buf, len) do {                       \
	size_t __len = (len);                                            \
	print_hex_dump(KERN_ERR, prefix_str,                             \
		       __len > 4 ? DUMP_PREFIX_OFFSET : DUMP_PREFIX_NONE,\
		       16, 4, buf, __len, false);                        \
} while (0)

int ufshcd_dump_regs(struct ufs_hba *hba, size_t offset, size_t len,
		     const char *prefix)
{
	u32 *regs;
	size_t pos;

	if (offset % 4 != 0 || len % 4 != 0) /* keep readl happy */
		return -EINVAL;

	regs = kzalloc(len, GFP_KERNEL);
	if (!regs)
		return -ENOMEM;

	for (pos = 0; pos < len; pos += 4)
		regs[pos / 4] = ufshcd_readl(hba, offset + pos);

	ufshcd_hex_dump(prefix, regs, len);
	kfree(regs);

	return 0;
}
EXPORT_SYMBOL_GPL(ufshcd_dump_regs);

enum {
	UFSHCD_MAX_CHANNEL	= 0,
	UFSHCD_MAX_ID		= 1,
	UFSHCD_CMD_PER_LUN	= 32,
	UFSHCD_CAN_QUEUE	= 32,
};

/* UFSHCD states */
enum {
	UFSHCD_STATE_RESET,
	UFSHCD_STATE_ERROR,
	UFSHCD_STATE_OPERATIONAL,
	UFSHCD_STATE_EH_SCHEDULED,
};

/* UFSHCD error handling flags */
enum {
	UFSHCD_EH_IN_PROGRESS = (1 << 0),
};

/* UFSHCD UIC layer error flags */
enum {
	UFSHCD_UIC_DL_PA_INIT_ERROR = (1 << 0), /* Data link layer error */
	UFSHCD_UIC_DL_NAC_RECEIVED_ERROR = (1 << 1), /* Data link layer error */
	UFSHCD_UIC_DL_TCx_REPLAY_ERROR = (1 << 2), /* Data link layer error */
	UFSHCD_UIC_NL_ERROR = (1 << 3), /* Network layer error */
	UFSHCD_UIC_TL_ERROR = (1 << 4), /* Transport Layer error */
	UFSHCD_UIC_DME_ERROR = (1 << 5), /* DME error */
};

#define ufshcd_set_eh_in_progress(h) \
	((h)->eh_flags |= UFSHCD_EH_IN_PROGRESS)
#define ufshcd_eh_in_progress(h) \
	((h)->eh_flags & UFSHCD_EH_IN_PROGRESS)
#define ufshcd_clear_eh_in_progress(h) \
	((h)->eh_flags &= ~UFSHCD_EH_IN_PROGRESS)

#define ufshcd_set_ufs_dev_active(h) \
	((h)->curr_dev_pwr_mode = UFS_ACTIVE_PWR_MODE)
#define ufshcd_set_ufs_dev_sleep(h) \
	((h)->curr_dev_pwr_mode = UFS_SLEEP_PWR_MODE)
#define ufshcd_set_ufs_dev_poweroff(h) \
	((h)->curr_dev_pwr_mode = UFS_POWERDOWN_PWR_MODE)
#define ufshcd_is_ufs_dev_active(h) \
	((h)->curr_dev_pwr_mode == UFS_ACTIVE_PWR_MODE)
#define ufshcd_is_ufs_dev_sleep(h) \
	((h)->curr_dev_pwr_mode == UFS_SLEEP_PWR_MODE)
#define ufshcd_is_ufs_dev_poweroff(h) \
	((h)->curr_dev_pwr_mode == UFS_POWERDOWN_PWR_MODE)

struct ufs_pm_lvl_states ufs_pm_lvl_states[] = {
	{UFS_ACTIVE_PWR_MODE, UIC_LINK_ACTIVE_STATE},
	{UFS_ACTIVE_PWR_MODE, UIC_LINK_HIBERN8_STATE},
	{UFS_SLEEP_PWR_MODE, UIC_LINK_ACTIVE_STATE},
	{UFS_SLEEP_PWR_MODE, UIC_LINK_HIBERN8_STATE},
	{UFS_POWERDOWN_PWR_MODE, UIC_LINK_HIBERN8_STATE},
	{UFS_POWERDOWN_PWR_MODE, UIC_LINK_OFF_STATE},
};

static inline enum ufs_dev_pwr_mode
ufs_get_pm_lvl_to_dev_pwr_mode(enum ufs_pm_level lvl)
{
	return ufs_pm_lvl_states[lvl].dev_state;
}

static inline enum uic_link_state
ufs_get_pm_lvl_to_link_pwr_state(enum ufs_pm_level lvl)
{
	return ufs_pm_lvl_states[lvl].link_state;
}

static inline enum ufs_pm_level
ufs_get_desired_pm_lvl_for_dev_link_state(enum ufs_dev_pwr_mode dev_state,
					enum uic_link_state link_state)
{
	enum ufs_pm_level lvl;

	for (lvl = UFS_PM_LVL_0; lvl < UFS_PM_LVL_MAX; lvl++) {
		if ((ufs_pm_lvl_states[lvl].dev_state == dev_state) &&
			(ufs_pm_lvl_states[lvl].link_state == link_state))
			return lvl;
	}

	/* if no match found, return the level 0 */
	return UFS_PM_LVL_0;
}

static struct ufs_dev_fix ufs_fixups[] = {
	/* UFS cards deviations table */
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_TOSHIBA, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G9C8KBADG",
		UFS_DEVICE_QUIRK_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G9D8KBADG",
		UFS_DEVICE_QUIRK_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_SKHYNIX, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_SAVECONFIGTIME),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hB8aL1" /*H28U62301AMR*/,
		UFS_DEVICE_QUIRK_HOST_VS_DEBUGSAVECONFIGTIME),

	END_FIX
};

static void ufshcd_tmc_handler(struct ufs_hba *hba);
static void ufshcd_async_scan(void *data, async_cookie_t cookie);
static int ufshcd_reset_and_restore(struct ufs_hba *hba);
static int ufshcd_eh_host_reset_handler(struct scsi_cmnd *cmd);
static int ufshcd_clear_tm_cmd(struct ufs_hba *hba, int tag);
static void ufshcd_hba_exit(struct ufs_hba *hba);
static int ufshcd_probe_hba(struct ufs_hba *hba);
static int __ufshcd_setup_clocks(struct ufs_hba *hba, bool on,
				 bool skip_ref_clk);
static int ufshcd_setup_clocks(struct ufs_hba *hba, bool on);
static int ufshcd_uic_hibern8_exit(struct ufs_hba *hba);
static int ufshcd_uic_hibern8_enter(struct ufs_hba *hba);
static inline void ufshcd_add_delay_before_dme_cmd(struct ufs_hba *hba);
static int ufshcd_host_reset_and_restore(struct ufs_hba *hba);
static void ufshcd_resume_clkscaling(struct ufs_hba *hba);
static void ufshcd_suspend_clkscaling(struct ufs_hba *hba);
static void __ufshcd_suspend_clkscaling(struct ufs_hba *hba);
static int ufshcd_scale_clks(struct ufs_hba *hba, bool scale_up);
static irqreturn_t ufshcd_intr(int irq, void *__hba);
static int ufshcd_change_power_mode(struct ufs_hba *hba,
			     struct ufs_pa_layer_attr *pwr_mode);
static inline bool ufshcd_valid_tag(struct ufs_hba *hba, int tag)
{
	return tag >= 0 && tag < hba->nutrs;
}

static inline int ufshcd_enable_irq(struct ufs_hba *hba)
{
	int ret = 0;

	if (!hba->is_irq_enabled) {
		ret = request_irq(hba->irq, ufshcd_intr, IRQF_SHARED, UFSHCD,
				hba);
		if (ret)
			dev_err(hba->dev, "%s: request_irq failed, ret=%d\n",
				__func__, ret);
		hba->is_irq_enabled = true;
	}

	return ret;
}

static inline void ufshcd_disable_irq(struct ufs_hba *hba)
{
	if (hba->is_irq_enabled) {
		free_irq(hba->irq, hba);
		hba->is_irq_enabled = false;
	}
}

static void ufshcd_scsi_unblock_requests(struct ufs_hba *hba)
{
	if (atomic_dec_and_test(&hba->scsi_block_reqs_cnt))
		scsi_unblock_requests(hba->host);
}

static void ufshcd_scsi_block_requests(struct ufs_hba *hba)
{
	if (atomic_inc_return(&hba->scsi_block_reqs_cnt) == 1)
		scsi_block_requests(hba->host);
}

static void ufshcd_add_cmd_upiu_trace(struct ufs_hba *hba, unsigned int tag,
		const char *str)
{
	struct utp_upiu_req *rq = hba->lrb[tag].ucd_req_ptr;

	trace_ufshcd_upiu(dev_name(hba->dev), str, &rq->header, &rq->sc.cdb);
}

static void ufshcd_add_query_upiu_trace(struct ufs_hba *hba, unsigned int tag,
		const char *str)
{
	struct utp_upiu_req *rq = hba->lrb[tag].ucd_req_ptr;

	trace_ufshcd_upiu(dev_name(hba->dev), str, &rq->header, &rq->qr);
}

static void ufshcd_add_tm_upiu_trace(struct ufs_hba *hba, unsigned int tag,
		const char *str)
{
	int off = (int)tag - hba->nutrs;
	struct utp_task_req_desc *descp = &hba->utmrdl_base_addr[off];

	trace_ufshcd_upiu(dev_name(hba->dev), str, &descp->req_header,
			&descp->input_param1);
}

static void ufshcd_add_command_trace(struct ufs_hba *hba,
		unsigned int tag, const char *str)
{
	sector_t lba = -1;
	u8 opcode = 0;
	u32 intr, doorbell;
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	int transfer_len = -1;

	if (!trace_ufshcd_command_enabled()) {
		/* trace UPIU W/O tracing command */
		if (lrbp->cmd)
			ufshcd_add_cmd_upiu_trace(hba, tag, str);
		return;
	}

	if (lrbp->cmd) { /* data phase exists */
		/* trace UPIU also */
		ufshcd_add_cmd_upiu_trace(hba, tag, str);
		opcode = (u8)(*lrbp->cmd->cmnd);
		if ((opcode == READ_10) || (opcode == WRITE_10)) {
			/*
			 * Currently we only fully trace read(10) and write(10)
			 * commands
			 */
			if (lrbp->cmd->request && lrbp->cmd->request->bio)
				lba =
				  lrbp->cmd->request->bio->bi_iter.bi_sector;
			transfer_len = be32_to_cpu(
				lrbp->ucd_req_ptr->sc.exp_data_transfer_len);
		}
	}

	intr = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
	doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	trace_ufshcd_command(dev_name(hba->dev), str, tag,
				doorbell, transfer_len, intr, lba, opcode);
}

static void ufshcd_print_clk_freqs(struct ufs_hba *hba)
{
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;

	if (list_empty(head))
		return;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk) && clki->min_freq &&
				clki->max_freq)
			dev_err(hba->dev, "clk: %s, rate: %u\n",
					clki->name, clki->curr_freq);
	}
}

static void ufshcd_print_err_hist(struct ufs_hba *hba,
				  struct ufs_err_reg_hist *err_hist,
				  char *err_name)
{
	int i;
	bool found = false;

	for (i = 0; i < UFS_ERR_REG_HIST_LENGTH; i++) {
		int p = (i + err_hist->pos) % UFS_ERR_REG_HIST_LENGTH;

		if (err_hist->reg[p] == 0)
			continue;
		dev_err(hba->dev, "%s[%d] = 0x%x at %lld us\n", err_name, p,
			err_hist->reg[p], ktime_to_us(err_hist->tstamp[p]));
		found = true;
	}

	if (!found)
		dev_err(hba->dev, "No record of %s errors\n", err_name);
}

static void ufshcd_print_host_regs(struct ufs_hba *hba)
{
	ufshcd_dump_regs(hba, 0, UFSHCI_REG_SPACE_SIZE, "host_regs: ");
	dev_err(hba->dev, "hba->ufs_version = 0x%x, hba->capabilities = 0x%x\n",
		hba->ufs_version, hba->capabilities);
	dev_err(hba->dev,
		"hba->outstanding_reqs = 0x%x, hba->outstanding_tasks = 0x%x\n",
		(u32)hba->outstanding_reqs, (u32)hba->outstanding_tasks);
	dev_err(hba->dev,
		"last_hibern8_exit_tstamp at %lld us, hibern8_exit_cnt = %d\n",
		ktime_to_us(hba->ufs_stats.last_hibern8_exit_tstamp),
		hba->ufs_stats.hibern8_exit_cnt);

	ufshcd_print_err_hist(hba, &hba->ufs_stats.pa_err, "pa_err");
	ufshcd_print_err_hist(hba, &hba->ufs_stats.dl_err, "dl_err");
	ufshcd_print_err_hist(hba, &hba->ufs_stats.nl_err, "nl_err");
	ufshcd_print_err_hist(hba, &hba->ufs_stats.tl_err, "tl_err");
	ufshcd_print_err_hist(hba, &hba->ufs_stats.dme_err, "dme_err");
	ufshcd_print_err_hist(hba, &hba->ufs_stats.auto_hibern8_err,
			      "auto_hibern8_err");
	ufshcd_print_err_hist(hba, &hba->ufs_stats.fatal_err, "fatal_err");
	ufshcd_print_err_hist(hba, &hba->ufs_stats.link_startup_err,
			      "link_startup_fail");
	ufshcd_print_err_hist(hba, &hba->ufs_stats.resume_err, "resume_fail");
	ufshcd_print_err_hist(hba, &hba->ufs_stats.suspend_err,
			      "suspend_fail");
	ufshcd_print_err_hist(hba, &hba->ufs_stats.dev_reset, "dev_reset");
	ufshcd_print_err_hist(hba, &hba->ufs_stats.host_reset, "host_reset");
	ufshcd_print_err_hist(hba, &hba->ufs_stats.task_abort, "task_abort");

	ufshcd_print_clk_freqs(hba);

	if (hba->vops && hba->vops->dbg_register_dump)
		hba->vops->dbg_register_dump(hba);
}

static
void ufshcd_print_trs(struct ufs_hba *hba, unsigned long bitmap, bool pr_prdt)
{
	struct ufshcd_lrb *lrbp;
	int prdt_length;
	int tag;

	for_each_set_bit(tag, &bitmap, hba->nutrs) {
		lrbp = &hba->lrb[tag];

		dev_err(hba->dev, "UPIU[%d] - issue time %lld us\n",
				tag, ktime_to_us(lrbp->issue_time_stamp));
		dev_err(hba->dev, "UPIU[%d] - complete time %lld us\n",
				tag, ktime_to_us(lrbp->compl_time_stamp));
		dev_err(hba->dev,
			"UPIU[%d] - Transfer Request Descriptor phys@0x%llx\n",
			tag, (u64)lrbp->utrd_dma_addr);

		ufshcd_hex_dump("UPIU TRD: ", lrbp->utr_descriptor_ptr,
				sizeof(struct utp_transfer_req_desc));
		dev_err(hba->dev, "UPIU[%d] - Request UPIU phys@0x%llx\n", tag,
			(u64)lrbp->ucd_req_dma_addr);
		ufshcd_hex_dump("UPIU REQ: ", lrbp->ucd_req_ptr,
				sizeof(struct utp_upiu_req));
		dev_err(hba->dev, "UPIU[%d] - Response UPIU phys@0x%llx\n", tag,
			(u64)lrbp->ucd_rsp_dma_addr);
		ufshcd_hex_dump("UPIU RSP: ", lrbp->ucd_rsp_ptr,
				sizeof(struct utp_upiu_rsp));

		prdt_length = le16_to_cpu(
			lrbp->utr_descriptor_ptr->prd_table_length);
		dev_err(hba->dev,
			"UPIU[%d] - PRDT - %d entries  phys@0x%llx\n",
			tag, prdt_length,
			(u64)lrbp->ucd_prdt_dma_addr);

		if (pr_prdt)
			ufshcd_hex_dump("UPIU PRDT: ", lrbp->ucd_prdt_ptr,
				sizeof(struct ufshcd_sg_entry) * prdt_length);
	}
}

static void ufshcd_print_tmrs(struct ufs_hba *hba, unsigned long bitmap)
{
	int tag;

	for_each_set_bit(tag, &bitmap, hba->nutmrs) {
		struct utp_task_req_desc *tmrdp = &hba->utmrdl_base_addr[tag];

		dev_err(hba->dev, "TM[%d] - Task Management Header\n", tag);
		ufshcd_hex_dump("", tmrdp, sizeof(*tmrdp));
	}
}

static void ufshcd_print_host_state(struct ufs_hba *hba)
{
	dev_err(hba->dev, "UFS Host state=%d\n", hba->ufshcd_state);
	dev_err(hba->dev, "lrb in use=0x%lx, outstanding reqs=0x%lx tasks=0x%lx\n",
		hba->lrb_in_use, hba->outstanding_reqs, hba->outstanding_tasks);
	dev_err(hba->dev, "saved_err=0x%x, saved_uic_err=0x%x\n",
		hba->saved_err, hba->saved_uic_err);
	dev_err(hba->dev, "Device power mode=%d, UIC link state=%d\n",
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	dev_err(hba->dev, "PM in progress=%d, sys. suspended=%d\n",
		hba->pm_op_in_progress, hba->is_sys_suspended);
	dev_err(hba->dev, "Auto BKOPS=%d, Host self-block=%d\n",
		hba->auto_bkops_enabled, hba->host->host_self_blocked);
	dev_err(hba->dev, "Clk gate=%d\n", hba->clk_gating.state);
	dev_err(hba->dev, "error handling flags=0x%x, req. abort count=%d\n",
		hba->eh_flags, hba->req_abort_count);
	dev_err(hba->dev, "Host capabilities=0x%x, caps=0x%x\n",
		hba->capabilities, hba->caps);
	dev_err(hba->dev, "quirks=0x%x, dev. quirks=0x%x\n", hba->quirks,
		hba->dev_quirks);
}

/**
 * ufshcd_print_pwr_info - print power params as saved in hba
 * power info
 * @hba: per-adapter instance
 */
static void ufshcd_print_pwr_info(struct ufs_hba *hba)
{
	static const char * const names[] = {
		"INVALID MODE",
		"FAST MODE",
		"SLOW_MODE",
		"INVALID MODE",
		"FASTAUTO_MODE",
		"SLOWAUTO_MODE",
		"INVALID MODE",
	};

	dev_err(hba->dev, "%s:[RX, TX]: gear=[%d, %d], lane[%d, %d], pwr[%s, %s], rate = %d\n",
		 __func__,
		 hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
		 hba->pwr_info.lane_rx, hba->pwr_info.lane_tx,
		 names[hba->pwr_info.pwr_rx],
		 names[hba->pwr_info.pwr_tx],
		 hba->pwr_info.hs_rate);
}

/*
 * ufshcd_wait_for_register - wait for register value to change
 * @hba - per-adapter interface
 * @reg - mmio register offset
 * @mask - mask to apply to read register value
 * @val - wait condition
 * @interval_us - polling interval in microsecs
 * @timeout_ms - timeout in millisecs
 * @can_sleep - perform sleep or just spin
 *
 * Returns -ETIMEDOUT on error, zero on success
 */
int ufshcd_wait_for_register(struct ufs_hba *hba, u32 reg, u32 mask,
				u32 val, unsigned long interval_us,
				unsigned long timeout_ms, bool can_sleep)
{
	int err = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);

	/* ignore bits that we don't intend to wait on */
	val = val & mask;

	while ((ufshcd_readl(hba, reg) & mask) != val) {
		if (can_sleep)
			usleep_range(interval_us, interval_us + 50);
		else
			udelay(interval_us);
		if (time_after(jiffies, timeout)) {
			if ((ufshcd_readl(hba, reg) & mask) != val)
				err = -ETIMEDOUT;
			break;
		}
	}

	return err;
}

/**
 * ufshcd_get_intr_mask - Get the interrupt bit mask
 * @hba: Pointer to adapter instance
 *
 * Returns interrupt bit mask per version
 */
static inline u32 ufshcd_get_intr_mask(struct ufs_hba *hba)
{
	u32 intr_mask = 0;

	switch (hba->ufs_version) {
	case UFSHCI_VERSION_10:
		intr_mask = INTERRUPT_MASK_ALL_VER_10;
		break;
	case UFSHCI_VERSION_11:
	case UFSHCI_VERSION_20:
		intr_mask = INTERRUPT_MASK_ALL_VER_11;
		break;
	case UFSHCI_VERSION_21:
	default:
		intr_mask = INTERRUPT_MASK_ALL_VER_21;
		break;
	}

	return intr_mask;
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
 * ufshcd_is_device_present - Check if any device connected to
 *			      the host controller
 * @hba: pointer to adapter instance
 *
 * Returns true if device present, false if no device detected
 */
static inline bool ufshcd_is_device_present(struct ufs_hba *hba)
{
	return (ufshcd_readl(hba, REG_CONTROLLER_STATUS) &
						DEVICE_PRESENT) ? true : false;
}

/**
 * ufshcd_get_tr_ocs - Get the UTRD Overall Command Status
 * @lrbp: pointer to local command reference block
 *
 * This function is used to get the OCS field from UTRD
 * Returns the OCS field in the UTRD
 */
static inline int ufshcd_get_tr_ocs(struct ufshcd_lrb *lrbp)
{
	return le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS;
}

/**
 * ufshcd_get_tm_free_slot - get a free slot for task management request
 * @hba: per adapter instance
 * @free_slot: pointer to variable with available slot value
 *
 * Get a free tag and lock it until ufshcd_put_tm_slot() is called.
 * Returns 0 if free slot is not available, else return 1 with tag value
 * in @free_slot.
 */
static bool ufshcd_get_tm_free_slot(struct ufs_hba *hba, int *free_slot)
{
	int tag;
	bool ret = false;

	if (!free_slot)
		goto out;

	do {
		tag = find_first_zero_bit(&hba->tm_slots_in_use, hba->nutmrs);
		if (tag >= hba->nutmrs)
			goto out;
	} while (test_and_set_bit_lock(tag, &hba->tm_slots_in_use));

	*free_slot = tag;
	ret = true;
out:
	return ret;
}

static inline void ufshcd_put_tm_slot(struct ufs_hba *hba, int slot)
{
	clear_bit_unlock(slot, &hba->tm_slots_in_use);
}

/**
 * ufshcd_utrl_clear - Clear a bit in UTRLCLR register
 * @hba: per adapter instance
 * @pos: position of the bit to be cleared
 */
static inline void ufshcd_utrl_clear(struct ufs_hba *hba, u32 pos)
{
	if (hba->quirks & UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR)
		ufshcd_writel(hba, (1 << pos), REG_UTP_TRANSFER_REQ_LIST_CLEAR);
	else
		ufshcd_writel(hba, ~(1 << pos),
				REG_UTP_TRANSFER_REQ_LIST_CLEAR);
}

/**
 * ufshcd_utmrl_clear - Clear a bit in UTRMLCLR register
 * @hba: per adapter instance
 * @pos: position of the bit to be cleared
 */
static inline void ufshcd_utmrl_clear(struct ufs_hba *hba, u32 pos)
{
	if (hba->quirks & UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR)
		ufshcd_writel(hba, (1 << pos), REG_UTP_TASK_REQ_LIST_CLEAR);
	else
		ufshcd_writel(hba, ~(1 << pos), REG_UTP_TASK_REQ_LIST_CLEAR);
}

/**
 * ufshcd_outstanding_req_clear - Clear a bit in outstanding request field
 * @hba: per adapter instance
 * @tag: position of the bit to be cleared
 */
static inline void ufshcd_outstanding_req_clear(struct ufs_hba *hba, int tag)
{
	__clear_bit(tag, &hba->outstanding_reqs);
}

/**
 * ufshcd_get_lists_status - Check UCRDY, UTRLRDY and UTMRLRDY
 * @reg: Register value of host controller status
 *
 * Returns integer, 0 on Success and positive value if failed
 */
static inline int ufshcd_get_lists_status(u32 reg)
{
	return !((reg & UFSHCD_STATUS_READY) == UFSHCD_STATUS_READY);
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
	       MASK_UIC_COMMAND_RESULT;
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
	return ufshcd_readl(hba, REG_UIC_COMMAND_ARG_3);
}

/**
 * ufshcd_get_req_rsp - returns the TR response transaction type
 * @ucd_rsp_ptr: pointer to response UPIU
 */
static inline int
ufshcd_get_req_rsp(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return be32_to_cpu(ucd_rsp_ptr->header.dword_0) >> 24;
}

/**
 * ufshcd_get_rsp_upiu_result - Get the result from response UPIU
 * @ucd_rsp_ptr: pointer to response UPIU
 *
 * This function gets the response status and scsi_status from response UPIU
 * Returns the response result code.
 */
static inline int
ufshcd_get_rsp_upiu_result(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return be32_to_cpu(ucd_rsp_ptr->header.dword_1) & MASK_RSP_UPIU_RESULT;
}

/*
 * ufshcd_get_rsp_upiu_data_seg_len - Get the data segment length
 *				from response UPIU
 * @ucd_rsp_ptr: pointer to response UPIU
 *
 * Return the data segment length.
 */
static inline unsigned int
ufshcd_get_rsp_upiu_data_seg_len(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return be32_to_cpu(ucd_rsp_ptr->header.dword_2) &
		MASK_RSP_UPIU_DATA_SEG_LEN;
}

/**
 * ufshcd_is_exception_event - Check if the device raised an exception event
 * @ucd_rsp_ptr: pointer to response UPIU
 *
 * The function checks if the device raised an exception event indicated in
 * the Device Information field of response UPIU.
 *
 * Returns true if exception is raised, false otherwise.
 */
static inline bool ufshcd_is_exception_event(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return be32_to_cpu(ucd_rsp_ptr->header.dword_2) &
			MASK_RSP_EXCEPTION_EVENT ? true : false;
}

/**
 * ufshcd_reset_intr_aggr - Reset interrupt aggregation values.
 * @hba: per adapter instance
 */
static inline void
ufshcd_reset_intr_aggr(struct ufs_hba *hba)
{
	ufshcd_writel(hba, INT_AGGR_ENABLE |
		      INT_AGGR_COUNTER_AND_TIMER_RESET,
		      REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL);
}

/**
 * ufshcd_config_intr_aggr - Configure interrupt aggregation values.
 * @hba: per adapter instance
 * @cnt: Interrupt aggregation counter threshold
 * @tmout: Interrupt aggregation timeout value
 */
static inline void
ufshcd_config_intr_aggr(struct ufs_hba *hba, u8 cnt, u8 tmout)
{
	ufshcd_writel(hba, INT_AGGR_ENABLE | INT_AGGR_PARAM_WRITE |
		      INT_AGGR_COUNTER_THLD_VAL(cnt) |
		      INT_AGGR_TIMEOUT_VAL(tmout),
		      REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL);
}

/**
 * ufshcd_disable_intr_aggr - Disables interrupt aggregation.
 * @hba: per adapter instance
 */
static inline void ufshcd_disable_intr_aggr(struct ufs_hba *hba)
{
	ufshcd_writel(hba, 0, REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL);
}

/**
 * ufshcd_enable_run_stop_reg - Enable run-stop registers,
 *			When run-stop registers are set to 1, it indicates the
 *			host controller that it can process the requests
 * @hba: per adapter instance
 */
static void ufshcd_enable_run_stop_reg(struct ufs_hba *hba)
{
	ufshcd_writel(hba, UTP_TASK_REQ_LIST_RUN_STOP_BIT,
		      REG_UTP_TASK_REQ_LIST_RUN_STOP);
	ufshcd_writel(hba, UTP_TRANSFER_REQ_LIST_RUN_STOP_BIT,
		      REG_UTP_TRANSFER_REQ_LIST_RUN_STOP);
}

/**
 * ufshcd_hba_start - Start controller initialization sequence
 * @hba: per adapter instance
 */
static inline void ufshcd_hba_start(struct ufs_hba *hba)
{
	ufshcd_writel(hba, CONTROLLER_ENABLE, REG_CONTROLLER_ENABLE);
}

/**
 * ufshcd_is_hba_active - Get controller state
 * @hba: per adapter instance
 *
 * Returns false if controller is active, true otherwise
 */
static inline bool ufshcd_is_hba_active(struct ufs_hba *hba)
{
	return (ufshcd_readl(hba, REG_CONTROLLER_ENABLE) & CONTROLLER_ENABLE)
		? false : true;
}

u32 ufshcd_get_local_unipro_ver(struct ufs_hba *hba)
{
	/* HCI version 1.0 and 1.1 supports UniPro 1.41 */
	if ((hba->ufs_version == UFSHCI_VERSION_10) ||
	    (hba->ufs_version == UFSHCI_VERSION_11))
		return UFS_UNIPRO_VER_1_41;
	else
		return UFS_UNIPRO_VER_1_6;
}
EXPORT_SYMBOL(ufshcd_get_local_unipro_ver);

static bool ufshcd_is_unipro_pa_params_tuning_req(struct ufs_hba *hba)
{
	/*
	 * If both host and device support UniPro ver1.6 or later, PA layer
	 * parameters tuning happens during link startup itself.
	 *
	 * We can manually tune PA layer parameters if either host or device
	 * doesn't support UniPro ver 1.6 or later. But to keep manual tuning
	 * logic simple, we will only do manual tuning if local unipro version
	 * doesn't support ver1.6 or later.
	 */
	if (ufshcd_get_local_unipro_ver(hba) < UFS_UNIPRO_VER_1_6)
		return true;
	else
		return false;
}

static int ufshcd_scale_clks(struct ufs_hba *hba, bool scale_up)
{
	int ret = 0;
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;
	ktime_t start = ktime_get();
	bool clk_state_changed = false;

	if (list_empty(head))
		goto out;

	ret = ufshcd_vops_clk_scale_notify(hba, scale_up, PRE_CHANGE);
	if (ret)
		return ret;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			if (scale_up && clki->max_freq) {
				if (clki->curr_freq == clki->max_freq)
					continue;

				clk_state_changed = true;
				ret = clk_set_rate(clki->clk, clki->max_freq);
				if (ret) {
					dev_err(hba->dev, "%s: %s clk set rate(%dHz) failed, %d\n",
						__func__, clki->name,
						clki->max_freq, ret);
					break;
				}
				trace_ufshcd_clk_scaling(dev_name(hba->dev),
						"scaled up", clki->name,
						clki->curr_freq,
						clki->max_freq);

				clki->curr_freq = clki->max_freq;

			} else if (!scale_up && clki->min_freq) {
				if (clki->curr_freq == clki->min_freq)
					continue;

				clk_state_changed = true;
				ret = clk_set_rate(clki->clk, clki->min_freq);
				if (ret) {
					dev_err(hba->dev, "%s: %s clk set rate(%dHz) failed, %d\n",
						__func__, clki->name,
						clki->min_freq, ret);
					break;
				}
				trace_ufshcd_clk_scaling(dev_name(hba->dev),
						"scaled down", clki->name,
						clki->curr_freq,
						clki->min_freq);
				clki->curr_freq = clki->min_freq;
			}
		}
		dev_dbg(hba->dev, "%s: clk: %s, rate: %lu\n", __func__,
				clki->name, clk_get_rate(clki->clk));
	}

	ret = ufshcd_vops_clk_scale_notify(hba, scale_up, POST_CHANGE);

out:
	if (clk_state_changed)
		trace_ufshcd_profile_clk_scaling(dev_name(hba->dev),
			(scale_up ? "up" : "down"),
			ktime_to_us(ktime_sub(ktime_get(), start)), ret);
	return ret;
}

/**
 * ufshcd_is_devfreq_scaling_required - check if scaling is required or not
 * @hba: per adapter instance
 * @scale_up: True if scaling up and false if scaling down
 *
 * Returns true if scaling is required, false otherwise.
 */
static bool ufshcd_is_devfreq_scaling_required(struct ufs_hba *hba,
					       bool scale_up)
{
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;

	if (list_empty(head))
		return false;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			if (scale_up && clki->max_freq) {
				if (clki->curr_freq == clki->max_freq)
					continue;
				return true;
			} else if (!scale_up && clki->min_freq) {
				if (clki->curr_freq == clki->min_freq)
					continue;
				return true;
			}
		}
	}

	return false;
}

static int ufshcd_wait_for_doorbell_clr(struct ufs_hba *hba,
					u64 wait_timeout_us)
{
	unsigned long flags;
	int ret = 0;
	u32 tm_doorbell;
	u32 tr_doorbell;
	bool timeout = false, do_last_check = false;
	ktime_t start;

	ufshcd_hold(hba, false);
	spin_lock_irqsave(hba->host->host_lock, flags);
	/*
	 * Wait for all the outstanding tasks/transfer requests.
	 * Verify by checking the doorbell registers are clear.
	 */
	start = ktime_get();
	do {
		if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL) {
			ret = -EBUSY;
			goto out;
		}

		tm_doorbell = ufshcd_readl(hba, REG_UTP_TASK_REQ_DOOR_BELL);
		tr_doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
		if (!tm_doorbell && !tr_doorbell) {
			timeout = false;
			break;
		} else if (do_last_check) {
			break;
		}

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		schedule();
		if (ktime_to_us(ktime_sub(ktime_get(), start)) >
		    wait_timeout_us) {
			timeout = true;
			/*
			 * We might have scheduled out for long time so make
			 * sure to check if doorbells are cleared by this time
			 * or not.
			 */
			do_last_check = true;
		}
		spin_lock_irqsave(hba->host->host_lock, flags);
	} while (tm_doorbell || tr_doorbell);

	if (timeout) {
		dev_err(hba->dev,
			"%s: timedout waiting for doorbell to clear (tm=0x%x, tr=0x%x)\n",
			__func__, tm_doorbell, tr_doorbell);
		ret = -EBUSY;
	}
out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_release(hba);
	return ret;
}

/**
 * ufshcd_scale_gear - scale up/down UFS gear
 * @hba: per adapter instance
 * @scale_up: True for scaling up gear and false for scaling down
 *
 * Returns 0 for success,
 * Returns -EBUSY if scaling can't happen at this time
 * Returns non-zero for any other errors
 */
static int ufshcd_scale_gear(struct ufs_hba *hba, bool scale_up)
{
	#define UFS_MIN_GEAR_TO_SCALE_DOWN	UFS_HS_G1
	int ret = 0;
	struct ufs_pa_layer_attr new_pwr_info;

	if (scale_up) {
		memcpy(&new_pwr_info, &hba->clk_scaling.saved_pwr_info.info,
		       sizeof(struct ufs_pa_layer_attr));
	} else {
		memcpy(&new_pwr_info, &hba->pwr_info,
		       sizeof(struct ufs_pa_layer_attr));

		if (hba->pwr_info.gear_tx > UFS_MIN_GEAR_TO_SCALE_DOWN
		    || hba->pwr_info.gear_rx > UFS_MIN_GEAR_TO_SCALE_DOWN) {
			/* save the current power mode */
			memcpy(&hba->clk_scaling.saved_pwr_info.info,
				&hba->pwr_info,
				sizeof(struct ufs_pa_layer_attr));

			/* scale down gear */
			new_pwr_info.gear_tx = UFS_MIN_GEAR_TO_SCALE_DOWN;
			new_pwr_info.gear_rx = UFS_MIN_GEAR_TO_SCALE_DOWN;
		}
	}

	/* check if the power mode needs to be changed or not? */
	ret = ufshcd_change_power_mode(hba, &new_pwr_info);

	if (ret)
		dev_err(hba->dev, "%s: failed err %d, old gear: (tx %d rx %d), new gear: (tx %d rx %d)",
			__func__, ret,
			hba->pwr_info.gear_tx, hba->pwr_info.gear_rx,
			new_pwr_info.gear_tx, new_pwr_info.gear_rx);

	return ret;
}

static int ufshcd_clock_scaling_prepare(struct ufs_hba *hba)
{
	#define DOORBELL_CLR_TOUT_US		(1000 * 1000) /* 1 sec */
	int ret = 0;
	/*
	 * make sure that there are no outstanding requests when
	 * clock scaling is in progress
	 */
	ufshcd_scsi_block_requests(hba);
	down_write(&hba->clk_scaling_lock);
	if (ufshcd_wait_for_doorbell_clr(hba, DOORBELL_CLR_TOUT_US)) {
		ret = -EBUSY;
		up_write(&hba->clk_scaling_lock);
		ufshcd_scsi_unblock_requests(hba);
	}

	return ret;
}

static void ufshcd_clock_scaling_unprepare(struct ufs_hba *hba)
{
	up_write(&hba->clk_scaling_lock);
	ufshcd_scsi_unblock_requests(hba);
}

/**
 * ufshcd_devfreq_scale - scale up/down UFS clocks and gear
 * @hba: per adapter instance
 * @scale_up: True for scaling up and false for scalin down
 *
 * Returns 0 for success,
 * Returns -EBUSY if scaling can't happen at this time
 * Returns non-zero for any other errors
 */
static int ufshcd_devfreq_scale(struct ufs_hba *hba, bool scale_up)
{
	int ret = 0;

	/* let's not get into low power until clock scaling is completed */
	ufshcd_hold(hba, false);

	ret = ufshcd_clock_scaling_prepare(hba);
	if (ret)
		return ret;

	/* scale down the gear before scaling down clocks */
	if (!scale_up) {
		ret = ufshcd_scale_gear(hba, false);
		if (ret)
			goto out;
	}

	ret = ufshcd_scale_clks(hba, scale_up);
	if (ret) {
		if (!scale_up)
			ufshcd_scale_gear(hba, true);
		goto out;
	}

	/* scale up the gear after scaling up clocks */
	if (scale_up) {
		ret = ufshcd_scale_gear(hba, true);
		if (ret) {
			ufshcd_scale_clks(hba, false);
			goto out;
		}
	}

	ret = ufshcd_vops_clk_scale_notify(hba, scale_up, POST_CHANGE);

out:
	ufshcd_clock_scaling_unprepare(hba);
	ufshcd_release(hba);
	return ret;
}

static void ufshcd_clk_scaling_suspend_work(struct work_struct *work)
{
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
					   clk_scaling.suspend_work);
	unsigned long irq_flags;

	spin_lock_irqsave(hba->host->host_lock, irq_flags);
	if (hba->clk_scaling.active_reqs || hba->clk_scaling.is_suspended) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		return;
	}
	hba->clk_scaling.is_suspended = true;
	spin_unlock_irqrestore(hba->host->host_lock, irq_flags);

	__ufshcd_suspend_clkscaling(hba);
}

static void ufshcd_clk_scaling_resume_work(struct work_struct *work)
{
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
					   clk_scaling.resume_work);
	unsigned long irq_flags;

	spin_lock_irqsave(hba->host->host_lock, irq_flags);
	if (!hba->clk_scaling.is_suspended) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		return;
	}
	hba->clk_scaling.is_suspended = false;
	spin_unlock_irqrestore(hba->host->host_lock, irq_flags);

	devfreq_resume_device(hba->devfreq);
}

static int ufshcd_devfreq_target(struct device *dev,
				unsigned long *freq, u32 flags)
{
	int ret = 0;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	ktime_t start;
	bool scale_up, sched_clk_scaling_suspend_work = false;
	struct list_head *clk_list = &hba->clk_list_head;
	struct ufs_clk_info *clki;
	unsigned long irq_flags;

	if (!ufshcd_is_clkscaling_supported(hba))
		return -EINVAL;

	spin_lock_irqsave(hba->host->host_lock, irq_flags);
	if (ufshcd_eh_in_progress(hba)) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		return 0;
	}

	if (!hba->clk_scaling.active_reqs)
		sched_clk_scaling_suspend_work = true;

	if (list_empty(clk_list)) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		goto out;
	}

	clki = list_first_entry(&hba->clk_list_head, struct ufs_clk_info, list);
	scale_up = (*freq == clki->max_freq) ? true : false;
	if (!ufshcd_is_devfreq_scaling_required(hba, scale_up)) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		ret = 0;
		goto out; /* no state change required */
	}
	spin_unlock_irqrestore(hba->host->host_lock, irq_flags);

	start = ktime_get();
	ret = ufshcd_devfreq_scale(hba, scale_up);

	trace_ufshcd_profile_clk_scaling(dev_name(hba->dev),
		(scale_up ? "up" : "down"),
		ktime_to_us(ktime_sub(ktime_get(), start)), ret);

out:
	if (sched_clk_scaling_suspend_work)
		queue_work(hba->clk_scaling.workq,
			   &hba->clk_scaling.suspend_work);

	return ret;
}


static int ufshcd_devfreq_get_dev_status(struct device *dev,
		struct devfreq_dev_status *stat)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_clk_scaling *scaling = &hba->clk_scaling;
	unsigned long flags;

	if (!ufshcd_is_clkscaling_supported(hba))
		return -EINVAL;

	memset(stat, 0, sizeof(*stat));

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (!scaling->window_start_t)
		goto start_window;

	if (scaling->is_busy_started)
		scaling->tot_busy_t += ktime_to_us(ktime_sub(ktime_get(),
					scaling->busy_start_t));

	stat->total_time = jiffies_to_usecs((long)jiffies -
				(long)scaling->window_start_t);
	stat->busy_time = scaling->tot_busy_t;
start_window:
	scaling->window_start_t = jiffies;
	scaling->tot_busy_t = 0;

	if (hba->outstanding_reqs) {
		scaling->busy_start_t = ktime_get();
		scaling->is_busy_started = true;
	} else {
		scaling->busy_start_t = 0;
		scaling->is_busy_started = false;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return 0;
}

static struct devfreq_dev_profile ufs_devfreq_profile = {
	.polling_ms	= 100,
	.target		= ufshcd_devfreq_target,
	.get_dev_status	= ufshcd_devfreq_get_dev_status,
};

static int ufshcd_devfreq_init(struct ufs_hba *hba)
{
	struct list_head *clk_list = &hba->clk_list_head;
	struct ufs_clk_info *clki;
	struct devfreq *devfreq;
	int ret;

	/* Skip devfreq if we don't have any clocks in the list */
	if (list_empty(clk_list))
		return 0;

	clki = list_first_entry(clk_list, struct ufs_clk_info, list);
	dev_pm_opp_add(hba->dev, clki->min_freq, 0);
	dev_pm_opp_add(hba->dev, clki->max_freq, 0);

	devfreq = devfreq_add_device(hba->dev,
			&ufs_devfreq_profile,
			DEVFREQ_GOV_SIMPLE_ONDEMAND,
			NULL);
	if (IS_ERR(devfreq)) {
		ret = PTR_ERR(devfreq);
		dev_err(hba->dev, "Unable to register with devfreq %d\n", ret);

		dev_pm_opp_remove(hba->dev, clki->min_freq);
		dev_pm_opp_remove(hba->dev, clki->max_freq);
		return ret;
	}

	hba->devfreq = devfreq;

	return 0;
}

static void ufshcd_devfreq_remove(struct ufs_hba *hba)
{
	struct list_head *clk_list = &hba->clk_list_head;
	struct ufs_clk_info *clki;

	if (!hba->devfreq)
		return;

	devfreq_remove_device(hba->devfreq);
	hba->devfreq = NULL;

	clki = list_first_entry(clk_list, struct ufs_clk_info, list);
	dev_pm_opp_remove(hba->dev, clki->min_freq);
	dev_pm_opp_remove(hba->dev, clki->max_freq);
}

static void __ufshcd_suspend_clkscaling(struct ufs_hba *hba)
{
	unsigned long flags;

	devfreq_suspend_device(hba->devfreq);
	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->clk_scaling.window_start_t = 0;
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

static void ufshcd_suspend_clkscaling(struct ufs_hba *hba)
{
	unsigned long flags;
	bool suspend = false;

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (!hba->clk_scaling.is_suspended) {
		suspend = true;
		hba->clk_scaling.is_suspended = true;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (suspend)
		__ufshcd_suspend_clkscaling(hba);
}

static void ufshcd_resume_clkscaling(struct ufs_hba *hba)
{
	unsigned long flags;
	bool resume = false;

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->clk_scaling.is_suspended) {
		resume = true;
		hba->clk_scaling.is_suspended = false;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (resume)
		devfreq_resume_device(hba->devfreq);
}

static ssize_t ufshcd_clkscale_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", hba->clk_scaling.is_allowed);
}

static ssize_t ufshcd_clkscale_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 value;
	int err;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	value = !!value;
	if (value == hba->clk_scaling.is_allowed)
		goto out;

	pm_runtime_get_sync(hba->dev);
	ufshcd_hold(hba, false);

	cancel_work_sync(&hba->clk_scaling.suspend_work);
	cancel_work_sync(&hba->clk_scaling.resume_work);

	hba->clk_scaling.is_allowed = value;

	if (value) {
		ufshcd_resume_clkscaling(hba);
	} else {
		ufshcd_suspend_clkscaling(hba);
		err = ufshcd_devfreq_scale(hba, true);
		if (err)
			dev_err(hba->dev, "%s: failed to scale clocks up %d\n",
					__func__, err);
	}

	ufshcd_release(hba);
	pm_runtime_put_sync(hba->dev);
out:
	return count;
}

static void ufshcd_clkscaling_init_sysfs(struct ufs_hba *hba)
{
	hba->clk_scaling.enable_attr.show = ufshcd_clkscale_enable_show;
	hba->clk_scaling.enable_attr.store = ufshcd_clkscale_enable_store;
	sysfs_attr_init(&hba->clk_scaling.enable_attr.attr);
	hba->clk_scaling.enable_attr.attr.name = "clkscale_enable";
	hba->clk_scaling.enable_attr.attr.mode = 0644;
	if (device_create_file(hba->dev, &hba->clk_scaling.enable_attr))
		dev_err(hba->dev, "Failed to create sysfs for clkscale_enable\n");
}

static void ufshcd_ungate_work(struct work_struct *work)
{
	int ret;
	unsigned long flags;
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
			clk_gating.ungate_work);

	cancel_delayed_work_sync(&hba->clk_gating.gate_work);

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->clk_gating.state == CLKS_ON) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		goto unblock_reqs;
	}

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_setup_clocks(hba, true);

	/* Exit from hibern8 */
	if (ufshcd_can_hibern8_during_gating(hba)) {
		/* Prevent gating in this path */
		hba->clk_gating.is_suspended = true;
		if (ufshcd_is_link_hibern8(hba)) {
			ret = ufshcd_uic_hibern8_exit(hba);
			if (ret)
				dev_err(hba->dev, "%s: hibern8 exit failed %d\n",
					__func__, ret);
			else
				ufshcd_set_link_active(hba);
		}
		hba->clk_gating.is_suspended = false;
	}
unblock_reqs:
	ufshcd_scsi_unblock_requests(hba);
}

/**
 * ufshcd_hold - Enable clocks that were gated earlier due to ufshcd_release.
 * Also, exit from hibern8 mode and set the link as active.
 * @hba: per adapter instance
 * @async: This indicates whether caller should ungate clocks asynchronously.
 */
int ufshcd_hold(struct ufs_hba *hba, bool async)
{
	int rc = 0;
	unsigned long flags;

	if (!ufshcd_is_clkgating_allowed(hba))
		goto out;
	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->clk_gating.active_reqs++;

	if (ufshcd_eh_in_progress(hba)) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		return 0;
	}

start:
	switch (hba->clk_gating.state) {
	case CLKS_ON:
		/*
		 * Wait for the ungate work to complete if in progress.
		 * Though the clocks may be in ON state, the link could
		 * still be in hibner8 state if hibern8 is allowed
		 * during clock gating.
		 * Make sure we exit hibern8 state also in addition to
		 * clocks being ON.
		 */
		if (ufshcd_can_hibern8_during_gating(hba) &&
		    ufshcd_is_link_hibern8(hba)) {
			spin_unlock_irqrestore(hba->host->host_lock, flags);
			flush_work(&hba->clk_gating.ungate_work);
			spin_lock_irqsave(hba->host->host_lock, flags);
			goto start;
		}
		break;
	case REQ_CLKS_OFF:
		if (cancel_delayed_work(&hba->clk_gating.gate_work)) {
			hba->clk_gating.state = CLKS_ON;
			trace_ufshcd_clk_gating(dev_name(hba->dev),
						hba->clk_gating.state);
			break;
		}
		/*
		 * If we are here, it means gating work is either done or
		 * currently running. Hence, fall through to cancel gating
		 * work and to enable clocks.
		 */
		/* fallthrough */
	case CLKS_OFF:
		ufshcd_scsi_block_requests(hba);
		hba->clk_gating.state = REQ_CLKS_ON;
		trace_ufshcd_clk_gating(dev_name(hba->dev),
					hba->clk_gating.state);
		queue_work(hba->clk_gating.clk_gating_workq,
			   &hba->clk_gating.ungate_work);
		/*
		 * fall through to check if we should wait for this
		 * work to be done or not.
		 */
		/* fallthrough */
	case REQ_CLKS_ON:
		if (async) {
			rc = -EAGAIN;
			hba->clk_gating.active_reqs--;
			break;
		}

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		flush_work(&hba->clk_gating.ungate_work);
		/* Make sure state is CLKS_ON before returning */
		spin_lock_irqsave(hba->host->host_lock, flags);
		goto start;
	default:
		dev_err(hba->dev, "%s: clk gating is in invalid state %d\n",
				__func__, hba->clk_gating.state);
		break;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
out:
	return rc;
}
EXPORT_SYMBOL_GPL(ufshcd_hold);

static void ufshcd_gate_work(struct work_struct *work)
{
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
			clk_gating.gate_work.work);
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	/*
	 * In case you are here to cancel this work the gating state
	 * would be marked as REQ_CLKS_ON. In this case save time by
	 * skipping the gating work and exit after changing the clock
	 * state to CLKS_ON.
	 */
	if (hba->clk_gating.is_suspended ||
		(hba->clk_gating.state == REQ_CLKS_ON)) {
		hba->clk_gating.state = CLKS_ON;
		trace_ufshcd_clk_gating(dev_name(hba->dev),
					hba->clk_gating.state);
		goto rel_lock;
	}

	if (hba->clk_gating.active_reqs
		|| hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL
		|| hba->lrb_in_use || hba->outstanding_tasks
		|| hba->active_uic_cmd || hba->uic_async_done)
		goto rel_lock;

	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/* put the link into hibern8 mode before turning off clocks */
	if (ufshcd_can_hibern8_during_gating(hba)) {
		if (ufshcd_uic_hibern8_enter(hba)) {
			hba->clk_gating.state = CLKS_ON;
			trace_ufshcd_clk_gating(dev_name(hba->dev),
						hba->clk_gating.state);
			goto out;
		}
		ufshcd_set_link_hibern8(hba);
	}

	if (!ufshcd_is_link_active(hba))
		ufshcd_setup_clocks(hba, false);
	else
		/* If link is active, device ref_clk can't be switched off */
		__ufshcd_setup_clocks(hba, false, true);

	/*
	 * In case you are here to cancel this work the gating state
	 * would be marked as REQ_CLKS_ON. In this case keep the state
	 * as REQ_CLKS_ON which would anyway imply that clocks are off
	 * and a request to turn them on is pending. By doing this way,
	 * we keep the state machine in tact and this would ultimately
	 * prevent from doing cancel work multiple times when there are
	 * new requests arriving before the current cancel work is done.
	 */
	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->clk_gating.state == REQ_CLKS_OFF) {
		hba->clk_gating.state = CLKS_OFF;
		trace_ufshcd_clk_gating(dev_name(hba->dev),
					hba->clk_gating.state);
	}
rel_lock:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
out:
	return;
}

/* host lock must be held before calling this variant */
static void __ufshcd_release(struct ufs_hba *hba)
{
	if (!ufshcd_is_clkgating_allowed(hba))
		return;

	hba->clk_gating.active_reqs--;

	if (hba->clk_gating.active_reqs || hba->clk_gating.is_suspended
		|| hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL
		|| hba->lrb_in_use || hba->outstanding_tasks
		|| hba->active_uic_cmd || hba->uic_async_done
		|| ufshcd_eh_in_progress(hba))
		return;

	hba->clk_gating.state = REQ_CLKS_OFF;
	trace_ufshcd_clk_gating(dev_name(hba->dev), hba->clk_gating.state);
	queue_delayed_work(hba->clk_gating.clk_gating_workq,
			   &hba->clk_gating.gate_work,
			   msecs_to_jiffies(hba->clk_gating.delay_ms));
}

void ufshcd_release(struct ufs_hba *hba)
{
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	__ufshcd_release(hba);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}
EXPORT_SYMBOL_GPL(ufshcd_release);

static ssize_t ufshcd_clkgate_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lu\n", hba->clk_gating.delay_ms);
}

static ssize_t ufshcd_clkgate_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long flags, value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->clk_gating.delay_ms = value;
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return count;
}

static ssize_t ufshcd_clkgate_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", hba->clk_gating.is_enabled);
}

static ssize_t ufshcd_clkgate_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long flags;
	u32 value;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	value = !!value;
	if (value == hba->clk_gating.is_enabled)
		goto out;

	if (value) {
		ufshcd_release(hba);
	} else {
		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->clk_gating.active_reqs++;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}

	hba->clk_gating.is_enabled = value;
out:
	return count;
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

static void ufshcd_exit_clk_scaling(struct ufs_hba *hba)
{
	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	destroy_workqueue(hba->clk_scaling.workq);
	ufshcd_devfreq_remove(hba);
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

static void ufshcd_exit_clk_gating(struct ufs_hba *hba)
{
	if (!ufshcd_is_clkgating_allowed(hba))
		return;
	device_remove_file(hba->dev, &hba->clk_gating.delay_attr);
	device_remove_file(hba->dev, &hba->clk_gating.enable_attr);
	cancel_work_sync(&hba->clk_gating.ungate_work);
	cancel_delayed_work_sync(&hba->clk_gating.gate_work);
	destroy_workqueue(hba->clk_gating.clk_gating_workq);
}

/* Must be called with host lock acquired */
static void ufshcd_clk_scaling_start_busy(struct ufs_hba *hba)
{
	bool queue_resume_work = false;

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	if (!hba->clk_scaling.active_reqs++)
		queue_resume_work = true;

	if (!hba->clk_scaling.is_allowed || hba->pm_op_in_progress)
		return;

	if (queue_resume_work)
		queue_work(hba->clk_scaling.workq,
			   &hba->clk_scaling.resume_work);

	if (!hba->clk_scaling.window_start_t) {
		hba->clk_scaling.window_start_t = jiffies;
		hba->clk_scaling.tot_busy_t = 0;
		hba->clk_scaling.is_busy_started = false;
	}

	if (!hba->clk_scaling.is_busy_started) {
		hba->clk_scaling.busy_start_t = ktime_get();
		hba->clk_scaling.is_busy_started = true;
	}
}

static void ufshcd_clk_scaling_update_busy(struct ufs_hba *hba)
{
	struct ufs_clk_scaling *scaling = &hba->clk_scaling;

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	if (!hba->outstanding_reqs && scaling->is_busy_started) {
		scaling->tot_busy_t += ktime_to_us(ktime_sub(ktime_get(),
					scaling->busy_start_t));
		scaling->busy_start_t = 0;
		scaling->is_busy_started = false;
	}
}
/**
 * ufshcd_send_command - Send SCSI or device management commands
 * @hba: per adapter instance
 * @task_tag: Task tag of the command
 */
static inline
void ufshcd_send_command(struct ufs_hba *hba, unsigned int task_tag)
{
	hba->lrb[task_tag].issue_time_stamp = ktime_get();
	hba->lrb[task_tag].compl_time_stamp = ktime_set(0, 0);
	ufshcd_clk_scaling_start_busy(hba);
	__set_bit(task_tag, &hba->outstanding_reqs);
	ufshcd_writel(hba, 1 << task_tag, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	/* Make sure that doorbell is committed immediately */
	wmb();
	ufshcd_add_command_trace(hba, task_tag, "send");
}

/**
 * ufshcd_copy_sense_data - Copy sense data in case of check condition
 * @lrbp: pointer to local reference block
 */
static inline void ufshcd_copy_sense_data(struct ufshcd_lrb *lrbp)
{
	int len;
	if (lrbp->sense_buffer &&
	    ufshcd_get_rsp_upiu_data_seg_len(lrbp->ucd_rsp_ptr)) {
		int len_to_copy;

		len = be16_to_cpu(lrbp->ucd_rsp_ptr->sr.sense_data_len);
		len_to_copy = min_t(int, UFS_SENSE_SIZE, len);

		memcpy(lrbp->sense_buffer, lrbp->ucd_rsp_ptr->sr.sense_data,
		       len_to_copy);
	}
}

/**
 * ufshcd_copy_query_response() - Copy the Query Response and the data
 * descriptor
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 */
static
int ufshcd_copy_query_response(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_query_res *query_res = &hba->dev_cmd.query.response;

	memcpy(&query_res->upiu_res, &lrbp->ucd_rsp_ptr->qr, QUERY_OSF_SIZE);

	/* Get the descriptor */
	if (hba->dev_cmd.query.descriptor &&
	    lrbp->ucd_rsp_ptr->qr.opcode == UPIU_QUERY_OPCODE_READ_DESC) {
		u8 *descp = (u8 *)lrbp->ucd_rsp_ptr +
				GENERAL_UPIU_REQUEST_SIZE;
		u16 resp_len;
		u16 buf_len;

		/* data segment length */
		resp_len = be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2) &
						MASK_QUERY_DATA_SEG_LEN;
		buf_len = be16_to_cpu(
				hba->dev_cmd.query.request.upiu_req.length);
		if (likely(buf_len >= resp_len)) {
			memcpy(hba->dev_cmd.query.descriptor, descp, resp_len);
		} else {
			dev_warn(hba->dev,
				"%s: Response size is bigger than buffer",
				__func__);
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * ufshcd_hba_capabilities - Read controller capabilities
 * @hba: per adapter instance
 */
static inline void ufshcd_hba_capabilities(struct ufs_hba *hba)
{
	hba->capabilities = ufshcd_readl(hba, REG_CONTROLLER_CAPABILITIES);                  // 读取hba->mmio_base + REG_CONTROLLER_CAPABILITIES的值保存到hba->capabilities

	/* nutrs and nutmrs are 0 based values */
	hba->nutrs = (hba->capabilities & MASK_TRANSFER_REQUESTS_SLOTS) + 1;                 // Number of UTP Transfer Requests
	hba->nutmrs =
	((hba->capabilities & MASK_TASK_MANAGEMENT_REQUEST_SLOTS) >> 16) + 1;                // Number of UTP Transfer Management Requests
}

/**
 * ufshcd_ready_for_uic_cmd - Check if controller is ready
 *                            to accept UIC commands
 * @hba: per adapter instance
 * Return true on success, else false
 */
static inline bool ufshcd_ready_for_uic_cmd(struct ufs_hba *hba)
{
	if (ufshcd_readl(hba, REG_CONTROLLER_STATUS) & UIC_COMMAND_READY)
		return true;
	else
		return false;
}

/**
 * ufshcd_get_upmcrs - Get the power mode change request status
 * @hba: Pointer to adapter instance
 *
 * This function gets the UPMCRS field of HCS register
 * Returns value of UPMCRS field
 */
static inline u8 ufshcd_get_upmcrs(struct ufs_hba *hba)
{
	return (ufshcd_readl(hba, REG_CONTROLLER_STATUS) >> 8) & 0x7;
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
	WARN_ON(hba->active_uic_cmd);

	hba->active_uic_cmd = uic_cmd;

	/* Write Args */
	ufshcd_writel(hba, uic_cmd->argument1, REG_UIC_COMMAND_ARG_1);
	ufshcd_writel(hba, uic_cmd->argument2, REG_UIC_COMMAND_ARG_2);
	ufshcd_writel(hba, uic_cmd->argument3, REG_UIC_COMMAND_ARG_3);

	/* Write UIC Cmd */
	ufshcd_writel(hba, uic_cmd->command & COMMAND_OPCODE_MASK,
		      REG_UIC_COMMAND);
}

/**
 * ufshcd_wait_for_uic_cmd - Wait complectioin of UIC command
 * @hba: per adapter instance
 * @uic_cmd: UIC command
 *
 * Must be called with mutex held.
 * Returns 0 only if success.
 */
static int
ufshcd_wait_for_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	int ret;
	unsigned long flags;

	if (wait_for_completion_timeout(&uic_cmd->done,
					msecs_to_jiffies(UIC_CMD_TIMEOUT)))
		ret = uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT;
	else
		ret = -ETIMEDOUT;

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->active_uic_cmd = NULL;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return ret;
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
	if (!ufshcd_ready_for_uic_cmd(hba)) {
		dev_err(hba->dev,
			"Controller not ready to accept UIC commands\n");
		return -EIO;
	}

	if (completion)
		init_completion(&uic_cmd->done);

	ufshcd_dispatch_uic_cmd(hba, uic_cmd);

	return 0;
}

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

	ufshcd_hold(hba, false);
	mutex_lock(&hba->uic_cmd_mutex);
	ufshcd_add_delay_before_dme_cmd(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = __ufshcd_send_uic_cmd(hba, uic_cmd, true);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (!ret)
		ret = ufshcd_wait_for_uic_cmd(hba, uic_cmd);

	mutex_unlock(&hba->uic_cmd_mutex);

	ufshcd_release(hba);
	return ret;
}

/**
 * ufshcd_map_sg - Map scatter-gather list to prdt
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 *
 * Returns 0 in case of success, non-zero value in case of failure
 */
static int ufshcd_map_sg(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufshcd_sg_entry *prd_table;
	struct scatterlist *sg;
	struct scsi_cmnd *cmd;
	int sg_segments;
	int i;

	cmd = lrbp->cmd;                                                                     // 取出cmd
	sg_segments = scsi_dma_map(cmd);                                                     // 
	if (sg_segments < 0)                                                                 // 若sg_segments < 0为true
		return sg_segments;

	if (sg_segments) {                                                                   // 若sg_segments不为0
		if (hba->quirks & UFSHCD_QUIRK_PRDT_BYTE_GRAN)                                   // hba->quirks & UFSHCD_QUIRK_PRDT_BYTE_GRAN
			lrbp->utr_descriptor_ptr->prd_table_length =
				cpu_to_le16((u16)(sg_segments *
					sizeof(struct ufshcd_sg_entry)));                                    // 
		else
			lrbp->utr_descriptor_ptr->prd_table_length =
				cpu_to_le16((u16) (sg_segments));                                        // 

		prd_table = (struct ufshcd_sg_entry *)lrbp->ucd_prdt_ptr;                        // 

		scsi_for_each_sg(cmd, sg, sg_segments, i) {                                      // 
			prd_table[i].size  =
				cpu_to_le32(((u32) sg_dma_len(sg))-1);
			prd_table[i].base_addr =
				cpu_to_le32(lower_32_bits(sg->dma_address));
			prd_table[i].upper_addr =
				cpu_to_le32(upper_32_bits(sg->dma_address));
			prd_table[i].reserved = 0;
		}
	} else {
		lrbp->utr_descriptor_ptr->prd_table_length = 0;                                  // 
	}

	return 0;                                                                            // 返回0
}

/**
 * ufshcd_enable_intr - enable interrupts
 * @hba: per adapter instance
 * @intrs: interrupt bits
 */
static void ufshcd_enable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	if (hba->ufs_version == UFSHCI_VERSION_10) {
		u32 rw;
		rw = set & INTERRUPT_MASK_RW_VER_10;
		set = rw | ((set ^ intrs) & intrs);
	} else {
		set |= intrs;
	}

	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}

/**
 * ufshcd_disable_intr - disable interrupts
 * @hba: per adapter instance
 * @intrs: interrupt bits
 */
static void ufshcd_disable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	if (hba->ufs_version == UFSHCI_VERSION_10) {
		u32 rw;
		rw = (set & INTERRUPT_MASK_RW_VER_10) &
			~(intrs & INTERRUPT_MASK_RW_VER_10);
		set = rw | ((set & intrs) & ~INTERRUPT_MASK_RW_VER_10);

	} else {
		set &= ~intrs;
	}

	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}

/**
 * ufshcd_prepare_req_desc_hdr() - Fills the requests header
 * descriptor according to request
 * @lrbp: pointer to local reference block
 * @upiu_flags: flags required in the header
 * @cmd_dir: requests data direction
 */
static void ufshcd_prepare_req_desc_hdr(struct ufshcd_lrb *lrbp,
			u32 *upiu_flags, enum dma_data_direction cmd_dir)
{
	struct utp_transfer_req_desc *req_desc = lrbp->utr_descriptor_ptr;                   // 取出req_desc
	u32 data_direction;
	u32 dword_0;

	if (cmd_dir == DMA_FROM_DEVICE) {                                                    // 若cmd_dir为DMA_FROM_DEVICE
		data_direction = UTP_DEVICE_TO_HOST;                                             // 设置data_direction为UTP_DEVICE_TO_HOST
		*upiu_flags = UPIU_CMD_FLAGS_READ;                                               // 设置*upiu_flags为UPIU_CMD_FLAGS_READ
	} else if (cmd_dir == DMA_TO_DEVICE) {                                               // 若cmd_dir为DMA_TO_DEVICE
		data_direction = UTP_HOST_TO_DEVICE;                                             // 设置data_direction为UTP_HOST_TO_DEVICE
		*upiu_flags = UPIU_CMD_FLAGS_WRITE;                                              // 设置*upiu_flags为UPIU_CMD_FLAGS_WRITE
	} else {
		data_direction = UTP_NO_DATA_TRANSFER;                                           // 设置data_direction为UTP_NO_DATA_TRANSFER
		*upiu_flags = UPIU_CMD_FLAGS_NONE;                                               // 设置*upiu_flags为UPIU_CMD_FLAGS_NONE
	}

	dword_0 = data_direction | (lrbp->command_type
				<< UPIU_COMMAND_TYPE_OFFSET);                                            // 
	if (lrbp->intr_cmd)                                                                  // 若lrbp->intr_cmd不为NULL
		dword_0 |= UTP_REQ_DESC_INT_CMD;                                                 // 

	/* Transfer request descriptor header fields */
	req_desc->header.dword_0 = cpu_to_le32(dword_0);                                     // 设置req_desc->header.dword_0为cpu_to_le32(dword_0)
	/* dword_1 is reserved, hence it is set to 0 */
	req_desc->header.dword_1 = 0;                                                        // 设置req_desc->header.dword_1为0
	/*
	 * assigning invalid value for command status. Controller
	 * updates OCS on command completion, with the command
	 * status
	 */
	req_desc->header.dword_2 =
		cpu_to_le32(OCS_INVALID_COMMAND_STATUS);                                         // 设置req_desc->header.dword_2为cpu_to_le32(OCS_INVALID_COMMAND_STATUS)
	/* dword_3 is reserved, hence it is set to 0 */
	req_desc->header.dword_3 = 0;                                                        // 设置req_desc->header.dword_3为0

	req_desc->prd_table_length = 0;                                                      // 设置req_desc->prd_table_length为0
}

/**
 * ufshcd_prepare_utp_scsi_cmd_upiu() - fills the utp_transfer_req_desc,
 * for scsi commands
 * @lrbp: local reference block pointer
 * @upiu_flags: flags
 */
static
void ufshcd_prepare_utp_scsi_cmd_upiu(struct ufshcd_lrb *lrbp, u32 upiu_flags)
{
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;
	unsigned short cdb_len;

	/* command descriptor fields */
	ucd_req_ptr->header.dword_0 = UPIU_HEADER_DWORD(
				UPIU_TRANSACTION_COMMAND, upiu_flags,
				lrbp->lun, lrbp->task_tag);
	ucd_req_ptr->header.dword_1 = UPIU_HEADER_DWORD(
				UPIU_COMMAND_SET_TYPE_SCSI, 0, 0, 0);

	/* Total EHS length and Data segment length will be zero */
	ucd_req_ptr->header.dword_2 = 0;

	ucd_req_ptr->sc.exp_data_transfer_len =
		cpu_to_be32(lrbp->cmd->sdb.length);

	cdb_len = min_t(unsigned short, lrbp->cmd->cmd_len, UFS_CDB_SIZE);
	memset(ucd_req_ptr->sc.cdb, 0, UFS_CDB_SIZE);
	memcpy(ucd_req_ptr->sc.cdb, lrbp->cmd->cmnd, cdb_len);

	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));
}

/**
 * ufshcd_prepare_utp_query_req_upiu() - fills the utp_transfer_req_desc,
 * for query requsts
 * @hba: UFS hba
 * @lrbp: local reference block pointer
 * @upiu_flags: flags
 */
static void ufshcd_prepare_utp_query_req_upiu(struct ufs_hba *hba,
				struct ufshcd_lrb *lrbp, u32 upiu_flags)
{
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;
	struct ufs_query *query = &hba->dev_cmd.query;
	u16 len = be16_to_cpu(query->request.upiu_req.length);

	/* Query request header */
	ucd_req_ptr->header.dword_0 = UPIU_HEADER_DWORD(
			UPIU_TRANSACTION_QUERY_REQ, upiu_flags,
			lrbp->lun, lrbp->task_tag);
	ucd_req_ptr->header.dword_1 = UPIU_HEADER_DWORD(
			0, query->request.query_func, 0, 0);

	/* Data segment length only need for WRITE_DESC */
	if (query->request.upiu_req.opcode == UPIU_QUERY_OPCODE_WRITE_DESC)
		ucd_req_ptr->header.dword_2 =
			UPIU_HEADER_DWORD(0, 0, (len >> 8), (u8)len);
	else
		ucd_req_ptr->header.dword_2 = 0;

	/* Copy the Query Request buffer as is */
	memcpy(&ucd_req_ptr->qr, &query->request.upiu_req,
			QUERY_OSF_SIZE);

	/* Copy the Descriptor */
	if (query->request.upiu_req.opcode == UPIU_QUERY_OPCODE_WRITE_DESC)
		memcpy(ucd_req_ptr + 1, query->descriptor, len);

	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));
}

static inline void ufshcd_prepare_utp_nop_upiu(struct ufshcd_lrb *lrbp)
{
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;

	memset(ucd_req_ptr, 0, sizeof(struct utp_upiu_req));

	/* command descriptor fields */
	ucd_req_ptr->header.dword_0 =
		UPIU_HEADER_DWORD(
			UPIU_TRANSACTION_NOP_OUT, 0, 0, lrbp->task_tag);
	/* clear rest of the fields of basic header */
	ucd_req_ptr->header.dword_1 = 0;
	ucd_req_ptr->header.dword_2 = 0;

	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));
}

/**
 * ufshcd_comp_devman_upiu - UFS Protocol Information Unit(UPIU)
 *			     for Device Management Purposes
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 */
static int ufshcd_comp_devman_upiu(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	u32 upiu_flags;
	int ret = 0;

	if ((hba->ufs_version == UFSHCI_VERSION_10) ||
	    (hba->ufs_version == UFSHCI_VERSION_11))
		lrbp->command_type = UTP_CMD_TYPE_DEV_MANAGE;
	else
		lrbp->command_type = UTP_CMD_TYPE_UFS_STORAGE;

	ufshcd_prepare_req_desc_hdr(lrbp, &upiu_flags, DMA_NONE);
	if (hba->dev_cmd.type == DEV_CMD_TYPE_QUERY)
		ufshcd_prepare_utp_query_req_upiu(hba, lrbp, upiu_flags);
	else if (hba->dev_cmd.type == DEV_CMD_TYPE_NOP)
		ufshcd_prepare_utp_nop_upiu(lrbp);
	else
		ret = -EINVAL;

	return ret;
}

/**
 * ufshcd_comp_scsi_upiu - UFS Protocol Information Unit(UPIU)
 *			   for SCSI Purposes
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 */
static int ufshcd_comp_scsi_upiu(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	u32 upiu_flags;
	int ret = 0;

	if ((hba->ufs_version == UFSHCI_VERSION_10) ||
	    (hba->ufs_version == UFSHCI_VERSION_11))                                         // 若hba->ufs_version不为UFSHCI_VERSION_10或UFSHCI_VERSION_11
		lrbp->command_type = UTP_CMD_TYPE_SCSI;                                          // 设置lrbp->command_type为UTP_CMD_TYPE_SCSI
	else
		lrbp->command_type = UTP_CMD_TYPE_UFS_STORAGE;                                   // 设置lrbp->command_type为UTP_CMD_TYPE_UFS_STORAGE

	if (likely(lrbp->cmd)) {                                                             // 若lrbp->cmd不为NULL
		ufshcd_prepare_req_desc_hdr(lrbp, &upiu_flags,
						lrbp->cmd->sc_data_direction);
		ufshcd_prepare_utp_scsi_cmd_upiu(lrbp, upiu_flags);
	} else {
		ret = -EINVAL;                                                                   // 设置ret为-EINVAL
	}

	return ret;                                                                          // 返回ret
}

/**
 * ufshcd_upiu_wlun_to_scsi_wlun - maps UPIU W-LUN id to SCSI W-LUN ID
 * @upiu_wlun_id: UPIU W-LUN id
 *
 * Returns SCSI W-LUN id
 */
static inline u16 ufshcd_upiu_wlun_to_scsi_wlun(u8 upiu_wlun_id)
{
	return (upiu_wlun_id & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE;
}

/**
 * ufshcd_queuecommand - main entry point for SCSI requests
 * @host: SCSI host pointer
 * @cmd: command from SCSI Midlayer
 *
 * Returns 0 for success, non-zero in case of failure
 */
static int ufshcd_queuecommand(struct Scsi_Host *host, struct scsi_cmnd *cmd)
{
	struct ufshcd_lrb *lrbp;
	struct ufs_hba *hba;
	unsigned long flags;
	int tag;
	int err = 0;

	hba = shost_priv(host);                                                              // hba = (void *)shost->hostdata

	tag = cmd->request->tag;                                                             // 取出tag
	if (!ufshcd_valid_tag(hba, tag)) {                                                   // 若tag >= 0 && tag < hba->nutrs为false
		dev_err(hba->dev,
			"%s: invalid command tag %d: cmd=0x%p, cmd->request=0x%p",
			__func__, tag, cmd, cmd->request);                                           // 打印提示信息
		BUG();                                                                           // 触发BUG
	}

	if (!down_read_trylock(&hba->clk_scaling_lock))                                      // down_read_trylock: trylock for reading -- returns 1 if successful, 0 if contention
		return SCSI_MLQUEUE_HOST_BUSY;                                                   // 返回SCSI_MLQUEUE_HOST_BUSY

	spin_lock_irqsave(hba->host->host_lock, flags);                                      // 获取锁hba->host->host_lock
	switch (hba->ufshcd_state) {                                                         // 根据hba->ufshcd_state确定下步行为
	case UFSHCD_STATE_OPERATIONAL:                                                       // UFSHCD_STATE_OPERATIONAL
		break;
	case UFSHCD_STATE_EH_SCHEDULED:                                                      // UFSHCD_STATE_EH_SCHEDULED
	case UFSHCD_STATE_RESET:                                                             // UFSHCD_STATE_RESET
		err = SCSI_MLQUEUE_HOST_BUSY;                                                    // 设置err为SCSI_MLQUEUE_HOST_BUSY
		goto out_unlock;                                                                 // 跳转至out_unlock
	case UFSHCD_STATE_ERROR:                                                             // UFSHCD_STATE_ERROR
		set_host_byte(cmd, DID_ERROR);                                                   // *
		cmd->scsi_done(cmd);                                                             // *
		goto out_unlock;                                                                 // 跳转至out_unlock
	default:
		dev_WARN_ONCE(hba->dev, 1, "%s: invalid state %d\n",
				__func__, hba->ufshcd_state);                                            // 打印提示信息
		set_host_byte(cmd, DID_BAD_TARGET);                                              // *
		cmd->scsi_done(cmd);                                                             // *
		goto out_unlock;                                                                 // 跳转至out_unlock
	}

	/* if error handling is in progress, don't issue commands */
	if (ufshcd_eh_in_progress(hba)) {                                                    // *
		set_host_byte(cmd, DID_ERROR);                                                   // *
		cmd->scsi_done(cmd);                                                             // *
		goto out_unlock;                                                                 // 跳转至out_unlock
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);                                 // 释放锁hba->host->host_lock

	hba->req_abort_count = 0;

	/* acquire the tag to make sure device cmds don't use it */
	if (test_and_set_bit_lock(tag, &hba->lrb_in_use)) {                                  // *
		/*
		 * Dev manage command in progress, requeue the command.
		 * Requeuing the command helps in cases where the request *may*
		 * find different tag instead of waiting for dev manage command
		 * completion.
		 */
		err = SCSI_MLQUEUE_HOST_BUSY;                                                    // 设置err为SCSI_MLQUEUE_HOST_BUSY
		goto out;                                                                        // 跳转至out
	}

	err = ufshcd_hold(hba, true);                                                        // *
	if (err) {                                                                           // 若err不为0
		err = SCSI_MLQUEUE_HOST_BUSY;                                                    // 设置err为SCSI_MLQUEUE_HOST_BUSY
		clear_bit_unlock(tag, &hba->lrb_in_use);                                         // *
		goto out;                                                                        // 跳转至out
	}
	WARN_ON(hba->clk_gating.state != CLKS_ON);                                           // 自检警告

	lrbp = &hba->lrb[tag];                                                               // 

	WARN_ON(lrbp->cmd);                                                                  // 自检警告
	lrbp->cmd = cmd;                                                                     // 
	lrbp->sense_bufflen = UFS_SENSE_SIZE;                                                // 
	lrbp->sense_buffer = cmd->sense_buffer;                                              // 
	lrbp->task_tag = tag;                                                                // 
	lrbp->lun = ufshcd_scsi_to_upiu_lun(cmd->device->lun);                               // 
	lrbp->intr_cmd = !ufshcd_is_intr_aggr_allowed(hba) ? true : false;                   // 
	lrbp->req_abort_skip = false;                                                        // 

	ufshcd_comp_scsi_upiu(hba, lrbp);                                                    // 

	err = ufshcd_map_sg(hba, lrbp);                                                      // map scatter-gather list to prdt
	if (err) {                                                                           // 若err不为0
		lrbp->cmd = NULL;                                                                // 设置lrbp->cmd为NULL
		clear_bit_unlock(tag, &hba->lrb_in_use);                                         // 
		goto out;                                                                        // 跳转至out
	}
	/* Make sure descriptors are ready before ringing the doorbell */
	wmb();                                                                               // Make sure descriptors are ready before ringing the doorbell

	/* issue command to the controller */
	spin_lock_irqsave(hba->host->host_lock, flags);                                      // 获取锁hba->host->host_lock
	ufshcd_vops_setup_xfer_req(hba, tag, (lrbp->cmd ? true : false));                    // 若已定义hba->vops->setup_xfer_req则调用hba->vops->setup_xfer_req，在ufs_hba_mtk_vops未定义setup_xfer_req成员
	ufshcd_send_command(hba, tag);                                                       // 重点
out_unlock:
	spin_unlock_irqrestore(hba->host->host_lock, flags);                                 // 释放锁hba->host->host_lock
out:
	up_read(&hba->clk_scaling_lock);                                                     // 释放锁hba->clk_scaling_lock
	return err;                                                                          // 返回err
}

static int ufshcd_compose_dev_cmd(struct ufs_hba *hba,
		struct ufshcd_lrb *lrbp, enum dev_cmd_type cmd_type, int tag)
{
	lrbp->cmd = NULL;
	lrbp->sense_bufflen = 0;
	lrbp->sense_buffer = NULL;
	lrbp->task_tag = tag;
	lrbp->lun = 0; /* device management cmd is not specific to any LUN */
	lrbp->intr_cmd = true; /* No interrupt aggregation */
	hba->dev_cmd.type = cmd_type;

	return ufshcd_comp_devman_upiu(hba, lrbp);
}

static int
ufshcd_clear_cmd(struct ufs_hba *hba, int tag)
{
	int err = 0;
	unsigned long flags;
	u32 mask = 1 << tag;

	/* clear outstanding transaction before retry */
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_utrl_clear(hba, tag);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/*
	 * wait for for h/w to clear corresponding bit in door-bell.
	 * max. wait is 1 sec.
	 */
	err = ufshcd_wait_for_register(hba,
			REG_UTP_TRANSFER_REQ_DOOR_BELL,
			mask, ~mask, 1000, 1000, true);

	return err;
}

static int
ufshcd_check_query_response(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_query_res *query_res = &hba->dev_cmd.query.response;

	/* Get the UPIU response */
	query_res->response = ufshcd_get_rsp_upiu_result(lrbp->ucd_rsp_ptr) >>
				UPIU_RSP_CODE_OFFSET;
	return query_res->response;
}

/**
 * ufshcd_dev_cmd_completion() - handles device management command responses
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 */
static int
ufshcd_dev_cmd_completion(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	int resp;
	int err = 0;

	hba->ufs_stats.last_hibern8_exit_tstamp = ktime_set(0, 0);
	resp = ufshcd_get_req_rsp(lrbp->ucd_rsp_ptr);

	switch (resp) {
	case UPIU_TRANSACTION_NOP_IN:
		if (hba->dev_cmd.type != DEV_CMD_TYPE_NOP) {
			err = -EINVAL;
			dev_err(hba->dev, "%s: unexpected response %x\n",
					__func__, resp);
		}
		break;
	case UPIU_TRANSACTION_QUERY_RSP:
		err = ufshcd_check_query_response(hba, lrbp);
		if (!err)
			err = ufshcd_copy_query_response(hba, lrbp);
		break;
	case UPIU_TRANSACTION_REJECT_UPIU:
		/* TODO: handle Reject UPIU Response */
		err = -EPERM;
		dev_err(hba->dev, "%s: Reject UPIU not fully implemented\n",
				__func__);
		break;
	default:
		err = -EINVAL;
		dev_err(hba->dev, "%s: Invalid device management cmd response: %x\n",
				__func__, resp);
		break;
	}

	return err;
}

static int ufshcd_wait_for_dev_cmd(struct ufs_hba *hba,
		struct ufshcd_lrb *lrbp, int max_timeout)
{
	int err = 0;
	unsigned long time_left;
	unsigned long flags;

	time_left = wait_for_completion_timeout(hba->dev_cmd.complete,
			msecs_to_jiffies(max_timeout));

	/* Make sure descriptors are ready before ringing the doorbell */
	wmb();
	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->dev_cmd.complete = NULL;
	if (likely(time_left)) {
		err = ufshcd_get_tr_ocs(lrbp);
		if (!err)
			err = ufshcd_dev_cmd_completion(hba, lrbp);
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (!time_left) {
		err = -ETIMEDOUT;
		dev_dbg(hba->dev, "%s: dev_cmd request timedout, tag %d\n",
			__func__, lrbp->task_tag);
		if (!ufshcd_clear_cmd(hba, lrbp->task_tag))
			/* successfully cleared the command, retry if needed */
			err = -EAGAIN;
		/*
		 * in case of an error, after clearing the doorbell,
		 * we also need to clear the outstanding_request
		 * field in hba
		 */
		ufshcd_outstanding_req_clear(hba, lrbp->task_tag);
	}

	return err;
}

/**
 * ufshcd_get_dev_cmd_tag - Get device management command tag
 * @hba: per-adapter instance
 * @tag_out: pointer to variable with available slot value
 *
 * Get a free slot and lock it until device management command
 * completes.
 *
 * Returns false if free slot is unavailable for locking, else
 * return true with tag value in @tag.
 */
static bool ufshcd_get_dev_cmd_tag(struct ufs_hba *hba, int *tag_out)
{
	int tag;
	bool ret = false;
	unsigned long tmp;

	if (!tag_out)
		goto out;

	do {
		tmp = ~hba->lrb_in_use;
		tag = find_last_bit(&tmp, hba->nutrs);
		if (tag >= hba->nutrs)
			goto out;
	} while (test_and_set_bit_lock(tag, &hba->lrb_in_use));

	*tag_out = tag;
	ret = true;
out:
	return ret;
}

static inline void ufshcd_put_dev_cmd_tag(struct ufs_hba *hba, int tag)
{
	clear_bit_unlock(tag, &hba->lrb_in_use);
}

/**
 * ufshcd_exec_dev_cmd - API for sending device management requests
 * @hba: UFS hba
 * @cmd_type: specifies the type (NOP, Query...)
 * @timeout: time in seconds
 *
 * NOTE: Since there is only one available tag for device management commands,
 * it is expected you hold the hba->dev_cmd.lock mutex.
 */
static int ufshcd_exec_dev_cmd(struct ufs_hba *hba,
		enum dev_cmd_type cmd_type, int timeout)
{
	struct ufshcd_lrb *lrbp;
	int err;
	int tag;
	struct completion wait;
	unsigned long flags;

	down_read(&hba->clk_scaling_lock);

	/*
	 * Get free slot, sleep if slots are unavailable.
	 * Even though we use wait_event() which sleeps indefinitely,
	 * the maximum wait time is bounded by SCSI request timeout.
	 */
	wait_event(hba->dev_cmd.tag_wq, ufshcd_get_dev_cmd_tag(hba, &tag));

	init_completion(&wait);
	lrbp = &hba->lrb[tag];
	WARN_ON(lrbp->cmd);
	err = ufshcd_compose_dev_cmd(hba, lrbp, cmd_type, tag);
	if (unlikely(err))
		goto out_put_tag;

	hba->dev_cmd.complete = &wait;

	ufshcd_add_query_upiu_trace(hba, tag, "query_send");
	/* Make sure descriptors are ready before ringing the doorbell */
	wmb();
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_vops_setup_xfer_req(hba, tag, (lrbp->cmd ? true : false));
	ufshcd_send_command(hba, tag);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	err = ufshcd_wait_for_dev_cmd(hba, lrbp, timeout);

	ufshcd_add_query_upiu_trace(hba, tag,
			err ? "query_complete_err" : "query_complete");

out_put_tag:
	ufshcd_put_dev_cmd_tag(hba, tag);
	wake_up(&hba->dev_cmd.tag_wq);
	up_read(&hba->clk_scaling_lock);
	return err;
}

/**
 * ufshcd_init_query() - init the query response and request parameters
 * @hba: per-adapter instance
 * @request: address of the request pointer to be initialized
 * @response: address of the response pointer to be initialized
 * @opcode: operation to perform
 * @idn: flag idn to access
 * @index: LU number to access
 * @selector: query/flag/descriptor further identification
 */
static inline void ufshcd_init_query(struct ufs_hba *hba,
		struct ufs_query_req **request, struct ufs_query_res **response,
		enum query_opcode opcode, u8 idn, u8 index, u8 selector)
{
	*request = &hba->dev_cmd.query.request;
	*response = &hba->dev_cmd.query.response;
	memset(*request, 0, sizeof(struct ufs_query_req));
	memset(*response, 0, sizeof(struct ufs_query_res));
	(*request)->upiu_req.opcode = opcode;
	(*request)->upiu_req.idn = idn;
	(*request)->upiu_req.index = index;
	(*request)->upiu_req.selector = selector;
}

static int ufshcd_query_flag_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum flag_idn idn, bool *flag_res)
{
	int ret;
	int retries;

	for (retries = 0; retries < QUERY_REQ_RETRIES; retries++) {
		ret = ufshcd_query_flag(hba, opcode, idn, flag_res);
		if (ret)
			dev_dbg(hba->dev,
				"%s: failed with error %d, retries %d\n",
				__func__, ret, retries);
		else
			break;
	}

	if (ret)
		dev_err(hba->dev,
			"%s: query attribute, opcode %d, idn %d, failed with error %d after %d retires\n",
			__func__, opcode, idn, ret, retries);
	return ret;
}

/**
 * ufshcd_query_flag() - API function for sending flag query requests
 * @hba: per-adapter instance
 * @opcode: flag query to perform
 * @idn: flag idn to access
 * @flag_res: the flag value after the query request completes
 *
 * Returns 0 for success, non-zero in case of failure
 */
int ufshcd_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
			enum flag_idn idn, bool *flag_res)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err, index = 0, selector = 0;
	int timeout = QUERY_REQ_TIMEOUT;

	BUG_ON(!hba);

	ufshcd_hold(hba, false);
	mutex_lock(&hba->dev_cmd.lock);
	ufshcd_init_query(hba, &request, &response, opcode, idn, index,
			selector);

	switch (opcode) {
	case UPIU_QUERY_OPCODE_SET_FLAG:
	case UPIU_QUERY_OPCODE_CLEAR_FLAG:
	case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_READ_REQUEST;
		if (!flag_res) {
			/* No dummy reads */
			dev_err(hba->dev, "%s: Invalid argument for read request\n",
					__func__);
			err = -EINVAL;
			goto out_unlock;
		}
		break;
	default:
		dev_err(hba->dev,
			"%s: Expected query flag opcode but got = %d\n",
			__func__, opcode);
		err = -EINVAL;
		goto out_unlock;
	}

	err = ufshcd_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, timeout);

	if (err) {
		dev_err(hba->dev,
			"%s: Sending flag query for idn %d failed, err = %d\n",
			__func__, idn, err);
		goto out_unlock;
	}

	if (flag_res)
		*flag_res = (be32_to_cpu(response->upiu_res.value) &
				MASK_QUERY_UPIU_FLAG_LOC) & 0x1;

out_unlock:
	mutex_unlock(&hba->dev_cmd.lock);
	ufshcd_release(hba);
	return err;
}

/**
 * ufshcd_query_attr - API function for sending attribute requests
 * @hba: per-adapter instance
 * @opcode: attribute opcode
 * @idn: attribute idn to access
 * @index: index field
 * @selector: selector field
 * @attr_val: the attribute value after the query request completes
 *
 * Returns 0 for success, non-zero in case of failure
*/
int ufshcd_query_attr(struct ufs_hba *hba, enum query_opcode opcode,
		      enum attr_idn idn, u8 index, u8 selector, u32 *attr_val)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err;

	BUG_ON(!hba);

	ufshcd_hold(hba, false);
	if (!attr_val) {
		dev_err(hba->dev, "%s: attribute value required for opcode 0x%x\n",
				__func__, opcode);
		err = -EINVAL;
		goto out;
	}

	mutex_lock(&hba->dev_cmd.lock);
	ufshcd_init_query(hba, &request, &response, opcode, idn, index,
			selector);

	switch (opcode) {
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST;
		request->upiu_req.value = cpu_to_be32(*attr_val);
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_READ_REQUEST;
		break;
	default:
		dev_err(hba->dev, "%s: Expected query attr opcode but got = 0x%.2x\n",
				__func__, opcode);
		err = -EINVAL;
		goto out_unlock;
	}

	err = ufshcd_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, QUERY_REQ_TIMEOUT);

	if (err) {
		dev_err(hba->dev, "%s: opcode 0x%.2x for idn %d failed, index %d, err = %d\n",
				__func__, opcode, idn, index, err);
		goto out_unlock;
	}

	*attr_val = be32_to_cpu(response->upiu_res.value);

out_unlock:
	mutex_unlock(&hba->dev_cmd.lock);
out:
	ufshcd_release(hba);
	return err;
}

/**
 * ufshcd_query_attr_retry() - API function for sending query
 * attribute with retries
 * @hba: per-adapter instance
 * @opcode: attribute opcode
 * @idn: attribute idn to access
 * @index: index field
 * @selector: selector field
 * @attr_val: the attribute value after the query request
 * completes
 *
 * Returns 0 for success, non-zero in case of failure
*/
static int ufshcd_query_attr_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum attr_idn idn, u8 index, u8 selector,
	u32 *attr_val)
{
	int ret = 0;
	u32 retries;

	 for (retries = QUERY_REQ_RETRIES; retries > 0; retries--) {
		ret = ufshcd_query_attr(hba, opcode, idn, index,
						selector, attr_val);
		if (ret)
			dev_dbg(hba->dev, "%s: failed with error %d, retries %d\n",
				__func__, ret, retries);
		else
			break;
	}

	if (ret)
		dev_err(hba->dev,
			"%s: query attribute, idn %d, failed with error %d after %d retires\n",
			__func__, idn, ret, QUERY_REQ_RETRIES);
	return ret;
}

static int __ufshcd_query_descriptor(struct ufs_hba *hba,
			enum query_opcode opcode, enum desc_idn idn, u8 index,
			u8 selector, u8 *desc_buf, int *buf_len)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err;

	BUG_ON(!hba);

	ufshcd_hold(hba, false);
	if (!desc_buf) {
		dev_err(hba->dev, "%s: descriptor buffer required for opcode 0x%x\n",
				__func__, opcode);
		err = -EINVAL;
		goto out;
	}

	if (*buf_len < QUERY_DESC_MIN_SIZE || *buf_len > QUERY_DESC_MAX_SIZE) {
		dev_err(hba->dev, "%s: descriptor buffer size (%d) is out of range\n",
				__func__, *buf_len);
		err = -EINVAL;
		goto out;
	}

	mutex_lock(&hba->dev_cmd.lock);
	ufshcd_init_query(hba, &request, &response, opcode, idn, index,
			selector);
	hba->dev_cmd.query.descriptor = desc_buf;
	request->upiu_req.length = cpu_to_be16(*buf_len);

	switch (opcode) {
	case UPIU_QUERY_OPCODE_WRITE_DESC:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST;
		break;
	case UPIU_QUERY_OPCODE_READ_DESC:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_READ_REQUEST;
		break;
	default:
		dev_err(hba->dev,
				"%s: Expected query descriptor opcode but got = 0x%.2x\n",
				__func__, opcode);
		err = -EINVAL;
		goto out_unlock;
	}

	err = ufshcd_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, QUERY_REQ_TIMEOUT);

	if (err) {
		dev_err(hba->dev, "%s: opcode 0x%.2x for idn %d failed, index %d, err = %d\n",
				__func__, opcode, idn, index, err);
		goto out_unlock;
	}

	hba->dev_cmd.query.descriptor = NULL;
	*buf_len = be16_to_cpu(response->upiu_res.length);

out_unlock:
	mutex_unlock(&hba->dev_cmd.lock);
out:
	ufshcd_release(hba);
	return err;
}

/**
 * ufshcd_query_descriptor_retry - API function for sending descriptor requests
 * @hba: per-adapter instance
 * @opcode: attribute opcode
 * @idn: attribute idn to access
 * @index: index field
 * @selector: selector field
 * @desc_buf: the buffer that contains the descriptor
 * @buf_len: length parameter passed to the device
 *
 * Returns 0 for success, non-zero in case of failure.
 * The buf_len parameter will contain, on return, the length parameter
 * received on the response.
 */
int ufshcd_query_descriptor_retry(struct ufs_hba *hba,
				  enum query_opcode opcode,
				  enum desc_idn idn, u8 index,
				  u8 selector,
				  u8 *desc_buf, int *buf_len)
{
	int err;
	int retries;

	for (retries = QUERY_REQ_RETRIES; retries > 0; retries--) {
		err = __ufshcd_query_descriptor(hba, opcode, idn, index,
						selector, desc_buf, buf_len);
		if (!err || err == -EINVAL)
			break;
	}

	return err;
}

/**
 * ufshcd_read_desc_length - read the specified descriptor length from header
 * @hba: Pointer to adapter instance
 * @desc_id: descriptor idn value
 * @desc_index: descriptor index
 * @desc_length: pointer to variable to read the length of descriptor
 *
 * Return 0 in case of success, non-zero otherwise
 */
static int ufshcd_read_desc_length(struct ufs_hba *hba,
	enum desc_idn desc_id,
	int desc_index,
	int *desc_length)
{
	int ret;
	u8 header[QUERY_DESC_HDR_SIZE];
	int header_len = QUERY_DESC_HDR_SIZE;

	if (desc_id >= QUERY_DESC_IDN_MAX)
		return -EINVAL;

	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					desc_id, desc_index, 0, header,
					&header_len);

	if (ret) {
		dev_err(hba->dev, "%s: Failed to get descriptor header id %d",
			__func__, desc_id);
		return ret;
	} else if (desc_id != header[QUERY_DESC_DESC_TYPE_OFFSET]) {
		dev_warn(hba->dev, "%s: descriptor header id %d and desc_id %d mismatch",
			__func__, header[QUERY_DESC_DESC_TYPE_OFFSET],
			desc_id);
		ret = -EINVAL;
	}

	*desc_length = header[QUERY_DESC_LENGTH_OFFSET];
	return ret;

}

/**
 * ufshcd_map_desc_id_to_length - map descriptor IDN to its length
 * @hba: Pointer to adapter instance
 * @desc_id: descriptor idn value
 * @desc_len: mapped desc length (out)
 *
 * Return 0 in case of success, non-zero otherwise
 */
int ufshcd_map_desc_id_to_length(struct ufs_hba *hba,
	enum desc_idn desc_id, int *desc_len)
{
	switch (desc_id) {
	case QUERY_DESC_IDN_DEVICE:
		*desc_len = hba->desc_size.dev_desc;
		break;
	case QUERY_DESC_IDN_POWER:
		*desc_len = hba->desc_size.pwr_desc;
		break;
	case QUERY_DESC_IDN_GEOMETRY:
		*desc_len = hba->desc_size.geom_desc;
		break;
	case QUERY_DESC_IDN_CONFIGURATION:
		*desc_len = hba->desc_size.conf_desc;
		break;
	case QUERY_DESC_IDN_UNIT:
		*desc_len = hba->desc_size.unit_desc;
		break;
	case QUERY_DESC_IDN_INTERCONNECT:
		*desc_len = hba->desc_size.interc_desc;
		break;
	case QUERY_DESC_IDN_STRING:
		*desc_len = QUERY_DESC_MAX_SIZE;
		break;
	case QUERY_DESC_IDN_HEALTH:
		*desc_len = hba->desc_size.hlth_desc;
		break;
	case QUERY_DESC_IDN_RFU_0:
	case QUERY_DESC_IDN_RFU_1:
		*desc_len = 0;
		break;
	default:
		*desc_len = 0;
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(ufshcd_map_desc_id_to_length);

/**
 * ufshcd_read_desc_param - read the specified descriptor parameter
 * @hba: Pointer to adapter instance
 * @desc_id: descriptor idn value
 * @desc_index: descriptor index
 * @param_offset: offset of the parameter to read
 * @param_read_buf: pointer to buffer where parameter would be read
 * @param_size: sizeof(param_read_buf)
 *
 * Return 0 in case of success, non-zero otherwise
 */
int ufshcd_read_desc_param(struct ufs_hba *hba,
			   enum desc_idn desc_id,
			   int desc_index,
			   u8 param_offset,
			   u8 *param_read_buf,
			   u8 param_size)
{
	int ret;
	u8 *desc_buf;
	int buff_len;
	bool is_kmalloc = true;

	/* Safety check */
	if (desc_id >= QUERY_DESC_IDN_MAX || !param_size)
		return -EINVAL;

	/* Get the max length of descriptor from structure filled up at probe
	 * time.
	 */
	ret = ufshcd_map_desc_id_to_length(hba, desc_id, &buff_len);

	/* Sanity checks */
	if (ret || !buff_len) {
		dev_err(hba->dev, "%s: Failed to get full descriptor length",
			__func__);
		return ret;
	}

	/* Check whether we need temp memory */
	if (param_offset != 0 || param_size < buff_len) {
		desc_buf = kmalloc(buff_len, GFP_KERNEL);
		if (!desc_buf)
			return -ENOMEM;
	} else {
		desc_buf = param_read_buf;
		is_kmalloc = false;
	}

	/* Request for full descriptor */
	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					desc_id, desc_index, 0,
					desc_buf, &buff_len);

	if (ret) {
		dev_err(hba->dev, "%s: Failed reading descriptor. desc_id %d, desc_index %d, param_offset %d, ret %d",
			__func__, desc_id, desc_index, param_offset, ret);
		goto out;
	}

	/* Sanity check */
	if (desc_buf[QUERY_DESC_DESC_TYPE_OFFSET] != desc_id) {
		dev_err(hba->dev, "%s: invalid desc_id %d in descriptor header",
			__func__, desc_buf[QUERY_DESC_DESC_TYPE_OFFSET]);
		ret = -EINVAL;
		goto out;
	}

	/* Check wherher we will not copy more data, than available */
	if (is_kmalloc && param_size > buff_len)
		param_size = buff_len;

	if (is_kmalloc)
		memcpy(param_read_buf, &desc_buf[param_offset], param_size);
out:
	if (is_kmalloc)
		kfree(desc_buf);
	return ret;
}

static inline int ufshcd_read_desc(struct ufs_hba *hba,
				   enum desc_idn desc_id,
				   int desc_index,
				   void *buf,
				   u32 size)
{
	return ufshcd_read_desc_param(hba, desc_id, desc_index, 0, buf, size);
}

static inline int ufshcd_read_power_desc(struct ufs_hba *hba,
					 u8 *buf,
					 u32 size)
{
	return ufshcd_read_desc(hba, QUERY_DESC_IDN_POWER, 0, buf, size);
}

static int ufshcd_read_device_desc(struct ufs_hba *hba, u8 *buf, u32 size)
{
	return ufshcd_read_desc(hba, QUERY_DESC_IDN_DEVICE, 0, buf, size);
}

/**
 * struct uc_string_id - unicode string
 *
 * @len: size of this descriptor inclusive
 * @type: descriptor type
 * @uc: unicode string character
 */
struct uc_string_id {
	u8 len;
	u8 type;
	wchar_t uc[0];
} __packed;

/* replace non-printable or non-ASCII characters with spaces */
static inline char ufshcd_remove_non_printable(u8 ch)
{
	return (ch >= 0x20 && ch <= 0x7e) ? ch : ' ';
}

/**
 * ufshcd_read_string_desc - read string descriptor
 * @hba: pointer to adapter instance
 * @desc_index: descriptor index
 * @buf: pointer to buffer where descriptor would be read,
 *       the caller should free the memory.
 * @ascii: if true convert from unicode to ascii characters
 *         null terminated string.
 *
 * Return:
 * *      string size on success.
 * *      -ENOMEM: on allocation failure
 * *      -EINVAL: on a wrong parameter
 */
int ufshcd_read_string_desc(struct ufs_hba *hba, u8 desc_index,
			    u8 **buf, bool ascii)
{
	struct uc_string_id *uc_str;
	u8 *str;
	int ret;

	if (!buf)
		return -EINVAL;

	uc_str = kzalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!uc_str)
		return -ENOMEM;

	ret = ufshcd_read_desc(hba, QUERY_DESC_IDN_STRING,
			       desc_index, uc_str,
			       QUERY_DESC_MAX_SIZE);
	if (ret < 0) {
		dev_err(hba->dev, "Reading String Desc failed after %d retries. err = %d\n",
			QUERY_REQ_RETRIES, ret);
		str = NULL;
		goto out;
	}

	if (uc_str->len <= QUERY_DESC_HDR_SIZE) {
		dev_dbg(hba->dev, "String Desc is of zero length\n");
		str = NULL;
		ret = 0;
		goto out;
	}

	if (ascii) {
		ssize_t ascii_len;
		int i;
		/* remove header and divide by 2 to move from UTF16 to UTF8 */
		ascii_len = (uc_str->len - QUERY_DESC_HDR_SIZE) / 2 + 1;
		str = kzalloc(ascii_len, GFP_KERNEL);
		if (!str) {
			ret = -ENOMEM;
			goto out;
		}

		/*
		 * the descriptor contains string in UTF16 format
		 * we need to convert to utf-8 so it can be displayed
		 */
		ret = utf16s_to_utf8s(uc_str->uc,
				      uc_str->len - QUERY_DESC_HDR_SIZE,
				      UTF16_BIG_ENDIAN, str, ascii_len);

		/* replace non-printable or non-ASCII characters with spaces */
		for (i = 0; i < ret; i++)
			str[i] = ufshcd_remove_non_printable(str[i]);

		str[ret++] = '\0';

	} else {
		str = kmemdup(uc_str, uc_str->len, GFP_KERNEL);
		if (!str) {
			ret = -ENOMEM;
			goto out;
		}
		ret = uc_str->len;
	}
out:
	*buf = str;
	kfree(uc_str);
	return ret;
}

/**
 * ufshcd_read_unit_desc_param - read the specified unit descriptor parameter
 * @hba: Pointer to adapter instance
 * @lun: lun id
 * @param_offset: offset of the parameter to read
 * @param_read_buf: pointer to buffer where parameter would be read
 * @param_size: sizeof(param_read_buf)
 *
 * Return 0 in case of success, non-zero otherwise
 */
static inline int ufshcd_read_unit_desc_param(struct ufs_hba *hba,
					      int lun,
					      enum unit_desc_param param_offset,
					      u8 *param_read_buf,
					      u32 param_size)
{
	/*
	 * Unit descriptors are only available for general purpose LUs (LUN id
	 * from 0 to 7) and RPMB Well known LU.
	 */
	if (!ufs_is_valid_unit_desc_lun(lun))
		return -EOPNOTSUPP;

	return ufshcd_read_desc_param(hba, QUERY_DESC_IDN_UNIT, lun,
				      param_offset, param_read_buf, param_size);
}

/**
 * ufshcd_memory_alloc - allocate memory for host memory space data structures
 * @hba: per adapter instance
 *
 * 1. Allocate DMA memory for Command Descriptor array
 *	Each command descriptor consist of Command UPIU, Response UPIU and PRDT
 * 2. Allocate DMA memory for UTP Transfer Request Descriptor List (UTRDL).
 * 3. Allocate DMA memory for UTP Task Management Request Descriptor List
 *	(UTMRDL)
 * 4. Allocate memory for local reference block(lrb).
 *
 * Returns 0 for success, non-zero in case of failure
 */
static int ufshcd_memory_alloc(struct ufs_hba *hba)                                      // 这是理解原理非常重要的接口
{
	size_t utmrdl_size, utrdl_size, ucdl_size;

	/* Allocate memory for UTP command descriptors */
	ucdl_size = (sizeof(struct utp_transfer_cmd_desc) * hba->nutrs);                     // 计算ucdl_size
	hba->ucdl_base_addr = dmam_alloc_coherent(hba->dev,
						  ucdl_size,
						  &hba->ucdl_dma_addr,
						  GFP_KERNEL);                                                   // 重点：为hba->ucdl_base_addr申请dma内存

	/*
	 * UFSHCI requires UTP command descriptor to be 128 byte aligned.
	 * make sure hba->ucdl_dma_addr is aligned to PAGE_SIZE
	 * if hba->ucdl_dma_addr is aligned to PAGE_SIZE, then it will
	 * be aligned to 128 bytes as well
	 */
	if (!hba->ucdl_base_addr ||
	    WARN_ON(hba->ucdl_dma_addr & (PAGE_SIZE - 1))) {                                 // *
		dev_err(hba->dev,
			"Command Descriptor Memory allocation failed\n");                            // 打印提示信息
		goto out;                                                                        // 跳转至out
	}

	/*
	 * Allocate memory for UTP Transfer descriptors
	 * UFSHCI requires 1024 byte alignment of UTRD
	 */
	utrdl_size = (sizeof(struct utp_transfer_req_desc) * hba->nutrs);                    // 计算utrdl_size
	hba->utrdl_base_addr = dmam_alloc_coherent(hba->dev,
						   utrdl_size,
						   &hba->utrdl_dma_addr,
						   GFP_KERNEL);                                                  // 重点：为hba->utrdl_base_addr申请dma内存
	if (!hba->utrdl_base_addr ||
	    WARN_ON(hba->utrdl_dma_addr & (PAGE_SIZE - 1))) {                                // *
		dev_err(hba->dev,
			"Transfer Descriptor Memory allocation failed\n");                           // 打印提示信息
		goto out;                                                                        // 跳转至out
	}

	/*
	 * Allocate memory for UTP Task Management descriptors
	 * UFSHCI requires 1024 byte alignment of UTMRD
	 */
	utmrdl_size = sizeof(struct utp_task_req_desc) * hba->nutmrs;                        // 计算utmrdl_size
	hba->utmrdl_base_addr = dmam_alloc_coherent(hba->dev,
						    utmrdl_size,
						    &hba->utmrdl_dma_addr,
						    GFP_KERNEL);                                                 // 重点：为hba->utmrdl_base_addr申请dma内存
	if (!hba->utmrdl_base_addr ||
	    WARN_ON(hba->utmrdl_dma_addr & (PAGE_SIZE - 1))) {                               // *
		dev_err(hba->dev,
		"Task Management Descriptor Memory allocation failed\n");                        // 打印提示信息
		goto out;                                                                        // 跳转至out
	}

	/* Allocate memory for local reference block */
	hba->lrb = devm_kcalloc(hba->dev,
				hba->nutrs, sizeof(struct ufshcd_lrb),
				GFP_KERNEL);                                                             // 重点：为hba->lrb申请内存
	if (!hba->lrb) {                                                                     // 若hba->lrb不为NULL
		dev_err(hba->dev, "LRB Memory allocation failed\n");                             // 打印提示信息
		goto out;                                                                        // 跳转至out
	}
	return 0;                                                                            // 返回0
out:
	return -ENOMEM;                                                                      // 返回-ENOMEM
}

/**
 * ufshcd_host_memory_configure - configure local reference block with
 *				memory offsets
 * @hba: per adapter instance
 *
 * Configure Host memory space
 * 1. Update Corresponding UTRD.UCDBA and UTRD.UCDBAU with UCD DMA
 * address.
 * 2. Update each UTRD with Response UPIU offset, Response UPIU length
 * and PRDT offset.
 * 3. Save the corresponding addresses of UTRD, UCD.CMD, UCD.RSP and UCD.PRDT
 * into local reference block.
 */
static void ufshcd_host_memory_configure(struct ufs_hba *hba)                            // 这是理解原理非常重要的接口
{
	struct utp_transfer_cmd_desc *cmd_descp;
	struct utp_transfer_req_desc *utrdlp;
	dma_addr_t cmd_desc_dma_addr;
	dma_addr_t cmd_desc_element_addr;
	u16 response_offset;
	u16 prdt_offset;
	int cmd_desc_size;
	int i;

	utrdlp = hba->utrdl_base_addr;                                                       // 获取utrdlp
	cmd_descp = hba->ucdl_base_addr;                                                     // 获取cmd_descp

	response_offset =
		offsetof(struct utp_transfer_cmd_desc, response_upiu);                           // #define offsetof(TYPE, MEMBER)	((size_t)&((TYPE *)0)->MEMBER)
	prdt_offset =
		offsetof(struct utp_transfer_cmd_desc, prd_table);                               // #define offsetof(TYPE, MEMBER)	((size_t)&((TYPE *)0)->MEMBER)

	cmd_desc_size = sizeof(struct utp_transfer_cmd_desc);                                // 
	cmd_desc_dma_addr = hba->ucdl_dma_addr;                                              // 

	for (i = 0; i < hba->nutrs; i++) {                                                   // 
		/* Configure UTRD with command descriptor base address */
		cmd_desc_element_addr =
				(cmd_desc_dma_addr + (cmd_desc_size * i));                               // 
		utrdlp[i].command_desc_base_addr_lo =
				cpu_to_le32(lower_32_bits(cmd_desc_element_addr));                       // 
		utrdlp[i].command_desc_base_addr_hi =
				cpu_to_le32(upper_32_bits(cmd_desc_element_addr));                       // 

		/* Response upiu and prdt offset should be in double words */
		if (hba->quirks & UFSHCD_QUIRK_PRDT_BYTE_GRAN) {                                 // 
			utrdlp[i].response_upiu_offset =
				cpu_to_le16(response_offset);                                            // 
			utrdlp[i].prd_table_offset =
				cpu_to_le16(prdt_offset);                                                // 
			utrdlp[i].response_upiu_length =
				cpu_to_le16(ALIGNED_UPIU_SIZE);                                          // 
		} else {
			utrdlp[i].response_upiu_offset =
				cpu_to_le16((response_offset >> 2));                                     // 
			utrdlp[i].prd_table_offset =
				cpu_to_le16((prdt_offset >> 2));                                         // 
			utrdlp[i].response_upiu_length =
				cpu_to_le16(ALIGNED_UPIU_SIZE >> 2);                                     // 
		}

		hba->lrb[i].utr_descriptor_ptr = (utrdlp + i);                                   // 
		hba->lrb[i].utrd_dma_addr = hba->utrdl_dma_addr +
				(i * sizeof(struct utp_transfer_req_desc));                              // 
		hba->lrb[i].ucd_req_ptr =
			(struct utp_upiu_req *)(cmd_descp + i);                                      // 
		hba->lrb[i].ucd_req_dma_addr = cmd_desc_element_addr;                            // 
		hba->lrb[i].ucd_rsp_ptr =
			(struct utp_upiu_rsp *)cmd_descp[i].response_upiu;                           // 
		hba->lrb[i].ucd_rsp_dma_addr = cmd_desc_element_addr +
				response_offset;                                                         // 
		hba->lrb[i].ucd_prdt_ptr =
			(struct ufshcd_sg_entry *)cmd_descp[i].prd_table;                            // 
		hba->lrb[i].ucd_prdt_dma_addr = cmd_desc_element_addr +
				prdt_offset;                                                             // 
	}
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

	uic_cmd.command = UIC_CMD_DME_LINK_STARTUP;

	ret = ufshcd_send_uic_cmd(hba, &uic_cmd);
	if (ret)
		dev_dbg(hba->dev,
			"dme-link-startup: error code %d\n", ret);
	return ret;
}
/**
 * ufshcd_dme_reset - UIC command for DME_RESET
 * @hba: per adapter instance
 *
 * DME_RESET command is issued in order to reset UniPro stack.
 * This function now deal with cold reset.
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_dme_reset(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {0};
	int ret;

	uic_cmd.command = UIC_CMD_DME_RESET;

	ret = ufshcd_send_uic_cmd(hba, &uic_cmd);
	if (ret)
		dev_err(hba->dev,
			"dme-reset: error code %d\n", ret);

	return ret;
}

/**
 * ufshcd_dme_enable - UIC command for DME_ENABLE
 * @hba: per adapter instance
 *
 * DME_ENABLE command is issued in order to enable UniPro stack.
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_dme_enable(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {0};
	int ret;

	uic_cmd.command = UIC_CMD_DME_ENABLE;

	ret = ufshcd_send_uic_cmd(hba, &uic_cmd);
	if (ret)
		dev_err(hba->dev,
			"dme-reset: error code %d\n", ret);

	return ret;
}

static inline void ufshcd_add_delay_before_dme_cmd(struct ufs_hba *hba)
{
	#define MIN_DELAY_BEFORE_DME_CMDS_US	1000
	unsigned long min_sleep_time_us;

	if (!(hba->quirks & UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS))
		return;

	/*
	 * last_dme_cmd_tstamp will be 0 only for 1st call to
	 * this function
	 */
	if (unlikely(!ktime_to_us(hba->last_dme_cmd_tstamp))) {
		min_sleep_time_us = MIN_DELAY_BEFORE_DME_CMDS_US;
	} else {
		unsigned long delta =
			(unsigned long) ktime_to_us(
				ktime_sub(ktime_get(),
				hba->last_dme_cmd_tstamp));

		if (delta < MIN_DELAY_BEFORE_DME_CMDS_US)
			min_sleep_time_us =
				MIN_DELAY_BEFORE_DME_CMDS_US - delta;
		else
			return; /* no more delay required */
	}

	/* allow sleep for extra 50us if needed */
	usleep_range(min_sleep_time_us, min_sleep_time_us + 50);
}

/**
 * ufshcd_dme_set_attr - UIC command for DME_SET, DME_PEER_SET
 * @hba: per adapter instance
 * @attr_sel: uic command argument1
 * @attr_set: attribute set type as uic command argument2
 * @mib_val: setting value as uic command argument3
 * @peer: indicate whether peer or local
 *
 * Returns 0 on success, non-zero value on failure
 */
int ufshcd_dme_set_attr(struct ufs_hba *hba, u32 attr_sel,
			u8 attr_set, u32 mib_val, u8 peer)
{
	struct uic_command uic_cmd = {0};
	static const char *const action[] = {
		"dme-set",
		"dme-peer-set"
	};
	const char *set = action[!!peer];
	int ret;
	int retries = UFS_UIC_COMMAND_RETRIES;

	uic_cmd.command = peer ?
		UIC_CMD_DME_PEER_SET : UIC_CMD_DME_SET;
	uic_cmd.argument1 = attr_sel;
	uic_cmd.argument2 = UIC_ARG_ATTR_TYPE(attr_set);
	uic_cmd.argument3 = mib_val;

	do {
		/* for peer attributes we retry upon failure */
		ret = ufshcd_send_uic_cmd(hba, &uic_cmd);
		if (ret)
			dev_dbg(hba->dev, "%s: attr-id 0x%x val 0x%x error code %d\n",
				set, UIC_GET_ATTR_ID(attr_sel), mib_val, ret);
	} while (ret && peer && --retries);

	if (ret)
		dev_err(hba->dev, "%s: attr-id 0x%x val 0x%x failed %d retries\n",
			set, UIC_GET_ATTR_ID(attr_sel), mib_val,
			UFS_UIC_COMMAND_RETRIES - retries);

	return ret;
}
EXPORT_SYMBOL_GPL(ufshcd_dme_set_attr);

/**
 * ufshcd_dme_get_attr - UIC command for DME_GET, DME_PEER_GET
 * @hba: per adapter instance
 * @attr_sel: uic command argument1
 * @mib_val: the value of the attribute as returned by the UIC command
 * @peer: indicate whether peer or local
 *
 * Returns 0 on success, non-zero value on failure
 */
int ufshcd_dme_get_attr(struct ufs_hba *hba, u32 attr_sel,
			u32 *mib_val, u8 peer)
{
	struct uic_command uic_cmd = {0};
	static const char *const action[] = {
		"dme-get",
		"dme-peer-get"
	};
	const char *get = action[!!peer];
	int ret;
	int retries = UFS_UIC_COMMAND_RETRIES;
	struct ufs_pa_layer_attr orig_pwr_info;
	struct ufs_pa_layer_attr temp_pwr_info;
	bool pwr_mode_change = false;

	if (peer && (hba->quirks & UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE)) {
		orig_pwr_info = hba->pwr_info;
		temp_pwr_info = orig_pwr_info;

		if (orig_pwr_info.pwr_tx == FAST_MODE ||
		    orig_pwr_info.pwr_rx == FAST_MODE) {
			temp_pwr_info.pwr_tx = FASTAUTO_MODE;
			temp_pwr_info.pwr_rx = FASTAUTO_MODE;
			pwr_mode_change = true;
		} else if (orig_pwr_info.pwr_tx == SLOW_MODE ||
		    orig_pwr_info.pwr_rx == SLOW_MODE) {
			temp_pwr_info.pwr_tx = SLOWAUTO_MODE;
			temp_pwr_info.pwr_rx = SLOWAUTO_MODE;
			pwr_mode_change = true;
		}
		if (pwr_mode_change) {
			ret = ufshcd_change_power_mode(hba, &temp_pwr_info);
			if (ret)
				goto out;
		}
	}

	uic_cmd.command = peer ?
		UIC_CMD_DME_PEER_GET : UIC_CMD_DME_GET;
	uic_cmd.argument1 = attr_sel;

	do {
		/* for peer attributes we retry upon failure */
		ret = ufshcd_send_uic_cmd(hba, &uic_cmd);
		if (ret)
			dev_dbg(hba->dev, "%s: attr-id 0x%x error code %d\n",
				get, UIC_GET_ATTR_ID(attr_sel), ret);
	} while (ret && peer && --retries);

	if (ret)
		dev_err(hba->dev, "%s: attr-id 0x%x failed %d retries\n",
			get, UIC_GET_ATTR_ID(attr_sel),
			UFS_UIC_COMMAND_RETRIES - retries);

	if (mib_val && !ret)
		*mib_val = uic_cmd.argument3;

	if (peer && (hba->quirks & UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE)
	    && pwr_mode_change)
		ufshcd_change_power_mode(hba, &orig_pwr_info);
out:
	return ret;
}
EXPORT_SYMBOL_GPL(ufshcd_dme_get_attr);

/**
 * ufshcd_uic_pwr_ctrl - executes UIC commands (which affects the link power
 * state) and waits for it to take effect.
 *
 * @hba: per adapter instance
 * @cmd: UIC command to execute
 *
 * DME operations like DME_SET(PA_PWRMODE), DME_HIBERNATE_ENTER &
 * DME_HIBERNATE_EXIT commands take some time to take its effect on both host
 * and device UniPro link and hence it's final completion would be indicated by
 * dedicated status bits in Interrupt Status register (UPMS, UHES, UHXS) in
 * addition to normal UIC command completion Status (UCCS). This function only
 * returns after the relevant status bits indicate the completion.
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_uic_pwr_ctrl(struct ufs_hba *hba, struct uic_command *cmd)
{
	struct completion uic_async_done;
	unsigned long flags;
	u8 status;
	int ret;
	bool reenable_intr = false;

	mutex_lock(&hba->uic_cmd_mutex);
	init_completion(&uic_async_done);
	ufshcd_add_delay_before_dme_cmd(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->uic_async_done = &uic_async_done;
	if (ufshcd_readl(hba, REG_INTERRUPT_ENABLE) & UIC_COMMAND_COMPL) {
		ufshcd_disable_intr(hba, UIC_COMMAND_COMPL);
		/*
		 * Make sure UIC command completion interrupt is disabled before
		 * issuing UIC command.
		 */
		wmb();
		reenable_intr = true;
	}
	ret = __ufshcd_send_uic_cmd(hba, cmd, false);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (ret) {
		dev_err(hba->dev,
			"pwr ctrl cmd 0x%x with mode 0x%x uic error %d\n",
			cmd->command, cmd->argument3, ret);
		goto out;
	}

	if (!wait_for_completion_timeout(hba->uic_async_done,
					 msecs_to_jiffies(UIC_CMD_TIMEOUT))) {
		dev_err(hba->dev,
			"pwr ctrl cmd 0x%x with mode 0x%x completion timeout\n",
			cmd->command, cmd->argument3);
		ret = -ETIMEDOUT;
		goto out;
	}

	status = ufshcd_get_upmcrs(hba);
	if (status != PWR_LOCAL) {
		dev_err(hba->dev,
			"pwr ctrl cmd 0x%x failed, host upmcrs:0x%x\n",
			cmd->command, status);
		ret = (status != PWR_OK) ? status : -1;
	}
out:
	if (ret) {
		ufshcd_print_host_state(hba);
		ufshcd_print_pwr_info(hba);
		ufshcd_print_host_regs(hba);
	}

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->active_uic_cmd = NULL;
	hba->uic_async_done = NULL;
	if (reenable_intr)
		ufshcd_enable_intr(hba, UIC_COMMAND_COMPL);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	mutex_unlock(&hba->uic_cmd_mutex);

	return ret;
}

/**
 * ufshcd_uic_change_pwr_mode - Perform the UIC power mode chage
 *				using DME_SET primitives.
 * @hba: per adapter instance
 * @mode: powr mode value
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_uic_change_pwr_mode(struct ufs_hba *hba, u8 mode)
{
	struct uic_command uic_cmd = {0};
	int ret;

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_PA_RXHSUNTERMCAP) {
		ret = ufshcd_dme_set(hba,
				UIC_ARG_MIB_SEL(PA_RXHSUNTERMCAP, 0), 1);
		if (ret) {
			dev_err(hba->dev, "%s: failed to enable PA_RXHSUNTERMCAP ret %d\n",
						__func__, ret);
			goto out;
		}
	}

	uic_cmd.command = UIC_CMD_DME_SET;
	uic_cmd.argument1 = UIC_ARG_MIB(PA_PWRMODE);
	uic_cmd.argument3 = mode;
	ufshcd_hold(hba, false);
	ret = ufshcd_uic_pwr_ctrl(hba, &uic_cmd);
	ufshcd_release(hba);

out:
	return ret;
}

static int ufshcd_link_recovery(struct ufs_hba *hba)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->ufshcd_state = UFSHCD_STATE_RESET;
	ufshcd_set_eh_in_progress(hba);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	ret = ufshcd_host_reset_and_restore(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (ret)
		hba->ufshcd_state = UFSHCD_STATE_ERROR;
	ufshcd_clear_eh_in_progress(hba);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		dev_err(hba->dev, "%s: link recovery failed, err %d",
			__func__, ret);

	return ret;
}

static int __ufshcd_uic_hibern8_enter(struct ufs_hba *hba)
{
	int ret;
	struct uic_command uic_cmd = {0};
	ktime_t start = ktime_get();

	ufshcd_vops_hibern8_notify(hba, UIC_CMD_DME_HIBER_ENTER, PRE_CHANGE);

	uic_cmd.command = UIC_CMD_DME_HIBER_ENTER;
	ret = ufshcd_uic_pwr_ctrl(hba, &uic_cmd);
	trace_ufshcd_profile_hibern8(dev_name(hba->dev), "enter",
			     ktime_to_us(ktime_sub(ktime_get(), start)), ret);

	if (ret) {
		dev_err(hba->dev, "%s: hibern8 enter failed. ret = %d\n",
			__func__, ret);

		/*
		 * If link recovery fails then return error so that caller
		 * don't retry the hibern8 enter again.
		 */
		if (ufshcd_link_recovery(hba))
			ret = -ENOLINK;
	} else
		ufshcd_vops_hibern8_notify(hba, UIC_CMD_DME_HIBER_ENTER,
								POST_CHANGE);

	return ret;
}

static int ufshcd_uic_hibern8_enter(struct ufs_hba *hba)
{
	int ret = 0, retries;

	for (retries = UIC_HIBERN8_ENTER_RETRIES; retries > 0; retries--) {
		ret = __ufshcd_uic_hibern8_enter(hba);
		if (!ret || ret == -ENOLINK)
			goto out;
	}
out:
	return ret;
}

static int ufshcd_uic_hibern8_exit(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {0};
	int ret;
	ktime_t start = ktime_get();

	ufshcd_vops_hibern8_notify(hba, UIC_CMD_DME_HIBER_EXIT, PRE_CHANGE);

	uic_cmd.command = UIC_CMD_DME_HIBER_EXIT;
	ret = ufshcd_uic_pwr_ctrl(hba, &uic_cmd);
	trace_ufshcd_profile_hibern8(dev_name(hba->dev), "exit",
			     ktime_to_us(ktime_sub(ktime_get(), start)), ret);

	if (ret) {
		dev_err(hba->dev, "%s: hibern8 exit failed. ret = %d\n",
			__func__, ret);
		ret = ufshcd_link_recovery(hba);
	} else {
		ufshcd_vops_hibern8_notify(hba, UIC_CMD_DME_HIBER_EXIT,
								POST_CHANGE);
		hba->ufs_stats.last_hibern8_exit_tstamp = ktime_get();
		hba->ufs_stats.hibern8_exit_cnt++;
	}

	return ret;
}

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

/**
 * ufshcd_make_hba_operational - Make UFS controller operational
 * @hba: per adapter instance
 *
 * To bring UFS host controller to operational state,
 * 1. Enable required interrupts
 * 2. Configure interrupt aggregation
 * 3. Program UTRL and UTMRL base address
 * 4. Configure run-stop-registers
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_make_hba_operational(struct ufs_hba *hba)
{
	int err = 0;
	u32 reg;

	/* Enable required interrupts */
	ufshcd_enable_intr(hba, UFSHCD_ENABLE_INTRS);

	/* Configure interrupt aggregation */
	if (ufshcd_is_intr_aggr_allowed(hba))
		ufshcd_config_intr_aggr(hba, hba->nutrs - 1, INT_AGGR_DEF_TO);
	else
		ufshcd_disable_intr_aggr(hba);

	/* Configure UTRL and UTMRL base address registers */
	ufshcd_writel(hba, lower_32_bits(hba->utrdl_dma_addr),
			REG_UTP_TRANSFER_REQ_LIST_BASE_L);
	ufshcd_writel(hba, upper_32_bits(hba->utrdl_dma_addr),
			REG_UTP_TRANSFER_REQ_LIST_BASE_H);
	ufshcd_writel(hba, lower_32_bits(hba->utmrdl_dma_addr),
			REG_UTP_TASK_REQ_LIST_BASE_L);
	ufshcd_writel(hba, upper_32_bits(hba->utmrdl_dma_addr),
			REG_UTP_TASK_REQ_LIST_BASE_H);

	/*
	 * Make sure base address and interrupt setup are updated before
	 * enabling the run/stop registers below.
	 */
	wmb();

	/*
	 * UCRDY, UTMRLDY and UTRLRDY bits must be 1
	 */
	reg = ufshcd_readl(hba, REG_CONTROLLER_STATUS);
	if (!(ufshcd_get_lists_status(reg))) {
		ufshcd_enable_run_stop_reg(hba);
	} else {
		dev_err(hba->dev,
			"Host controller not ready to process requests");
		err = -EIO;
		goto out;
	}

out:
	return err;
}

/**
 * ufshcd_hba_stop - Send controller to reset state
 * @hba: per adapter instance
 * @can_sleep: perform sleep or just spin
 */
static inline void ufshcd_hba_stop(struct ufs_hba *hba, bool can_sleep)
{
	int err;

	ufshcd_writel(hba, CONTROLLER_DISABLE,  REG_CONTROLLER_ENABLE);
	err = ufshcd_wait_for_register(hba, REG_CONTROLLER_ENABLE,
					CONTROLLER_ENABLE, CONTROLLER_DISABLE,
					10, 1, can_sleep);
	if (err)
		dev_err(hba->dev, "%s: Controller disable failed\n", __func__);
}

/**
 * ufshcd_hba_execute_hce - initialize the controller
 * @hba: per adapter instance
 *
 * The controller resets itself and controller firmware initialization
 * sequence kicks off. When controller is ready it will set
 * the Host Controller Enable bit to 1.
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_hba_execute_hce(struct ufs_hba *hba)
{
	int retry;

	if (!ufshcd_is_hba_active(hba))
		/* change controller state to "reset state" */
		ufshcd_hba_stop(hba, true);

	/* UniPro link is disabled at this point */
	ufshcd_set_link_off(hba);

	ufshcd_vops_hce_enable_notify(hba, PRE_CHANGE);

	/* start controller initialization sequence */
	ufshcd_hba_start(hba);

	/*
	 * To initialize a UFS host controller HCE bit must be set to 1.
	 * During initialization the HCE bit value changes from 1->0->1.
	 * When the host controller completes initialization sequence
	 * it sets the value of HCE bit to 1. The same HCE bit is read back
	 * to check if the controller has completed initialization sequence.
	 * So without this delay the value HCE = 1, set in the previous
	 * instruction might be read back.
	 * This delay can be changed based on the controller.
	 */
	usleep_range(1000, 1100);

	/* wait for the host controller to complete initialization */
	retry = 10;
	while (ufshcd_is_hba_active(hba)) {
		if (retry) {
			retry--;
		} else {
			dev_err(hba->dev,
				"Controller enable failed\n");
			return -EIO;
		}
		usleep_range(5000, 5100);
	}

	/* enable UIC related interrupts */
	ufshcd_enable_intr(hba, UFSHCD_UIC_MASK);

	ufshcd_vops_hce_enable_notify(hba, POST_CHANGE);

	return 0;
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
static int ufshcd_disable_tx_lcc(struct ufs_hba *hba, bool peer)
{
	int tx_lanes, i, err = 0;

	if (!peer)
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
			       &tx_lanes);
	else
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
				    &tx_lanes);
	for (i = 0; i < tx_lanes; i++) {
		if (!peer)
			err = ufshcd_dme_set(hba,
				UIC_ARG_MIB_SEL(TX_LCC_ENABLE,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(i)),
					0);
		else
			err = ufshcd_dme_peer_set(hba,
				UIC_ARG_MIB_SEL(TX_LCC_ENABLE,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(i)),
					0);
		if (err) {
			dev_err(hba->dev, "%s: TX LCC Disable failed, peer = %d, lane = %d, err = %d",
				__func__, peer, i, err);
			break;
		}
	}

	return err;
}

static inline int ufshcd_disable_device_tx_lcc(struct ufs_hba *hba)
{
	return ufshcd_disable_tx_lcc(hba, true);
}

static void ufshcd_update_reg_hist(struct ufs_err_reg_hist *reg_hist,
				   u32 reg)
{
	reg_hist->reg[reg_hist->pos] = reg;
	reg_hist->tstamp[reg_hist->pos] = ktime_get();
	reg_hist->pos = (reg_hist->pos + 1) % UFS_ERR_REG_HIST_LENGTH;
}

/**
 * ufshcd_link_startup - Initialize unipro link startup
 * @hba: per adapter instance
 *
 * Returns 0 for success, non-zero in case of failure
 */
static int ufshcd_link_startup(struct ufs_hba *hba)
{
	int ret;
	int retries = DME_LINKSTARTUP_RETRIES;
	bool link_startup_again = false;

	/*
	 * If UFS device isn't active then we will have to issue link startup
	 * 2 times to make sure the device state move to active.
	 */
	if (!ufshcd_is_ufs_dev_active(hba))
		link_startup_again = true;

link_startup:
	do {
		ufshcd_vops_link_startup_notify(hba, PRE_CHANGE);

		ret = ufshcd_dme_link_startup(hba);

		/* check if device is detected by inter-connect layer */
		if (!ret && !ufshcd_is_device_present(hba)) {
			ufshcd_update_reg_hist(&hba->ufs_stats.link_startup_err,
					       0);
			dev_err(hba->dev, "%s: Device not present\n", __func__);
			ret = -ENXIO;
			goto out;
		}

		/*
		 * DME link lost indication is only received when link is up,
		 * but we can't be sure if the link is up until link startup
		 * succeeds. So reset the local Uni-Pro and try again.
		 */
		if (ret && ufshcd_hba_enable(hba)) {
			ufshcd_update_reg_hist(&hba->ufs_stats.link_startup_err,
					       (u32)ret);
			goto out;
		}
	} while (ret && retries--);

	if (ret) {
		/* failed to get the link up... retire */
		ufshcd_update_reg_hist(&hba->ufs_stats.link_startup_err,
				       (u32)ret);
		goto out;
	}

	if (link_startup_again) {
		link_startup_again = false;
		retries = DME_LINKSTARTUP_RETRIES;
		goto link_startup;
	}

	/* Mark that link is up in PWM-G1, 1-lane, SLOW-AUTO mode */
	ufshcd_init_pwr_info(hba);
	ufshcd_print_pwr_info(hba);

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_LCC) {
		ret = ufshcd_disable_device_tx_lcc(hba);
		if (ret)
			goto out;
	}

	/* Include any host controller configuration via UIC commands */
	ret = ufshcd_vops_link_startup_notify(hba, POST_CHANGE);
	if (ret)
		goto out;

	ret = ufshcd_make_hba_operational(hba);
out:
	if (ret) {
		dev_err(hba->dev, "link startup failed %d\n", ret);
		ufshcd_print_host_state(hba);
		ufshcd_print_pwr_info(hba);
		ufshcd_print_host_regs(hba);
	}
	return ret;
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
 * ufshcd_set_queue_depth - set lun queue depth
 * @sdev: pointer to SCSI device
 *
 * Read bLUQueueDepth value and activate scsi tagged command
 * queueing. For WLUN, queue depth is set to 1. For best-effort
 * cases (bLUQueueDepth = 0) the queue depth is set to a maximum
 * value that host can queue.
 */
static void ufshcd_set_queue_depth(struct scsi_device *sdev)
{
	int ret = 0;
	u8 lun_qdepth;
	struct ufs_hba *hba;

	hba = shost_priv(sdev->host);

	lun_qdepth = hba->nutrs;
	ret = ufshcd_read_unit_desc_param(hba,
					  ufshcd_scsi_to_upiu_lun(sdev->lun),
					  UNIT_DESC_PARAM_LU_Q_DEPTH,
					  &lun_qdepth,
					  sizeof(lun_qdepth));

	/* Some WLUN doesn't support unit descriptor */
	if (ret == -EOPNOTSUPP)
		lun_qdepth = 1;
	else if (!lun_qdepth)
		/* eventually, we can figure out the real queue depth */
		lun_qdepth = hba->nutrs;
	else
		lun_qdepth = min_t(int, lun_qdepth, hba->nutrs);

	dev_dbg(hba->dev, "%s: activate tcq with queue depth %d\n",
			__func__, lun_qdepth);
	scsi_change_queue_depth(sdev, lun_qdepth);
}

/*
 * ufshcd_get_lu_wp - returns the "b_lu_write_protect" from UNIT DESCRIPTOR
 * @hba: per-adapter instance
 * @lun: UFS device lun id
 * @b_lu_write_protect: pointer to buffer to hold the LU's write protect info
 *
 * Returns 0 in case of success and b_lu_write_protect status would be returned
 * @b_lu_write_protect parameter.
 * Returns -ENOTSUPP if reading b_lu_write_protect is not supported.
 * Returns -EINVAL in case of invalid parameters passed to this function.
 */
static int ufshcd_get_lu_wp(struct ufs_hba *hba,
			    u8 lun,
			    u8 *b_lu_write_protect)
{
	int ret;

	if (!b_lu_write_protect)
		ret = -EINVAL;
	/*
	 * According to UFS device spec, RPMB LU can't be write
	 * protected so skip reading bLUWriteProtect parameter for
	 * it. For other W-LUs, UNIT DESCRIPTOR is not available.
	 */
	else if (lun >= UFS_UPIU_MAX_GENERAL_LUN)
		ret = -ENOTSUPP;
	else
		ret = ufshcd_read_unit_desc_param(hba,
					  lun,
					  UNIT_DESC_PARAM_LU_WR_PROTECT,
					  b_lu_write_protect,
					  sizeof(*b_lu_write_protect));
	return ret;
}

/**
 * ufshcd_get_lu_power_on_wp_status - get LU's power on write protect
 * status
 * @hba: per-adapter instance
 * @sdev: pointer to SCSI device
 *
 */
static inline void ufshcd_get_lu_power_on_wp_status(struct ufs_hba *hba,
						    struct scsi_device *sdev)
{
	if (hba->dev_info.f_power_on_wp_en &&
	    !hba->dev_info.is_lu_power_on_wp) {
		u8 b_lu_write_protect;

		if (!ufshcd_get_lu_wp(hba, ufshcd_scsi_to_upiu_lun(sdev->lun),
				      &b_lu_write_protect) &&
		    (b_lu_write_protect == UFS_LU_POWER_ON_WP))
			hba->dev_info.is_lu_power_on_wp = true;
	}
}

/**
 * ufshcd_slave_alloc - handle initial SCSI device configurations
 * @sdev: pointer to SCSI device
 *
 * Returns success
 */
static int ufshcd_slave_alloc(struct scsi_device *sdev)
{
	struct ufs_hba *hba;

	hba = shost_priv(sdev->host);

	/* Mode sense(6) is not supported by UFS, so use Mode sense(10) */
	sdev->use_10_for_ms = 1;

	/* allow SCSI layer to restart the device in case of errors */
	sdev->allow_restart = 1;

	/* REPORT SUPPORTED OPERATION CODES is not supported */
	sdev->no_report_opcodes = 1;

	/* WRITE_SAME command is not supported */
	sdev->no_write_same = 1;

	ufshcd_set_queue_depth(sdev);

	ufshcd_get_lu_power_on_wp_status(hba, sdev);

	return 0;
}

/**
 * ufshcd_change_queue_depth - change queue depth
 * @sdev: pointer to SCSI device
 * @depth: required depth to set
 *
 * Change queue depth and make sure the max. limits are not crossed.
 */
static int ufshcd_change_queue_depth(struct scsi_device *sdev, int depth)
{
	struct ufs_hba *hba = shost_priv(sdev->host);

	if (depth > hba->nutrs)
		depth = hba->nutrs;
	return scsi_change_queue_depth(sdev, depth);
}

/**
 * ufshcd_slave_configure - adjust SCSI device configurations
 * @sdev: pointer to SCSI device
 */
static int ufshcd_slave_configure(struct scsi_device *sdev)
{
	struct request_queue *q = sdev->request_queue;

	blk_queue_update_dma_pad(q, PRDT_DATA_BYTE_COUNT_PAD - 1);
	return 0;
}

/**
 * ufshcd_slave_destroy - remove SCSI device configurations
 * @sdev: pointer to SCSI device
 */
static void ufshcd_slave_destroy(struct scsi_device *sdev)
{
	struct ufs_hba *hba;

	hba = shost_priv(sdev->host);
	/* Drop the reference as it won't be needed anymore */
	if (ufshcd_scsi_to_upiu_lun(sdev->lun) == UFS_UPIU_UFS_DEVICE_WLUN) {
		unsigned long flags;

		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->sdev_ufs_device = NULL;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}
}

/**
 * ufshcd_scsi_cmd_status - Update SCSI command result based on SCSI status
 * @lrbp: pointer to local reference block of completed command
 * @scsi_status: SCSI command status
 *
 * Returns value base on SCSI command status
 */
static inline int
ufshcd_scsi_cmd_status(struct ufshcd_lrb *lrbp, int scsi_status)
{
	int result = 0;

	switch (scsi_status) {
	case SAM_STAT_CHECK_CONDITION:
		ufshcd_copy_sense_data(lrbp);
		/* fallthrough */
	case SAM_STAT_GOOD:
		result |= DID_OK << 16 |
			  COMMAND_COMPLETE << 8 |
			  scsi_status;
		break;
	case SAM_STAT_TASK_SET_FULL:
	case SAM_STAT_BUSY:
	case SAM_STAT_TASK_ABORTED:
		ufshcd_copy_sense_data(lrbp);
		result |= scsi_status;
		break;
	default:
		result |= DID_ERROR << 16;
		break;
	} /* end of switch */

	return result;
}

/**
 * ufshcd_transfer_rsp_status - Get overall status of the response
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block of completed command
 *
 * Returns result of the command to notify SCSI midlayer
 */
static inline int
ufshcd_transfer_rsp_status(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	int result = 0;
	int scsi_status;
	int ocs;

	/* overall command status of utrd */
	ocs = ufshcd_get_tr_ocs(lrbp);

	switch (ocs) {
	case OCS_SUCCESS:
		result = ufshcd_get_req_rsp(lrbp->ucd_rsp_ptr);
		hba->ufs_stats.last_hibern8_exit_tstamp = ktime_set(0, 0);
		switch (result) {
		case UPIU_TRANSACTION_RESPONSE:
			/*
			 * get the response UPIU result to extract
			 * the SCSI command status
			 */
			result = ufshcd_get_rsp_upiu_result(lrbp->ucd_rsp_ptr);

			/*
			 * get the result based on SCSI status response
			 * to notify the SCSI midlayer of the command status
			 */
			scsi_status = result & MASK_SCSI_STATUS;
			result = ufshcd_scsi_cmd_status(lrbp, scsi_status);

			/*
			 * Currently we are only supporting BKOPs exception
			 * events hence we can ignore BKOPs exception event
			 * during power management callbacks. BKOPs exception
			 * event is not expected to be raised in runtime suspend
			 * callback as it allows the urgent bkops.
			 * During system suspend, we are anyway forcefully
			 * disabling the bkops and if urgent bkops is needed
			 * it will be enabled on system resume. Long term
			 * solution could be to abort the system suspend if
			 * UFS device needs urgent BKOPs.
			 */
			if (!hba->pm_op_in_progress &&
			    ufshcd_is_exception_event(lrbp->ucd_rsp_ptr))
				schedule_work(&hba->eeh_work);
			break;
		case UPIU_TRANSACTION_REJECT_UPIU:
			/* TODO: handle Reject UPIU Response */
			result = DID_ERROR << 16;
			dev_err(hba->dev,
				"Reject UPIU not fully implemented\n");
			break;
		default:
			dev_err(hba->dev,
				"Unexpected request response code = %x\n",
				result);
			result = DID_ERROR << 16;
			break;
		}
		break;
	case OCS_ABORTED:
		result |= DID_ABORT << 16;
		break;
	case OCS_INVALID_COMMAND_STATUS:
		result |= DID_REQUEUE << 16;
		break;
	case OCS_INVALID_CMD_TABLE_ATTR:
	case OCS_INVALID_PRDT_ATTR:
	case OCS_MISMATCH_DATA_BUF_SIZE:
	case OCS_MISMATCH_RESP_UPIU_SIZE:
	case OCS_PEER_COMM_FAILURE:
	case OCS_FATAL_ERROR:
	default:
		result |= DID_ERROR << 16;
		dev_err(hba->dev,
				"OCS error from controller = %x for tag %d\n",
				ocs, lrbp->task_tag);
		ufshcd_print_host_regs(hba);
		ufshcd_print_host_state(hba);
		break;
	} /* end of switch */

	if (host_byte(result) != DID_OK)
		ufshcd_print_trs(hba, 1 << lrbp->task_tag, true);
	return result;
}

/**
 * ufshcd_uic_cmd_compl - handle completion of uic command
 * @hba: per adapter instance
 * @intr_status: interrupt status generated by the controller
 */
static void ufshcd_uic_cmd_compl(struct ufs_hba *hba, u32 intr_status)
{
	if ((intr_status & UIC_COMMAND_COMPL) && hba->active_uic_cmd) {
		hba->active_uic_cmd->argument2 |=
			ufshcd_get_uic_cmd_result(hba);
		hba->active_uic_cmd->argument3 =
			ufshcd_get_dme_attr_val(hba);
		complete(&hba->active_uic_cmd->done);
	}

	if ((intr_status & UFSHCD_UIC_PWR_MASK) && hba->uic_async_done)
		complete(hba->uic_async_done);
}

/**
 * __ufshcd_transfer_req_compl - handle SCSI and query command completion
 * @hba: per adapter instance
 * @completed_reqs: requests to complete
 */
static void __ufshcd_transfer_req_compl(struct ufs_hba *hba,
					unsigned long completed_reqs)
{
	struct ufshcd_lrb *lrbp;
	struct scsi_cmnd *cmd;
	int result;
	int index;

	for_each_set_bit(index, &completed_reqs, hba->nutrs) {
		lrbp = &hba->lrb[index];
		cmd = lrbp->cmd;
		if (cmd) {
			ufshcd_add_command_trace(hba, index, "complete");
			result = ufshcd_transfer_rsp_status(hba, lrbp);
			scsi_dma_unmap(cmd);
			cmd->result = result;
			/* Mark completed command as NULL in LRB */
			lrbp->cmd = NULL;
			clear_bit_unlock(index, &hba->lrb_in_use);
			/* Do not touch lrbp after scsi done */
			cmd->scsi_done(cmd);
			__ufshcd_release(hba);
		} else if (lrbp->command_type == UTP_CMD_TYPE_DEV_MANAGE ||
			lrbp->command_type == UTP_CMD_TYPE_UFS_STORAGE) {
			if (hba->dev_cmd.complete) {
				ufshcd_add_command_trace(hba, index,
						"dev_complete");
				complete(hba->dev_cmd.complete);
			}
		}
		if (ufshcd_is_clkscaling_supported(hba))
			hba->clk_scaling.active_reqs--;

		lrbp->compl_time_stamp = ktime_get();
	}

	/* clear corresponding bits of completed commands */
	hba->outstanding_reqs ^= completed_reqs;

	ufshcd_clk_scaling_update_busy(hba);

	/* we might have free'd some tags above */
	wake_up(&hba->dev_cmd.tag_wq);
}

/**
 * ufshcd_transfer_req_compl - handle SCSI and query command completion
 * @hba: per adapter instance
 */
static void ufshcd_transfer_req_compl(struct ufs_hba *hba)
{
	unsigned long completed_reqs;
	u32 tr_doorbell;

	/* Resetting interrupt aggregation counters first and reading the
	 * DOOR_BELL afterward allows us to handle all the completed requests.
	 * In order to prevent other interrupts starvation the DB is read once
	 * after reset. The down side of this solution is the possibility of
	 * false interrupt if device completes another request after resetting
	 * aggregation and before reading the DB.
	 */
	if (ufshcd_is_intr_aggr_allowed(hba) &&
	    !(hba->quirks & UFSHCI_QUIRK_SKIP_RESET_INTR_AGGR))
		ufshcd_reset_intr_aggr(hba);

	tr_doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	completed_reqs = tr_doorbell ^ hba->outstanding_reqs;

	__ufshcd_transfer_req_compl(hba, completed_reqs);
}

/**
 * ufshcd_disable_ee - disable exception event
 * @hba: per-adapter instance
 * @mask: exception event to disable
 *
 * Disables exception event in the device so that the EVENT_ALERT
 * bit is not set.
 *
 * Returns zero on success, non-zero error value on failure.
 */
static int ufshcd_disable_ee(struct ufs_hba *hba, u16 mask)
{
	int err = 0;
	u32 val;

	if (!(hba->ee_ctrl_mask & mask))
		goto out;

	val = hba->ee_ctrl_mask & ~mask;
	val &= MASK_EE_STATUS;
	err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
			QUERY_ATTR_IDN_EE_CONTROL, 0, 0, &val);
	if (!err)
		hba->ee_ctrl_mask &= ~mask;
out:
	return err;
}

/**
 * ufshcd_enable_ee - enable exception event
 * @hba: per-adapter instance
 * @mask: exception event to enable
 *
 * Enable corresponding exception event in the device to allow
 * device to alert host in critical scenarios.
 *
 * Returns zero on success, non-zero error value on failure.
 */
static int ufshcd_enable_ee(struct ufs_hba *hba, u16 mask)
{
	int err = 0;
	u32 val;

	if (hba->ee_ctrl_mask & mask)
		goto out;

	val = hba->ee_ctrl_mask | mask;
	val &= MASK_EE_STATUS;
	err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
			QUERY_ATTR_IDN_EE_CONTROL, 0, 0, &val);
	if (!err)
		hba->ee_ctrl_mask |= mask;
out:
	return err;
}

/**
 * ufshcd_enable_auto_bkops - Allow device managed BKOPS
 * @hba: per-adapter instance
 *
 * Allow device to manage background operations on its own. Enabling
 * this might lead to inconsistent latencies during normal data transfers
 * as the device is allowed to manage its own way of handling background
 * operations.
 *
 * Returns zero on success, non-zero on failure.
 */
static int ufshcd_enable_auto_bkops(struct ufs_hba *hba)
{
	int err = 0;

	if (hba->auto_bkops_enabled)
		goto out;

	err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG,
			QUERY_FLAG_IDN_BKOPS_EN, NULL);
	if (err) {
		dev_err(hba->dev, "%s: failed to enable bkops %d\n",
				__func__, err);
		goto out;
	}

	hba->auto_bkops_enabled = true;
	trace_ufshcd_auto_bkops_state(dev_name(hba->dev), "Enabled");

	/* No need of URGENT_BKOPS exception from the device */
	err = ufshcd_disable_ee(hba, MASK_EE_URGENT_BKOPS);
	if (err)
		dev_err(hba->dev, "%s: failed to disable exception event %d\n",
				__func__, err);
out:
	return err;
}

/**
 * ufshcd_disable_auto_bkops - block device in doing background operations
 * @hba: per-adapter instance
 *
 * Disabling background operations improves command response latency but
 * has drawback of device moving into critical state where the device is
 * not-operable. Make sure to call ufshcd_enable_auto_bkops() whenever the
 * host is idle so that BKOPS are managed effectively without any negative
 * impacts.
 *
 * Returns zero on success, non-zero on failure.
 */
static int ufshcd_disable_auto_bkops(struct ufs_hba *hba)
{
	int err = 0;

	if (!hba->auto_bkops_enabled)
		goto out;

	/*
	 * If host assisted BKOPs is to be enabled, make sure
	 * urgent bkops exception is allowed.
	 */
	err = ufshcd_enable_ee(hba, MASK_EE_URGENT_BKOPS);
	if (err) {
		dev_err(hba->dev, "%s: failed to enable exception event %d\n",
				__func__, err);
		goto out;
	}

	err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG,
			QUERY_FLAG_IDN_BKOPS_EN, NULL);
	if (err) {
		dev_err(hba->dev, "%s: failed to disable bkops %d\n",
				__func__, err);
		ufshcd_disable_ee(hba, MASK_EE_URGENT_BKOPS);
		goto out;
	}

	hba->auto_bkops_enabled = false;
	trace_ufshcd_auto_bkops_state(dev_name(hba->dev), "Disabled");
out:
	return err;
}

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

static inline int ufshcd_get_bkops_status(struct ufs_hba *hba, u32 *status)
{
	return ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_BKOPS_STATUS, 0, 0, status);
}

/**
 * ufshcd_bkops_ctrl - control the auto bkops based on current bkops status
 * @hba: per-adapter instance
 * @status: bkops_status value
 *
 * Read the bkops_status from the UFS device and Enable fBackgroundOpsEn
 * flag in the device to permit background operations if the device
 * bkops_status is greater than or equal to "status" argument passed to
 * this function, disable otherwise.
 *
 * Returns 0 for success, non-zero in case of failure.
 *
 * NOTE: Caller of this function can check the "hba->auto_bkops_enabled" flag
 * to know whether auto bkops is enabled or disabled after this function
 * returns control to it.
 */
static int ufshcd_bkops_ctrl(struct ufs_hba *hba,
			     enum bkops_status status)
{
	int err;
	u32 curr_status = 0;

	err = ufshcd_get_bkops_status(hba, &curr_status);
	if (err) {
		dev_err(hba->dev, "%s: failed to get BKOPS status %d\n",
				__func__, err);
		goto out;
	} else if (curr_status > BKOPS_STATUS_MAX) {
		dev_err(hba->dev, "%s: invalid BKOPS status %d\n",
				__func__, curr_status);
		err = -EINVAL;
		goto out;
	}

	if (curr_status >= status)
		err = ufshcd_enable_auto_bkops(hba);
	else
		err = ufshcd_disable_auto_bkops(hba);
out:
	return err;
}

/**
 * ufshcd_urgent_bkops - handle urgent bkops exception event
 * @hba: per-adapter instance
 *
 * Enable fBackgroundOpsEn flag in the device to permit background
 * operations.
 *
 * If BKOPs is enabled, this function returns 0, 1 if the bkops in not enabled
 * and negative error value for any other failure.
 */
static int ufshcd_urgent_bkops(struct ufs_hba *hba)
{
	return ufshcd_bkops_ctrl(hba, hba->urgent_bkops_lvl);
}

static inline int ufshcd_get_ee_status(struct ufs_hba *hba, u32 *status)
{
	return ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_EE_STATUS, 0, 0, status);
}

static void ufshcd_bkops_exception_event_handler(struct ufs_hba *hba)
{
	int err;
	u32 curr_status = 0;

	if (hba->is_urgent_bkops_lvl_checked)
		goto enable_auto_bkops;

	err = ufshcd_get_bkops_status(hba, &curr_status);
	if (err) {
		dev_err(hba->dev, "%s: failed to get BKOPS status %d\n",
				__func__, err);
		goto out;
	}

	/*
	 * We are seeing that some devices are raising the urgent bkops
	 * exception events even when BKOPS status doesn't indicate performace
	 * impacted or critical. Handle these device by determining their urgent
	 * bkops status at runtime.
	 */
	if (curr_status < BKOPS_STATUS_PERF_IMPACT) {
		dev_err(hba->dev, "%s: device raised urgent BKOPS exception for bkops status %d\n",
				__func__, curr_status);
		/* update the current status as the urgent bkops level */
		hba->urgent_bkops_lvl = curr_status;
		hba->is_urgent_bkops_lvl_checked = true;
	}

enable_auto_bkops:
	err = ufshcd_enable_auto_bkops(hba);
out:
	if (err < 0)
		dev_err(hba->dev, "%s: failed to handle urgent bkops %d\n",
				__func__, err);
}

/**
 * ufshcd_exception_event_handler - handle exceptions raised by device
 * @work: pointer to work data
 *
 * Read bExceptionEventStatus attribute from the device and handle the
 * exception event accordingly.
 */
static void ufshcd_exception_event_handler(struct work_struct *work)
{
	struct ufs_hba *hba;
	int err;
	u32 status = 0;
	hba = container_of(work, struct ufs_hba, eeh_work);

	pm_runtime_get_sync(hba->dev);
	scsi_block_requests(hba->host);
	err = ufshcd_get_ee_status(hba, &status);
	if (err) {
		dev_err(hba->dev, "%s: failed to get exception status %d\n",
				__func__, err);
		goto out;
	}

	status &= hba->ee_ctrl_mask;

	if (status & MASK_EE_URGENT_BKOPS)
		ufshcd_bkops_exception_event_handler(hba);

out:
	scsi_unblock_requests(hba->host);
	pm_runtime_put_sync(hba->dev);
	return;
}

/* Complete requests that have door-bell cleared */
static void ufshcd_complete_requests(struct ufs_hba *hba)
{
	ufshcd_transfer_req_compl(hba);
	ufshcd_tmc_handler(hba);
}

/**
 * ufshcd_quirk_dl_nac_errors - This function checks if error handling is
 *				to recover from the DL NAC errors or not.
 * @hba: per-adapter instance
 *
 * Returns true if error handling is required, false otherwise
 */
static bool ufshcd_quirk_dl_nac_errors(struct ufs_hba *hba)
{
	unsigned long flags;
	bool err_handling = true;

	spin_lock_irqsave(hba->host->host_lock, flags);
	/*
	 * UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS only workaround the
	 * device fatal error and/or DL NAC & REPLAY timeout errors.
	 */
	if (hba->saved_err & (CONTROLLER_FATAL_ERROR | SYSTEM_BUS_FATAL_ERROR))
		goto out;

	if ((hba->saved_err & DEVICE_FATAL_ERROR) ||
	    ((hba->saved_err & UIC_ERROR) &&
	     (hba->saved_uic_err & UFSHCD_UIC_DL_TCx_REPLAY_ERROR)))
		goto out;

	if ((hba->saved_err & UIC_ERROR) &&
	    (hba->saved_uic_err & UFSHCD_UIC_DL_NAC_RECEIVED_ERROR)) {
		int err;
		/*
		 * wait for 50ms to see if we can get any other errors or not.
		 */
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		msleep(50);
		spin_lock_irqsave(hba->host->host_lock, flags);

		/*
		 * now check if we have got any other severe errors other than
		 * DL NAC error?
		 */
		if ((hba->saved_err & INT_FATAL_ERRORS) ||
		    ((hba->saved_err & UIC_ERROR) &&
		    (hba->saved_uic_err & ~UFSHCD_UIC_DL_NAC_RECEIVED_ERROR)))
			goto out;

		/*
		 * As DL NAC is the only error received so far, send out NOP
		 * command to confirm if link is still active or not.
		 *   - If we don't get any response then do error recovery.
		 *   - If we get response then clear the DL NAC error bit.
		 */

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		err = ufshcd_verify_dev_init(hba);
		spin_lock_irqsave(hba->host->host_lock, flags);

		if (err)
			goto out;

		/* Link seems to be alive hence ignore the DL NAC errors */
		if (hba->saved_uic_err == UFSHCD_UIC_DL_NAC_RECEIVED_ERROR)
			hba->saved_err &= ~UIC_ERROR;
		/* clear NAC error */
		hba->saved_uic_err &= ~UFSHCD_UIC_DL_NAC_RECEIVED_ERROR;
		if (!hba->saved_uic_err) {
			err_handling = false;
			goto out;
		}
	}
out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return err_handling;
}

/**
 * ufshcd_err_handler - handle UFS errors that require s/w attention
 * @work: pointer to work structure
 */
static void ufshcd_err_handler(struct work_struct *work)
{
	struct ufs_hba *hba;
	unsigned long flags;
	u32 err_xfer = 0;
	u32 err_tm = 0;
	int err = 0;
	int tag;
	bool needs_reset = false;

	hba = container_of(work, struct ufs_hba, eh_work);

	pm_runtime_get_sync(hba->dev);
	ufshcd_hold(hba, false);

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->ufshcd_state == UFSHCD_STATE_RESET)
		goto out;

	hba->ufshcd_state = UFSHCD_STATE_RESET;
	ufshcd_set_eh_in_progress(hba);

	/* Complete requests that have door-bell cleared by h/w */
	ufshcd_complete_requests(hba);

	if (hba->dev_quirks & UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS) {
		bool ret;

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		/* release the lock as ufshcd_quirk_dl_nac_errors() may sleep */
		ret = ufshcd_quirk_dl_nac_errors(hba);
		spin_lock_irqsave(hba->host->host_lock, flags);
		if (!ret)
			goto skip_err_handling;
	}
	if ((hba->saved_err & INT_FATAL_ERRORS) ||
	    (hba->saved_err & UFSHCD_UIC_HIBERN8_MASK) ||
	    ((hba->saved_err & UIC_ERROR) &&
	    (hba->saved_uic_err & (UFSHCD_UIC_DL_PA_INIT_ERROR |
				   UFSHCD_UIC_DL_NAC_RECEIVED_ERROR |
				   UFSHCD_UIC_DL_TCx_REPLAY_ERROR))))
		needs_reset = true;

	/*
	 * if host reset is required then skip clearing the pending
	 * transfers forcefully because they will automatically get
	 * cleared after link startup.
	 */
	if (needs_reset)
		goto skip_pending_xfer_clear;

	/* release lock as clear command might sleep */
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	/* Clear pending transfer requests */
	for_each_set_bit(tag, &hba->outstanding_reqs, hba->nutrs) {
		if (ufshcd_clear_cmd(hba, tag)) {
			err_xfer = true;
			goto lock_skip_pending_xfer_clear;
		}
	}

	/* Clear pending task management requests */
	for_each_set_bit(tag, &hba->outstanding_tasks, hba->nutmrs) {
		if (ufshcd_clear_tm_cmd(hba, tag)) {
			err_tm = true;
			goto lock_skip_pending_xfer_clear;
		}
	}

lock_skip_pending_xfer_clear:
	spin_lock_irqsave(hba->host->host_lock, flags);

	/* Complete the requests that are cleared by s/w */
	ufshcd_complete_requests(hba);

	if (err_xfer || err_tm)
		needs_reset = true;

skip_pending_xfer_clear:
	/* Fatal errors need reset */
	if (needs_reset) {
		unsigned long max_doorbells = (1UL << hba->nutrs) - 1;

		/*
		 * ufshcd_reset_and_restore() does the link reinitialization
		 * which will need atleast one empty doorbell slot to send the
		 * device management commands (NOP and query commands).
		 * If there is no slot empty at this moment then free up last
		 * slot forcefully.
		 */
		if (hba->outstanding_reqs == max_doorbells)
			__ufshcd_transfer_req_compl(hba,
						    (1UL << (hba->nutrs - 1)));

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		err = ufshcd_reset_and_restore(hba);
		spin_lock_irqsave(hba->host->host_lock, flags);
		if (err) {
			dev_err(hba->dev, "%s: reset and restore failed\n",
					__func__);
			hba->ufshcd_state = UFSHCD_STATE_ERROR;
		}
		/*
		 * Inform scsi mid-layer that we did reset and allow to handle
		 * Unit Attention properly.
		 */
		scsi_report_bus_reset(hba->host, 0);
		hba->saved_err = 0;
		hba->saved_uic_err = 0;
	}

skip_err_handling:
	if (!needs_reset) {
		hba->ufshcd_state = UFSHCD_STATE_OPERATIONAL;
		if (hba->saved_err || hba->saved_uic_err)
			dev_err_ratelimited(hba->dev, "%s: exit: saved_err 0x%x saved_uic_err 0x%x",
			    __func__, hba->saved_err, hba->saved_uic_err);
	}

	ufshcd_clear_eh_in_progress(hba);

out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_scsi_unblock_requests(hba);
	ufshcd_release(hba);
	pm_runtime_put_sync(hba->dev);
}

/**
 * ufshcd_update_uic_error - check and set fatal UIC error flags.
 * @hba: per-adapter instance
 */
static void ufshcd_update_uic_error(struct ufs_hba *hba)
{
	u32 reg;

	/* PHY layer lane error */
	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER);
	/* Ignore LINERESET indication, as this is not an error */
	if ((reg & UIC_PHY_ADAPTER_LAYER_ERROR) &&
			(reg & UIC_PHY_ADAPTER_LAYER_LANE_ERR_MASK)) {
		/*
		 * To know whether this error is fatal or not, DB timeout
		 * must be checked but this error is handled separately.
		 */
		dev_dbg(hba->dev, "%s: UIC Lane error reported\n", __func__);
		ufshcd_update_reg_hist(&hba->ufs_stats.pa_err, reg);
	}

	/* PA_INIT_ERROR is fatal and needs UIC reset */
	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_DATA_LINK_LAYER);
	if (reg)
		ufshcd_update_reg_hist(&hba->ufs_stats.dl_err, reg);

	if (reg & UIC_DATA_LINK_LAYER_ERROR_PA_INIT)
		hba->uic_error |= UFSHCD_UIC_DL_PA_INIT_ERROR;
	else if (hba->dev_quirks &
		   UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS) {
		if (reg & UIC_DATA_LINK_LAYER_ERROR_NAC_RECEIVED)
			hba->uic_error |=
				UFSHCD_UIC_DL_NAC_RECEIVED_ERROR;
		else if (reg & UIC_DATA_LINK_LAYER_ERROR_TCx_REPLAY_TIMEOUT)
			hba->uic_error |= UFSHCD_UIC_DL_TCx_REPLAY_ERROR;
	}

	/* UIC NL/TL/DME errors needs software retry */
	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_NETWORK_LAYER);
	if (reg) {
		ufshcd_update_reg_hist(&hba->ufs_stats.nl_err, reg);
		hba->uic_error |= UFSHCD_UIC_NL_ERROR;
	}

	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_TRANSPORT_LAYER);
	if (reg) {
		ufshcd_update_reg_hist(&hba->ufs_stats.tl_err, reg);
		hba->uic_error |= UFSHCD_UIC_TL_ERROR;
	}

	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_DME);
	if (reg) {
		ufshcd_update_reg_hist(&hba->ufs_stats.dme_err, reg);
		hba->uic_error |= UFSHCD_UIC_DME_ERROR;
	}

	dev_dbg(hba->dev, "%s: UIC error flags = 0x%08x\n",
			__func__, hba->uic_error);
}

static bool ufshcd_is_auto_hibern8_error(struct ufs_hba *hba,
					 u32 intr_mask)
{
	if (!ufshcd_is_auto_hibern8_supported(hba))
		return false;

	if (!(intr_mask & UFSHCD_UIC_HIBERN8_MASK))
		return false;

	if (hba->active_uic_cmd &&
	    (hba->active_uic_cmd->command == UIC_CMD_DME_HIBER_ENTER ||
	    hba->active_uic_cmd->command == UIC_CMD_DME_HIBER_EXIT))
		return false;

	return true;
}

/**
 * ufshcd_check_errors - Check for errors that need s/w attention
 * @hba: per-adapter instance
 */
static void ufshcd_check_errors(struct ufs_hba *hba)
{
	bool queue_eh_work = false;

	if (hba->errors & INT_FATAL_ERRORS) {
		ufshcd_update_reg_hist(&hba->ufs_stats.fatal_err, hba->errors);
		queue_eh_work = true;
	}

	if (hba->errors & UIC_ERROR) {
		hba->uic_error = 0;
		ufshcd_update_uic_error(hba);
		if (hba->uic_error)
			queue_eh_work = true;
	}

	if (hba->errors & UFSHCD_UIC_HIBERN8_MASK) {
		dev_err(hba->dev,
			"%s: Auto Hibern8 %s failed - status: 0x%08x, upmcrs: 0x%08x\n",
			__func__, (hba->errors & UIC_HIBERNATE_ENTER) ?
			"Enter" : "Exit",
			hba->errors, ufshcd_get_upmcrs(hba));
		ufshcd_update_reg_hist(&hba->ufs_stats.auto_hibern8_err,
				       hba->errors);
		queue_eh_work = true;
	}

	if (queue_eh_work) {
		/*
		 * update the transfer error masks to sticky bits, let's do this
		 * irrespective of current ufshcd_state.
		 */
		hba->saved_err |= hba->errors;
		hba->saved_uic_err |= hba->uic_error;

		/* handle fatal errors only when link is functional */
		if (hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL) {
			/* block commands from scsi mid-layer */
			ufshcd_scsi_block_requests(hba);

			hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED;

			/* dump controller state before resetting */
			if (hba->saved_err & (INT_FATAL_ERRORS | UIC_ERROR)) {
				bool pr_prdt = !!(hba->saved_err &
						SYSTEM_BUS_FATAL_ERROR);

				dev_err(hba->dev, "%s: saved_err 0x%x saved_uic_err 0x%x\n",
					__func__, hba->saved_err,
					hba->saved_uic_err);

				ufshcd_print_host_regs(hba);
				ufshcd_print_pwr_info(hba);
				ufshcd_print_tmrs(hba, hba->outstanding_tasks);
				ufshcd_print_trs(hba, hba->outstanding_reqs,
							pr_prdt);
			}
			schedule_work(&hba->eh_work);
		}
	}
	/*
	 * if (!queue_eh_work) -
	 * Other errors are either non-fatal where host recovers
	 * itself without s/w intervention or errors that will be
	 * handled by the SCSI core layer.
	 */
}

/**
 * ufshcd_tmc_handler - handle task management function completion
 * @hba: per adapter instance
 */
static void ufshcd_tmc_handler(struct ufs_hba *hba)
{
	u32 tm_doorbell;

	tm_doorbell = ufshcd_readl(hba, REG_UTP_TASK_REQ_DOOR_BELL);
	hba->tm_condition = tm_doorbell ^ hba->outstanding_tasks;
	wake_up(&hba->tm_wq);
}

/**
 * ufshcd_sl_intr - Interrupt service routine
 * @hba: per adapter instance
 * @intr_status: contains interrupts generated by the controller
 */
static void ufshcd_sl_intr(struct ufs_hba *hba, u32 intr_status)
{
	hba->errors = UFSHCD_ERROR_MASK & intr_status;

	if (ufshcd_is_auto_hibern8_error(hba, intr_status))
		hba->errors |= (UFSHCD_UIC_HIBERN8_MASK & intr_status);

	if (hba->errors)
		ufshcd_check_errors(hba);

	if (intr_status & UFSHCD_UIC_MASK)
		ufshcd_uic_cmd_compl(hba, intr_status);

	if (intr_status & UTP_TASK_REQ_COMPL)
		ufshcd_tmc_handler(hba);

	if (intr_status & UTP_TRANSFER_REQ_COMPL)
		ufshcd_transfer_req_compl(hba);
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

	spin_lock(hba->host->host_lock);
	intr_status = ufshcd_readl(hba, REG_INTERRUPT_STATUS);

	/*
	 * There could be max of hba->nutrs reqs in flight and in worst case
	 * if the reqs get finished 1 by 1 after the interrupt status is
	 * read, make sure we handle them by checking the interrupt status
	 * again in a loop until we process all of the reqs before returning.
	 */
	do {
		enabled_intr_status =
			intr_status & ufshcd_readl(hba, REG_INTERRUPT_ENABLE);
		if (intr_status)
			ufshcd_writel(hba, intr_status, REG_INTERRUPT_STATUS);
		if (enabled_intr_status) {
			ufshcd_sl_intr(hba, enabled_intr_status);
			retval = IRQ_HANDLED;
		}

		intr_status = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
	} while (intr_status && --retries);

	spin_unlock(hba->host->host_lock);
	return retval;
}

static int ufshcd_clear_tm_cmd(struct ufs_hba *hba, int tag)
{
	int err = 0;
	u32 mask = 1 << tag;
	unsigned long flags;

	if (!test_bit(tag, &hba->outstanding_tasks))
		goto out;

	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_utmrl_clear(hba, tag);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/* poll for max. 1 sec to clear door bell register by h/w */
	err = ufshcd_wait_for_register(hba,
			REG_UTP_TASK_REQ_DOOR_BELL,
			mask, 0, 1000, 1000, true);
out:
	return err;
}

static int __ufshcd_issue_tm_cmd(struct ufs_hba *hba,
		struct utp_task_req_desc *treq, u8 tm_function)
{
	struct Scsi_Host *host = hba->host;
	unsigned long flags;
	int free_slot, task_tag, err;

	/*
	 * Get free slot, sleep if slots are unavailable.
	 * Even though we use wait_event() which sleeps indefinitely,
	 * the maximum wait time is bounded by %TM_CMD_TIMEOUT.
	 */
	wait_event(hba->tm_tag_wq, ufshcd_get_tm_free_slot(hba, &free_slot));
	ufshcd_hold(hba, false);

	spin_lock_irqsave(host->host_lock, flags);
	task_tag = hba->nutrs + free_slot;

	treq->req_header.dword_0 |= cpu_to_be32(task_tag);

	memcpy(hba->utmrdl_base_addr + free_slot, treq, sizeof(*treq));
	ufshcd_vops_setup_task_mgmt(hba, free_slot, tm_function);

	/* send command to the controller */
	__set_bit(free_slot, &hba->outstanding_tasks);

	/* Make sure descriptors are ready before ringing the task doorbell */
	wmb();

	ufshcd_writel(hba, 1 << free_slot, REG_UTP_TASK_REQ_DOOR_BELL);
	/* Make sure that doorbell is committed immediately */
	wmb();

	spin_unlock_irqrestore(host->host_lock, flags);

	ufshcd_add_tm_upiu_trace(hba, task_tag, "tm_send");

	/* wait until the task management command is completed */
	err = wait_event_timeout(hba->tm_wq,
			test_bit(free_slot, &hba->tm_condition),
			msecs_to_jiffies(TM_CMD_TIMEOUT));
	if (!err) {
		ufshcd_add_tm_upiu_trace(hba, task_tag, "tm_complete_err");
		dev_err(hba->dev, "%s: task management cmd 0x%.2x timed-out\n",
				__func__, tm_function);
		if (ufshcd_clear_tm_cmd(hba, free_slot))
			dev_WARN(hba->dev, "%s: unable clear tm cmd (slot %d) after timeout\n",
					__func__, free_slot);
		err = -ETIMEDOUT;
	} else {
		err = 0;
		memcpy(treq, hba->utmrdl_base_addr + free_slot, sizeof(*treq));

		ufshcd_add_tm_upiu_trace(hba, task_tag, "tm_complete");
	}

	spin_lock_irqsave(hba->host->host_lock, flags);
	__clear_bit(free_slot, &hba->outstanding_tasks);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	clear_bit(free_slot, &hba->tm_condition);
	ufshcd_put_tm_slot(hba, free_slot);
	wake_up(&hba->tm_tag_wq);

	ufshcd_release(hba);
	return err;
}

/**
 * ufshcd_issue_tm_cmd - issues task management commands to controller
 * @hba: per adapter instance
 * @lun_id: LUN ID to which TM command is sent
 * @task_id: task ID to which the TM command is applicable
 * @tm_function: task management function opcode
 * @tm_response: task management service response return value
 *
 * Returns non-zero value on error, zero on success.
 */
static int ufshcd_issue_tm_cmd(struct ufs_hba *hba, int lun_id, int task_id,
		u8 tm_function, u8 *tm_response)
{
	struct utp_task_req_desc treq = { { 0 }, };
	int ocs_value, err;

	/* Configure task request descriptor */
	treq.header.dword_0 = cpu_to_le32(UTP_REQ_DESC_INT_CMD);
	treq.header.dword_2 = cpu_to_le32(OCS_INVALID_COMMAND_STATUS);

	/* Configure task request UPIU */
	treq.req_header.dword_0 = cpu_to_be32(lun_id << 8) |
				  cpu_to_be32(UPIU_TRANSACTION_TASK_REQ << 24);
	treq.req_header.dword_1 = cpu_to_be32(tm_function << 16);

	/*
	 * The host shall provide the same value for LUN field in the basic
	 * header and for Input Parameter.
	 */
	treq.input_param1 = cpu_to_be32(lun_id);
	treq.input_param2 = cpu_to_be32(task_id);

	err = __ufshcd_issue_tm_cmd(hba, &treq, tm_function);
	if (err == -ETIMEDOUT)
		return err;

	ocs_value = le32_to_cpu(treq.header.dword_2) & MASK_OCS;
	if (ocs_value != OCS_SUCCESS)
		dev_err(hba->dev, "%s: failed, ocs = 0x%x\n",
				__func__, ocs_value);
	else if (tm_response)
		*tm_response = be32_to_cpu(treq.output_param1) &
				MASK_TM_SERVICE_RESP;
	return err;
}

/**
 * ufshcd_issue_devman_upiu_cmd - API for sending "utrd" type requests
 * @hba:	per-adapter instance
 * @req_upiu:	upiu request
 * @rsp_upiu:	upiu reply
 * @msgcode:	message code, one of UPIU Transaction Codes Initiator to Target
 * @desc_buff:	pointer to descriptor buffer, NULL if NA
 * @buff_len:	descriptor size, 0 if NA
 * @desc_op:	descriptor operation
 *
 * Those type of requests uses UTP Transfer Request Descriptor - utrd.
 * Therefore, it "rides" the device management infrastructure: uses its tag and
 * tasks work queues.
 *
 * Since there is only one available tag for device management commands,
 * the caller is expected to hold the hba->dev_cmd.lock mutex.
 */
static int ufshcd_issue_devman_upiu_cmd(struct ufs_hba *hba,
					struct utp_upiu_req *req_upiu,
					struct utp_upiu_req *rsp_upiu,
					u8 *desc_buff, int *buff_len,
					int cmd_type,
					enum query_opcode desc_op)
{
	struct ufshcd_lrb *lrbp;
	int err = 0;
	int tag;
	struct completion wait;
	unsigned long flags;
	u32 upiu_flags;

	down_read(&hba->clk_scaling_lock);

	wait_event(hba->dev_cmd.tag_wq, ufshcd_get_dev_cmd_tag(hba, &tag));

	init_completion(&wait);
	lrbp = &hba->lrb[tag];
	WARN_ON(lrbp->cmd);

	lrbp->cmd = NULL;
	lrbp->sense_bufflen = 0;
	lrbp->sense_buffer = NULL;
	lrbp->task_tag = tag;
	lrbp->lun = 0;
	lrbp->intr_cmd = true;
	hba->dev_cmd.type = cmd_type;

	switch (hba->ufs_version) {
	case UFSHCI_VERSION_10:
	case UFSHCI_VERSION_11:
		lrbp->command_type = UTP_CMD_TYPE_DEV_MANAGE;
		break;
	default:
		lrbp->command_type = UTP_CMD_TYPE_UFS_STORAGE;
		break;
	}

	/* update the task tag in the request upiu */
	req_upiu->header.dword_0 |= cpu_to_be32(tag);

	ufshcd_prepare_req_desc_hdr(lrbp, &upiu_flags, DMA_NONE);

	/* just copy the upiu request as it is */
	memcpy(lrbp->ucd_req_ptr, req_upiu, sizeof(*lrbp->ucd_req_ptr));
	if (desc_buff && desc_op == UPIU_QUERY_OPCODE_WRITE_DESC) {
		/* The Data Segment Area is optional depending upon the query
		 * function value. for WRITE DESCRIPTOR, the data segment
		 * follows right after the tsf.
		 */
		memcpy(lrbp->ucd_req_ptr + 1, desc_buff, *buff_len);
		*buff_len = 0;
	}

	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));

	hba->dev_cmd.complete = &wait;

	/* Make sure descriptors are ready before ringing the doorbell */
	wmb();
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_send_command(hba, tag);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/*
	 * ignore the returning value here - ufshcd_check_query_response is
	 * bound to fail since dev_cmd.query and dev_cmd.type were left empty.
	 * read the response directly ignoring all errors.
	 */
	ufshcd_wait_for_dev_cmd(hba, lrbp, QUERY_REQ_TIMEOUT);

	/* just copy the upiu response as it is */
	memcpy(rsp_upiu, lrbp->ucd_rsp_ptr, sizeof(*rsp_upiu));
	if (desc_buff && desc_op == UPIU_QUERY_OPCODE_READ_DESC) {
		u8 *descp = (u8 *)lrbp->ucd_rsp_ptr + sizeof(*rsp_upiu);
		u16 resp_len = be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2) &
			       MASK_QUERY_DATA_SEG_LEN;

		if (*buff_len >= resp_len) {
			memcpy(desc_buff, descp, resp_len);
			*buff_len = resp_len;
		} else {
			dev_warn(hba->dev, "rsp size is bigger than buffer");
			*buff_len = 0;
			err = -EINVAL;
		}
	}

	ufshcd_put_dev_cmd_tag(hba, tag);
	wake_up(&hba->dev_cmd.tag_wq);
	up_read(&hba->clk_scaling_lock);
	return err;
}

/**
 * ufshcd_exec_raw_upiu_cmd - API function for sending raw upiu commands
 * @hba:	per-adapter instance
 * @req_upiu:	upiu request
 * @rsp_upiu:	upiu reply - only 8 DW as we do not support scsi commands
 * @msgcode:	message code, one of UPIU Transaction Codes Initiator to Target
 * @desc_buff:	pointer to descriptor buffer, NULL if NA
 * @buff_len:	descriptor size, 0 if NA
 * @desc_op:	descriptor operation
 *
 * Supports UTP Transfer requests (nop and query), and UTP Task
 * Management requests.
 * It is up to the caller to fill the upiu conent properly, as it will
 * be copied without any further input validations.
 */
int ufshcd_exec_raw_upiu_cmd(struct ufs_hba *hba,
			     struct utp_upiu_req *req_upiu,
			     struct utp_upiu_req *rsp_upiu,
			     int msgcode,
			     u8 *desc_buff, int *buff_len,
			     enum query_opcode desc_op)
{
	int err;
	int cmd_type = DEV_CMD_TYPE_QUERY;
	struct utp_task_req_desc treq = { { 0 }, };
	int ocs_value;
	u8 tm_f = be32_to_cpu(req_upiu->header.dword_1) >> 16 & MASK_TM_FUNC;

	switch (msgcode) {
	case UPIU_TRANSACTION_NOP_OUT:
		cmd_type = DEV_CMD_TYPE_NOP;
		/* fall through */
	case UPIU_TRANSACTION_QUERY_REQ:
		ufshcd_hold(hba, false);
		mutex_lock(&hba->dev_cmd.lock);
		err = ufshcd_issue_devman_upiu_cmd(hba, req_upiu, rsp_upiu,
						   desc_buff, buff_len,
						   cmd_type, desc_op);
		mutex_unlock(&hba->dev_cmd.lock);
		ufshcd_release(hba);

		break;
	case UPIU_TRANSACTION_TASK_REQ:
		treq.header.dword_0 = cpu_to_le32(UTP_REQ_DESC_INT_CMD);
		treq.header.dword_2 = cpu_to_le32(OCS_INVALID_COMMAND_STATUS);

		memcpy(&treq.req_header, req_upiu, sizeof(*req_upiu));

		err = __ufshcd_issue_tm_cmd(hba, &treq, tm_f);
		if (err == -ETIMEDOUT)
			break;

		ocs_value = le32_to_cpu(treq.header.dword_2) & MASK_OCS;
		if (ocs_value != OCS_SUCCESS) {
			dev_err(hba->dev, "%s: failed, ocs = 0x%x\n", __func__,
				ocs_value);
			break;
		}

		memcpy(rsp_upiu, &treq.rsp_header, sizeof(*rsp_upiu));

		break;
	default:
		err = -EINVAL;

		break;
	}

	return err;
}

/**
 * ufshcd_eh_device_reset_handler - device reset handler registered to
 *                                    scsi layer.
 * @cmd: SCSI command pointer
 *
 * Returns SUCCESS/FAILED
 */
static int ufshcd_eh_device_reset_handler(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	unsigned int tag;
	u32 pos;
	int err;
	u8 resp = 0xF;
	struct ufshcd_lrb *lrbp;
	unsigned long flags;

	host = cmd->device->host;
	hba = shost_priv(host);
	tag = cmd->request->tag;

	lrbp = &hba->lrb[tag];
	err = ufshcd_issue_tm_cmd(hba, lrbp->lun, 0, UFS_LOGICAL_RESET, &resp);
	if (err || resp != UPIU_TASK_MANAGEMENT_FUNC_COMPL) {
		if (!err)
			err = resp;
		goto out;
	}

	/* clear the commands that were pending for corresponding LUN */
	for_each_set_bit(pos, &hba->outstanding_reqs, hba->nutrs) {
		if (hba->lrb[pos].lun == lrbp->lun) {
			err = ufshcd_clear_cmd(hba, pos);
			if (err)
				break;
		}
	}
	spin_lock_irqsave(host->host_lock, flags);
	ufshcd_transfer_req_compl(hba);
	spin_unlock_irqrestore(host->host_lock, flags);

out:
	hba->req_abort_count = 0;
	ufshcd_update_reg_hist(&hba->ufs_stats.dev_reset, (u32)err);
	if (!err) {
		err = SUCCESS;
	} else {
		dev_err(hba->dev, "%s: failed with err %d\n", __func__, err);
		err = FAILED;
	}
	return err;
}

static void ufshcd_set_req_abort_skip(struct ufs_hba *hba, unsigned long bitmap)
{
	struct ufshcd_lrb *lrbp;
	int tag;

	for_each_set_bit(tag, &bitmap, hba->nutrs) {
		lrbp = &hba->lrb[tag];
		lrbp->req_abort_skip = true;
	}
}

/**
 * ufshcd_abort - abort a specific command
 * @cmd: SCSI command pointer
 *
 * Abort the pending command in device by sending UFS_ABORT_TASK task management
 * command, and in host controller by clearing the door-bell register. There can
 * be race between controller sending the command to the device while abort is
 * issued. To avoid that, first issue UFS_QUERY_TASK to check if the command is
 * really issued and then try to abort it.
 *
 * Returns SUCCESS/FAILED
 */
static int ufshcd_abort(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	unsigned long flags;
	unsigned int tag;
	int err = 0;
	int poll_cnt;
	u8 resp = 0xF;
	struct ufshcd_lrb *lrbp;
	u32 reg;

	host = cmd->device->host;
	hba = shost_priv(host);
	tag = cmd->request->tag;
	lrbp = &hba->lrb[tag];
	if (!ufshcd_valid_tag(hba, tag)) {
		dev_err(hba->dev,
			"%s: invalid command tag %d: cmd=0x%p, cmd->request=0x%p",
			__func__, tag, cmd, cmd->request);
		BUG();
	}

	/*
	 * Task abort to the device W-LUN is illegal. When this command
	 * will fail, due to spec violation, scsi err handling next step
	 * will be to send LU reset which, again, is a spec violation.
	 * To avoid these unnecessary/illegal step we skip to the last error
	 * handling stage: reset and restore.
	 */
	if (lrbp->lun == UFS_UPIU_UFS_DEVICE_WLUN)
		return ufshcd_eh_host_reset_handler(cmd);

	ufshcd_hold(hba, false);
	reg = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	/* If command is already aborted/completed, return SUCCESS */
	if (!(test_bit(tag, &hba->outstanding_reqs))) {
		dev_err(hba->dev,
			"%s: cmd at tag %d already completed, outstanding=0x%lx, doorbell=0x%x\n",
			__func__, tag, hba->outstanding_reqs, reg);
		goto out;
	}

	if (!(reg & (1 << tag))) {
		dev_err(hba->dev,
		"%s: cmd was completed, but without a notifying intr, tag = %d",
		__func__, tag);
	}

	/* Print Transfer Request of aborted task */
	dev_err(hba->dev, "%s: Device abort task at tag %d\n", __func__, tag);

	/*
	 * Print detailed info about aborted request.
	 * As more than one request might get aborted at the same time,
	 * print full information only for the first aborted request in order
	 * to reduce repeated printouts. For other aborted requests only print
	 * basic details.
	 */
	scsi_print_command(hba->lrb[tag].cmd);
	if (!hba->req_abort_count) {
		ufshcd_update_reg_hist(&hba->ufs_stats.task_abort, 0);
		ufshcd_print_host_regs(hba);
		ufshcd_print_host_state(hba);
		ufshcd_print_pwr_info(hba);
		ufshcd_print_trs(hba, 1 << tag, true);
	} else {
		ufshcd_print_trs(hba, 1 << tag, false);
	}
	hba->req_abort_count++;

	/* Skip task abort in case previous aborts failed and report failure */
	if (lrbp->req_abort_skip) {
		err = -EIO;
		goto out;
	}

	for (poll_cnt = 100; poll_cnt; poll_cnt--) {
		err = ufshcd_issue_tm_cmd(hba, lrbp->lun, lrbp->task_tag,
				UFS_QUERY_TASK, &resp);
		if (!err && resp == UPIU_TASK_MANAGEMENT_FUNC_SUCCEEDED) {
			/* cmd pending in the device */
			dev_err(hba->dev, "%s: cmd pending in the device. tag = %d\n",
				__func__, tag);
			break;
		} else if (!err && resp == UPIU_TASK_MANAGEMENT_FUNC_COMPL) {
			/*
			 * cmd not pending in the device, check if it is
			 * in transition.
			 */
			dev_err(hba->dev, "%s: cmd at tag %d not pending in the device.\n",
				__func__, tag);
			reg = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
			if (reg & (1 << tag)) {
				/* sleep for max. 200us to stabilize */
				usleep_range(100, 200);
				continue;
			}
			/* command completed already */
			dev_err(hba->dev, "%s: cmd at tag %d successfully cleared from DB.\n",
				__func__, tag);
			goto out;
		} else {
			dev_err(hba->dev,
				"%s: no response from device. tag = %d, err %d\n",
				__func__, tag, err);
			if (!err)
				err = resp; /* service response error */
			goto out;
		}
	}

	if (!poll_cnt) {
		err = -EBUSY;
		goto out;
	}

	err = ufshcd_issue_tm_cmd(hba, lrbp->lun, lrbp->task_tag,
			UFS_ABORT_TASK, &resp);
	if (err || resp != UPIU_TASK_MANAGEMENT_FUNC_COMPL) {
		if (!err) {
			err = resp; /* service response error */
			dev_err(hba->dev, "%s: issued. tag = %d, err %d\n",
				__func__, tag, err);
		}
		goto out;
	}

	err = ufshcd_clear_cmd(hba, tag);
	if (err) {
		dev_err(hba->dev, "%s: Failed clearing cmd at tag %d, err %d\n",
			__func__, tag, err);
		goto out;
	}

	scsi_dma_unmap(cmd);

	spin_lock_irqsave(host->host_lock, flags);
	ufshcd_outstanding_req_clear(hba, tag);
	hba->lrb[tag].cmd = NULL;
	spin_unlock_irqrestore(host->host_lock, flags);

	clear_bit_unlock(tag, &hba->lrb_in_use);
	wake_up(&hba->dev_cmd.tag_wq);

out:
	if (!err) {
		err = SUCCESS;
	} else {
		dev_err(hba->dev, "%s: failed with err %d\n", __func__, err);
		ufshcd_set_req_abort_skip(hba, hba->outstanding_reqs);
		err = FAILED;
	}

	/*
	 * This ufshcd_release() corresponds to the original scsi cmd that got
	 * aborted here (as we won't get any IRQ for it).
	 */
	ufshcd_release(hba);
	return err;
}

/**
 * ufshcd_host_reset_and_restore - reset and restore host controller
 * @hba: per-adapter instance
 *
 * Note that host controller reset may issue DME_RESET to
 * local and remote (device) Uni-Pro stack and the attributes
 * are reset to default state.
 *
 * Returns zero on success, non-zero on failure
 */
static int ufshcd_host_reset_and_restore(struct ufs_hba *hba)
{
	int err;
	unsigned long flags;

	/* Reset the host controller */
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_hba_stop(hba, false);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/* scale up clocks to max frequency before full reinitialization */
	ufshcd_scale_clks(hba, true);

	err = ufshcd_hba_enable(hba);
	if (err)
		goto out;

	/* Establish the link again and restore the device */
	err = ufshcd_probe_hba(hba);

	if (!err && (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL))
		err = -EIO;
out:
	if (err)
		dev_err(hba->dev, "%s: Host init failed %d\n", __func__, err);
	ufshcd_update_reg_hist(&hba->ufs_stats.host_reset, (u32)err);
	return err;
}

/**
 * ufshcd_reset_and_restore - reset and re-initialize host/device
 * @hba: per-adapter instance
 *
 * Reset and recover device, host and re-establish link. This
 * is helpful to recover the communication in fatal error conditions.
 *
 * Returns zero on success, non-zero on failure
 */
static int ufshcd_reset_and_restore(struct ufs_hba *hba)
{
	int err = 0;
	unsigned long flags;
	int retries = MAX_HOST_RESET_RETRIES;

	do {
		/* Reset the attached device */
		ufshcd_vops_device_reset(hba);

		err = ufshcd_host_reset_and_restore(hba);
	} while (err && --retries);

	/*
	 * After reset the door-bell might be cleared, complete
	 * outstanding requests in s/w here.
	 */
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_transfer_req_compl(hba);
	ufshcd_tmc_handler(hba);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return err;
}

/**
 * ufshcd_eh_host_reset_handler - host reset handler registered to scsi layer
 * @cmd: SCSI command pointer
 *
 * Returns SUCCESS/FAILED
 */
static int ufshcd_eh_host_reset_handler(struct scsi_cmnd *cmd)
{
	int err;
	unsigned long flags;
	struct ufs_hba *hba;

	hba = shost_priv(cmd->device->host);

	ufshcd_hold(hba, false);
	/*
	 * Check if there is any race with fatal error handling.
	 * If so, wait for it to complete. Even though fatal error
	 * handling does reset and restore in some cases, don't assume
	 * anything out of it. We are just avoiding race here.
	 */
	do {
		spin_lock_irqsave(hba->host->host_lock, flags);
		if (!(work_pending(&hba->eh_work) ||
			    hba->ufshcd_state == UFSHCD_STATE_RESET ||
			    hba->ufshcd_state == UFSHCD_STATE_EH_SCHEDULED))
			break;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		dev_dbg(hba->dev, "%s: reset in progress\n", __func__);
		flush_work(&hba->eh_work);
	} while (1);

	hba->ufshcd_state = UFSHCD_STATE_RESET;
	ufshcd_set_eh_in_progress(hba);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	err = ufshcd_reset_and_restore(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (!err) {
		err = SUCCESS;
		hba->ufshcd_state = UFSHCD_STATE_OPERATIONAL;
	} else {
		err = FAILED;
		hba->ufshcd_state = UFSHCD_STATE_ERROR;
	}
	ufshcd_clear_eh_in_progress(hba);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	ufshcd_release(hba);
	return err;
}

/**
 * ufshcd_get_max_icc_level - calculate the ICC level
 * @sup_curr_uA: max. current supported by the regulator
 * @start_scan: row at the desc table to start scan from
 * @buff: power descriptor buffer
 *
 * Returns calculated max ICC level for specific regulator
 */
static u32 ufshcd_get_max_icc_level(int sup_curr_uA, u32 start_scan, char *buff)
{
	int i;
	int curr_uA;
	u16 data;
	u16 unit;

	for (i = start_scan; i >= 0; i--) {
		data = be16_to_cpup((__be16 *)&buff[2 * i]);
		unit = (data & ATTR_ICC_LVL_UNIT_MASK) >>
						ATTR_ICC_LVL_UNIT_OFFSET;
		curr_uA = data & ATTR_ICC_LVL_VALUE_MASK;
		switch (unit) {
		case UFSHCD_NANO_AMP:
			curr_uA = curr_uA / 1000;
			break;
		case UFSHCD_MILI_AMP:
			curr_uA = curr_uA * 1000;
			break;
		case UFSHCD_AMP:
			curr_uA = curr_uA * 1000 * 1000;
			break;
		case UFSHCD_MICRO_AMP:
		default:
			break;
		}
		if (sup_curr_uA >= curr_uA)
			break;
	}
	if (i < 0) {
		i = 0;
		pr_err("%s: Couldn't find valid icc_level = %d", __func__, i);
	}

	return (u32)i;
}

/**
 * ufshcd_calc_icc_level - calculate the max ICC level
 * In case regulators are not initialized we'll return 0
 * @hba: per-adapter instance
 * @desc_buf: power descriptor buffer to extract ICC levels from.
 * @len: length of desc_buff
 *
 * Returns calculated ICC level
 */
static u32 ufshcd_find_max_sup_active_icc_level(struct ufs_hba *hba,
							u8 *desc_buf, int len)
{
	u32 icc_level = 0;

	if (!hba->vreg_info.vcc || !hba->vreg_info.vccq ||
						!hba->vreg_info.vccq2) {
		dev_err(hba->dev,
			"%s: Regulator capability was not set, actvIccLevel=%d",
							__func__, icc_level);
		goto out;
	}

	if (hba->vreg_info.vcc && hba->vreg_info.vcc->max_uA)
		icc_level = ufshcd_get_max_icc_level(
				hba->vreg_info.vcc->max_uA,
				POWER_DESC_MAX_ACTV_ICC_LVLS - 1,
				&desc_buf[PWR_DESC_ACTIVE_LVLS_VCC_0]);

	if (hba->vreg_info.vccq && hba->vreg_info.vccq->max_uA)
		icc_level = ufshcd_get_max_icc_level(
				hba->vreg_info.vccq->max_uA,
				icc_level,
				&desc_buf[PWR_DESC_ACTIVE_LVLS_VCCQ_0]);

	if (hba->vreg_info.vccq2 && hba->vreg_info.vccq2->max_uA)
		icc_level = ufshcd_get_max_icc_level(
				hba->vreg_info.vccq2->max_uA,
				icc_level,
				&desc_buf[PWR_DESC_ACTIVE_LVLS_VCCQ2_0]);
out:
	return icc_level;
}

static void ufshcd_init_icc_levels(struct ufs_hba *hba)
{
	int ret;
	int buff_len = hba->desc_size.pwr_desc;
	u8 *desc_buf;

	desc_buf = kmalloc(buff_len, GFP_KERNEL);
	if (!desc_buf)
		return;

	ret = ufshcd_read_power_desc(hba, desc_buf, buff_len);
	if (ret) {
		dev_err(hba->dev,
			"%s: Failed reading power descriptor.len = %d ret = %d",
			__func__, buff_len, ret);
		goto out;
	}

	hba->init_prefetch_data.icc_level =
			ufshcd_find_max_sup_active_icc_level(hba,
			desc_buf, buff_len);
	dev_dbg(hba->dev, "%s: setting icc_level 0x%x",
			__func__, hba->init_prefetch_data.icc_level);

	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
		QUERY_ATTR_IDN_ACTIVE_ICC_LVL, 0, 0,
		&hba->init_prefetch_data.icc_level);

	if (ret)
		dev_err(hba->dev,
			"%s: Failed configuring bActiveICCLevel = %d ret = %d",
			__func__, hba->init_prefetch_data.icc_level , ret);

out:
	kfree(desc_buf);
}

/*
 * The Well Known LUNS (SAM-3) in our int representation of a LUN
 */
#define SCSI_W_LUN_BASE 0xc100
#define SCSI_W_LUN_REPORT_LUNS (SCSI_W_LUN_BASE + 1)
#define SCSI_W_LUN_ACCESS_CONTROL (SCSI_W_LUN_BASE + 2)
#define SCSI_W_LUN_TARGET_LOG_PAGE (SCSI_W_LUN_BASE + 3)

static inline int scsi_is_wlun(u64 lun)
{
	return (lun & 0xff00) == SCSI_W_LUN_BASE;
}

/*
 * UFS device may have standard LUs and LUN id could be from 0x00 to
 * 0x7F. Standard LUs use "Peripheral Device Addressing Format".
 * UFS device may also have the Well Known LUs (also referred as W-LU)
 * which again could be from 0x00 to 0x7F. For W-LUs, device only use
 * the "Extended Addressing Format" which means the W-LUNs would be
 * from 0xc100 (SCSI_W_LUN_BASE) onwards.
 * This means max. LUN number reported from UFS device could be 0xC17F.
 */
#define UFS_UPIU_MAX_UNIT_NUM_ID	0x7F
#define UFS_MAX_LUNS		(SCSI_W_LUN_BASE + UFS_UPIU_MAX_UNIT_NUM_ID)
#define UFS_UPIU_WLUN_ID	(1 << 7)                                                     // spec规定：The 8-bit LUN field in UPIU is used to provide either LUN or W-LUN
#define UFS_UPIU_MAX_GENERAL_LUN	8

/* Well known logical unit id in LUN field of UPIU */
enum {
	UFS_UPIU_REPORT_LUNS_WLUN	= 0x81,                                                  // 0b 1000 0001
	UFS_UPIU_UFS_DEVICE_WLUN	= 0xD0,                                                  // 0b 1101 0000
	UFS_UPIU_BOOT_WLUN		= 0xB0,                                                      // 0b 1011 0000
	UFS_UPIU_RPMB_WLUN		= 0xC4,                                                      // 0b 1100 0100
};

/**
 * ufshcd_upiu_wlun_to_scsi_wlun - maps UPIU W-LUN id to SCSI W-LUN ID
 * @upiu_wlun_id: UPIU W-LUN id
 *
 * Returns SCSI W-LUN id
 */
static inline u16 ufshcd_upiu_wlun_to_scsi_wlun(u8 upiu_wlun_id)
{
	return (upiu_wlun_id & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE;
}

/**
 * ufshcd_scsi_add_wlus - Adds required W-LUs
 * @hba: per-adapter instance
 *
 * UFS device specification requires the UFS devices to support 4 well known
 * logical units:
 *	"REPORT_LUNS" (address: 01h)
 *	"UFS Device" (address: 50h)
 *	"RPMB" (address: 44h)
 *	"BOOT" (address: 30h)
 * UFS device's power management needs to be controlled by "POWER CONDITION"
 * field of SSU (START STOP UNIT) command. But this "power condition" field
 * will take effect only when its sent to "UFS device" well known logical unit
 * hence we require the scsi_device instance to represent this logical unit in
 * order for the UFS host driver to send the SSU command for power management.
 *
 * We also require the scsi_device instance for "RPMB" (Replay Protected Memory
 * Block) LU so user space process can control this LU. User space may also
 * want to have access to BOOT LU.
 *
 * This function adds scsi device instances for each of all well known LUs
 * (except "REPORT LUNS" LU).
 *
 * Returns zero on success (all required W-LUs are added successfully),
 * non-zero error value on failure (if failed to add any of the required W-LU).
 */
static int ufshcd_scsi_add_wlus(struct ufs_hba *hba)
{
	int ret = 0;
	struct scsi_device *sdev_rpmb;                                                       // 定义sdev_rpmb
	struct scsi_device *sdev_boot;                                                       // 定义sdev_boot

	hba->sdev_ufs_device = __scsi_add_device(hba->host, 0, 0,
		ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_UFS_DEVICE_WLUN), NULL);                  // 返回struct scsi_device实例
	if (IS_ERR(hba->sdev_ufs_device)) {
		ret = PTR_ERR(hba->sdev_ufs_device);
		hba->sdev_ufs_device = NULL;
		goto out;
	}
	scsi_device_put(hba->sdev_ufs_device);

	sdev_rpmb = __scsi_add_device(hba->host, 0, 0,
		ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_RPMB_WLUN), NULL);                        // 
	if (IS_ERR(sdev_rpmb)) {
		ret = PTR_ERR(sdev_rpmb);
		goto remove_sdev_ufs_device;
	}
	scsi_device_put(sdev_rpmb);

	sdev_boot = __scsi_add_device(hba->host, 0, 0,
		ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_BOOT_WLUN), NULL);                        // 
	if (IS_ERR(sdev_boot))
		dev_err(hba->dev, "%s: BOOT WLUN not found\n", __func__);
	else
		scsi_device_put(sdev_boot);
	goto out;

remove_sdev_ufs_device:
	scsi_remove_device(hba->sdev_ufs_device);
out:
	return ret;
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

static void ufs_put_device_desc(struct ufs_dev_desc *dev_desc)
{
	kfree(dev_desc->model);
	dev_desc->model = NULL;
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

/**
 * ufshcd_tune_pa_tactivate - Tunes PA_TActivate of local UniPro
 * @hba: per-adapter instance
 *
 * PA_TActivate parameter can be tuned manually if UniPro version is less than
 * 1.61. PA_TActivate needs to be greater than or equal to peerM-PHY's
 * RX_MIN_ACTIVATETIME_CAPABILITY attribute. This optimal value can help reduce
 * the hibern8 exit latency.
 *
 * Returns zero on success, non-zero error value on failure.
 */
static int ufshcd_tune_pa_tactivate(struct ufs_hba *hba)
{
	int ret = 0;
	u32 peer_rx_min_activatetime = 0, tuned_pa_tactivate;

	ret = ufshcd_dme_peer_get(hba,
				  UIC_ARG_MIB_SEL(
					RX_MIN_ACTIVATETIME_CAPABILITY,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)),
				  &peer_rx_min_activatetime);
	if (ret)
		goto out;

	/* make sure proper unit conversion is applied */
	tuned_pa_tactivate =
		((peer_rx_min_activatetime * RX_MIN_ACTIVATETIME_UNIT_US)
		 / PA_TACTIVATE_TIME_UNIT_US);
	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE),
			     tuned_pa_tactivate);

out:
	return ret;
}

/**
 * ufshcd_tune_pa_hibern8time - Tunes PA_Hibern8Time of local UniPro
 * @hba: per-adapter instance
 *
 * PA_Hibern8Time parameter can be tuned manually if UniPro version is less than
 * 1.61. PA_Hibern8Time needs to be maximum of local M-PHY's
 * TX_HIBERN8TIME_CAPABILITY & peer M-PHY's RX_HIBERN8TIME_CAPABILITY.
 * This optimal value can help reduce the hibern8 exit latency.
 *
 * Returns zero on success, non-zero error value on failure.
 */
static int ufshcd_tune_pa_hibern8time(struct ufs_hba *hba)
{
	int ret = 0;
	u32 local_tx_hibern8_time_cap = 0, peer_rx_hibern8_time_cap = 0;
	u32 max_hibern8_time, tuned_pa_hibern8time;

	ret = ufshcd_dme_get(hba,
			     UIC_ARG_MIB_SEL(TX_HIBERN8TIME_CAPABILITY,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
				  &local_tx_hibern8_time_cap);
	if (ret)
		goto out;

	ret = ufshcd_dme_peer_get(hba,
				  UIC_ARG_MIB_SEL(RX_HIBERN8TIME_CAPABILITY,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)),
				  &peer_rx_hibern8_time_cap);
	if (ret)
		goto out;

	max_hibern8_time = max(local_tx_hibern8_time_cap,
			       peer_rx_hibern8_time_cap);
	/* make sure proper unit conversion is applied */
	tuned_pa_hibern8time = ((max_hibern8_time * HIBERN8TIME_UNIT_US)
				/ PA_HIBERN8_TIME_UNIT_US);
	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_HIBERN8TIME),
			     tuned_pa_hibern8time);
out:
	return ret;
}

/**
 * ufshcd_quirk_tune_host_pa_tactivate - Ensures that host PA_TACTIVATE is
 * less than device PA_TACTIVATE time.
 * @hba: per-adapter instance
 *
 * Some UFS devices require host PA_TACTIVATE to be lower than device
 * PA_TACTIVATE, we need to enable UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE quirk
 * for such devices.
 *
 * Returns zero on success, non-zero error value on failure.
 */
static int ufshcd_quirk_tune_host_pa_tactivate(struct ufs_hba *hba)
{
	int ret = 0;
	u32 granularity, peer_granularity;
	u32 pa_tactivate, peer_pa_tactivate;
	u32 pa_tactivate_us, peer_pa_tactivate_us;
	u8 gran_to_us_table[] = {1, 4, 8, 16, 32, 100};

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_GRANULARITY),
				  &granularity);
	if (ret)
		goto out;

	ret = ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_GRANULARITY),
				  &peer_granularity);
	if (ret)
		goto out;

	if ((granularity < PA_GRANULARITY_MIN_VAL) ||
	    (granularity > PA_GRANULARITY_MAX_VAL)) {
		dev_err(hba->dev, "%s: invalid host PA_GRANULARITY %d",
			__func__, granularity);
		return -EINVAL;
	}

	if ((peer_granularity < PA_GRANULARITY_MIN_VAL) ||
	    (peer_granularity > PA_GRANULARITY_MAX_VAL)) {
		dev_err(hba->dev, "%s: invalid device PA_GRANULARITY %d",
			__func__, peer_granularity);
		return -EINVAL;
	}

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_TACTIVATE), &pa_tactivate);
	if (ret)
		goto out;

	ret = ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_TACTIVATE),
				  &peer_pa_tactivate);
	if (ret)
		goto out;

	pa_tactivate_us = pa_tactivate * gran_to_us_table[granularity - 1];
	peer_pa_tactivate_us = peer_pa_tactivate *
			     gran_to_us_table[peer_granularity - 1];

	if (pa_tactivate_us > peer_pa_tactivate_us) {
		u32 new_peer_pa_tactivate;

		new_peer_pa_tactivate = pa_tactivate_us /
				      gran_to_us_table[peer_granularity - 1];
		new_peer_pa_tactivate++;
		ret = ufshcd_dme_peer_set(hba, UIC_ARG_MIB(PA_TACTIVATE),
					  new_peer_pa_tactivate);
	}

out:
	return ret;
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

static void ufshcd_clear_dbg_ufs_stats(struct ufs_hba *hba)
{
	hba->ufs_stats.hibern8_exit_cnt = 0;
	hba->ufs_stats.last_hibern8_exit_tstamp = ktime_set(0, 0);
	hba->req_abort_count = 0;
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

static struct ufs_ref_clk ufs_ref_clk_freqs[] = {
	{19200000, REF_CLK_FREQ_19_2_MHZ},
	{26000000, REF_CLK_FREQ_26_MHZ},
	{38400000, REF_CLK_FREQ_38_4_MHZ},
	{52000000, REF_CLK_FREQ_52_MHZ},
	{0, REF_CLK_FREQ_INVAL},
};

static enum ufs_ref_clk_freq
ufs_get_bref_clk_from_hz(unsigned long freq)
{
	int i;

	for (i = 0; ufs_ref_clk_freqs[i].freq_hz; i++)
		if (ufs_ref_clk_freqs[i].freq_hz == freq)
			return ufs_ref_clk_freqs[i].val;

	return REF_CLK_FREQ_INVAL;
}

void ufshcd_parse_dev_ref_clk_freq(struct ufs_hba *hba, struct clk *refclk)
{
	unsigned long freq;

	freq = clk_get_rate(refclk);

	hba->dev_ref_clk_freq =
		ufs_get_bref_clk_from_hz(freq);

	if (hba->dev_ref_clk_freq == REF_CLK_FREQ_INVAL)
		dev_err(hba->dev,
		"invalid ref_clk setting = %ld\n", freq);
}

static int ufshcd_set_dev_ref_clk(struct ufs_hba *hba)
{
	int err;
	u32 ref_clk;
	u32 freq = hba->dev_ref_clk_freq;

	err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_REF_CLK_FREQ, 0, 0, &ref_clk);

	if (err) {
		dev_err(hba->dev, "failed reading bRefClkFreq. err = %d\n",
			err);
		goto out;
	}

	if (ref_clk == freq)
		goto out; /* nothing to update */

	err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
			QUERY_ATTR_IDN_REF_CLK_FREQ, 0, 0, &freq);

	if (err) {
		dev_err(hba->dev, "bRefClkFreq setting to %lu Hz failed\n",
			ufs_ref_clk_freqs[freq].freq_hz);
		goto out;
	}

	dev_dbg(hba->dev, "bRefClkFreq setting to %lu Hz succeeded\n",
			ufs_ref_clk_freqs[freq].freq_hz);

out:
	return err;
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

	ret = ufshcd_link_startup(hba);
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
		ret = ufshcd_config_pwr_mode(hba, &hba->max_pwr_info.info);
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

		scsi_scan_host(hba->host);
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

/**
 * ufshcd_async_scan - asynchronous execution for probing hba
 * @data: data pointer to pass to this function
 * @cookie: cookie data
 */
static void ufshcd_async_scan(void *data, async_cookie_t cookie)
{
	struct ufs_hba *hba = (struct ufs_hba *)data;

	ufshcd_probe_hba(hba);                                                               // *
}

static enum blk_eh_timer_return ufshcd_eh_timed_out(struct scsi_cmnd *scmd)
{
	unsigned long flags;
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	int index;
	bool found = false;

	if (!scmd || !scmd->device || !scmd->device->host)
		return BLK_EH_DONE;

	host = scmd->device->host;
	hba = shost_priv(host);
	if (!hba)
		return BLK_EH_DONE;

	spin_lock_irqsave(host->host_lock, flags);

	for_each_set_bit(index, &hba->outstanding_reqs, hba->nutrs) {
		if (hba->lrb[index].cmd == scmd) {
			found = true;
			break;
		}
	}

	spin_unlock_irqrestore(host->host_lock, flags);

	/*
	 * Bypass SCSI error handling and reset the block layer timer if this
	 * SCSI command was not actually dispatched to UFS driver, otherwise
	 * let SCSI layer handle the error as usual.
	 */
	return found ? BLK_EH_DONE : BLK_EH_RESET_TIMER;
}

static const struct attribute_group *ufshcd_driver_groups[] = {
	&ufs_sysfs_unit_descriptor_group,
	&ufs_sysfs_lun_attributes_group,
	NULL,
};

static struct scsi_host_template ufshcd_driver_template = {
	.module			= THIS_MODULE,
	.name			= UFSHCD,                                                            // 
	.proc_name		= UFSHCD,                                                            // 
	.queuecommand		= ufshcd_queuecommand,                                           // 重点
	.slave_alloc		= ufshcd_slave_alloc,                                            // 
	.slave_configure	= ufshcd_slave_configure,                                        // 
	.slave_destroy		= ufshcd_slave_destroy,                                          // 
	.change_queue_depth	= ufshcd_change_queue_depth,                                     // 
	.eh_abort_handler	= ufshcd_abort,                                                  // 
	.eh_device_reset_handler = ufshcd_eh_device_reset_handler,                           // 
	.eh_host_reset_handler   = ufshcd_eh_host_reset_handler,                             // 
	.eh_timed_out		= ufshcd_eh_timed_out,                                           // 
	.this_id		= -1,                                                                // 
	.sg_tablesize		= SG_ALL,                                                        // 
	.cmd_per_lun		= UFSHCD_CMD_PER_LUN,                                            // 
	.can_queue		= UFSHCD_CAN_QUEUE,                                                  // 
	.max_segment_size	= PRDT_DATA_BYTE_COUNT_MAX,                                      // #define PRDT_DATA_BYTE_COUNT_MAX (256 * 1024)
	.max_host_blocked	= 1,                                                             // 
	.track_queue_depth	= 1,                                                             // 
	.sdev_groups		= ufshcd_driver_groups,                                          // 
	.dma_boundary		= PAGE_SIZE - 1,                                                 // 
};

static int ufshcd_config_vreg_load(struct device *dev, struct ufs_vreg *vreg,
				   int ua)
{
	int ret;

	if (!vreg)
		return 0;

	/*
	 * "set_load" operation shall be required on those regulators
	 * which specifically configured current limitation. Otherwise
	 * zero max_uA may cause unexpected behavior when regulator is
	 * enabled or set as high power mode.
	 */
	if (!vreg->max_uA)
		return 0;

	ret = regulator_set_load(vreg->reg, ua);
	if (ret < 0) {
		dev_err(dev, "%s: %s set load (ua=%d) failed, err=%d\n",
				__func__, vreg->name, ua, ret);
	}

	return ret;
}

static inline int ufshcd_config_vreg_lpm(struct ufs_hba *hba,
					 struct ufs_vreg *vreg)
{
	return ufshcd_config_vreg_load(hba->dev, vreg, UFS_VREG_LPM_LOAD_UA);
}

static inline int ufshcd_config_vreg_hpm(struct ufs_hba *hba,
					 struct ufs_vreg *vreg)
{
	if (!vreg)
		return 0;

	return ufshcd_config_vreg_load(hba->dev, vreg, vreg->max_uA);
}

static int ufshcd_config_vreg(struct device *dev,
		struct ufs_vreg *vreg, bool on)
{
	int ret = 0;
	struct regulator *reg;
	const char *name;
	int min_uV, uA_load;

	BUG_ON(!vreg);

	reg = vreg->reg;
	name = vreg->name;

	if (regulator_count_voltages(reg) > 0) {
		if (vreg->min_uV && vreg->max_uV) {
			min_uV = on ? vreg->min_uV : 0;
			ret = regulator_set_voltage(reg, min_uV, vreg->max_uV);
			if (ret) {
				dev_err(dev,
					"%s: %s set voltage failed, err=%d\n",
					__func__, name, ret);
				goto out;
			}
		}

		uA_load = on ? vreg->max_uA : 0;
		ret = ufshcd_config_vreg_load(dev, vreg, uA_load);
		if (ret)
			goto out;
	}
out:
	return ret;
}

static int ufshcd_enable_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg || vreg->enabled)
		goto out;

	ret = ufshcd_config_vreg(dev, vreg, true);
	if (!ret)
		ret = regulator_enable(vreg->reg);

	if (!ret)
		vreg->enabled = true;
	else
		dev_err(dev, "%s: %s enable failed, err=%d\n",
				__func__, vreg->name, ret);
out:
	return ret;
}

static int ufshcd_disable_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg || !vreg->enabled)
		goto out;

	ret = regulator_disable(vreg->reg);

	if (!ret) {
		/* ignore errors on applying disable config */
		ufshcd_config_vreg(dev, vreg, false);
		vreg->enabled = false;
	} else {
		dev_err(dev, "%s: %s disable failed, err=%d\n",
				__func__, vreg->name, ret);
	}
out:
	return ret;
}

static int ufshcd_setup_vreg(struct ufs_hba *hba, bool on)
{
	int ret = 0;
	struct device *dev = hba->dev;                                                       // 获取dev
	struct ufs_vreg_info *info = &hba->vreg_info;                                        // 获取info

	ret = ufshcd_toggle_vreg(dev, info->vcc, on);                                        // turn info->vcc on
	if (ret)                                                                             // 若ret不为0
		goto out;                                                                        // 跳转至out

	ret = ufshcd_toggle_vreg(dev, info->vccq, on);                                       // turn info->vccq on
	if (ret)                                                                             // 若ret不为0
		goto out;                                                                        // 返回out

	ret = ufshcd_toggle_vreg(dev, info->vccq2, on);                                      // turn info->vccq2 on
	if (ret)                                                                             // 若ret不为0
		goto out;                                                                        // 跳转至out

out:
	if (ret) {                                                                           // 若ret不为0
		ufshcd_toggle_vreg(dev, info->vccq2, false);                                     // turn info->vccq2 off
		ufshcd_toggle_vreg(dev, info->vccq, false);                                      // turn info->vccq off
		ufshcd_toggle_vreg(dev, info->vcc, false);                                       // turn info->vcc off
	}
	return ret;                                                                          // 返回ret
}

static int ufshcd_setup_hba_vreg(struct ufs_hba *hba, bool on)
{
	struct ufs_vreg_info *info = &hba->vreg_info;                                        // 获取info

	return ufshcd_toggle_vreg(hba->dev, info->vdd_hba, on);                              // turn info->vdd_hba on
}

static int ufshcd_get_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg)                                                                           // 若vreg为NULL
		goto out;                                                                        // 返回out

	vreg->reg = devm_regulator_get(dev, vreg->name);                                     // 获取vreg->reg
	if (IS_ERR(vreg->reg)) {                                                             // 若IS_ERR(vreg->reg)为true
		ret = PTR_ERR(vreg->reg);                                                        // 设置ret为PTR_ERR(vreg->reg)
		dev_err(dev, "%s: %s get failed, err=%d\n",
				__func__, vreg->name, ret);                                              // 打印提示信息
	}
out:
	return ret;                                                                          // 返回ret
}

static int ufshcd_init_vreg(struct ufs_hba *hba)
{
	int ret = 0;
	struct device *dev = hba->dev;                                                       // 获取dev
	struct ufs_vreg_info *info = &hba->vreg_info;                                        // 获取info

	ret = ufshcd_get_vreg(dev, info->vcc);                                               // 根据info->vcc->name获取regulator存到info->vcc->reg中
	if (ret)                                                                             // 若ret不为0
		goto out;                                                                        // 跳转至out

	ret = ufshcd_get_vreg(dev, info->vccq);                                              // 根据info->vccq->name获取regulator存到info->vccq->reg中
	if (ret)                                                                             // 若ret不为0
		goto out;                                                                        // 跳转至out

	ret = ufshcd_get_vreg(dev, info->vccq2);                                             // 根据info->vccq2->name获取regulator存到info->vccq2->reg中
out:
	return ret;                                                                          // 返回ret
}

static int ufshcd_init_hba_vreg(struct ufs_hba *hba)
{
	struct ufs_vreg_info *info = &hba->vreg_info;                                        // 取出info

	if (info)                                                                            // 若info不为NULL
		return ufshcd_get_vreg(hba->dev, info->vdd_hba);                                 // 根据info->vdd_hba->name获取regulator存到info->vdd_hba->reg中

	return 0;                                                                            // 返回0
}

static int __ufshcd_setup_clocks(struct ufs_hba *hba, bool on,
					bool skip_ref_clk)
{
	int ret = 0;
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;
	unsigned long flags;
	ktime_t start = ktime_get();
	bool clk_state_changed = false;

	if (list_empty(head))
		goto out;

	/*
	 * vendor specific setup_clocks ops may depend on clocks managed by
	 * this standard driver hence call the vendor specific setup_clocks
	 * before disabling the clocks managed here.
	 */
	if (!on) {
		ret = ufshcd_vops_setup_clocks(hba, on, PRE_CHANGE);
		if (ret)
			return ret;
	}

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			if (skip_ref_clk && !strcmp(clki->name, "ref_clk"))
				continue;

			clk_state_changed = on ^ clki->enabled;
			if (on && !clki->enabled) {
				ret = clk_prepare_enable(clki->clk);
				if (ret) {
					dev_err(hba->dev, "%s: %s prepare enable failed, %d\n",
						__func__, clki->name, ret);
					goto out;
				}
			} else if (!on && clki->enabled) {
				clk_disable_unprepare(clki->clk);
			}
			clki->enabled = on;
			dev_dbg(hba->dev, "%s: clk: %s %sabled\n", __func__,
					clki->name, on ? "en" : "dis");
		}
	}

	/*
	 * vendor specific setup_clocks ops may depend on clocks managed by
	 * this standard driver hence call the vendor specific setup_clocks
	 * after enabling the clocks managed here.
	 */
	if (on) {
		ret = ufshcd_vops_setup_clocks(hba, on, POST_CHANGE);
		if (ret)
			return ret;
	}

out:
	if (ret) {
		list_for_each_entry(clki, head, list) {
			if (!IS_ERR_OR_NULL(clki->clk) && clki->enabled)
				clk_disable_unprepare(clki->clk);
		}
	} else if (!ret && on) {
		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->clk_gating.state = CLKS_ON;
		trace_ufshcd_clk_gating(dev_name(hba->dev),
					hba->clk_gating.state);
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}

	if (clk_state_changed)
		trace_ufshcd_profile_clk_gating(dev_name(hba->dev),
			(on ? "on" : "off"),
			ktime_to_us(ktime_sub(ktime_get(), start)), ret);
	return ret;
}

static int ufshcd_setup_clocks(struct ufs_hba *hba, bool on)
{
	return  __ufshcd_setup_clocks(hba, on, false);
}

static int ufshcd_init_clocks(struct ufs_hba *hba)
{
	int ret = 0;
	struct ufs_clk_info *clki;
	struct device *dev = hba->dev;
	struct list_head *head = &hba->clk_list_head;

	if (list_empty(head))
		goto out;

	list_for_each_entry(clki, head, list) {
		if (!clki->name)
			continue;

		clki->clk = devm_clk_get(dev, clki->name);
		if (IS_ERR(clki->clk)) {
			ret = PTR_ERR(clki->clk);
			dev_err(dev, "%s: %s clk get failed, %d\n",
					__func__, clki->name, ret);
			goto out;
		}

		/*
		 * Parse device ref clk freq as per device tree "ref_clk".
		 * Default dev_ref_clk_freq is set to REF_CLK_FREQ_INVAL
		 * in ufshcd_alloc_host().
		 */
		if (!strcmp(clki->name, "ref_clk"))
			ufshcd_parse_dev_ref_clk_freq(hba, clki->clk);

		if (clki->max_freq) {
			ret = clk_set_rate(clki->clk, clki->max_freq);
			if (ret) {
				dev_err(hba->dev, "%s: %s clk set rate(%dHz) failed, %d\n",
					__func__, clki->name,
					clki->max_freq, ret);
				goto out;
			}
			clki->curr_freq = clki->max_freq;
		}
		dev_dbg(dev, "%s: clk: %s, rate: %lu\n", __func__,
				clki->name, clk_get_rate(clki->clk));
	}
out:
	return ret;
}

static int ufshcd_variant_hba_init(struct ufs_hba *hba)
{
	int err = 0;

	if (!hba->vops)
		goto out;

	err = ufshcd_vops_init(hba);
	if (err)
		goto out;

	err = ufshcd_vops_setup_regulators(hba, true);
	if (err)
		goto out_exit;

	goto out;

out_exit:
	ufshcd_vops_exit(hba);
out:
	if (err)
		dev_err(hba->dev, "%s: variant %s init failed err %d\n",
			__func__, ufshcd_get_var_name(hba), err);
	return err;
}

static void ufshcd_variant_hba_exit(struct ufs_hba *hba)
{
	if (!hba->vops)
		return;

	ufshcd_vops_setup_regulators(hba, false);

	ufshcd_vops_exit(hba);
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
	err = ufshcd_init_hba_vreg(hba);                                                     // 根据hba->vreg_info.vdd_hba->name获取regulator存到hba->vreg_info.vdd_hba->reg中
	if (err)                                                                             // 若err不为0
		goto out;                                                                        // 跳转至out

	err = ufshcd_setup_hba_vreg(hba, true);                                              // turn info->vdd_hba on
	if (err)                                                                             // 若err不为0
		goto out;                                                                        // 跳转至out

	err = ufshcd_init_clocks(hba);                                                       // *
	if (err)                                                                             // 若err不为0
		goto out_disable_hba_vreg;                                                       // 跳转至out_disable_hba_vreg

	err = ufshcd_setup_clocks(hba, true);                                                // *
	if (err)                                                                             // 若err不为0
		goto out_disable_hba_vreg;                                                       // 跳转至out_disable_hba_vreg

	err = ufshcd_init_vreg(hba);                                                         // 获取hba->vreg_info.vcc、hba->vreg_info.vccq、hba->vreg_info.vccq2
	if (err)                                                                             // 若err不为0
		goto out_disable_clks;                                                           // 跳转至out_disable_clks

	err = ufshcd_setup_vreg(hba, true);                                                  // turn hba->vreg_info.vcc & hba->vreg_info.vccq & hba->vreg_info.vccq2 on
	if (err)                                                                             // 若err不为0
		goto out_disable_clks;                                                           // 跳转至out_disable_clks

	err = ufshcd_variant_hba_init(hba);                                                  // *
	if (err)                                                                             // 若err不为0
		goto out_disable_vreg;                                                           // 跳转至out_disable_vreg

	hba->is_powered = true;                                                              // 设置hba->is_powered为true
	goto out;                                                                            // 跳转至out

out_disable_vreg:
	ufshcd_setup_vreg(hba, false);
out_disable_clks:
	ufshcd_setup_clocks(hba, false);
out_disable_hba_vreg:
	ufshcd_setup_hba_vreg(hba, false);
out:
	return err;                                                                          // 返回err
}

static void ufshcd_hba_exit(struct ufs_hba *hba)
{
	if (hba->is_powered) {
		ufshcd_variant_hba_exit(hba);
		ufshcd_setup_vreg(hba, false);
		ufshcd_suspend_clkscaling(hba);
		if (ufshcd_is_clkscaling_supported(hba))
			if (hba->devfreq)
				ufshcd_suspend_clkscaling(hba);
		ufshcd_setup_clocks(hba, false);
		ufshcd_setup_hba_vreg(hba, false);
		hba->is_powered = false;
	}
}

static int
ufshcd_send_request_sense(struct ufs_hba *hba, struct scsi_device *sdp)                  // 重点：这是一个scsi_execute使用案例，要好好研究一下
{
	unsigned char cmd[6] = {REQUEST_SENSE,
				0,
				0,
				0,
				UFS_SENSE_SIZE,
				0};
	char *buffer;
	int ret;

	buffer = kzalloc(UFS_SENSE_SIZE, GFP_KERNEL);
	if (!buffer) {
		ret = -ENOMEM;
		goto out;
	}

	ret = scsi_execute(sdp, cmd, DMA_FROM_DEVICE, buffer,
			UFS_SENSE_SIZE, NULL, NULL,
			msecs_to_jiffies(1000), 3, 0, RQF_PM, NULL);
	if (ret)
		pr_err("%s: failed with err %d\n", __func__, ret);

	kfree(buffer);
out:
	return ret;
}

/**
 * ufshcd_set_dev_pwr_mode - sends START STOP UNIT command to set device
 *			     power mode
 * @hba: per adapter instance
 * @pwr_mode: device power mode to set
 *
 * Returns 0 if requested power mode is set successfully
 * Returns non-zero if failed to set the requested power mode
 */
static int ufshcd_set_dev_pwr_mode(struct ufs_hba *hba,
				     enum ufs_dev_pwr_mode pwr_mode)
{
	unsigned char cmd[6] = { START_STOP };
	struct scsi_sense_hdr sshdr;
	struct scsi_device *sdp;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdp = hba->sdev_ufs_device;
	if (sdp) {
		ret = scsi_device_get(sdp);
		if (!ret && !scsi_device_online(sdp)) {
			ret = -ENODEV;
			scsi_device_put(sdp);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	/*
	 * If scsi commands fail, the scsi mid-layer schedules scsi error-
	 * handling, which would wait for host to be resumed. Since we know
	 * we are functional while we are here, skip host resume in error
	 * handling context.
	 */
	hba->host->eh_noresume = 1;
	if (hba->wlun_dev_clr_ua) {
		ret = ufshcd_send_request_sense(hba, sdp);
		if (ret)
			goto out;
		/* Unit attention condition is cleared now */
		hba->wlun_dev_clr_ua = false;
	}

	cmd[4] = pwr_mode << 4;

	/*
	 * Current function would be generally called from the power management
	 * callbacks hence set the RQF_PM flag so that it doesn't resume the
	 * already suspended childs.
	 */
	ret = scsi_execute(sdp, cmd, DMA_NONE, NULL, 0, NULL, &sshdr,
			START_STOP_TIMEOUT, 0, 0, RQF_PM, NULL);
	if (ret) {
		sdev_printk(KERN_WARNING, sdp,
			    "START_STOP failed for power mode: %d, result %x\n",
			    pwr_mode, ret);
		if (driver_byte(ret) == DRIVER_SENSE)
			scsi_print_sense_hdr(sdp, NULL, &sshdr);
	}

	if (!ret)
		hba->curr_dev_pwr_mode = pwr_mode;
out:
	scsi_device_put(sdp);
	hba->host->eh_noresume = 0;
	return ret;
}

static int ufshcd_link_state_transition(struct ufs_hba *hba,
					enum uic_link_state req_link_state,
					int check_for_bkops)
{
	int ret = 0;

	if (req_link_state == hba->uic_link_state)
		return 0;

	if (req_link_state == UIC_LINK_HIBERN8_STATE) {
		ret = ufshcd_uic_hibern8_enter(hba);
		if (!ret)
			ufshcd_set_link_hibern8(hba);
		else
			goto out;
	}
	/*
	 * If autobkops is enabled, link can't be turned off because
	 * turning off the link would also turn off the device.
	 */
	else if ((req_link_state == UIC_LINK_OFF_STATE) &&
		   (!check_for_bkops || (check_for_bkops &&
		    !hba->auto_bkops_enabled))) {
		/*
		 * Let's make sure that link is in low power mode, we are doing
		 * this currently by putting the link in Hibern8. Otherway to
		 * put the link in low power mode is to send the DME end point
		 * to device and then send the DME reset command to local
		 * unipro. But putting the link in hibern8 is much faster.
		 */
		ret = ufshcd_uic_hibern8_enter(hba);
		if (ret)
			goto out;
		/*
		 * Change controller state to "reset state" which
		 * should also put the link in off/reset state
		 */
		ufshcd_hba_stop(hba, true);
		/*
		 * TODO: Check if we need any delay to make sure that
		 * controller is reset
		 */
		ufshcd_set_link_off(hba);
	}

out:
	return ret;
}

static void ufshcd_vreg_set_lpm(struct ufs_hba *hba)
{
	/*
	 * It seems some UFS devices may keep drawing more than sleep current
	 * (atleast for 500us) from UFS rails (especially from VCCQ rail).
	 * To avoid this situation, add 2ms delay before putting these UFS
	 * rails in LPM mode.
	 */
	if (!ufshcd_is_link_active(hba) &&
	    hba->dev_quirks & UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM)
		usleep_range(2000, 2100);

	/*
	 * If UFS device is either in UFS_Sleep turn off VCC rail to save some
	 * power.
	 *
	 * If UFS device and link is in OFF state, all power supplies (VCC,
	 * VCCQ, VCCQ2) can be turned off if power on write protect is not
	 * required. If UFS link is inactive (Hibern8 or OFF state) and device
	 * is in sleep state, put VCCQ & VCCQ2 rails in LPM mode.
	 *
	 * Ignore the error returned by ufshcd_toggle_vreg() as device is anyway
	 * in low power state which would save some power.
	 */
	if (ufshcd_is_ufs_dev_poweroff(hba) && ufshcd_is_link_off(hba) &&
	    !hba->dev_info.is_lu_power_on_wp) {
		ufshcd_setup_vreg(hba, false);
	} else if (!ufshcd_is_ufs_dev_active(hba)) {
		ufshcd_toggle_vreg(hba->dev, hba->vreg_info.vcc, false);
		if (!ufshcd_is_link_active(hba)) {
			ufshcd_config_vreg_lpm(hba, hba->vreg_info.vccq);
			ufshcd_config_vreg_lpm(hba, hba->vreg_info.vccq2);
		}
	}
}

static int ufshcd_vreg_set_hpm(struct ufs_hba *hba)
{
	int ret = 0;

	if (ufshcd_is_ufs_dev_poweroff(hba) && ufshcd_is_link_off(hba) &&
	    !hba->dev_info.is_lu_power_on_wp) {
		ret = ufshcd_setup_vreg(hba, true);
	} else if (!ufshcd_is_ufs_dev_active(hba)) {
		if (!ret && !ufshcd_is_link_active(hba)) {
			ret = ufshcd_config_vreg_hpm(hba, hba->vreg_info.vccq);
			if (ret)
				goto vcc_disable;
			ret = ufshcd_config_vreg_hpm(hba, hba->vreg_info.vccq2);
			if (ret)
				goto vccq_lpm;
		}
		ret = ufshcd_toggle_vreg(hba->dev, hba->vreg_info.vcc, true);
	}
	goto out;

vccq_lpm:
	ufshcd_config_vreg_lpm(hba, hba->vreg_info.vccq);
vcc_disable:
	ufshcd_toggle_vreg(hba->dev, hba->vreg_info.vcc, false);
out:
	return ret;
}

static void ufshcd_hba_vreg_set_lpm(struct ufs_hba *hba)
{
	if (ufshcd_is_link_off(hba))
		ufshcd_setup_hba_vreg(hba, false);
}

static void ufshcd_hba_vreg_set_hpm(struct ufs_hba *hba)
{
	if (ufshcd_is_link_off(hba))
		ufshcd_setup_hba_vreg(hba, true);
}

/**
 * ufshcd_suspend - helper function for suspend operations
 * @hba: per adapter instance
 * @pm_op: desired low power operation type
 *
 * This function will try to put the UFS device and link into low power
 * mode based on the "rpm_lvl" (Runtime PM level) or "spm_lvl"
 * (System PM level).
 *
 * If this function is called during shutdown, it will make sure that
 * both UFS device and UFS link is powered off.
 *
 * NOTE: UFS device & link must be active before we enter in this function.
 *
 * Returns 0 for success and non-zero for failure
 */
static int ufshcd_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int ret = 0;
	enum ufs_pm_level pm_lvl;
	enum ufs_dev_pwr_mode req_dev_pwr_mode;
	enum uic_link_state req_link_state;

	hba->pm_op_in_progress = 1;
	if (!ufshcd_is_shutdown_pm(pm_op)) {
		pm_lvl = ufshcd_is_runtime_pm(pm_op) ?
			 hba->rpm_lvl : hba->spm_lvl;
		req_dev_pwr_mode = ufs_get_pm_lvl_to_dev_pwr_mode(pm_lvl);
		req_link_state = ufs_get_pm_lvl_to_link_pwr_state(pm_lvl);
	} else {
		req_dev_pwr_mode = UFS_POWERDOWN_PWR_MODE;
		req_link_state = UIC_LINK_OFF_STATE;
	}

	/*
	 * If we can't transition into any of the low power modes
	 * just gate the clocks.
	 */
	ufshcd_hold(hba, false);
	hba->clk_gating.is_suspended = true;

	if (hba->clk_scaling.is_allowed) {
		cancel_work_sync(&hba->clk_scaling.suspend_work);
		cancel_work_sync(&hba->clk_scaling.resume_work);
		ufshcd_suspend_clkscaling(hba);
	}

	if (req_dev_pwr_mode == UFS_ACTIVE_PWR_MODE &&
			req_link_state == UIC_LINK_ACTIVE_STATE) {
		goto disable_clks;
	}

	if ((req_dev_pwr_mode == hba->curr_dev_pwr_mode) &&
	    (req_link_state == hba->uic_link_state))
		goto enable_gating;

	/* UFS device & link must be active before we enter in this function */
	if (!ufshcd_is_ufs_dev_active(hba) || !ufshcd_is_link_active(hba)) {
		ret = -EINVAL;
		goto enable_gating;
	}

	if (ufshcd_is_runtime_pm(pm_op)) {
		if (ufshcd_can_autobkops_during_suspend(hba)) {
			/*
			 * The device is idle with no requests in the queue,
			 * allow background operations if bkops status shows
			 * that performance might be impacted.
			 */
			ret = ufshcd_urgent_bkops(hba);
			if (ret)
				goto enable_gating;
		} else {
			/* make sure that auto bkops is disabled */
			ufshcd_disable_auto_bkops(hba);
		}
	}

	if ((req_dev_pwr_mode != hba->curr_dev_pwr_mode) &&
	     ((ufshcd_is_runtime_pm(pm_op) && !hba->auto_bkops_enabled) ||
	       !ufshcd_is_runtime_pm(pm_op))) {
		/* ensure that bkops is disabled */
		ufshcd_disable_auto_bkops(hba);
		ret = ufshcd_set_dev_pwr_mode(hba, req_dev_pwr_mode);
		if (ret)
			goto enable_gating;
	}

	ret = ufshcd_link_state_transition(hba, req_link_state, 1);
	if (ret)
		goto set_dev_active;

	ufshcd_vreg_set_lpm(hba);

disable_clks:
	/*
	 * Call vendor specific suspend callback. As these callbacks may access
	 * vendor specific host controller register space call them before the
	 * host clocks are ON.
	 */
	ret = ufshcd_vops_suspend(hba, pm_op);
	if (ret)
		goto set_link_active;

	if (!ufshcd_is_link_active(hba))
		ufshcd_setup_clocks(hba, false);
	else
		/* If link is active, device ref_clk can't be switched off */
		__ufshcd_setup_clocks(hba, false, true);

	hba->clk_gating.state = CLKS_OFF;
	trace_ufshcd_clk_gating(dev_name(hba->dev), hba->clk_gating.state);
	/*
	 * Disable the host irq as host controller as there won't be any
	 * host controller transaction expected till resume.
	 */
	ufshcd_disable_irq(hba);
	/* Put the host controller in low power mode if possible */
	ufshcd_hba_vreg_set_lpm(hba);
	goto out;

set_link_active:
	if (hba->clk_scaling.is_allowed)
		ufshcd_resume_clkscaling(hba);
	ufshcd_vreg_set_hpm(hba);
	if (ufshcd_is_link_hibern8(hba) && !ufshcd_uic_hibern8_exit(hba))
		ufshcd_set_link_active(hba);
	else if (ufshcd_is_link_off(hba))
		ufshcd_host_reset_and_restore(hba);
set_dev_active:
	if (!ufshcd_set_dev_pwr_mode(hba, UFS_ACTIVE_PWR_MODE))
		ufshcd_disable_auto_bkops(hba);
enable_gating:
	if (hba->clk_scaling.is_allowed)
		ufshcd_resume_clkscaling(hba);
	hba->clk_gating.is_suspended = false;
	ufshcd_release(hba);
out:
	hba->pm_op_in_progress = 0;
	if (ret)
		ufshcd_update_reg_hist(&hba->ufs_stats.suspend_err, (u32)ret);
	return ret;
}

/**
 * ufshcd_resume - helper function for resume operations
 * @hba: per adapter instance
 * @pm_op: runtime PM or system PM
 *
 * This function basically brings the UFS device, UniPro link and controller
 * to active state.
 *
 * Returns 0 for success and non-zero for failure
 */
static int ufshcd_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int ret;
	enum uic_link_state old_link_state;

	hba->pm_op_in_progress = 1;
	old_link_state = hba->uic_link_state;

	ufshcd_hba_vreg_set_hpm(hba);
	/* Make sure clocks are enabled before accessing controller */
	ret = ufshcd_setup_clocks(hba, true);
	if (ret)
		goto out;

	/* enable the host irq as host controller would be active soon */
	ret = ufshcd_enable_irq(hba);
	if (ret)
		goto disable_irq_and_vops_clks;

	ret = ufshcd_vreg_set_hpm(hba);
	if (ret)
		goto disable_irq_and_vops_clks;

	/*
	 * Call vendor specific resume callback. As these callbacks may access
	 * vendor specific host controller register space call them when the
	 * host clocks are ON.
	 */
	ret = ufshcd_vops_resume(hba, pm_op);
	if (ret)
		goto disable_vreg;

	if (ufshcd_is_link_hibern8(hba)) {
		ret = ufshcd_uic_hibern8_exit(hba);
		if (!ret)
			ufshcd_set_link_active(hba);
		else
			goto vendor_suspend;
	} else if (ufshcd_is_link_off(hba)) {
		ret = ufshcd_host_reset_and_restore(hba);
		/*
		 * ufshcd_host_reset_and_restore() should have already
		 * set the link state as active
		 */
		if (ret || !ufshcd_is_link_active(hba))
			goto vendor_suspend;
	}

	if (!ufshcd_is_ufs_dev_active(hba)) {
		ret = ufshcd_set_dev_pwr_mode(hba, UFS_ACTIVE_PWR_MODE);
		if (ret)
			goto set_old_link_state;
	}

	if (ufshcd_keep_autobkops_enabled_except_suspend(hba))
		ufshcd_enable_auto_bkops(hba);
	else
		/*
		 * If BKOPs operations are urgently needed at this moment then
		 * keep auto-bkops enabled or else disable it.
		 */
		ufshcd_urgent_bkops(hba);

	hba->clk_gating.is_suspended = false;

	if (hba->clk_scaling.is_allowed)
		ufshcd_resume_clkscaling(hba);

	/* Schedule clock gating in case of no access to UFS device yet */
	ufshcd_release(hba);

	/* Enable Auto-Hibernate if configured */
	ufshcd_auto_hibern8_enable(hba);

	goto out;

set_old_link_state:
	ufshcd_link_state_transition(hba, old_link_state, 0);
vendor_suspend:
	ufshcd_vops_suspend(hba, pm_op);
disable_vreg:
	ufshcd_vreg_set_lpm(hba);
disable_irq_and_vops_clks:
	ufshcd_disable_irq(hba);
	if (hba->clk_scaling.is_allowed)
		ufshcd_suspend_clkscaling(hba);
	ufshcd_setup_clocks(hba, false);
out:
	hba->pm_op_in_progress = 0;
	if (ret)
		ufshcd_update_reg_hist(&hba->ufs_stats.resume_err, (u32)ret);
	return ret;
}

/**
 * ufshcd_system_suspend - system suspend routine
 * @hba: per adapter instance
 *
 * Check the description of ufshcd_suspend() function for more details.
 *
 * Returns 0 for success and non-zero for failure
 */
int ufshcd_system_suspend(struct ufs_hba *hba)
{
	int ret = 0;
	ktime_t start = ktime_get();

	if (!hba || !hba->is_powered)
		return 0;

	if ((ufs_get_pm_lvl_to_dev_pwr_mode(hba->spm_lvl) ==
	     hba->curr_dev_pwr_mode) &&
	    (ufs_get_pm_lvl_to_link_pwr_state(hba->spm_lvl) ==
	     hba->uic_link_state))
		goto out;

	if (pm_runtime_suspended(hba->dev)) {
		/*
		 * UFS device and/or UFS link low power states during runtime
		 * suspend seems to be different than what is expected during
		 * system suspend. Hence runtime resume the devic & link and
		 * let the system suspend low power states to take effect.
		 * TODO: If resume takes longer time, we might have optimize
		 * it in future by not resuming everything if possible.
		 */
		ret = ufshcd_runtime_resume(hba);
		if (ret)
			goto out;
	}

	ret = ufshcd_suspend(hba, UFS_SYSTEM_PM);
out:
	trace_ufshcd_system_suspend(dev_name(hba->dev), ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	if (!ret)
		hba->is_sys_suspended = true;
	return ret;
}
EXPORT_SYMBOL(ufshcd_system_suspend);

/**
 * ufshcd_system_resume - system resume routine
 * @hba: per adapter instance
 *
 * Returns 0 for success and non-zero for failure
 */

int ufshcd_system_resume(struct ufs_hba *hba)
{
	int ret = 0;
	ktime_t start = ktime_get();

	if (!hba)
		return -EINVAL;

	if (!hba->is_powered || pm_runtime_suspended(hba->dev))
		/*
		 * Let the runtime resume take care of resuming
		 * if runtime suspended.
		 */
		goto out;
	else
		ret = ufshcd_resume(hba, UFS_SYSTEM_PM);
out:
	trace_ufshcd_system_resume(dev_name(hba->dev), ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	if (!ret)
		hba->is_sys_suspended = false;
	return ret;
}
EXPORT_SYMBOL(ufshcd_system_resume);

/**
 * ufshcd_runtime_suspend - runtime suspend routine
 * @hba: per adapter instance
 *
 * Check the description of ufshcd_suspend() function for more details.
 *
 * Returns 0 for success and non-zero for failure
 */
int ufshcd_runtime_suspend(struct ufs_hba *hba)
{
	int ret = 0;
	ktime_t start = ktime_get();

	if (!hba)
		return -EINVAL;

	if (!hba->is_powered)
		goto out;
	else
		ret = ufshcd_suspend(hba, UFS_RUNTIME_PM);
out:
	trace_ufshcd_runtime_suspend(dev_name(hba->dev), ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	return ret;
}
EXPORT_SYMBOL(ufshcd_runtime_suspend);

/**
 * ufshcd_runtime_resume - runtime resume routine
 * @hba: per adapter instance
 *
 * This function basically brings the UFS device, UniPro link and controller
 * to active state. Following operations are done in this function:
 *
 * 1. Turn on all the controller related clocks
 * 2. Bring the UniPro link out of Hibernate state
 * 3. If UFS device is in sleep state, turn ON VCC rail and bring the UFS device
 *    to active state.
 * 4. If auto-bkops is enabled on the device, disable it.
 *
 * So following would be the possible power state after this function return
 * successfully:
 *	S1: UFS device in Active state with VCC rail ON
 *	    UniPro link in Active state
 *	    All the UFS/UniPro controller clocks are ON
 *
 * Returns 0 for success and non-zero for failure
 */
int ufshcd_runtime_resume(struct ufs_hba *hba)
{
	int ret = 0;
	ktime_t start = ktime_get();

	if (!hba)
		return -EINVAL;

	if (!hba->is_powered)
		goto out;
	else
		ret = ufshcd_resume(hba, UFS_RUNTIME_PM);
out:
	trace_ufshcd_runtime_resume(dev_name(hba->dev), ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	return ret;
}
EXPORT_SYMBOL(ufshcd_runtime_resume);

int ufshcd_runtime_idle(struct ufs_hba *hba)
{
	return 0;
}
EXPORT_SYMBOL(ufshcd_runtime_idle);

/**
 * ufshcd_shutdown - shutdown routine
 * @hba: per adapter instance
 *
 * This function would power off both UFS device and UFS link.
 *
 * Returns 0 always to allow force shutdown even in case of errors.
 */
int ufshcd_shutdown(struct ufs_hba *hba)
{
	int ret = 0;

	if (!hba->is_powered)
		goto out;

	if (ufshcd_is_ufs_dev_poweroff(hba) && ufshcd_is_link_off(hba))
		goto out;

	if (pm_runtime_suspended(hba->dev)) {
		ret = ufshcd_runtime_resume(hba);
		if (ret)
			goto out;
	}

	ret = ufshcd_suspend(hba, UFS_SHUTDOWN_PM);
out:
	if (ret)
		dev_err(hba->dev, "%s failed, err %d\n", __func__, ret);
	/* allow force shutdown even in case of errors */
	return 0;
}
EXPORT_SYMBOL(ufshcd_shutdown);

/**
 * ufshcd_remove - de-allocate SCSI host and host memory space
 *		data structure memory
 * @hba: per adapter instance
 */
void ufshcd_remove(struct ufs_hba *hba)
{
	ufs_bsg_remove(hba);
	ufs_sysfs_remove_nodes(hba->dev);
	scsi_remove_host(hba->host);
	/* disable interrupts */
	ufshcd_disable_intr(hba, hba->intr_mask);
	ufshcd_hba_stop(hba, true);

	ufshcd_exit_clk_scaling(hba);
	ufshcd_exit_clk_gating(hba);
	if (ufshcd_is_clkscaling_supported(hba))
		device_remove_file(hba->dev, &hba->clk_scaling.enable_attr);
	ufshcd_hba_exit(hba);
}
EXPORT_SYMBOL_GPL(ufshcd_remove);

/**
 * ufshcd_dealloc_host - deallocate Host Bus Adapter (HBA)
 * @hba: pointer to Host Bus Adapter (HBA)
 */
void ufshcd_dealloc_host(struct ufs_hba *hba)
{
	scsi_host_put(hba->host);
}
EXPORT_SYMBOL_GPL(ufshcd_dealloc_host);

/**
 * ufshcd_set_dma_mask - Set dma mask based on the controller
 *			 addressing capability
 * @hba: per adapter instance
 *
 * Returns 0 for success, non-zero for failure
 */
static int ufshcd_set_dma_mask(struct ufs_hba *hba)
{
	if (hba->capabilities & MASK_64_ADDRESSING_SUPPORT) {
		if (!dma_set_mask_and_coherent(hba->dev, DMA_BIT_MASK(64)))
			return 0;
	}
	return dma_set_mask_and_coherent(hba->dev, DMA_BIT_MASK(32));
}

/**
 * ufshcd_alloc_host - allocate Host Bus Adapter (HBA)
 * @dev: pointer to device handle
 * @hba_handle: driver private handle
 * Returns 0 on success, non-zero value on failure
 */
int ufshcd_alloc_host(struct device *dev, struct ufs_hba **hba_handle)
{
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	int err = 0;

	if (!dev) {                                                                          // 若dev为NULL
		dev_err(dev,
		"Invalid memory reference for dev is NULL\n");                                   // 打印提示信息
		err = -ENODEV;                                                                   // 设置err为-ENODEV
		goto out_error;                                                                  // 跳转至out_error
	}

	host = scsi_host_alloc(&ufshcd_driver_template,
				sizeof(struct ufs_hba));                                                 // register a scsi host adapter instance
	if (!host) {                                                                         // 若host为NULL
		dev_err(dev, "scsi_host_alloc failed\n");                                        // 打印提示信息
		err = -ENOMEM;                                                                   // 设置err为-ENOMEM
		goto out_error;                                                                  // 跳转至out_error
	}
	hba = shost_priv(host);                                                              // hba = (void *)shost->hostdata
	hba->host = host;                                                                    // 设置hba->host为host
	hba->dev = dev;                                                                      // 设置hba->dev为dev
	*hba_handle = hba;                                                                   // 保存hba到*hba_handle
	hba->dev_ref_clk_freq = REF_CLK_FREQ_INVAL;                                          // 设置hba->dev_ref_clk_freq为REF_CLK_FREQ_INVAL

	INIT_LIST_HEAD(&hba->clk_list_head);                                                 // 初始化hba->clk_list_head

out_error:
	return err;                                                                          // 返回err
}
EXPORT_SYMBOL(ufshcd_alloc_host);

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
	struct Scsi_Host *host = hba->host;                                                  // 取出host
	struct device *dev = hba->dev;                                                       // 取出dev

	if (!mmio_base) {                                                                    // host的寄存器基地址，若mmio_base为NULL
		dev_err(hba->dev,
		"Invalid memory reference for mmio_base is NULL\n");                             // 打印提示信息
		err = -ENODEV;                                                                   // 设置err为-ENODEV
		goto out_error;                                                                  // 跳转至out_error
	}

	hba->mmio_base = mmio_base;                                                          // 保存mmio_base到hba->mmio_base
	hba->irq = irq;                                                                      // 保存irq到hba->irq

	/* Set descriptor lengths to specification defaults */
	ufshcd_def_desc_sizes(hba);                                                          // 初始化hba->desc_size成员

	err = ufshcd_hba_init(hba);                                                          // 主要初始化regulators & clocks etc.
	if (err)                                                                             // 若err不为0
		goto out_error;                                                                  // 跳转至out_error

	/* Read capabilities registers */
	ufshcd_hba_capabilities(hba);                                                        // 读取hba->mmio_base + REG_CONTROLLER_CAPABILITIES的值保存到hba->capabilities，并根据hba->capabilities计算hba->nutrs和hba->nutmrs

	/* Get UFS version supported by the controller */
	hba->ufs_version = ufshcd_get_ufs_version(hba);                                      // 读取hba->mmio_base + REG_UFS_VERSION的值保存到hba->ufs_version

	if ((hba->ufs_version != UFSHCI_VERSION_10) &&
	    (hba->ufs_version != UFSHCI_VERSION_11) &&
	    (hba->ufs_version != UFSHCI_VERSION_20) &&
	    (hba->ufs_version != UFSHCI_VERSION_21))                                         // 若hba->ufs_version不与UFSHCI_VERSION_10、UFSHCI_VERSION_11、UFSHCI_VERSION_20和UFSHCI_VERSION_21中任何一个相等
		dev_err(hba->dev, "invalid UFS version 0x%x\n",
			hba->ufs_version);                                                           // 打印提示信息

	/* Get Interrupt bit mask per version */
	hba->intr_mask = ufshcd_get_intr_mask(hba);                                          // 根据hba->ufs_version设置intr_mask

	err = ufshcd_set_dma_mask(hba);                                                      // 根据hba->capabilities设置dma属性
	if (err) {                                                                           // 若err不为0
		dev_err(hba->dev, "set dma mask failed\n");                                      // 打印提示信息
		goto out_disable;                                                                // 跳转至out_disable
	}

	/* Allocate memory for host memory space */
	err = ufshcd_memory_alloc(hba);                                                      // 重点：为hba->ucdl_base_addr & hba->utrdl_base_addr & hba->utmrdl_base_addr申请dma内存，为hba->lrb申请普通内存
	if (err) {                                                                           // 若err不为0
		dev_err(hba->dev, "Memory allocation failed\n");                                 // 打印提示信息
		goto out_disable;                                                                // 跳转至out_disable
	}

	/* Configure LRB */
	ufshcd_host_memory_configure(hba);                                                   // 重点：configure local reference block with memory offsets

	host->can_queue = hba->nutrs;                                                        // Number of UTP Transfer Requests
	host->cmd_per_lun = hba->nutrs;                                                      // Number of UTP Transfer Requests
	host->max_id = UFSHCD_MAX_ID;                                                        // UFSHCD_MAX_ID = 1
	host->max_lun = UFS_MAX_LUNS;                                                        // 设置host->max_lun为UFS_MAX_LUNS
	host->max_channel = UFSHCD_MAX_CHANNEL;                                              // UFSHCD_MAX_CHANNEL = 0
	host->unique_id = host->host_no;                                                     // 设置host->unique_id为host->host_no
	host->max_cmd_len = UFS_CDB_SIZE;                                                    // #define UFS_CDB_SIZE 16

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

	ufshcd_init_clk_gating(hba);                                                         // *

	ufshcd_init_clk_scaling(hba);                                                        // *

	/*
	 * In order to avoid any spurious interrupt immediately after
	 * registering UFS controller interrupt handler, clear any pending UFS
	 * interrupt status and disable all the UFS interrupts.
	 */
	ufshcd_writel(hba, ufshcd_readl(hba, REG_INTERRUPT_STATUS),
		      REG_INTERRUPT_STATUS);                                                     // 读取hba->mmio_base + REG_INTERRUPT_STATUS的值再写回到hba->mmio_base + REG_INTERRUPT_STATUS，猜测写hba->mmio_base + REG_INTERRUPT_STATUS将会reset其值
	ufshcd_writel(hba, 0, REG_INTERRUPT_ENABLE);                                         // 写0到hba->mmio_base + REG_INTERRUPT_ENABLE
	/*
	 * Make sure that UFS interrupts are disabled and any pending interrupt
	 * status is cleared before registering UFS interrupt handler.
	 */
	mb();                                                                                // Make sure that UFS interrupts are disabled and any pending interrupt status is cleared before registering UFS interrupt handler

	/* IRQ registration */
	err = devm_request_irq(dev, irq, ufshcd_intr, IRQF_SHARED, UFSHCD, hba);             // 注册中断irq
	if (err) {                                                                           // 若err不为0
		dev_err(hba->dev, "request irq failed\n");                                       // 打印提示信息
		goto exit_gating;                                                                // 跳转至exit_gating
	} else {
		hba->is_irq_enabled = true;                                                      // 设置hba->is_irq_enabled为true
	}

	err = scsi_add_host(host, hba->dev);                                                 // *
	if (err) {                                                                           // 若err不为0
		dev_err(hba->dev, "scsi_add_host failed\n");                                     // 打印提示信息
		goto exit_gating;                                                                // 跳转至exit_gating
	}

	/* Reset the attached device */
	ufshcd_vops_device_reset(hba);                                                       // *

	/* Host controller enable */
	err = ufshcd_hba_enable(hba);                                                        // *
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
						UIC_LINK_HIBERN8_STATE);                                         // *
	hba->spm_lvl = ufs_get_desired_pm_lvl_for_dev_link_state(
						UFS_SLEEP_PWR_MODE,
						UIC_LINK_HIBERN8_STATE);                                         // *

	/* Set the default auto-hiberate idle timer value to 150 ms */
	if (ufshcd_is_auto_hibern8_supported(hba) && !hba->ahit) {
		hba->ahit = FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 150) |
			    FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 3);                               // *
	}

	/* Hold auto suspend until async scan completes */
	pm_runtime_get_sync(dev);                                                            // *
	atomic_set(&hba->scsi_block_reqs_cnt, 0);                                            // 设置hba->scsi_block_reqs_cnt为0
	/*
	 * We are assuming that device wasn't put in sleep/power-down
	 * state exclusively during the boot stage before kernel.
	 * This assumption helps avoid doing link startup twice during
	 * ufshcd_probe_hba().
	 */
	ufshcd_set_ufs_dev_active(hba);                                                      // *

	async_schedule(ufshcd_async_scan, hba);                                              // *
	ufs_sysfs_add_nodes(hba->dev);                                                       // *

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
	return err;                                                                          // 返回err
}
EXPORT_SYMBOL_GPL(ufshcd_init);

MODULE_AUTHOR("Santosh Yaragnavi <santosh.sy@samsung.com>");
MODULE_AUTHOR("Vinayak Holikatti <h.vinayak@samsung.com>");
MODULE_DESCRIPTION("Generic UFS host controller driver Core");
MODULE_LICENSE("GPL");
MODULE_VERSION(UFSHCD_DRIVER_VERSION);

======================================================================================================================================================

/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SCSI_DISK_H
#define _SCSI_DISK_H

/*
 * More than enough for everybody ;)  The huge number of majors
 * is a leftover from 16bit dev_t days, we don't really need that
 * much numberspace.
 */
#define SD_MAJORS	16

/*
 * Time out in seconds for disks and Magneto-opticals (which are slower).
 */
#define SD_TIMEOUT		(30 * HZ)
#define SD_MOD_TIMEOUT		(75 * HZ)
/*
 * Flush timeout is a multiplier over the standard device timeout which is
 * user modifiable via sysfs but initially set to SD_TIMEOUT
 */
#define SD_FLUSH_TIMEOUT_MULTIPLIER	2
#define SD_WRITE_SAME_TIMEOUT	(120 * HZ)

/*
 * Number of allowed retries
 */
#define SD_MAX_RETRIES		5
#define SD_PASSTHROUGH_RETRIES	1
#define SD_MAX_MEDIUM_TIMEOUTS	2

/*
 * Size of the initial data buffer for mode and read capacity data
 */
#define SD_BUF_SIZE		512

/*
 * Number of sectors at the end of the device to avoid multi-sector
 * accesses to in the case of last_sector_bug
 */
#define SD_LAST_BUGGY_SECTORS	8

enum {
	SD_EXT_CDB_SIZE = 32,	/* Extended CDB size */
	SD_MEMPOOL_SIZE = 2,	/* CDB pool size */
};

enum {
	SD_DEF_XFER_BLOCKS = 0xffff,
	SD_MAX_XFER_BLOCKS = 0xffffffff,
	SD_MAX_WS10_BLOCKS = 0xffff,
	SD_MAX_WS16_BLOCKS = 0x7fffff,
};

enum {
	SD_LBP_FULL = 0,	/* Full logical block provisioning */
	SD_LBP_UNMAP,		/* Use UNMAP command */
	SD_LBP_WS16,		/* Use WRITE SAME(16) with UNMAP bit */
	SD_LBP_WS10,		/* Use WRITE SAME(10) with UNMAP bit */
	SD_LBP_ZERO,		/* Use WRITE SAME(10) with zero payload */
	SD_LBP_DISABLE,		/* Discard disabled due to failed cmd */
};

enum {
	SD_ZERO_WRITE = 0,	/* Use WRITE(10/16) command */
	SD_ZERO_WS,		/* Use WRITE SAME(10/16) command */
	SD_ZERO_WS16_UNMAP,	/* Use WRITE SAME(16) with UNMAP */
	SD_ZERO_WS10_UNMAP,	/* Use WRITE SAME(10) with UNMAP */
};

struct scsi_disk {
	struct scsi_driver *driver;	/* always &sd_template */
	struct scsi_device *device;
	struct device	dev;
	struct gendisk	*disk;
	struct opal_dev *opal_dev;
#ifdef CONFIG_BLK_DEV_ZONED
	u32		nr_zones;
	u32		zone_blocks;
	u32		zones_optimal_open;
	u32		zones_optimal_nonseq;
	u32		zones_max_open;
#endif
	atomic_t	openers;
	sector_t	capacity;	/* size in logical blocks */
	u32		max_xfer_blocks;
	u32		opt_xfer_blocks;
	u32		max_ws_blocks;
	u32		max_unmap_blocks;
	u32		unmap_granularity;
	u32		unmap_alignment;
	u32		index;
	unsigned int	physical_block_size;
	unsigned int	max_medium_access_timeouts;
	unsigned int	medium_access_timed_out;
	u8		media_present;
	u8		write_prot;
	u8		protection_type;/* Data Integrity Field */
	u8		provisioning_mode;
	u8		zeroing_mode;
	unsigned	ATO : 1;	/* state of disk ATO bit */
	unsigned	cache_override : 1; /* temp override of WCE,RCD */
	unsigned	WCE : 1;	/* state of disk WCE bit */
	unsigned	RCD : 1;	/* state of disk RCD bit, unused */
	unsigned	DPOFUA : 1;	/* state of disk DPOFUA bit */
	unsigned	first_scan : 1;
	unsigned	lbpme : 1;
	unsigned	lbprz : 1;
	unsigned	lbpu : 1;
	unsigned	lbpws : 1;
	unsigned	lbpws10 : 1;
	unsigned	lbpvpd : 1;
	unsigned	ws10 : 1;
	unsigned	ws16 : 1;
	unsigned	rc_basis: 2;
	unsigned	zoned: 2;
	unsigned	urswrz : 1;
	unsigned	security : 1;
	unsigned	ignore_medium_access_errors : 1;
};
#define to_scsi_disk(obj) container_of(obj,struct scsi_disk,dev)

static inline struct scsi_disk *scsi_disk(struct gendisk *disk)
{
	return container_of(disk->private_data, struct scsi_disk, driver);
}

#define sd_printk(prefix, sdsk, fmt, a...)				\
        (sdsk)->disk ?							\
	      sdev_prefix_printk(prefix, (sdsk)->device,		\
				 (sdsk)->disk->disk_name, fmt, ##a) :	\
	      sdev_printk(prefix, (sdsk)->device, fmt, ##a)

#define sd_first_printk(prefix, sdsk, fmt, a...)			\
	do {								\
		if ((sdsk)->first_scan)					\
			sd_printk(prefix, sdsk, fmt, ##a);		\
	} while (0)

static inline int scsi_medium_access_command(struct scsi_cmnd *scmd)
{
	switch (scmd->cmnd[0]) {
	case READ_6:
	case READ_10:
	case READ_12:
	case READ_16:
	case SYNCHRONIZE_CACHE:
	case VERIFY:
	case VERIFY_12:
	case VERIFY_16:
	case WRITE_6:
	case WRITE_10:
	case WRITE_12:
	case WRITE_16:
	case WRITE_SAME:
	case WRITE_SAME_16:
	case UNMAP:
		return 1;
	case VARIABLE_LENGTH_CMD:
		switch (scmd->cmnd[9]) {
		case READ_32:
		case VERIFY_32:
		case WRITE_32:
		case WRITE_SAME_32:
			return 1;
		}
	}

	return 0;
}

static inline sector_t logical_to_sectors(struct scsi_device *sdev, sector_t blocks)
{
	return blocks << (ilog2(sdev->sector_size) - 9);
}

static inline unsigned int logical_to_bytes(struct scsi_device *sdev, sector_t blocks)
{
	return blocks * sdev->sector_size;
}

static inline sector_t bytes_to_logical(struct scsi_device *sdev, unsigned int bytes)
{
	return bytes >> ilog2(sdev->sector_size);
}

static inline sector_t sectors_to_logical(struct scsi_device *sdev, sector_t sector)
{
	return sector >> (ilog2(sdev->sector_size) - 9);
}

#ifdef CONFIG_BLK_DEV_INTEGRITY

extern void sd_dif_config_host(struct scsi_disk *);

#else /* CONFIG_BLK_DEV_INTEGRITY */

static inline void sd_dif_config_host(struct scsi_disk *disk)
{
}

#endif /* CONFIG_BLK_DEV_INTEGRITY */

static inline int sd_is_zoned(struct scsi_disk *sdkp)
{
	return sdkp->zoned == 1 || sdkp->device->type == TYPE_ZBC;
}

#ifdef CONFIG_BLK_DEV_ZONED

extern int sd_zbc_read_zones(struct scsi_disk *sdkp, unsigned char *buffer);
extern void sd_zbc_print_zones(struct scsi_disk *sdkp);
extern blk_status_t sd_zbc_setup_reset_cmnd(struct scsi_cmnd *cmd, bool all);
extern void sd_zbc_complete(struct scsi_cmnd *cmd, unsigned int good_bytes,
			    struct scsi_sense_hdr *sshdr);
extern int sd_zbc_report_zones(struct gendisk *disk, sector_t sector,
			       struct blk_zone *zones, unsigned int *nr_zones);

#else /* CONFIG_BLK_DEV_ZONED */

static inline int sd_zbc_read_zones(struct scsi_disk *sdkp,
				    unsigned char *buf)
{
	return 0;
}

static inline void sd_zbc_print_zones(struct scsi_disk *sdkp) {}

static inline blk_status_t sd_zbc_setup_reset_cmnd(struct scsi_cmnd *cmd,
						   bool all)
{
	return BLK_STS_TARGET;
}

static inline void sd_zbc_complete(struct scsi_cmnd *cmd,
				   unsigned int good_bytes,
				   struct scsi_sense_hdr *sshdr) {}

#define sd_zbc_report_zones NULL

#endif /* CONFIG_BLK_DEV_ZONED */

#endif /* _SCSI_DISK_H */

// SPDX-License-Identifier: GPL-2.0-only
/*
 *      sd.c Copyright (C) 1992 Drew Eckhardt
 *           Copyright (C) 1993, 1994, 1995, 1999 Eric Youngdale
 *
 *      Linux scsi disk driver
 *              Initial versions: Drew Eckhardt
 *              Subsequent revisions: Eric Youngdale
 *	Modification history:
 *       - Drew Eckhardt <drew@colorado.edu> original
 *       - Eric Youngdale <eric@andante.org> add scatter-gather, multiple 
 *         outstanding request, and other enhancements.
 *         Support loadable low-level scsi drivers.
 *       - Jirka Hanika <geo@ff.cuni.cz> support more scsi disks using 
 *         eight major numbers.
 *       - Richard Gooch <rgooch@atnf.csiro.au> support devfs.
 *	 - Torben Mathiasen <tmm@image.dk> Resource allocation fixes in 
 *	   sd_init and cleanups.
 *	 - Alex Davis <letmein@erols.com> Fix problem where partition info
 *	   not being read in sd_open. Fix problem where removable media 
 *	   could be ejected after sd_open.
 *	 - Douglas Gilbert <dgilbert@interlog.com> cleanup for lk 2.5.x
 *	 - Badari Pulavarty <pbadari@us.ibm.com>, Matthew Wilcox 
 *	   <willy@debian.org>, Kurt Garloff <garloff@suse.de>: 
 *	   Support 32k/1M disks.
 *
 *	Logging policy (needs CONFIG_SCSI_LOGGING defined):
 *	 - setting up transfer: SCSI_LOG_HLQUEUE levels 1 and 2
 *	 - end of transfer (bh + scsi_lib): SCSI_LOG_HLCOMPLETE level 1
 *	 - entering sd_ioctl: SCSI_LOG_IOCTL level 1
 *	 - entering other commands: SCSI_LOG_HLQUEUE level 3
 *	Note: when the logging level is set by the user, it must be greater
 *	than the level indicated above to trigger output.	
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/bio.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/errno.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/blk-pm.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/string_helpers.h>
#include <linux/async.h>
#include <linux/slab.h>
#include <linux/sed-opal.h>
#include <linux/pm_runtime.h>
#include <linux/pr.h>
#include <linux/t10-pi.h>
#include <linux/uaccess.h>
#include <asm/unaligned.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/scsicam.h>

#include "sd.h"
#include "scsi_priv.h"
#include "scsi_logging.h"

MODULE_AUTHOR("Eric Youngdale");
MODULE_DESCRIPTION("SCSI disk (sd) driver");
MODULE_LICENSE("GPL");

MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK0_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK1_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK2_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK3_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK4_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK5_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK6_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK7_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK8_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK9_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK10_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK11_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK12_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK13_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK14_MAJOR);
MODULE_ALIAS_BLOCKDEV_MAJOR(SCSI_DISK15_MAJOR);
MODULE_ALIAS_SCSI_DEVICE(TYPE_DISK);
MODULE_ALIAS_SCSI_DEVICE(TYPE_MOD);
MODULE_ALIAS_SCSI_DEVICE(TYPE_RBC);
MODULE_ALIAS_SCSI_DEVICE(TYPE_ZBC);

#if !defined(CONFIG_DEBUG_BLOCK_EXT_DEVT)
#define SD_MINORS	16
#else
#define SD_MINORS	0
#endif

static void sd_config_discard(struct scsi_disk *, unsigned int);
static void sd_config_write_same(struct scsi_disk *);
static int  sd_revalidate_disk(struct gendisk *);
static void sd_unlock_native_capacity(struct gendisk *disk);
static int  sd_probe(struct device *);
static int  sd_remove(struct device *);
static void sd_shutdown(struct device *);
static int sd_suspend_system(struct device *);
static int sd_suspend_runtime(struct device *);
static int sd_resume(struct device *);
static void sd_rescan(struct device *);
static blk_status_t sd_init_command(struct scsi_cmnd *SCpnt);
static void sd_uninit_command(struct scsi_cmnd *SCpnt);
static int sd_done(struct scsi_cmnd *);
static void sd_eh_reset(struct scsi_cmnd *);
static int sd_eh_action(struct scsi_cmnd *, int);
static void sd_read_capacity(struct scsi_disk *sdkp, unsigned char *buffer);
static void scsi_disk_release(struct device *cdev);
static void sd_print_sense_hdr(struct scsi_disk *, struct scsi_sense_hdr *);
static void sd_print_result(const struct scsi_disk *, const char *, int);

static DEFINE_IDA(sd_index_ida);

/* This semaphore is used to mediate the 0->1 reference get in the
 * face of object destruction (i.e. we can't allow a get on an
 * object after last put) */
static DEFINE_MUTEX(sd_ref_mutex);

static struct kmem_cache *sd_cdb_cache;
static mempool_t *sd_cdb_pool;
static mempool_t *sd_page_pool;

static const char *sd_cache_types[] = {
	"write through", "none", "write back",
	"write back, no read (daft)"
};

static void sd_set_flush_flag(struct scsi_disk *sdkp)
{
	bool wc = false, fua = false;

	if (sdkp->WCE) {
		wc = true;
		if (sdkp->DPOFUA)
			fua = true;
	}

	blk_queue_write_cache(sdkp->disk->queue, wc, fua);
}

static ssize_t
cache_type_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ct, rcd, wce, sp;
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;
	char buffer[64];
	char *buffer_data;
	struct scsi_mode_data data;
	struct scsi_sense_hdr sshdr;
	static const char temp[] = "temporary ";
	int len;

	if (sdp->type != TYPE_DISK && sdp->type != TYPE_ZBC)
		/* no cache control on RBC devices; theoretically they
		 * can do it, but there's probably so many exceptions
		 * it's not worth the risk */
		return -EINVAL;

	if (strncmp(buf, temp, sizeof(temp) - 1) == 0) {
		buf += sizeof(temp) - 1;
		sdkp->cache_override = 1;
	} else {
		sdkp->cache_override = 0;
	}

	ct = sysfs_match_string(sd_cache_types, buf);
	if (ct < 0)
		return -EINVAL;

	rcd = ct & 0x01 ? 1 : 0;
	wce = (ct & 0x02) && !sdkp->write_prot ? 1 : 0;

	if (sdkp->cache_override) {
		sdkp->WCE = wce;
		sdkp->RCD = rcd;
		sd_set_flush_flag(sdkp);
		return count;
	}

	if (scsi_mode_sense(sdp, 0x08, 8, buffer, sizeof(buffer), SD_TIMEOUT,
			    SD_MAX_RETRIES, &data, NULL))
		return -EINVAL;
	len = min_t(size_t, sizeof(buffer), data.length - data.header_length -
		  data.block_descriptor_length);
	buffer_data = buffer + data.header_length +
		data.block_descriptor_length;
	buffer_data[2] &= ~0x05;
	buffer_data[2] |= wce << 2 | rcd;
	sp = buffer_data[0] & 0x80 ? 1 : 0;
	buffer_data[0] &= ~0x80;

	/*
	 * Ensure WP, DPOFUA, and RESERVED fields are cleared in
	 * received mode parameter buffer before doing MODE SELECT.
	 */
	data.device_specific = 0;

	if (scsi_mode_select(sdp, 1, sp, 8, buffer_data, len, SD_TIMEOUT,
			     SD_MAX_RETRIES, &data, &sshdr)) {
		if (scsi_sense_valid(&sshdr))
			sd_print_sense_hdr(sdkp, &sshdr);
		return -EINVAL;
	}
	revalidate_disk(sdkp->disk);
	return count;
}

static ssize_t
manage_start_stop_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;

	return sprintf(buf, "%u\n", sdp->manage_start_stop);
}

static ssize_t
manage_start_stop_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;
	bool v;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (kstrtobool(buf, &v))
		return -EINVAL;

	sdp->manage_start_stop = v;

	return count;
}
static DEVICE_ATTR_RW(manage_start_stop);

static ssize_t
allow_restart_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->device->allow_restart);
}

static ssize_t
allow_restart_store(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	bool v;
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (sdp->type != TYPE_DISK && sdp->type != TYPE_ZBC)
		return -EINVAL;

	if (kstrtobool(buf, &v))
		return -EINVAL;

	sdp->allow_restart = v;

	return count;
}
static DEVICE_ATTR_RW(allow_restart);

static ssize_t
cache_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	int ct = sdkp->RCD + 2*sdkp->WCE;

	return sprintf(buf, "%s\n", sd_cache_types[ct]);
}
static DEVICE_ATTR_RW(cache_type);

static ssize_t
FUA_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->DPOFUA);
}
static DEVICE_ATTR_RO(FUA);

static ssize_t
protection_type_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->protection_type);
}

static ssize_t
protection_type_store(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	unsigned int val;
	int err;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	err = kstrtouint(buf, 10, &val);

	if (err)
		return err;

	if (val <= T10_PI_TYPE3_PROTECTION)
		sdkp->protection_type = val;

	return count;
}
static DEVICE_ATTR_RW(protection_type);

static ssize_t
protection_mode_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;
	unsigned int dif, dix;

	dif = scsi_host_dif_capable(sdp->host, sdkp->protection_type);
	dix = scsi_host_dix_capable(sdp->host, sdkp->protection_type);

	if (!dix && scsi_host_dix_capable(sdp->host, T10_PI_TYPE0_PROTECTION)) {
		dif = 0;
		dix = 1;
	}

	if (!dif && !dix)
		return sprintf(buf, "none\n");

	return sprintf(buf, "%s%u\n", dix ? "dix" : "dif", dif);
}
static DEVICE_ATTR_RO(protection_mode);

static ssize_t
app_tag_own_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->ATO);
}
static DEVICE_ATTR_RO(app_tag_own);

static ssize_t
thin_provisioning_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->lbpme);
}
static DEVICE_ATTR_RO(thin_provisioning);

/* sysfs_match_string() requires dense arrays */
static const char *lbp_mode[] = {
	[SD_LBP_FULL]		= "full",
	[SD_LBP_UNMAP]		= "unmap",
	[SD_LBP_WS16]		= "writesame_16",
	[SD_LBP_WS10]		= "writesame_10",
	[SD_LBP_ZERO]		= "writesame_zero",
	[SD_LBP_DISABLE]	= "disabled",
};

static ssize_t
provisioning_mode_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%s\n", lbp_mode[sdkp->provisioning_mode]);
}

static ssize_t
provisioning_mode_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;
	int mode;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (sd_is_zoned(sdkp)) {
		sd_config_discard(sdkp, SD_LBP_DISABLE);
		return count;
	}

	if (sdp->type != TYPE_DISK)
		return -EINVAL;

	mode = sysfs_match_string(lbp_mode, buf);
	if (mode < 0)
		return -EINVAL;

	sd_config_discard(sdkp, mode);

	return count;
}
static DEVICE_ATTR_RW(provisioning_mode);

/* sysfs_match_string() requires dense arrays */
static const char *zeroing_mode[] = {
	[SD_ZERO_WRITE]		= "write",
	[SD_ZERO_WS]		= "writesame",
	[SD_ZERO_WS16_UNMAP]	= "writesame_16_unmap",
	[SD_ZERO_WS10_UNMAP]	= "writesame_10_unmap",
};

static ssize_t
zeroing_mode_show(struct device *dev, struct device_attribute *attr,
		  char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%s\n", zeroing_mode[sdkp->zeroing_mode]);
}

static ssize_t
zeroing_mode_store(struct device *dev, struct device_attribute *attr,
		   const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	int mode;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	mode = sysfs_match_string(zeroing_mode, buf);
	if (mode < 0)
		return -EINVAL;

	sdkp->zeroing_mode = mode;

	return count;
}
static DEVICE_ATTR_RW(zeroing_mode);

static ssize_t
max_medium_access_timeouts_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->max_medium_access_timeouts);
}

static ssize_t
max_medium_access_timeouts_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	int err;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	err = kstrtouint(buf, 10, &sdkp->max_medium_access_timeouts);

	return err ? err : count;
}
static DEVICE_ATTR_RW(max_medium_access_timeouts);

static ssize_t
max_write_same_blocks_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);

	return sprintf(buf, "%u\n", sdkp->max_ws_blocks);
}

static ssize_t
max_write_same_blocks_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct scsi_device *sdp = sdkp->device;
	unsigned long max;
	int err;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (sdp->type != TYPE_DISK && sdp->type != TYPE_ZBC)
		return -EINVAL;

	err = kstrtoul(buf, 10, &max);

	if (err)
		return err;

	if (max == 0)
		sdp->no_write_same = 1;
	else if (max <= SD_MAX_WS16_BLOCKS) {
		sdp->no_write_same = 0;
		sdkp->max_ws_blocks = max;
	}

	sd_config_write_same(sdkp);

	return count;
}
static DEVICE_ATTR_RW(max_write_same_blocks);

static struct attribute *sd_disk_attrs[] = {
	&dev_attr_cache_type.attr,
	&dev_attr_FUA.attr,
	&dev_attr_allow_restart.attr,
	&dev_attr_manage_start_stop.attr,
	&dev_attr_protection_type.attr,
	&dev_attr_protection_mode.attr,
	&dev_attr_app_tag_own.attr,
	&dev_attr_thin_provisioning.attr,
	&dev_attr_provisioning_mode.attr,
	&dev_attr_zeroing_mode.attr,
	&dev_attr_max_write_same_blocks.attr,
	&dev_attr_max_medium_access_timeouts.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sd_disk);

static struct class sd_disk_class = {
	.name		= "scsi_disk",
	.owner		= THIS_MODULE,
	.dev_release	= scsi_disk_release,
	.dev_groups	= sd_disk_groups,
};

static const struct dev_pm_ops sd_pm_ops = {
	.suspend		= sd_suspend_system,
	.resume			= sd_resume,
	.poweroff		= sd_suspend_system,
	.restore		= sd_resume,
	.runtime_suspend	= sd_suspend_runtime,
	.runtime_resume		= sd_resume,
};

static struct scsi_driver sd_template = {
	.gendrv = {
		.name		= "sd",
		.owner		= THIS_MODULE,
		.probe		= sd_probe,
		.probe_type	= PROBE_PREFER_ASYNCHRONOUS,
		.remove		= sd_remove,
		.shutdown	= sd_shutdown,
		.pm		= &sd_pm_ops,
	},
	.rescan			= sd_rescan,
	.init_command		= sd_init_command,
	.uninit_command		= sd_uninit_command,
	.done			= sd_done,
	.eh_action		= sd_eh_action,
	.eh_reset		= sd_eh_reset,
};

/*
 * Dummy kobj_map->probe function.
 * The default ->probe function will call modprobe, which is
 * pointless as this module is already loaded.
 */
static struct kobject *sd_default_probe(dev_t devt, int *partno, void *data)
{
	return NULL;
}

/*
 * Device no to disk mapping:
 * 
 *       major         disc2     disc  p1
 *   |............|.............|....|....| <- dev_t
 *    31        20 19          8 7  4 3  0
 * 
 * Inside a major, we have 16k disks, however mapped non-
 * contiguously. The first 16 disks are for major0, the next
 * ones with major1, ... Disk 256 is for major0 again, disk 272 
 * for major1, ... 
 * As we stay compatible with our numbering scheme, we can reuse 
 * the well-know SCSI majors 8, 65--71, 136--143.
 */
static int sd_major(int major_idx)
{
	switch (major_idx) {
	case 0:
		return SCSI_DISK0_MAJOR;
	case 1 ... 7:
		return SCSI_DISK1_MAJOR + major_idx - 1;
	case 8 ... 15:
		return SCSI_DISK8_MAJOR + major_idx - 8;
	default:
		BUG();
		return 0;	/* shut up gcc */
	}
}

static struct scsi_disk *scsi_disk_get(struct gendisk *disk)
{
	struct scsi_disk *sdkp = NULL;

	mutex_lock(&sd_ref_mutex);

	if (disk->private_data) {
		sdkp = scsi_disk(disk);
		if (scsi_device_get(sdkp->device) == 0)
			get_device(&sdkp->dev);
		else
			sdkp = NULL;
	}
	mutex_unlock(&sd_ref_mutex);
	return sdkp;
}

static void scsi_disk_put(struct scsi_disk *sdkp)
{
	struct scsi_device *sdev = sdkp->device;

	mutex_lock(&sd_ref_mutex);
	put_device(&sdkp->dev);
	scsi_device_put(sdev);
	mutex_unlock(&sd_ref_mutex);
}

#ifdef CONFIG_BLK_SED_OPAL
static int sd_sec_submit(void *data, u16 spsp, u8 secp, void *buffer,
		size_t len, bool send)
{
	struct scsi_device *sdev = data;
	u8 cdb[12] = { 0, };
	int ret;

	cdb[0] = send ? SECURITY_PROTOCOL_OUT : SECURITY_PROTOCOL_IN;
	cdb[1] = secp;
	put_unaligned_be16(spsp, &cdb[2]);
	put_unaligned_be32(len, &cdb[6]);

	ret = scsi_execute_req(sdev, cdb,
			send ? DMA_TO_DEVICE : DMA_FROM_DEVICE,
			buffer, len, NULL, SD_TIMEOUT, SD_MAX_RETRIES, NULL);
	return ret <= 0 ? ret : -EIO;
}
#endif /* CONFIG_BLK_SED_OPAL */

/*
 * Look up the DIX operation based on whether the command is read or
 * write and whether dix and dif are enabled.
 */
static unsigned int sd_prot_op(bool write, bool dix, bool dif)
{
	/* Lookup table: bit 2 (write), bit 1 (dix), bit 0 (dif) */
	static const unsigned int ops[] = {	/* wrt dix dif */
		SCSI_PROT_NORMAL,		/*  0	0   0  */
		SCSI_PROT_READ_STRIP,		/*  0	0   1  */
		SCSI_PROT_READ_INSERT,		/*  0	1   0  */
		SCSI_PROT_READ_PASS,		/*  0	1   1  */
		SCSI_PROT_NORMAL,		/*  1	0   0  */
		SCSI_PROT_WRITE_INSERT,		/*  1	0   1  */
		SCSI_PROT_WRITE_STRIP,		/*  1	1   0  */
		SCSI_PROT_WRITE_PASS,		/*  1	1   1  */
	};

	return ops[write << 2 | dix << 1 | dif];
}

/*
 * Returns a mask of the protection flags that are valid for a given DIX
 * operation.
 */
static unsigned int sd_prot_flag_mask(unsigned int prot_op)
{
	static const unsigned int flag_mask[] = {
		[SCSI_PROT_NORMAL]		= 0,

		[SCSI_PROT_READ_STRIP]		= SCSI_PROT_TRANSFER_PI |
						  SCSI_PROT_GUARD_CHECK |
						  SCSI_PROT_REF_CHECK |
						  SCSI_PROT_REF_INCREMENT,

		[SCSI_PROT_READ_INSERT]		= SCSI_PROT_REF_INCREMENT |
						  SCSI_PROT_IP_CHECKSUM,

		[SCSI_PROT_READ_PASS]		= SCSI_PROT_TRANSFER_PI |
						  SCSI_PROT_GUARD_CHECK |
						  SCSI_PROT_REF_CHECK |
						  SCSI_PROT_REF_INCREMENT |
						  SCSI_PROT_IP_CHECKSUM,

		[SCSI_PROT_WRITE_INSERT]	= SCSI_PROT_TRANSFER_PI |
						  SCSI_PROT_REF_INCREMENT,

		[SCSI_PROT_WRITE_STRIP]		= SCSI_PROT_GUARD_CHECK |
						  SCSI_PROT_REF_CHECK |
						  SCSI_PROT_REF_INCREMENT |
						  SCSI_PROT_IP_CHECKSUM,

		[SCSI_PROT_WRITE_PASS]		= SCSI_PROT_TRANSFER_PI |
						  SCSI_PROT_GUARD_CHECK |
						  SCSI_PROT_REF_CHECK |
						  SCSI_PROT_REF_INCREMENT |
						  SCSI_PROT_IP_CHECKSUM,
	};

	return flag_mask[prot_op];
}

static unsigned char sd_setup_protect_cmnd(struct scsi_cmnd *scmd,
					   unsigned int dix, unsigned int dif)
{
	struct bio *bio = scmd->request->bio;
	unsigned int prot_op = sd_prot_op(rq_data_dir(scmd->request), dix, dif);
	unsigned int protect = 0;

	if (dix) {				/* DIX Type 0, 1, 2, 3 */
		if (bio_integrity_flagged(bio, BIP_IP_CHECKSUM))
			scmd->prot_flags |= SCSI_PROT_IP_CHECKSUM;

		if (bio_integrity_flagged(bio, BIP_CTRL_NOCHECK) == false)
			scmd->prot_flags |= SCSI_PROT_GUARD_CHECK;
	}

	if (dif != T10_PI_TYPE3_PROTECTION) {	/* DIX/DIF Type 0, 1, 2 */
		scmd->prot_flags |= SCSI_PROT_REF_INCREMENT;

		if (bio_integrity_flagged(bio, BIP_CTRL_NOCHECK) == false)
			scmd->prot_flags |= SCSI_PROT_REF_CHECK;
	}

	if (dif) {				/* DIX/DIF Type 1, 2, 3 */
		scmd->prot_flags |= SCSI_PROT_TRANSFER_PI;

		if (bio_integrity_flagged(bio, BIP_DISK_NOCHECK))
			protect = 3 << 5;	/* Disable target PI checking */
		else
			protect = 1 << 5;	/* Enable target PI checking */
	}

	scsi_set_prot_op(scmd, prot_op);
	scsi_set_prot_type(scmd, dif);
	scmd->prot_flags &= sd_prot_flag_mask(prot_op);

	return protect;
}

static void sd_config_discard(struct scsi_disk *sdkp, unsigned int mode)
{
	struct request_queue *q = sdkp->disk->queue;
	unsigned int logical_block_size = sdkp->device->sector_size;
	unsigned int max_blocks = 0;

	q->limits.discard_alignment =
		sdkp->unmap_alignment * logical_block_size;
	q->limits.discard_granularity =
		max(sdkp->physical_block_size,
		    sdkp->unmap_granularity * logical_block_size);
	sdkp->provisioning_mode = mode;

	switch (mode) {

	case SD_LBP_FULL:
	case SD_LBP_DISABLE:
		blk_queue_max_discard_sectors(q, 0);
		blk_queue_flag_clear(QUEUE_FLAG_DISCARD, q);
		return;

	case SD_LBP_UNMAP:
		max_blocks = min_not_zero(sdkp->max_unmap_blocks,
					  (u32)SD_MAX_WS16_BLOCKS);
		break;

	case SD_LBP_WS16:
		if (sdkp->device->unmap_limit_for_ws)
			max_blocks = sdkp->max_unmap_blocks;
		else
			max_blocks = sdkp->max_ws_blocks;

		max_blocks = min_not_zero(max_blocks, (u32)SD_MAX_WS16_BLOCKS);
		break;

	case SD_LBP_WS10:
		if (sdkp->device->unmap_limit_for_ws)
			max_blocks = sdkp->max_unmap_blocks;
		else
			max_blocks = sdkp->max_ws_blocks;

		max_blocks = min_not_zero(max_blocks, (u32)SD_MAX_WS10_BLOCKS);
		break;

	case SD_LBP_ZERO:
		max_blocks = min_not_zero(sdkp->max_ws_blocks,
					  (u32)SD_MAX_WS10_BLOCKS);
		break;
	}

	blk_queue_max_discard_sectors(q, max_blocks * (logical_block_size >> 9));
	blk_queue_flag_set(QUEUE_FLAG_DISCARD, q);
}

static blk_status_t sd_setup_unmap_cmnd(struct scsi_cmnd *cmd)
{
	struct scsi_device *sdp = cmd->device;
	struct request *rq = cmd->request;
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));
	unsigned int data_len = 24;
	char *buf;

	rq->special_vec.bv_page = mempool_alloc(sd_page_pool, GFP_ATOMIC);
	if (!rq->special_vec.bv_page)
		return BLK_STS_RESOURCE;
	clear_highpage(rq->special_vec.bv_page);
	rq->special_vec.bv_offset = 0;
	rq->special_vec.bv_len = data_len;
	rq->rq_flags |= RQF_SPECIAL_PAYLOAD;

	cmd->cmd_len = 10;
	cmd->cmnd[0] = UNMAP;
	cmd->cmnd[8] = 24;

	buf = page_address(rq->special_vec.bv_page);
	put_unaligned_be16(6 + 16, &buf[0]);
	put_unaligned_be16(16, &buf[2]);
	put_unaligned_be64(lba, &buf[8]);
	put_unaligned_be32(nr_blocks, &buf[16]);

	cmd->allowed = SD_MAX_RETRIES;
	cmd->transfersize = data_len;
	rq->timeout = SD_TIMEOUT;

	return scsi_init_io(cmd);
}

static blk_status_t sd_setup_write_same16_cmnd(struct scsi_cmnd *cmd,
		bool unmap)
{
	struct scsi_device *sdp = cmd->device;
	struct request *rq = cmd->request;
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));
	u32 data_len = sdp->sector_size;

	rq->special_vec.bv_page = mempool_alloc(sd_page_pool, GFP_ATOMIC);
	if (!rq->special_vec.bv_page)
		return BLK_STS_RESOURCE;
	clear_highpage(rq->special_vec.bv_page);
	rq->special_vec.bv_offset = 0;
	rq->special_vec.bv_len = data_len;
	rq->rq_flags |= RQF_SPECIAL_PAYLOAD;

	cmd->cmd_len = 16;
	cmd->cmnd[0] = WRITE_SAME_16;
	if (unmap)
		cmd->cmnd[1] = 0x8; /* UNMAP */
	put_unaligned_be64(lba, &cmd->cmnd[2]);
	put_unaligned_be32(nr_blocks, &cmd->cmnd[10]);

	cmd->allowed = SD_MAX_RETRIES;
	cmd->transfersize = data_len;
	rq->timeout = unmap ? SD_TIMEOUT : SD_WRITE_SAME_TIMEOUT;

	return scsi_init_io(cmd);
}

static blk_status_t sd_setup_write_same10_cmnd(struct scsi_cmnd *cmd,
		bool unmap)
{
	struct scsi_device *sdp = cmd->device;
	struct request *rq = cmd->request;
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));
	u32 data_len = sdp->sector_size;

	rq->special_vec.bv_page = mempool_alloc(sd_page_pool, GFP_ATOMIC);
	if (!rq->special_vec.bv_page)
		return BLK_STS_RESOURCE;
	clear_highpage(rq->special_vec.bv_page);
	rq->special_vec.bv_offset = 0;
	rq->special_vec.bv_len = data_len;
	rq->rq_flags |= RQF_SPECIAL_PAYLOAD;

	cmd->cmd_len = 10;
	cmd->cmnd[0] = WRITE_SAME;
	if (unmap)
		cmd->cmnd[1] = 0x8; /* UNMAP */
	put_unaligned_be32(lba, &cmd->cmnd[2]);
	put_unaligned_be16(nr_blocks, &cmd->cmnd[7]);

	cmd->allowed = SD_MAX_RETRIES;
	cmd->transfersize = data_len;
	rq->timeout = unmap ? SD_TIMEOUT : SD_WRITE_SAME_TIMEOUT;

	return scsi_init_io(cmd);
}

static blk_status_t sd_setup_write_zeroes_cmnd(struct scsi_cmnd *cmd)
{
	struct request *rq = cmd->request;
	struct scsi_device *sdp = cmd->device;
	struct scsi_disk *sdkp = scsi_disk(rq->rq_disk);
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));

	if (!(rq->cmd_flags & REQ_NOUNMAP)) {
		switch (sdkp->zeroing_mode) {
		case SD_ZERO_WS16_UNMAP:
			return sd_setup_write_same16_cmnd(cmd, true);
		case SD_ZERO_WS10_UNMAP:
			return sd_setup_write_same10_cmnd(cmd, true);
		}
	}

	if (sdp->no_write_same)
		return BLK_STS_TARGET;

	if (sdkp->ws16 || lba > 0xffffffff || nr_blocks > 0xffff)
		return sd_setup_write_same16_cmnd(cmd, false);

	return sd_setup_write_same10_cmnd(cmd, false);
}

static void sd_config_write_same(struct scsi_disk *sdkp)
{
	struct request_queue *q = sdkp->disk->queue;
	unsigned int logical_block_size = sdkp->device->sector_size;

	if (sdkp->device->no_write_same) {
		sdkp->max_ws_blocks = 0;
		goto out;
	}

	/* Some devices can not handle block counts above 0xffff despite
	 * supporting WRITE SAME(16). Consequently we default to 64k
	 * blocks per I/O unless the device explicitly advertises a
	 * bigger limit.
	 */
	if (sdkp->max_ws_blocks > SD_MAX_WS10_BLOCKS)
		sdkp->max_ws_blocks = min_not_zero(sdkp->max_ws_blocks,
						   (u32)SD_MAX_WS16_BLOCKS);
	else if (sdkp->ws16 || sdkp->ws10 || sdkp->device->no_report_opcodes)
		sdkp->max_ws_blocks = min_not_zero(sdkp->max_ws_blocks,
						   (u32)SD_MAX_WS10_BLOCKS);
	else {
		sdkp->device->no_write_same = 1;
		sdkp->max_ws_blocks = 0;
	}

	if (sdkp->lbprz && sdkp->lbpws)
		sdkp->zeroing_mode = SD_ZERO_WS16_UNMAP;
	else if (sdkp->lbprz && sdkp->lbpws10)
		sdkp->zeroing_mode = SD_ZERO_WS10_UNMAP;
	else if (sdkp->max_ws_blocks)
		sdkp->zeroing_mode = SD_ZERO_WS;
	else
		sdkp->zeroing_mode = SD_ZERO_WRITE;

	if (sdkp->max_ws_blocks &&
	    sdkp->physical_block_size > logical_block_size) {
		/*
		 * Reporting a maximum number of blocks that is not aligned
		 * on the device physical size would cause a large write same
		 * request to be split into physically unaligned chunks by
		 * __blkdev_issue_write_zeroes() and __blkdev_issue_write_same()
		 * even if the caller of these functions took care to align the
		 * large request. So make sure the maximum reported is aligned
		 * to the device physical block size. This is only an optional
		 * optimization for regular disks, but this is mandatory to
		 * avoid failure of large write same requests directed at
		 * sequential write required zones of host-managed ZBC disks.
		 */
		sdkp->max_ws_blocks =
			round_down(sdkp->max_ws_blocks,
				   bytes_to_logical(sdkp->device,
						    sdkp->physical_block_size));
	}

out:
	blk_queue_max_write_same_sectors(q, sdkp->max_ws_blocks *
					 (logical_block_size >> 9));
	blk_queue_max_write_zeroes_sectors(q, sdkp->max_ws_blocks *
					 (logical_block_size >> 9));
}

/**
 * sd_setup_write_same_cmnd - write the same data to multiple blocks
 * @cmd: command to prepare
 *
 * Will set up either WRITE SAME(10) or WRITE SAME(16) depending on
 * the preference indicated by the target device.
 **/
static blk_status_t sd_setup_write_same_cmnd(struct scsi_cmnd *cmd)
{
	struct request *rq = cmd->request;
	struct scsi_device *sdp = cmd->device;
	struct scsi_disk *sdkp = scsi_disk(rq->rq_disk);
	struct bio *bio = rq->bio;
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));
	blk_status_t ret;

	if (sdkp->device->no_write_same)
		return BLK_STS_TARGET;

	BUG_ON(bio_offset(bio) || bio_iovec(bio).bv_len != sdp->sector_size);

	rq->timeout = SD_WRITE_SAME_TIMEOUT;

	if (sdkp->ws16 || lba > 0xffffffff || nr_blocks > 0xffff) {
		cmd->cmd_len = 16;
		cmd->cmnd[0] = WRITE_SAME_16;
		put_unaligned_be64(lba, &cmd->cmnd[2]);
		put_unaligned_be32(nr_blocks, &cmd->cmnd[10]);
	} else {
		cmd->cmd_len = 10;
		cmd->cmnd[0] = WRITE_SAME;
		put_unaligned_be32(lba, &cmd->cmnd[2]);
		put_unaligned_be16(nr_blocks, &cmd->cmnd[7]);
	}

	cmd->transfersize = sdp->sector_size;
	cmd->allowed = SD_MAX_RETRIES;

	/*
	 * For WRITE SAME the data transferred via the DATA OUT buffer is
	 * different from the amount of data actually written to the target.
	 *
	 * We set up __data_len to the amount of data transferred via the
	 * DATA OUT buffer so that blk_rq_map_sg sets up the proper S/G list
	 * to transfer a single sector of data first, but then reset it to
	 * the amount of data to be written right after so that the I/O path
	 * knows how much to actually write.
	 */
	rq->__data_len = sdp->sector_size;
	ret = scsi_init_io(cmd);
	rq->__data_len = blk_rq_bytes(rq);

	return ret;
}

static blk_status_t sd_setup_flush_cmnd(struct scsi_cmnd *cmd)
{
	struct request *rq = cmd->request;

	/* flush requests don't perform I/O, zero the S/G table */
	memset(&cmd->sdb, 0, sizeof(cmd->sdb));

	cmd->cmnd[0] = SYNCHRONIZE_CACHE;
	cmd->cmd_len = 10;
	cmd->transfersize = 0;
	cmd->allowed = SD_MAX_RETRIES;

	rq->timeout = rq->q->rq_timeout * SD_FLUSH_TIMEOUT_MULTIPLIER;
	return BLK_STS_OK;
}

static blk_status_t sd_setup_rw32_cmnd(struct scsi_cmnd *cmd, bool write,
				       sector_t lba, unsigned int nr_blocks,
				       unsigned char flags)
{
	cmd->cmnd = mempool_alloc(sd_cdb_pool, GFP_ATOMIC);
	if (unlikely(cmd->cmnd == NULL))
		return BLK_STS_RESOURCE;

	cmd->cmd_len = SD_EXT_CDB_SIZE;
	memset(cmd->cmnd, 0, cmd->cmd_len);

	cmd->cmnd[0]  = VARIABLE_LENGTH_CMD;
	cmd->cmnd[7]  = 0x18; /* Additional CDB len */
	cmd->cmnd[9]  = write ? WRITE_32 : READ_32;
	cmd->cmnd[10] = flags;
	put_unaligned_be64(lba, &cmd->cmnd[12]);
	put_unaligned_be32(lba, &cmd->cmnd[20]); /* Expected Indirect LBA */
	put_unaligned_be32(nr_blocks, &cmd->cmnd[28]);

	return BLK_STS_OK;
}

static blk_status_t sd_setup_rw16_cmnd(struct scsi_cmnd *cmd, bool write,
				       sector_t lba, unsigned int nr_blocks,
				       unsigned char flags)
{
	cmd->cmd_len  = 16;
	cmd->cmnd[0]  = write ? WRITE_16 : READ_16;
	cmd->cmnd[1]  = flags;
	cmd->cmnd[14] = 0;
	cmd->cmnd[15] = 0;
	put_unaligned_be64(lba, &cmd->cmnd[2]);
	put_unaligned_be32(nr_blocks, &cmd->cmnd[10]);

	return BLK_STS_OK;
}

static blk_status_t sd_setup_rw10_cmnd(struct scsi_cmnd *cmd, bool write,
				       sector_t lba, unsigned int nr_blocks,
				       unsigned char flags)
{
	cmd->cmd_len = 10;
	cmd->cmnd[0] = write ? WRITE_10 : READ_10;
	cmd->cmnd[1] = flags;
	cmd->cmnd[6] = 0;
	cmd->cmnd[9] = 0;
	put_unaligned_be32(lba, &cmd->cmnd[2]);
	put_unaligned_be16(nr_blocks, &cmd->cmnd[7]);

	return BLK_STS_OK;
}

static blk_status_t sd_setup_rw6_cmnd(struct scsi_cmnd *cmd, bool write,
				      sector_t lba, unsigned int nr_blocks,
				      unsigned char flags)
{
	/* Avoid that 0 blocks gets translated into 256 blocks. */
	if (WARN_ON_ONCE(nr_blocks == 0))
		return BLK_STS_IOERR;

	if (unlikely(flags & 0x8)) {
		/*
		 * This happens only if this drive failed 10byte rw
		 * command with ILLEGAL_REQUEST during operation and
		 * thus turned off use_10_for_rw.
		 */
		scmd_printk(KERN_ERR, cmd, "FUA write on READ/WRITE(6) drive\n");
		return BLK_STS_IOERR;
	}

	cmd->cmd_len = 6;
	cmd->cmnd[0] = write ? WRITE_6 : READ_6;
	cmd->cmnd[1] = (lba >> 16) & 0x1f;
	cmd->cmnd[2] = (lba >> 8) & 0xff;
	cmd->cmnd[3] = lba & 0xff;
	cmd->cmnd[4] = nr_blocks;
	cmd->cmnd[5] = 0;

	return BLK_STS_OK;
}

static blk_status_t sd_setup_read_write_cmnd(struct scsi_cmnd *cmd)
{
	struct request *rq = cmd->request;
	struct scsi_device *sdp = cmd->device;
	struct scsi_disk *sdkp = scsi_disk(rq->rq_disk);
	sector_t lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	sector_t threshold;
	unsigned int nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));
	unsigned int mask = logical_to_sectors(sdp, 1) - 1;
	bool write = rq_data_dir(rq) == WRITE;
	unsigned char protect, fua;
	blk_status_t ret;
	unsigned int dif;
	bool dix;

	ret = scsi_init_io(cmd);
	if (ret != BLK_STS_OK)
		return ret;

	if (!scsi_device_online(sdp) || sdp->changed) {
		scmd_printk(KERN_ERR, cmd, "device offline or changed\n");
		return BLK_STS_IOERR;
	}

	if (blk_rq_pos(rq) + blk_rq_sectors(rq) > get_capacity(rq->rq_disk)) {
		scmd_printk(KERN_ERR, cmd, "access beyond end of device\n");
		return BLK_STS_IOERR;
	}

	if ((blk_rq_pos(rq) & mask) || (blk_rq_sectors(rq) & mask)) {
		scmd_printk(KERN_ERR, cmd, "request not aligned to the logical block size\n");
		return BLK_STS_IOERR;
	}

	/*
	 * Some SD card readers can't handle accesses which touch the
	 * last one or two logical blocks. Split accesses as needed.
	 */
	threshold = sdkp->capacity - SD_LAST_BUGGY_SECTORS;

	if (unlikely(sdp->last_sector_bug && lba + nr_blocks > threshold)) {
		if (lba < threshold) {
			/* Access up to the threshold but not beyond */
			nr_blocks = threshold - lba;
		} else {
			/* Access only a single logical block */
			nr_blocks = 1;
		}
	}

	fua = rq->cmd_flags & REQ_FUA ? 0x8 : 0;
	dix = scsi_prot_sg_count(cmd);
	dif = scsi_host_dif_capable(cmd->device->host, sdkp->protection_type);

	if (dif || dix)
		protect = sd_setup_protect_cmnd(cmd, dix, dif);
	else
		protect = 0;

	if (protect && sdkp->protection_type == T10_PI_TYPE2_PROTECTION) {
		ret = sd_setup_rw32_cmnd(cmd, write, lba, nr_blocks,
					 protect | fua);
	} else if (sdp->use_16_for_rw || (nr_blocks > 0xffff)) {
		ret = sd_setup_rw16_cmnd(cmd, write, lba, nr_blocks,
					 protect | fua);
	} else if ((nr_blocks > 0xff) || (lba > 0x1fffff) ||
		   sdp->use_10_for_rw || protect) {
		ret = sd_setup_rw10_cmnd(cmd, write, lba, nr_blocks,
					 protect | fua);
	} else {
		ret = sd_setup_rw6_cmnd(cmd, write, lba, nr_blocks,
					protect | fua);
	}

	if (unlikely(ret != BLK_STS_OK))
		return ret;

	/*
	 * We shouldn't disconnect in the middle of a sector, so with a dumb
	 * host adapter, it's safe to assume that we can at least transfer
	 * this many bytes between each connect / disconnect.
	 */
	cmd->transfersize = sdp->sector_size;
	cmd->underflow = nr_blocks << 9;
	cmd->allowed = SD_MAX_RETRIES;
	cmd->sdb.length = nr_blocks * sdp->sector_size;

	SCSI_LOG_HLQUEUE(1,
			 scmd_printk(KERN_INFO, cmd,
				     "%s: block=%llu, count=%d\n", __func__,
				     (unsigned long long)blk_rq_pos(rq),
				     blk_rq_sectors(rq)));
	SCSI_LOG_HLQUEUE(2,
			 scmd_printk(KERN_INFO, cmd,
				     "%s %d/%u 512 byte blocks.\n",
				     write ? "writing" : "reading", nr_blocks,
				     blk_rq_sectors(rq)));

	/*
	 * This indicates that the command is ready from our end to be
	 * queued.
	 */
	return BLK_STS_OK;
}

static blk_status_t sd_init_command(struct scsi_cmnd *cmd)
{
	struct request *rq = cmd->request;

	switch (req_op(rq)) {
	case REQ_OP_DISCARD:
		switch (scsi_disk(rq->rq_disk)->provisioning_mode) {
		case SD_LBP_UNMAP:
			return sd_setup_unmap_cmnd(cmd);
		case SD_LBP_WS16:
			return sd_setup_write_same16_cmnd(cmd, true);
		case SD_LBP_WS10:
			return sd_setup_write_same10_cmnd(cmd, true);
		case SD_LBP_ZERO:
			return sd_setup_write_same10_cmnd(cmd, false);
		default:
			return BLK_STS_TARGET;
		}
	case REQ_OP_WRITE_ZEROES:
		return sd_setup_write_zeroes_cmnd(cmd);
	case REQ_OP_WRITE_SAME:
		return sd_setup_write_same_cmnd(cmd);
	case REQ_OP_FLUSH:
		return sd_setup_flush_cmnd(cmd);
	case REQ_OP_READ:
	case REQ_OP_WRITE:
		return sd_setup_read_write_cmnd(cmd);
	case REQ_OP_ZONE_RESET:
		return sd_zbc_setup_reset_cmnd(cmd, false);
	case REQ_OP_ZONE_RESET_ALL:
		return sd_zbc_setup_reset_cmnd(cmd, true);
	default:
		WARN_ON_ONCE(1);
		return BLK_STS_NOTSUPP;
	}
}

static void sd_uninit_command(struct scsi_cmnd *SCpnt)
{
	struct request *rq = SCpnt->request;
	u8 *cmnd;

	if (rq->rq_flags & RQF_SPECIAL_PAYLOAD)
		mempool_free(rq->special_vec.bv_page, sd_page_pool);

	if (SCpnt->cmnd != scsi_req(rq)->cmd) {
		cmnd = SCpnt->cmnd;
		SCpnt->cmnd = NULL;
		SCpnt->cmd_len = 0;
		mempool_free(cmnd, sd_cdb_pool);
	}
}

/**
 *	sd_open - open a scsi disk device
 *	@bdev: Block device of the scsi disk to open
 *	@mode: FMODE_* mask
 *
 *	Returns 0 if successful. Returns a negated errno value in case 
 *	of error.
 *
 *	Note: This can be called from a user context (e.g. fsck(1) )
 *	or from within the kernel (e.g. as a result of a mount(1) ).
 *	In the latter case @inode and @filp carry an abridged amount
 *	of information as noted above.
 *
 *	Locking: called with bdev->bd_mutex held.
 **/
static int sd_open(struct block_device *bdev, fmode_t mode)
{
	struct scsi_disk *sdkp = scsi_disk_get(bdev->bd_disk);
	struct scsi_device *sdev;
	int retval;

	if (!sdkp)
		return -ENXIO;

	SCSI_LOG_HLQUEUE(3, sd_printk(KERN_INFO, sdkp, "sd_open\n"));

	sdev = sdkp->device;

	/*
	 * If the device is in error recovery, wait until it is done.
	 * If the device is offline, then disallow any access to it.
	 */
	retval = -ENXIO;
	if (!scsi_block_when_processing_errors(sdev))
		goto error_out;

	if (sdev->removable || sdkp->write_prot)
		check_disk_change(bdev);

	/*
	 * If the drive is empty, just let the open fail.
	 */
	retval = -ENOMEDIUM;
	if (sdev->removable && !sdkp->media_present && !(mode & FMODE_NDELAY))
		goto error_out;

	/*
	 * If the device has the write protect tab set, have the open fail
	 * if the user expects to be able to write to the thing.
	 */
	retval = -EROFS;
	if (sdkp->write_prot && (mode & FMODE_WRITE))
		goto error_out;

	/*
	 * It is possible that the disk changing stuff resulted in
	 * the device being taken offline.  If this is the case,
	 * report this to the user, and don't pretend that the
	 * open actually succeeded.
	 */
	retval = -ENXIO;
	if (!scsi_device_online(sdev))
		goto error_out;

	if ((atomic_inc_return(&sdkp->openers) == 1) && sdev->removable) {
		if (scsi_block_when_processing_errors(sdev))
			scsi_set_medium_removal(sdev, SCSI_REMOVAL_PREVENT);
	}

	return 0;

error_out:
	scsi_disk_put(sdkp);
	return retval;	
}

/**
 *	sd_release - invoked when the (last) close(2) is called on this
 *	scsi disk.
 *	@disk: disk to release
 *	@mode: FMODE_* mask
 *
 *	Returns 0. 
 *
 *	Note: may block (uninterruptible) if error recovery is underway
 *	on this disk.
 *
 *	Locking: called with bdev->bd_mutex held.
 **/
static void sd_release(struct gendisk *disk, fmode_t mode)
{
	struct scsi_disk *sdkp = scsi_disk(disk);
	struct scsi_device *sdev = sdkp->device;

	SCSI_LOG_HLQUEUE(3, sd_printk(KERN_INFO, sdkp, "sd_release\n"));

	if (atomic_dec_return(&sdkp->openers) == 0 && sdev->removable) {
		if (scsi_block_when_processing_errors(sdev))
			scsi_set_medium_removal(sdev, SCSI_REMOVAL_ALLOW);
	}

	scsi_disk_put(sdkp);
}

static int sd_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct scsi_disk *sdkp = scsi_disk(bdev->bd_disk);
	struct scsi_device *sdp = sdkp->device;
	struct Scsi_Host *host = sdp->host;
	sector_t capacity = logical_to_sectors(sdp, sdkp->capacity);
	int diskinfo[4];

	/* default to most commonly used values */
	diskinfo[0] = 0x40;	/* 1 << 6 */
	diskinfo[1] = 0x20;	/* 1 << 5 */
	diskinfo[2] = capacity >> 11;

	/* override with calculated, extended default, or driver values */
	if (host->hostt->bios_param)
		host->hostt->bios_param(sdp, bdev, capacity, diskinfo);
	else
		scsicam_bios_param(bdev, capacity, diskinfo);

	geo->heads = diskinfo[0];
	geo->sectors = diskinfo[1];
	geo->cylinders = diskinfo[2];
	return 0;
}

/**
 *	sd_ioctl - process an ioctl
 *	@bdev: target block device
 *	@mode: FMODE_* mask
 *	@cmd: ioctl command number
 *	@arg: this is third argument given to ioctl(2) system call.
 *	Often contains a pointer.
 *
 *	Returns 0 if successful (some ioctls return positive numbers on
 *	success as well). Returns a negated errno value in case of error.
 *
 *	Note: most ioctls are forward onto the block subsystem or further
 *	down in the scsi subsystem.
 **/
static int sd_ioctl(struct block_device *bdev, fmode_t mode,
		    unsigned int cmd, unsigned long arg)
{
	struct gendisk *disk = bdev->bd_disk;
	struct scsi_disk *sdkp = scsi_disk(disk);
	struct scsi_device *sdp = sdkp->device;
	void __user *p = (void __user *)arg;
	int error;
    
	SCSI_LOG_IOCTL(1, sd_printk(KERN_INFO, sdkp, "sd_ioctl: disk=%s, "
				    "cmd=0x%x\n", disk->disk_name, cmd));

	error = scsi_verify_blk_ioctl(bdev, cmd);
	if (error < 0)
		return error;

	/*
	 * If we are in the middle of error recovery, don't let anyone
	 * else try and use this device.  Also, if error recovery fails, it
	 * may try and take the device offline, in which case all further
	 * access to the device is prohibited.
	 */
	error = scsi_ioctl_block_when_processing_errors(sdp, cmd,
			(mode & FMODE_NDELAY) != 0);
	if (error)
		goto out;

	if (is_sed_ioctl(cmd))
		return sed_ioctl(sdkp->opal_dev, cmd, p);

	/*
	 * Send SCSI addressing ioctls directly to mid level, send other
	 * ioctls to block level and then onto mid level if they can't be
	 * resolved.
	 */
	switch (cmd) {
		case SCSI_IOCTL_GET_IDLUN:
		case SCSI_IOCTL_GET_BUS_NUMBER:
			error = scsi_ioctl(sdp, cmd, p);
			break;
		default:
			error = scsi_cmd_blk_ioctl(bdev, mode, cmd, p);
			if (error != -ENOTTY)
				break;
			error = scsi_ioctl(sdp, cmd, p);
			break;
	}
out:
	return error;
}

static void set_media_not_present(struct scsi_disk *sdkp)
{
	if (sdkp->media_present)
		sdkp->device->changed = 1;

	if (sdkp->device->removable) {
		sdkp->media_present = 0;
		sdkp->capacity = 0;
	}
}

static int media_not_present(struct scsi_disk *sdkp,
			     struct scsi_sense_hdr *sshdr)
{
	if (!scsi_sense_valid(sshdr))
		return 0;

	/* not invoked for commands that could return deferred errors */
	switch (sshdr->sense_key) {
	case UNIT_ATTENTION:
	case NOT_READY:
		/* medium not present */
		if (sshdr->asc == 0x3A) {
			set_media_not_present(sdkp);
			return 1;
		}
	}
	return 0;
}

/**
 *	sd_check_events - check media events
 *	@disk: kernel device descriptor
 *	@clearing: disk events currently being cleared
 *
 *	Returns mask of DISK_EVENT_*.
 *
 *	Note: this function is invoked from the block subsystem.
 **/
static unsigned int sd_check_events(struct gendisk *disk, unsigned int clearing)
{
	struct scsi_disk *sdkp = scsi_disk_get(disk);
	struct scsi_device *sdp;
	int retval;

	if (!sdkp)
		return 0;

	sdp = sdkp->device;
	SCSI_LOG_HLQUEUE(3, sd_printk(KERN_INFO, sdkp, "sd_check_events\n"));

	/*
	 * If the device is offline, don't send any commands - just pretend as
	 * if the command failed.  If the device ever comes back online, we
	 * can deal with it then.  It is only because of unrecoverable errors
	 * that we would ever take a device offline in the first place.
	 */
	if (!scsi_device_online(sdp)) {
		set_media_not_present(sdkp);
		goto out;
	}

	/*
	 * Using TEST_UNIT_READY enables differentiation between drive with
	 * no cartridge loaded - NOT READY, drive with changed cartridge -
	 * UNIT ATTENTION, or with same cartridge - GOOD STATUS.
	 *
	 * Drives that auto spin down. eg iomega jaz 1G, will be started
	 * by sd_spinup_disk() from sd_revalidate_disk(), which happens whenever
	 * sd_revalidate() is called.
	 */
	if (scsi_block_when_processing_errors(sdp)) {
		struct scsi_sense_hdr sshdr = { 0, };

		retval = scsi_test_unit_ready(sdp, SD_TIMEOUT, SD_MAX_RETRIES,
					      &sshdr);

		/* failed to execute TUR, assume media not present */
		if (host_byte(retval)) {
			set_media_not_present(sdkp);
			goto out;
		}

		if (media_not_present(sdkp, &sshdr))
			goto out;
	}

	/*
	 * For removable scsi disk we have to recognise the presence
	 * of a disk in the drive.
	 */
	if (!sdkp->media_present)
		sdp->changed = 1;
	sdkp->media_present = 1;
out:
	/*
	 * sdp->changed is set under the following conditions:
	 *
	 *	Medium present state has changed in either direction.
	 *	Device has indicated UNIT_ATTENTION.
	 */
	retval = sdp->changed ? DISK_EVENT_MEDIA_CHANGE : 0;
	sdp->changed = 0;
	scsi_disk_put(sdkp);
	return retval;
}

static int sd_sync_cache(struct scsi_disk *sdkp, struct scsi_sense_hdr *sshdr)
{
	int retries, res;
	struct scsi_device *sdp = sdkp->device;
	const int timeout = sdp->request_queue->rq_timeout
		* SD_FLUSH_TIMEOUT_MULTIPLIER;
	struct scsi_sense_hdr my_sshdr;

	if (!scsi_device_online(sdp))
		return -ENODEV;

	/* caller might not be interested in sense, but we need it */
	if (!sshdr)
		sshdr = &my_sshdr;

	for (retries = 3; retries > 0; --retries) {
		unsigned char cmd[10] = { 0 };

		cmd[0] = SYNCHRONIZE_CACHE;
		/*
		 * Leave the rest of the command zero to indicate
		 * flush everything.
		 */
		res = scsi_execute(sdp, cmd, DMA_NONE, NULL, 0, NULL, sshdr,
				timeout, SD_MAX_RETRIES, 0, RQF_PM, NULL);
		if (res == 0)
			break;
	}

	if (res) {
		sd_print_result(sdkp, "Synchronize Cache(10) failed", res);

		if (driver_byte(res) == DRIVER_SENSE)
			sd_print_sense_hdr(sdkp, sshdr);

		/* we need to evaluate the error return  */
		if (scsi_sense_valid(sshdr) &&
			(sshdr->asc == 0x3a ||	/* medium not present */
			 sshdr->asc == 0x20 ||	/* invalid command */
			 (sshdr->asc == 0x74 && sshdr->ascq == 0x71)))	/* drive is password locked */
				/* this is no error here */
				return 0;

		switch (host_byte(res)) {
		/* ignore errors due to racing a disconnection */
		case DID_BAD_TARGET:
		case DID_NO_CONNECT:
			return 0;
		/* signal the upper layer it might try again */
		case DID_BUS_BUSY:
		case DID_IMM_RETRY:
		case DID_REQUEUE:
		case DID_SOFT_ERROR:
			return -EBUSY;
		default:
			return -EIO;
		}
	}
	return 0;
}

static void sd_rescan(struct device *dev)
{
	struct scsi_disk *sdkp = dev_get_drvdata(dev);

	revalidate_disk(sdkp->disk);
}


#ifdef CONFIG_COMPAT
/* 
 * This gets directly called from VFS. When the ioctl 
 * is not recognized we go back to the other translation paths. 
 */
static int sd_compat_ioctl(struct block_device *bdev, fmode_t mode,
			   unsigned int cmd, unsigned long arg)
{
	struct scsi_device *sdev = scsi_disk(bdev->bd_disk)->device;
	int error;

	error = scsi_ioctl_block_when_processing_errors(sdev, cmd,
			(mode & FMODE_NDELAY) != 0);
	if (error)
		return error;
	       
	/* 
	 * Let the static ioctl translation table take care of it.
	 */
	if (!sdev->host->hostt->compat_ioctl)
		return -ENOIOCTLCMD; 
	return sdev->host->hostt->compat_ioctl(sdev, cmd, (void __user *)arg);
}
#endif

static char sd_pr_type(enum pr_type type)
{
	switch (type) {
	case PR_WRITE_EXCLUSIVE:
		return 0x01;
	case PR_EXCLUSIVE_ACCESS:
		return 0x03;
	case PR_WRITE_EXCLUSIVE_REG_ONLY:
		return 0x05;
	case PR_EXCLUSIVE_ACCESS_REG_ONLY:
		return 0x06;
	case PR_WRITE_EXCLUSIVE_ALL_REGS:
		return 0x07;
	case PR_EXCLUSIVE_ACCESS_ALL_REGS:
		return 0x08;
	default:
		return 0;
	}
};

static int sd_pr_command(struct block_device *bdev, u8 sa,
		u64 key, u64 sa_key, u8 type, u8 flags)
{
	struct scsi_device *sdev = scsi_disk(bdev->bd_disk)->device;
	struct scsi_sense_hdr sshdr;
	int result;
	u8 cmd[16] = { 0, };
	u8 data[24] = { 0, };

	cmd[0] = PERSISTENT_RESERVE_OUT;
	cmd[1] = sa;
	cmd[2] = type;
	put_unaligned_be32(sizeof(data), &cmd[5]);

	put_unaligned_be64(key, &data[0]);
	put_unaligned_be64(sa_key, &data[8]);
	data[20] = flags;

	result = scsi_execute_req(sdev, cmd, DMA_TO_DEVICE, &data, sizeof(data),
			&sshdr, SD_TIMEOUT, SD_MAX_RETRIES, NULL);

	if (driver_byte(result) == DRIVER_SENSE &&
	    scsi_sense_valid(&sshdr)) {
		sdev_printk(KERN_INFO, sdev, "PR command failed: %d\n", result);
		scsi_print_sense_hdr(sdev, NULL, &sshdr);
	}

	return result;
}

static int sd_pr_register(struct block_device *bdev, u64 old_key, u64 new_key,
		u32 flags)
{
	if (flags & ~PR_FL_IGNORE_KEY)
		return -EOPNOTSUPP;
	return sd_pr_command(bdev, (flags & PR_FL_IGNORE_KEY) ? 0x06 : 0x00,
			old_key, new_key, 0,
			(1 << 0) /* APTPL */);
}

static int sd_pr_reserve(struct block_device *bdev, u64 key, enum pr_type type,
		u32 flags)
{
	if (flags)
		return -EOPNOTSUPP;
	return sd_pr_command(bdev, 0x01, key, 0, sd_pr_type(type), 0);
}

static int sd_pr_release(struct block_device *bdev, u64 key, enum pr_type type)
{
	return sd_pr_command(bdev, 0x02, key, 0, sd_pr_type(type), 0);
}

static int sd_pr_preempt(struct block_device *bdev, u64 old_key, u64 new_key,
		enum pr_type type, bool abort)
{
	return sd_pr_command(bdev, abort ? 0x05 : 0x04, old_key, new_key,
			     sd_pr_type(type), 0);
}

static int sd_pr_clear(struct block_device *bdev, u64 key)
{
	return sd_pr_command(bdev, 0x03, key, 0, 0, 0);
}

static const struct pr_ops sd_pr_ops = {
	.pr_register	= sd_pr_register,
	.pr_reserve	= sd_pr_reserve,
	.pr_release	= sd_pr_release,
	.pr_preempt	= sd_pr_preempt,
	.pr_clear	= sd_pr_clear,
};

static const struct block_device_operations sd_fops = {
	.owner			= THIS_MODULE,
	.open			= sd_open,
	.release		= sd_release,
	.ioctl			= sd_ioctl,
	.getgeo			= sd_getgeo,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= sd_compat_ioctl,
#endif
	.check_events		= sd_check_events,
	.revalidate_disk	= sd_revalidate_disk,
	.unlock_native_capacity	= sd_unlock_native_capacity,
	.report_zones		= sd_zbc_report_zones,
	.pr_ops			= &sd_pr_ops,
};

/**
 *	sd_eh_reset - reset error handling callback
 *	@scmd:		sd-issued command that has failed
 *
 *	This function is called by the SCSI midlayer before starting
 *	SCSI EH. When counting medium access failures we have to be
 *	careful to register it only only once per device and SCSI EH run;
 *	there might be several timed out commands which will cause the
 *	'max_medium_access_timeouts' counter to trigger after the first
 *	SCSI EH run already and set the device to offline.
 *	So this function resets the internal counter before starting SCSI EH.
 **/
static void sd_eh_reset(struct scsi_cmnd *scmd)
{
	struct scsi_disk *sdkp = scsi_disk(scmd->request->rq_disk);

	/* New SCSI EH run, reset gate variable */
	sdkp->ignore_medium_access_errors = false;
}

/**
 *	sd_eh_action - error handling callback
 *	@scmd:		sd-issued command that has failed
 *	@eh_disp:	The recovery disposition suggested by the midlayer
 *
 *	This function is called by the SCSI midlayer upon completion of an
 *	error test command (currently TEST UNIT READY). The result of sending
 *	the eh command is passed in eh_disp.  We're looking for devices that
 *	fail medium access commands but are OK with non access commands like
 *	test unit ready (so wrongly see the device as having a successful
 *	recovery)
 **/
static int sd_eh_action(struct scsi_cmnd *scmd, int eh_disp)
{
	struct scsi_disk *sdkp = scsi_disk(scmd->request->rq_disk);
	struct scsi_device *sdev = scmd->device;

	if (!scsi_device_online(sdev) ||
	    !scsi_medium_access_command(scmd) ||
	    host_byte(scmd->result) != DID_TIME_OUT ||
	    eh_disp != SUCCESS)
		return eh_disp;

	/*
	 * The device has timed out executing a medium access command.
	 * However, the TEST UNIT READY command sent during error
	 * handling completed successfully. Either the device is in the
	 * process of recovering or has it suffered an internal failure
	 * that prevents access to the storage medium.
	 */
	if (!sdkp->ignore_medium_access_errors) {
		sdkp->medium_access_timed_out++;
		sdkp->ignore_medium_access_errors = true;
	}

	/*
	 * If the device keeps failing read/write commands but TEST UNIT
	 * READY always completes successfully we assume that medium
	 * access is no longer possible and take the device offline.
	 */
	if (sdkp->medium_access_timed_out >= sdkp->max_medium_access_timeouts) {
		scmd_printk(KERN_ERR, scmd,
			    "Medium access timeout failure. Offlining disk!\n");
		mutex_lock(&sdev->state_mutex);
		scsi_device_set_state(sdev, SDEV_OFFLINE);
		mutex_unlock(&sdev->state_mutex);

		return SUCCESS;
	}

	return eh_disp;
}

static unsigned int sd_completed_bytes(struct scsi_cmnd *scmd)
{
	struct request *req = scmd->request;
	struct scsi_device *sdev = scmd->device;
	unsigned int transferred, good_bytes;
	u64 start_lba, end_lba, bad_lba;

	/*
	 * Some commands have a payload smaller than the device logical
	 * block size (e.g. INQUIRY on a 4K disk).
	 */
	if (scsi_bufflen(scmd) <= sdev->sector_size)
		return 0;

	/* Check if we have a 'bad_lba' information */
	if (!scsi_get_sense_info_fld(scmd->sense_buffer,
				     SCSI_SENSE_BUFFERSIZE,
				     &bad_lba))
		return 0;

	/*
	 * If the bad lba was reported incorrectly, we have no idea where
	 * the error is.
	 */
	start_lba = sectors_to_logical(sdev, blk_rq_pos(req));
	end_lba = start_lba + bytes_to_logical(sdev, scsi_bufflen(scmd));
	if (bad_lba < start_lba || bad_lba >= end_lba)
		return 0;

	/*
	 * resid is optional but mostly filled in.  When it's unused,
	 * its value is zero, so we assume the whole buffer transferred
	 */
	transferred = scsi_bufflen(scmd) - scsi_get_resid(scmd);

	/* This computation should always be done in terms of the
	 * resolution of the device's medium.
	 */
	good_bytes = logical_to_bytes(sdev, bad_lba - start_lba);

	return min(good_bytes, transferred);
}

/**
 *	sd_done - bottom half handler: called when the lower level
 *	driver has completed (successfully or otherwise) a scsi command.
 *	@SCpnt: mid-level's per command structure.
 *
 *	Note: potentially run from within an ISR. Must not block.
 **/
static int sd_done(struct scsi_cmnd *SCpnt)
{
	int result = SCpnt->result;
	unsigned int good_bytes = result ? 0 : scsi_bufflen(SCpnt);
	unsigned int sector_size = SCpnt->device->sector_size;
	unsigned int resid;
	struct scsi_sense_hdr sshdr;
	struct scsi_disk *sdkp = scsi_disk(SCpnt->request->rq_disk);
	struct request *req = SCpnt->request;
	int sense_valid = 0;
	int sense_deferred = 0;

	switch (req_op(req)) {
	case REQ_OP_DISCARD:
	case REQ_OP_WRITE_ZEROES:
	case REQ_OP_WRITE_SAME:
	case REQ_OP_ZONE_RESET:
	case REQ_OP_ZONE_RESET_ALL:
		if (!result) {
			good_bytes = blk_rq_bytes(req);
			scsi_set_resid(SCpnt, 0);
		} else {
			good_bytes = 0;
			scsi_set_resid(SCpnt, blk_rq_bytes(req));
		}
		break;
	default:
		/*
		 * In case of bogus fw or device, we could end up having
		 * an unaligned partial completion. Check this here and force
		 * alignment.
		 */
		resid = scsi_get_resid(SCpnt);
		if (resid & (sector_size - 1)) {
			sd_printk(KERN_INFO, sdkp,
				"Unaligned partial completion (resid=%u, sector_sz=%u)\n",
				resid, sector_size);
			scsi_print_command(SCpnt);
			resid = min(scsi_bufflen(SCpnt),
				    round_up(resid, sector_size));
			scsi_set_resid(SCpnt, resid);
		}
	}

	if (result) {
		sense_valid = scsi_command_normalize_sense(SCpnt, &sshdr);
		if (sense_valid)
			sense_deferred = scsi_sense_is_deferred(&sshdr);
	}
	sdkp->medium_access_timed_out = 0;

	if (driver_byte(result) != DRIVER_SENSE &&
	    (!sense_valid || sense_deferred))
		goto out;

	switch (sshdr.sense_key) {
	case HARDWARE_ERROR:
	case MEDIUM_ERROR:
		good_bytes = sd_completed_bytes(SCpnt);
		break;
	case RECOVERED_ERROR:
		good_bytes = scsi_bufflen(SCpnt);
		break;
	case NO_SENSE:
		/* This indicates a false check condition, so ignore it.  An
		 * unknown amount of data was transferred so treat it as an
		 * error.
		 */
		SCpnt->result = 0;
		memset(SCpnt->sense_buffer, 0, SCSI_SENSE_BUFFERSIZE);
		break;
	case ABORTED_COMMAND:
		if (sshdr.asc == 0x10)  /* DIF: Target detected corruption */
			good_bytes = sd_completed_bytes(SCpnt);
		break;
	case ILLEGAL_REQUEST:
		switch (sshdr.asc) {
		case 0x10:	/* DIX: Host detected corruption */
			good_bytes = sd_completed_bytes(SCpnt);
			break;
		case 0x20:	/* INVALID COMMAND OPCODE */
		case 0x24:	/* INVALID FIELD IN CDB */
			switch (SCpnt->cmnd[0]) {
			case UNMAP:
				sd_config_discard(sdkp, SD_LBP_DISABLE);
				break;
			case WRITE_SAME_16:
			case WRITE_SAME:
				if (SCpnt->cmnd[1] & 8) { /* UNMAP */
					sd_config_discard(sdkp, SD_LBP_DISABLE);
				} else {
					sdkp->device->no_write_same = 1;
					sd_config_write_same(sdkp);
					req->rq_flags |= RQF_QUIET;
				}
				break;
			}
		}
		break;
	default:
		break;
	}

 out:
	if (sd_is_zoned(sdkp))
		sd_zbc_complete(SCpnt, good_bytes, &sshdr);

	SCSI_LOG_HLCOMPLETE(1, scmd_printk(KERN_INFO, SCpnt,
					   "sd_done: completed %d of %d bytes\n",
					   good_bytes, scsi_bufflen(SCpnt)));

	return good_bytes;
}

/*
 * spinup disk - called only in sd_revalidate_disk()
 */
static void
sd_spinup_disk(struct scsi_disk *sdkp)
{
	unsigned char cmd[10];
	unsigned long spintime_expire = 0;
	int retries, spintime;
	unsigned int the_result;
	struct scsi_sense_hdr sshdr;
	int sense_valid = 0;

	spintime = 0;

	/* Spin up drives, as required.  Only do this at boot time */
	/* Spinup needs to be done for module loads too. */
	do {
		retries = 0;

		do {
			cmd[0] = TEST_UNIT_READY;
			memset((void *) &cmd[1], 0, 9);

			the_result = scsi_execute_req(sdkp->device, cmd,
						      DMA_NONE, NULL, 0,
						      &sshdr, SD_TIMEOUT,
						      SD_MAX_RETRIES, NULL);

			/*
			 * If the drive has indicated to us that it
			 * doesn't have any media in it, don't bother
			 * with any more polling.
			 */
			if (media_not_present(sdkp, &sshdr))
				return;

			if (the_result)
				sense_valid = scsi_sense_valid(&sshdr);
			retries++;
		} while (retries < 3 && 
			 (!scsi_status_is_good(the_result) ||
			  ((driver_byte(the_result) == DRIVER_SENSE) &&
			  sense_valid && sshdr.sense_key == UNIT_ATTENTION)));

		if (driver_byte(the_result) != DRIVER_SENSE) {
			/* no sense, TUR either succeeded or failed
			 * with a status error */
			if(!spintime && !scsi_status_is_good(the_result)) {
				sd_print_result(sdkp, "Test Unit Ready failed",
						the_result);
			}
			break;
		}

		/*
		 * The device does not want the automatic start to be issued.
		 */
		if (sdkp->device->no_start_on_add)
			break;

		if (sense_valid && sshdr.sense_key == NOT_READY) {
			if (sshdr.asc == 4 && sshdr.ascq == 3)
				break;	/* manual intervention required */
			if (sshdr.asc == 4 && sshdr.ascq == 0xb)
				break;	/* standby */
			if (sshdr.asc == 4 && sshdr.ascq == 0xc)
				break;	/* unavailable */
			if (sshdr.asc == 4 && sshdr.ascq == 0x1b)
				break;	/* sanitize in progress */
			/*
			 * Issue command to spin up drive when not ready
			 */
			if (!spintime) {
				sd_printk(KERN_NOTICE, sdkp, "Spinning up disk...");
				cmd[0] = START_STOP;
				cmd[1] = 1;	/* Return immediately */
				memset((void *) &cmd[2], 0, 8);
				cmd[4] = 1;	/* Start spin cycle */
				if (sdkp->device->start_stop_pwr_cond)
					cmd[4] |= 1 << 4;
				scsi_execute_req(sdkp->device, cmd, DMA_NONE,
						 NULL, 0, &sshdr,
						 SD_TIMEOUT, SD_MAX_RETRIES,
						 NULL);
				spintime_expire = jiffies + 100 * HZ;
				spintime = 1;
			}
			/* Wait 1 second for next try */
			msleep(1000);
			printk(KERN_CONT ".");

		/*
		 * Wait for USB flash devices with slow firmware.
		 * Yes, this sense key/ASC combination shouldn't
		 * occur here.  It's characteristic of these devices.
		 */
		} else if (sense_valid &&
				sshdr.sense_key == UNIT_ATTENTION &&
				sshdr.asc == 0x28) {
			if (!spintime) {
				spintime_expire = jiffies + 5 * HZ;
				spintime = 1;
			}
			/* Wait 1 second for next try */
			msleep(1000);
		} else {
			/* we don't understand the sense code, so it's
			 * probably pointless to loop */
			if(!spintime) {
				sd_printk(KERN_NOTICE, sdkp, "Unit Not Ready\n");
				sd_print_sense_hdr(sdkp, &sshdr);
			}
			break;
		}
				
	} while (spintime && time_before_eq(jiffies, spintime_expire));

	if (spintime) {
		if (scsi_status_is_good(the_result))
			printk(KERN_CONT "ready\n");
		else
			printk(KERN_CONT "not responding...\n");
	}
}

/*
 * Determine whether disk supports Data Integrity Field.
 */
static int sd_read_protection_type(struct scsi_disk *sdkp, unsigned char *buffer)
{
	struct scsi_device *sdp = sdkp->device;
	u8 type;
	int ret = 0;

	if (scsi_device_protection(sdp) == 0 || (buffer[12] & 1) == 0)
		return ret;

	type = ((buffer[12] >> 1) & 7) + 1; /* P_TYPE 0 = Type 1 */

	if (type > T10_PI_TYPE3_PROTECTION)
		ret = -ENODEV;
	else if (scsi_host_dif_capable(sdp->host, type))
		ret = 1;

	if (sdkp->first_scan || type != sdkp->protection_type)
		switch (ret) {
		case -ENODEV:
			sd_printk(KERN_ERR, sdkp, "formatted with unsupported" \
				  " protection type %u. Disabling disk!\n",
				  type);
			break;
		case 1:
			sd_printk(KERN_NOTICE, sdkp,
				  "Enabling DIF Type %u protection\n", type);
			break;
		case 0:
			sd_printk(KERN_NOTICE, sdkp,
				  "Disabling DIF Type %u protection\n", type);
			break;
		}

	sdkp->protection_type = type;

	return ret;
}

static void read_capacity_error(struct scsi_disk *sdkp, struct scsi_device *sdp,
			struct scsi_sense_hdr *sshdr, int sense_valid,
			int the_result)
{
	if (driver_byte(the_result) == DRIVER_SENSE)
		sd_print_sense_hdr(sdkp, sshdr);
	else
		sd_printk(KERN_NOTICE, sdkp, "Sense not available.\n");

	/*
	 * Set dirty bit for removable devices if not ready -
	 * sometimes drives will not report this properly.
	 */
	if (sdp->removable &&
	    sense_valid && sshdr->sense_key == NOT_READY)
		set_media_not_present(sdkp);

	/*
	 * We used to set media_present to 0 here to indicate no media
	 * in the drive, but some drives fail read capacity even with
	 * media present, so we can't do that.
	 */
	sdkp->capacity = 0; /* unknown mapped to zero - as usual */
}

#define RC16_LEN 32
#if RC16_LEN > SD_BUF_SIZE
#error RC16_LEN must not be more than SD_BUF_SIZE
#endif

#define READ_CAPACITY_RETRIES_ON_RESET	10

static int read_capacity_16(struct scsi_disk *sdkp, struct scsi_device *sdp,
						unsigned char *buffer)
{
	unsigned char cmd[16];
	struct scsi_sense_hdr sshdr;
	int sense_valid = 0;
	int the_result;
	int retries = 3, reset_retries = READ_CAPACITY_RETRIES_ON_RESET;
	unsigned int alignment;
	unsigned long long lba;
	unsigned sector_size;

	if (sdp->no_read_capacity_16)
		return -EINVAL;

	do {
		memset(cmd, 0, 16);
		cmd[0] = SERVICE_ACTION_IN_16;
		cmd[1] = SAI_READ_CAPACITY_16;
		cmd[13] = RC16_LEN;
		memset(buffer, 0, RC16_LEN);

		the_result = scsi_execute_req(sdp, cmd, DMA_FROM_DEVICE,
					buffer, RC16_LEN, &sshdr,
					SD_TIMEOUT, SD_MAX_RETRIES, NULL);

		if (media_not_present(sdkp, &sshdr))
			return -ENODEV;

		if (the_result) {
			sense_valid = scsi_sense_valid(&sshdr);
			if (sense_valid &&
			    sshdr.sense_key == ILLEGAL_REQUEST &&
			    (sshdr.asc == 0x20 || sshdr.asc == 0x24) &&
			    sshdr.ascq == 0x00)
				/* Invalid Command Operation Code or
				 * Invalid Field in CDB, just retry
				 * silently with RC10 */
				return -EINVAL;
			if (sense_valid &&
			    sshdr.sense_key == UNIT_ATTENTION &&
			    sshdr.asc == 0x29 && sshdr.ascq == 0x00)
				/* Device reset might occur several times,
				 * give it one more chance */
				if (--reset_retries > 0)
					continue;
		}
		retries--;

	} while (the_result && retries);

	if (the_result) {
		sd_print_result(sdkp, "Read Capacity(16) failed", the_result);
		read_capacity_error(sdkp, sdp, &sshdr, sense_valid, the_result);
		return -EINVAL;
	}

	sector_size = get_unaligned_be32(&buffer[8]);
	lba = get_unaligned_be64(&buffer[0]);

	if (sd_read_protection_type(sdkp, buffer) < 0) {
		sdkp->capacity = 0;
		return -ENODEV;
	}

	/* Logical blocks per physical block exponent */
	sdkp->physical_block_size = (1 << (buffer[13] & 0xf)) * sector_size;

	/* RC basis */
	sdkp->rc_basis = (buffer[12] >> 4) & 0x3;

	/* Lowest aligned logical block */
	alignment = ((buffer[14] & 0x3f) << 8 | buffer[15]) * sector_size;
	blk_queue_alignment_offset(sdp->request_queue, alignment);
	if (alignment && sdkp->first_scan)
		sd_printk(KERN_NOTICE, sdkp,
			  "physical block alignment offset: %u\n", alignment);

	if (buffer[14] & 0x80) { /* LBPME */
		sdkp->lbpme = 1;

		if (buffer[14] & 0x40) /* LBPRZ */
			sdkp->lbprz = 1;

		sd_config_discard(sdkp, SD_LBP_WS16);
	}

	sdkp->capacity = lba + 1;
	return sector_size;
}

static int read_capacity_10(struct scsi_disk *sdkp, struct scsi_device *sdp,
						unsigned char *buffer)
{
	unsigned char cmd[16];
	struct scsi_sense_hdr sshdr;
	int sense_valid = 0;
	int the_result;
	int retries = 3, reset_retries = READ_CAPACITY_RETRIES_ON_RESET;
	sector_t lba;
	unsigned sector_size;

	do {
		cmd[0] = READ_CAPACITY;
		memset(&cmd[1], 0, 9);
		memset(buffer, 0, 8);

		the_result = scsi_execute_req(sdp, cmd, DMA_FROM_DEVICE,
					buffer, 8, &sshdr,
					SD_TIMEOUT, SD_MAX_RETRIES, NULL);

		if (media_not_present(sdkp, &sshdr))
			return -ENODEV;

		if (the_result) {
			sense_valid = scsi_sense_valid(&sshdr);
			if (sense_valid &&
			    sshdr.sense_key == UNIT_ATTENTION &&
			    sshdr.asc == 0x29 && sshdr.ascq == 0x00)
				/* Device reset might occur several times,
				 * give it one more chance */
				if (--reset_retries > 0)
					continue;
		}
		retries--;

	} while (the_result && retries);

	if (the_result) {
		sd_print_result(sdkp, "Read Capacity(10) failed", the_result);
		read_capacity_error(sdkp, sdp, &sshdr, sense_valid, the_result);
		return -EINVAL;
	}

	sector_size = get_unaligned_be32(&buffer[4]);
	lba = get_unaligned_be32(&buffer[0]);

	if (sdp->no_read_capacity_16 && (lba == 0xffffffff)) {
		/* Some buggy (usb cardreader) devices return an lba of
		   0xffffffff when the want to report a size of 0 (with
		   which they really mean no media is present) */
		sdkp->capacity = 0;
		sdkp->physical_block_size = sector_size;
		return sector_size;
	}

	sdkp->capacity = lba + 1;
	sdkp->physical_block_size = sector_size;
	return sector_size;
}

static int sd_try_rc16_first(struct scsi_device *sdp)
{
	if (sdp->host->max_cmd_len < 16)
		return 0;
	if (sdp->try_rc_10_first)
		return 0;
	if (sdp->scsi_level > SCSI_SPC_2)
		return 1;
	if (scsi_device_protection(sdp))
		return 1;
	return 0;
}

/*
 * read disk capacity
 */
static void
sd_read_capacity(struct scsi_disk *sdkp, unsigned char *buffer)
{
	int sector_size;
	struct scsi_device *sdp = sdkp->device;

	if (sd_try_rc16_first(sdp)) {
		sector_size = read_capacity_16(sdkp, sdp, buffer);
		if (sector_size == -EOVERFLOW)
			goto got_data;
		if (sector_size == -ENODEV)
			return;
		if (sector_size < 0)
			sector_size = read_capacity_10(sdkp, sdp, buffer);
		if (sector_size < 0)
			return;
	} else {
		sector_size = read_capacity_10(sdkp, sdp, buffer);
		if (sector_size == -EOVERFLOW)
			goto got_data;
		if (sector_size < 0)
			return;
		if ((sizeof(sdkp->capacity) > 4) &&
		    (sdkp->capacity > 0xffffffffULL)) {
			int old_sector_size = sector_size;
			sd_printk(KERN_NOTICE, sdkp, "Very big device. "
					"Trying to use READ CAPACITY(16).\n");
			sector_size = read_capacity_16(sdkp, sdp, buffer);
			if (sector_size < 0) {
				sd_printk(KERN_NOTICE, sdkp,
					"Using 0xffffffff as device size\n");
				sdkp->capacity = 1 + (sector_t) 0xffffffff;
				sector_size = old_sector_size;
				goto got_data;
			}
			/* Remember that READ CAPACITY(16) succeeded */
			sdp->try_rc_10_first = 0;
		}
	}

	/* Some devices are known to return the total number of blocks,
	 * not the highest block number.  Some devices have versions
	 * which do this and others which do not.  Some devices we might
	 * suspect of doing this but we don't know for certain.
	 *
	 * If we know the reported capacity is wrong, decrement it.  If
	 * we can only guess, then assume the number of blocks is even
	 * (usually true but not always) and err on the side of lowering
	 * the capacity.
	 */
	if (sdp->fix_capacity ||
	    (sdp->guess_capacity && (sdkp->capacity & 0x01))) {
		sd_printk(KERN_INFO, sdkp, "Adjusting the sector count "
				"from its reported value: %llu\n",
				(unsigned long long) sdkp->capacity);
		--sdkp->capacity;
	}

got_data:
	if (sector_size == 0) {
		sector_size = 512;
		sd_printk(KERN_NOTICE, sdkp, "Sector size 0 reported, "
			  "assuming 512.\n");
	}

	if (sector_size != 512 &&
	    sector_size != 1024 &&
	    sector_size != 2048 &&
	    sector_size != 4096) {
		sd_printk(KERN_NOTICE, sdkp, "Unsupported sector size %d.\n",
			  sector_size);
		/*
		 * The user might want to re-format the drive with
		 * a supported sectorsize.  Once this happens, it
		 * would be relatively trivial to set the thing up.
		 * For this reason, we leave the thing in the table.
		 */
		sdkp->capacity = 0;
		/*
		 * set a bogus sector size so the normal read/write
		 * logic in the block layer will eventually refuse any
		 * request on this device without tripping over power
		 * of two sector size assumptions
		 */
		sector_size = 512;
	}
	blk_queue_logical_block_size(sdp->request_queue, sector_size);
	blk_queue_physical_block_size(sdp->request_queue,
				      sdkp->physical_block_size);
	sdkp->device->sector_size = sector_size;

	if (sdkp->capacity > 0xffffffff)
		sdp->use_16_for_rw = 1;

}

/*
 * Print disk capacity
 */
static void
sd_print_capacity(struct scsi_disk *sdkp,
		  sector_t old_capacity)
{
	int sector_size = sdkp->device->sector_size;
	char cap_str_2[10], cap_str_10[10];

	if (!sdkp->first_scan && old_capacity == sdkp->capacity)
		return;

	string_get_size(sdkp->capacity, sector_size,
			STRING_UNITS_2, cap_str_2, sizeof(cap_str_2));
	string_get_size(sdkp->capacity, sector_size,
			STRING_UNITS_10, cap_str_10, sizeof(cap_str_10));

	sd_printk(KERN_NOTICE, sdkp,
		  "%llu %d-byte logical blocks: (%s/%s)\n",
		  (unsigned long long)sdkp->capacity,
		  sector_size, cap_str_10, cap_str_2);

	if (sdkp->physical_block_size != sector_size)
		sd_printk(KERN_NOTICE, sdkp,
			  "%u-byte physical blocks\n",
			  sdkp->physical_block_size);

	sd_zbc_print_zones(sdkp);
}

/* called with buffer of length 512 */
static inline int
sd_do_mode_sense(struct scsi_device *sdp, int dbd, int modepage,
		 unsigned char *buffer, int len, struct scsi_mode_data *data,
		 struct scsi_sense_hdr *sshdr)
{
	return scsi_mode_sense(sdp, dbd, modepage, buffer, len,
			       SD_TIMEOUT, SD_MAX_RETRIES, data,
			       sshdr);
}

/*
 * read write protect setting, if possible - called only in sd_revalidate_disk()
 * called with buffer of length SD_BUF_SIZE
 */
static void
sd_read_write_protect_flag(struct scsi_disk *sdkp, unsigned char *buffer)
{
	int res;
	struct scsi_device *sdp = sdkp->device;
	struct scsi_mode_data data;
	int old_wp = sdkp->write_prot;

	set_disk_ro(sdkp->disk, 0);
	if (sdp->skip_ms_page_3f) {
		sd_first_printk(KERN_NOTICE, sdkp, "Assuming Write Enabled\n");
		return;
	}

	if (sdp->use_192_bytes_for_3f) {
		res = sd_do_mode_sense(sdp, 0, 0x3F, buffer, 192, &data, NULL);
	} else {
		/*
		 * First attempt: ask for all pages (0x3F), but only 4 bytes.
		 * We have to start carefully: some devices hang if we ask
		 * for more than is available.
		 */
		res = sd_do_mode_sense(sdp, 0, 0x3F, buffer, 4, &data, NULL);

		/*
		 * Second attempt: ask for page 0 When only page 0 is
		 * implemented, a request for page 3F may return Sense Key
		 * 5: Illegal Request, Sense Code 24: Invalid field in
		 * CDB.
		 */
		if (!scsi_status_is_good(res))
			res = sd_do_mode_sense(sdp, 0, 0, buffer, 4, &data, NULL);

		/*
		 * Third attempt: ask 255 bytes, as we did earlier.
		 */
		if (!scsi_status_is_good(res))
			res = sd_do_mode_sense(sdp, 0, 0x3F, buffer, 255,
					       &data, NULL);
	}

	if (!scsi_status_is_good(res)) {
		sd_first_printk(KERN_WARNING, sdkp,
			  "Test WP failed, assume Write Enabled\n");
	} else {
		sdkp->write_prot = ((data.device_specific & 0x80) != 0);
		set_disk_ro(sdkp->disk, sdkp->write_prot);
		if (sdkp->first_scan || old_wp != sdkp->write_prot) {
			sd_printk(KERN_NOTICE, sdkp, "Write Protect is %s\n",
				  sdkp->write_prot ? "on" : "off");
			sd_printk(KERN_DEBUG, sdkp, "Mode Sense: %4ph\n", buffer);
		}
	}
}

/*
 * sd_read_cache_type - called only from sd_revalidate_disk()
 * called with buffer of length SD_BUF_SIZE
 */
static void
sd_read_cache_type(struct scsi_disk *sdkp, unsigned char *buffer)
{
	int len = 0, res;
	struct scsi_device *sdp = sdkp->device;

	int dbd;
	int modepage;
	int first_len;
	struct scsi_mode_data data;
	struct scsi_sense_hdr sshdr;
	int old_wce = sdkp->WCE;
	int old_rcd = sdkp->RCD;
	int old_dpofua = sdkp->DPOFUA;


	if (sdkp->cache_override)
		return;

	first_len = 4;
	if (sdp->skip_ms_page_8) {
		if (sdp->type == TYPE_RBC)
			goto defaults;
		else {
			if (sdp->skip_ms_page_3f)
				goto defaults;
			modepage = 0x3F;
			if (sdp->use_192_bytes_for_3f)
				first_len = 192;
			dbd = 0;
		}
	} else if (sdp->type == TYPE_RBC) {
		modepage = 6;
		dbd = 8;
	} else {
		modepage = 8;
		dbd = 0;
	}

	/* cautiously ask */
	res = sd_do_mode_sense(sdp, dbd, modepage, buffer, first_len,
			&data, &sshdr);

	if (!scsi_status_is_good(res))
		goto bad_sense;

	if (!data.header_length) {
		modepage = 6;
		first_len = 0;
		sd_first_printk(KERN_ERR, sdkp,
				"Missing header in MODE_SENSE response\n");
	}

	/* that went OK, now ask for the proper length */
	len = data.length;

	/*
	 * We're only interested in the first three bytes, actually.
	 * But the data cache page is defined for the first 20.
	 */
	if (len < 3)
		goto bad_sense;
	else if (len > SD_BUF_SIZE) {
		sd_first_printk(KERN_NOTICE, sdkp, "Truncating mode parameter "
			  "data from %d to %d bytes\n", len, SD_BUF_SIZE);
		len = SD_BUF_SIZE;
	}
	if (modepage == 0x3F && sdp->use_192_bytes_for_3f)
		len = 192;

	/* Get the data */
	if (len > first_len)
		res = sd_do_mode_sense(sdp, dbd, modepage, buffer, len,
				&data, &sshdr);

	if (scsi_status_is_good(res)) {
		int offset = data.header_length + data.block_descriptor_length;

		while (offset < len) {
			u8 page_code = buffer[offset] & 0x3F;
			u8 spf       = buffer[offset] & 0x40;

			if (page_code == 8 || page_code == 6) {
				/* We're interested only in the first 3 bytes.
				 */
				if (len - offset <= 2) {
					sd_first_printk(KERN_ERR, sdkp,
						"Incomplete mode parameter "
							"data\n");
					goto defaults;
				} else {
					modepage = page_code;
					goto Page_found;
				}
			} else {
				/* Go to the next page */
				if (spf && len - offset > 3)
					offset += 4 + (buffer[offset+2] << 8) +
						buffer[offset+3];
				else if (!spf && len - offset > 1)
					offset += 2 + buffer[offset+1];
				else {
					sd_first_printk(KERN_ERR, sdkp,
							"Incomplete mode "
							"parameter data\n");
					goto defaults;
				}
			}
		}

		sd_first_printk(KERN_ERR, sdkp, "No Caching mode page found\n");
		goto defaults;

	Page_found:
		if (modepage == 8) {
			sdkp->WCE = ((buffer[offset + 2] & 0x04) != 0);
			sdkp->RCD = ((buffer[offset + 2] & 0x01) != 0);
		} else {
			sdkp->WCE = ((buffer[offset + 2] & 0x01) == 0);
			sdkp->RCD = 0;
		}

		sdkp->DPOFUA = (data.device_specific & 0x10) != 0;
		if (sdp->broken_fua) {
			sd_first_printk(KERN_NOTICE, sdkp, "Disabling FUA\n");
			sdkp->DPOFUA = 0;
		} else if (sdkp->DPOFUA && !sdkp->device->use_10_for_rw &&
			   !sdkp->device->use_16_for_rw) {
			sd_first_printk(KERN_NOTICE, sdkp,
				  "Uses READ/WRITE(6), disabling FUA\n");
			sdkp->DPOFUA = 0;
		}

		/* No cache flush allowed for write protected devices */
		if (sdkp->WCE && sdkp->write_prot)
			sdkp->WCE = 0;

		if (sdkp->first_scan || old_wce != sdkp->WCE ||
		    old_rcd != sdkp->RCD || old_dpofua != sdkp->DPOFUA)
			sd_printk(KERN_NOTICE, sdkp,
				  "Write cache: %s, read cache: %s, %s\n",
				  sdkp->WCE ? "enabled" : "disabled",
				  sdkp->RCD ? "disabled" : "enabled",
				  sdkp->DPOFUA ? "supports DPO and FUA"
				  : "doesn't support DPO or FUA");

		return;
	}

bad_sense:
	if (scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == ILLEGAL_REQUEST &&
	    sshdr.asc == 0x24 && sshdr.ascq == 0x0)
		/* Invalid field in CDB */
		sd_first_printk(KERN_NOTICE, sdkp, "Cache data unavailable\n");
	else
		sd_first_printk(KERN_ERR, sdkp,
				"Asking for cache data failed\n");

defaults:
	if (sdp->wce_default_on) {
		sd_first_printk(KERN_NOTICE, sdkp,
				"Assuming drive cache: write back\n");
		sdkp->WCE = 1;
	} else {
		sd_first_printk(KERN_ERR, sdkp,
				"Assuming drive cache: write through\n");
		sdkp->WCE = 0;
	}
	sdkp->RCD = 0;
	sdkp->DPOFUA = 0;
}

/*
 * The ATO bit indicates whether the DIF application tag is available
 * for use by the operating system.
 */
static void sd_read_app_tag_own(struct scsi_disk *sdkp, unsigned char *buffer)
{
	int res, offset;
	struct scsi_device *sdp = sdkp->device;
	struct scsi_mode_data data;
	struct scsi_sense_hdr sshdr;

	if (sdp->type != TYPE_DISK && sdp->type != TYPE_ZBC)
		return;

	if (sdkp->protection_type == 0)
		return;

	res = scsi_mode_sense(sdp, 1, 0x0a, buffer, 36, SD_TIMEOUT,
			      SD_MAX_RETRIES, &data, &sshdr);

	if (!scsi_status_is_good(res) || !data.header_length ||
	    data.length < 6) {
		sd_first_printk(KERN_WARNING, sdkp,
			  "getting Control mode page failed, assume no ATO\n");

		if (scsi_sense_valid(&sshdr))
			sd_print_sense_hdr(sdkp, &sshdr);

		return;
	}

	offset = data.header_length + data.block_descriptor_length;

	if ((buffer[offset] & 0x3f) != 0x0a) {
		sd_first_printk(KERN_ERR, sdkp, "ATO Got wrong page\n");
		return;
	}

	if ((buffer[offset + 5] & 0x80) == 0)
		return;

	sdkp->ATO = 1;

	return;
}

/**
 * sd_read_block_limits - Query disk device for preferred I/O sizes.
 * @sdkp: disk to query
 */
static void sd_read_block_limits(struct scsi_disk *sdkp)
{
	unsigned int sector_sz = sdkp->device->sector_size;
	const int vpd_len = 64;
	unsigned char *buffer = kmalloc(vpd_len, GFP_KERNEL);

	if (!buffer ||
	    /* Block Limits VPD */
	    scsi_get_vpd_page(sdkp->device, 0xb0, buffer, vpd_len))
		goto out;

	blk_queue_io_min(sdkp->disk->queue,
			 get_unaligned_be16(&buffer[6]) * sector_sz);

	sdkp->max_xfer_blocks = get_unaligned_be32(&buffer[8]);
	sdkp->opt_xfer_blocks = get_unaligned_be32(&buffer[12]);

	if (buffer[3] == 0x3c) {
		unsigned int lba_count, desc_count;

		sdkp->max_ws_blocks = (u32)get_unaligned_be64(&buffer[36]);

		if (!sdkp->lbpme)
			goto out;

		lba_count = get_unaligned_be32(&buffer[20]);
		desc_count = get_unaligned_be32(&buffer[24]);

		if (lba_count && desc_count)
			sdkp->max_unmap_blocks = lba_count;

		sdkp->unmap_granularity = get_unaligned_be32(&buffer[28]);

		if (buffer[32] & 0x80)
			sdkp->unmap_alignment =
				get_unaligned_be32(&buffer[32]) & ~(1 << 31);

		if (!sdkp->lbpvpd) { /* LBP VPD page not provided */

			if (sdkp->max_unmap_blocks)
				sd_config_discard(sdkp, SD_LBP_UNMAP);
			else
				sd_config_discard(sdkp, SD_LBP_WS16);

		} else {	/* LBP VPD page tells us what to use */
			if (sdkp->lbpu && sdkp->max_unmap_blocks)
				sd_config_discard(sdkp, SD_LBP_UNMAP);
			else if (sdkp->lbpws)
				sd_config_discard(sdkp, SD_LBP_WS16);
			else if (sdkp->lbpws10)
				sd_config_discard(sdkp, SD_LBP_WS10);
			else
				sd_config_discard(sdkp, SD_LBP_DISABLE);
		}
	}

 out:
	kfree(buffer);
}

/**
 * sd_read_block_characteristics - Query block dev. characteristics
 * @sdkp: disk to query
 */
static void sd_read_block_characteristics(struct scsi_disk *sdkp)
{
	struct request_queue *q = sdkp->disk->queue;
	unsigned char *buffer;
	u16 rot;
	const int vpd_len = 64;

	buffer = kmalloc(vpd_len, GFP_KERNEL);

	if (!buffer ||
	    /* Block Device Characteristics VPD */
	    scsi_get_vpd_page(sdkp->device, 0xb1, buffer, vpd_len))
		goto out;

	rot = get_unaligned_be16(&buffer[4]);

	if (rot == 1) {
		blk_queue_flag_set(QUEUE_FLAG_NONROT, q);
		blk_queue_flag_clear(QUEUE_FLAG_ADD_RANDOM, q);
	}

	if (sdkp->device->type == TYPE_ZBC) {
		/* Host-managed */
		q->limits.zoned = BLK_ZONED_HM;
	} else {
		sdkp->zoned = (buffer[8] >> 4) & 3;
		if (sdkp->zoned == 1)
			/* Host-aware */
			q->limits.zoned = BLK_ZONED_HA;
		else
			/*
			 * Treat drive-managed devices as
			 * regular block devices.
			 */
			q->limits.zoned = BLK_ZONED_NONE;
	}
	if (blk_queue_is_zoned(q) && sdkp->first_scan)
		sd_printk(KERN_NOTICE, sdkp, "Host-%s zoned block device\n",
		      q->limits.zoned == BLK_ZONED_HM ? "managed" : "aware");

 out:
	kfree(buffer);
}

/**
 * sd_read_block_provisioning - Query provisioning VPD page
 * @sdkp: disk to query
 */
static void sd_read_block_provisioning(struct scsi_disk *sdkp)
{
	unsigned char *buffer;
	const int vpd_len = 8;

	if (sdkp->lbpme == 0)
		return;

	buffer = kmalloc(vpd_len, GFP_KERNEL);

	if (!buffer || scsi_get_vpd_page(sdkp->device, 0xb2, buffer, vpd_len))
		goto out;

	sdkp->lbpvpd	= 1;
	sdkp->lbpu	= (buffer[5] >> 7) & 1;	/* UNMAP */
	sdkp->lbpws	= (buffer[5] >> 6) & 1;	/* WRITE SAME(16) with UNMAP */
	sdkp->lbpws10	= (buffer[5] >> 5) & 1;	/* WRITE SAME(10) with UNMAP */

 out:
	kfree(buffer);
}

static void sd_read_write_same(struct scsi_disk *sdkp, unsigned char *buffer)
{
	struct scsi_device *sdev = sdkp->device;

	if (sdev->host->no_write_same) {
		sdev->no_write_same = 1;

		return;
	}

	if (scsi_report_opcode(sdev, buffer, SD_BUF_SIZE, INQUIRY) < 0) {
		/* too large values might cause issues with arcmsr */
		int vpd_buf_len = 64;

		sdev->no_report_opcodes = 1;

		/* Disable WRITE SAME if REPORT SUPPORTED OPERATION
		 * CODES is unsupported and the device has an ATA
		 * Information VPD page (SAT).
		 */
		if (!scsi_get_vpd_page(sdev, 0x89, buffer, vpd_buf_len))
			sdev->no_write_same = 1;
	}

	if (scsi_report_opcode(sdev, buffer, SD_BUF_SIZE, WRITE_SAME_16) == 1)
		sdkp->ws16 = 1;

	if (scsi_report_opcode(sdev, buffer, SD_BUF_SIZE, WRITE_SAME) == 1)
		sdkp->ws10 = 1;
}

static void sd_read_security(struct scsi_disk *sdkp, unsigned char *buffer)
{
	struct scsi_device *sdev = sdkp->device;

	if (!sdev->security_supported)
		return;

	if (scsi_report_opcode(sdev, buffer, SD_BUF_SIZE,
			SECURITY_PROTOCOL_IN) == 1 &&
	    scsi_report_opcode(sdev, buffer, SD_BUF_SIZE,
			SECURITY_PROTOCOL_OUT) == 1)
		sdkp->security = 1;
}

/*
 * Determine the device's preferred I/O size for reads and writes
 * unless the reported value is unreasonably small, large, not a
 * multiple of the physical block size, or simply garbage.
 */
static bool sd_validate_opt_xfer_size(struct scsi_disk *sdkp,
				      unsigned int dev_max)
{
	struct scsi_device *sdp = sdkp->device;
	unsigned int opt_xfer_bytes =
		logical_to_bytes(sdp, sdkp->opt_xfer_blocks);

	if (sdkp->opt_xfer_blocks == 0)
		return false;

	if (sdkp->opt_xfer_blocks > dev_max) {
		sd_first_printk(KERN_WARNING, sdkp,
				"Optimal transfer size %u logical blocks " \
				"> dev_max (%u logical blocks)\n",
				sdkp->opt_xfer_blocks, dev_max);
		return false;
	}

	if (sdkp->opt_xfer_blocks > SD_DEF_XFER_BLOCKS) {
		sd_first_printk(KERN_WARNING, sdkp,
				"Optimal transfer size %u logical blocks " \
				"> sd driver limit (%u logical blocks)\n",
				sdkp->opt_xfer_blocks, SD_DEF_XFER_BLOCKS);
		return false;
	}

	if (opt_xfer_bytes < PAGE_SIZE) {
		sd_first_printk(KERN_WARNING, sdkp,
				"Optimal transfer size %u bytes < " \
				"PAGE_SIZE (%u bytes)\n",
				opt_xfer_bytes, (unsigned int)PAGE_SIZE);
		return false;
	}

	if (opt_xfer_bytes & (sdkp->physical_block_size - 1)) {
		sd_first_printk(KERN_WARNING, sdkp,
				"Optimal transfer size %u bytes not a " \
				"multiple of physical block size (%u bytes)\n",
				opt_xfer_bytes, sdkp->physical_block_size);
		return false;
	}

	sd_first_printk(KERN_INFO, sdkp, "Optimal transfer size %u bytes\n",
			opt_xfer_bytes);
	return true;
}

/**
 *	sd_revalidate_disk - called the first time a new disk is seen,
 *	performs disk spin up, read_capacity, etc.
 *	@disk: struct gendisk we care about
 **/
static int sd_revalidate_disk(struct gendisk *disk)
{
	struct scsi_disk *sdkp = scsi_disk(disk);
	struct scsi_device *sdp = sdkp->device;
	struct request_queue *q = sdkp->disk->queue;
	sector_t old_capacity = sdkp->capacity;
	unsigned char *buffer;
	unsigned int dev_max, rw_max;

	SCSI_LOG_HLQUEUE(3, sd_printk(KERN_INFO, sdkp,
				      "sd_revalidate_disk\n"));

	/*
	 * If the device is offline, don't try and read capacity or any
	 * of the other niceties.
	 */
	if (!scsi_device_online(sdp))
		goto out;

	buffer = kmalloc(SD_BUF_SIZE, GFP_KERNEL);
	if (!buffer) {
		sd_printk(KERN_WARNING, sdkp, "sd_revalidate_disk: Memory "
			  "allocation failure.\n");
		goto out;
	}

	sd_spinup_disk(sdkp);

	/*
	 * Without media there is no reason to ask; moreover, some devices
	 * react badly if we do.
	 */
	if (sdkp->media_present) {
		sd_read_capacity(sdkp, buffer);

		/*
		 * set the default to rotational.  All non-rotational devices
		 * support the block characteristics VPD page, which will
		 * cause this to be updated correctly and any device which
		 * doesn't support it should be treated as rotational.
		 */
		blk_queue_flag_clear(QUEUE_FLAG_NONROT, q);
		blk_queue_flag_set(QUEUE_FLAG_ADD_RANDOM, q);

		if (scsi_device_supports_vpd(sdp)) {
			sd_read_block_provisioning(sdkp);
			sd_read_block_limits(sdkp);
			sd_read_block_characteristics(sdkp);
			sd_zbc_read_zones(sdkp, buffer);
		}

		sd_print_capacity(sdkp, old_capacity);

		sd_read_write_protect_flag(sdkp, buffer);
		sd_read_cache_type(sdkp, buffer);
		sd_read_app_tag_own(sdkp, buffer);
		sd_read_write_same(sdkp, buffer);
		sd_read_security(sdkp, buffer);
	}

	/*
	 * We now have all cache related info, determine how we deal
	 * with flush requests.
	 */
	sd_set_flush_flag(sdkp);

	/* Initial block count limit based on CDB TRANSFER LENGTH field size. */
	dev_max = sdp->use_16_for_rw ? SD_MAX_XFER_BLOCKS : SD_DEF_XFER_BLOCKS;

	/* Some devices report a maximum block count for READ/WRITE requests. */
	dev_max = min_not_zero(dev_max, sdkp->max_xfer_blocks);
	q->limits.max_dev_sectors = logical_to_sectors(sdp, dev_max);

	if (sd_validate_opt_xfer_size(sdkp, dev_max)) {
		q->limits.io_opt = logical_to_bytes(sdp, sdkp->opt_xfer_blocks);
		rw_max = logical_to_sectors(sdp, sdkp->opt_xfer_blocks);
	} else
		rw_max = min_not_zero(logical_to_sectors(sdp, dev_max),
				      (sector_t)BLK_DEF_MAX_SECTORS);

	/* Do not exceed controller limit */
	rw_max = min(rw_max, queue_max_hw_sectors(q));

	/*
	 * Only update max_sectors if previously unset or if the current value
	 * exceeds the capabilities of the hardware.
	 */
	if (sdkp->first_scan ||
	    q->limits.max_sectors > q->limits.max_dev_sectors ||
	    q->limits.max_sectors > q->limits.max_hw_sectors)
		q->limits.max_sectors = rw_max;

	sdkp->first_scan = 0;

	set_capacity(disk, logical_to_sectors(sdp, sdkp->capacity));
	sd_config_write_same(sdkp);
	kfree(buffer);

 out:
	return 0;
}

/**
 *	sd_unlock_native_capacity - unlock native capacity
 *	@disk: struct gendisk to set capacity for
 *
 *	Block layer calls this function if it detects that partitions
 *	on @disk reach beyond the end of the device.  If the SCSI host
 *	implements ->unlock_native_capacity() method, it's invoked to
 *	give it a chance to adjust the device capacity.
 *
 *	CONTEXT:
 *	Defined by block layer.  Might sleep.
 */
static void sd_unlock_native_capacity(struct gendisk *disk)
{
	struct scsi_device *sdev = scsi_disk(disk)->device;

	if (sdev->host->hostt->unlock_native_capacity)
		sdev->host->hostt->unlock_native_capacity(sdev);
}

/**
 *	sd_format_disk_name - format disk name
 *	@prefix: name prefix - ie. "sd" for SCSI disks
 *	@index: index of the disk to format name for
 *	@buf: output buffer
 *	@buflen: length of the output buffer
 *
 *	SCSI disk names starts at sda.  The 26th device is sdz and the
 *	27th is sdaa.  The last one for two lettered suffix is sdzz
 *	which is followed by sdaaa.
 *
 *	This is basically 26 base counting with one extra 'nil' entry
 *	at the beginning from the second digit on and can be
 *	determined using similar method as 26 base conversion with the
 *	index shifted -1 after each digit is computed.
 *
 *	CONTEXT:
 *	Don't care.
 *
 *	RETURNS:
 *	0 on success, -errno on failure.
 */
static int sd_format_disk_name(char *prefix, int index, char *buf, int buflen)
{
	const int base = 'z' - 'a' + 1;
	char *begin = buf + strlen(prefix);
	char *end = buf + buflen;
	char *p;
	int unit;

	p = end - 1;
	*p = '\0';
	unit = base;
	do {
		if (p == begin)
			return -EINVAL;
		*--p = 'a' + (index % unit);
		index = (index / unit) - 1;
	} while (index >= 0);

	memmove(begin, p, end - p);
	memcpy(buf, prefix, strlen(prefix));

	return 0;
}

/**
 *	sd_probe - called during driver initialization and whenever a
 *	new scsi device is attached to the system. It is called once
 *	for each scsi device (not just disks) present.
 *	@dev: pointer to device object
 *
 *	Returns 0 if successful (or not interested in this scsi device 
 *	(e.g. scanner)); 1 when there is an error.
 *
 *	Note: this function is invoked from the scsi mid-level.
 *	This function sets up the mapping between a given 
 *	<host,channel,id,lun> (found in sdp) and new device name 
 *	(e.g. /dev/sda). More precisely it is the block device major 
 *	and minor number that is chosen here.
 *
 *	Assume sd_probe is not re-entrant (for time being)
 *	Also think about sd_probe() and sd_remove() running coincidentally.
 **/
static int sd_probe(struct device *dev)
{
	struct scsi_device *sdp = to_scsi_device(dev);
	struct scsi_disk *sdkp;
	struct gendisk *gd;
	int index;
	int error;

	scsi_autopm_get_device(sdp);
	error = -ENODEV;
	if (sdp->type != TYPE_DISK &&
	    sdp->type != TYPE_ZBC &&
	    sdp->type != TYPE_MOD &&
	    sdp->type != TYPE_RBC)
		goto out;

#ifndef CONFIG_BLK_DEV_ZONED
	if (sdp->type == TYPE_ZBC)
		goto out;
#endif
	SCSI_LOG_HLQUEUE(3, sdev_printk(KERN_INFO, sdp,
					"sd_probe\n"));

	error = -ENOMEM;
	sdkp = kzalloc(sizeof(*sdkp), GFP_KERNEL);
	if (!sdkp)
		goto out;

	gd = alloc_disk(SD_MINORS);
	if (!gd)
		goto out_free;

	index = ida_alloc(&sd_index_ida, GFP_KERNEL);
	if (index < 0) {
		sdev_printk(KERN_WARNING, sdp, "sd_probe: memory exhausted.\n");
		goto out_put;
	}

	error = sd_format_disk_name("sd", index, gd->disk_name, DISK_NAME_LEN);
	if (error) {
		sdev_printk(KERN_WARNING, sdp, "SCSI disk (sd) name length exceeded.\n");
		goto out_free_index;
	}

	sdkp->device = sdp;
	sdkp->driver = &sd_template;
	sdkp->disk = gd;
	sdkp->index = index;
	atomic_set(&sdkp->openers, 0);
	atomic_set(&sdkp->device->ioerr_cnt, 0);

	if (!sdp->request_queue->rq_timeout) {
		if (sdp->type != TYPE_MOD)
			blk_queue_rq_timeout(sdp->request_queue, SD_TIMEOUT);
		else
			blk_queue_rq_timeout(sdp->request_queue,
					     SD_MOD_TIMEOUT);
	}

	device_initialize(&sdkp->dev);
	sdkp->dev.parent = dev;
	sdkp->dev.class = &sd_disk_class;
	dev_set_name(&sdkp->dev, "%s", dev_name(dev));

	error = device_add(&sdkp->dev);
	if (error)
		goto out_free_index;

	get_device(dev);
	dev_set_drvdata(dev, sdkp);

	gd->major = sd_major((index & 0xf0) >> 4);
	gd->first_minor = ((index & 0xf) << 4) | (index & 0xfff00);

	gd->fops = &sd_fops;
	gd->private_data = &sdkp->driver;
	gd->queue = sdkp->device->request_queue;

	/* defaults, until the device tells us otherwise */
	sdp->sector_size = 512;
	sdkp->capacity = 0;
	sdkp->media_present = 1;
	sdkp->write_prot = 0;
	sdkp->cache_override = 0;
	sdkp->WCE = 0;
	sdkp->RCD = 0;
	sdkp->ATO = 0;
	sdkp->first_scan = 1;
	sdkp->max_medium_access_timeouts = SD_MAX_MEDIUM_TIMEOUTS;

	sd_revalidate_disk(gd);

	gd->flags = GENHD_FL_EXT_DEVT;
	if (sdp->removable) {
		gd->flags |= GENHD_FL_REMOVABLE;
		gd->events |= DISK_EVENT_MEDIA_CHANGE;
		gd->event_flags = DISK_EVENT_FLAG_POLL | DISK_EVENT_FLAG_UEVENT;
	}

	blk_pm_runtime_init(sdp->request_queue, dev);
	device_add_disk(dev, gd, NULL);
	if (sdkp->capacity)
		sd_dif_config_host(sdkp);

	sd_revalidate_disk(gd);

	if (sdkp->security) {
		sdkp->opal_dev = init_opal_dev(sdp, &sd_sec_submit);
		if (sdkp->opal_dev)
			sd_printk(KERN_NOTICE, sdkp, "supports TCG Opal\n");
	}

	sd_printk(KERN_NOTICE, sdkp, "Attached SCSI %sdisk\n",
		  sdp->removable ? "removable " : "");
	scsi_autopm_put_device(sdp);

	return 0;

 out_free_index:
	ida_free(&sd_index_ida, index);
 out_put:
	put_disk(gd);
 out_free:
	kfree(sdkp);
 out:
	scsi_autopm_put_device(sdp);
	return error;
}

/**
 *	sd_remove - called whenever a scsi disk (previously recognized by
 *	sd_probe) is detached from the system. It is called (potentially
 *	multiple times) during sd module unload.
 *	@dev: pointer to device object
 *
 *	Note: this function is invoked from the scsi mid-level.
 *	This function potentially frees up a device name (e.g. /dev/sdc)
 *	that could be re-used by a subsequent sd_probe().
 *	This function is not called when the built-in sd driver is "exit-ed".
 **/
static int sd_remove(struct device *dev)
{
	struct scsi_disk *sdkp;
	dev_t devt;

	sdkp = dev_get_drvdata(dev);
	devt = disk_devt(sdkp->disk);
	scsi_autopm_get_device(sdkp->device);

	async_synchronize_full_domain(&scsi_sd_pm_domain);
	device_del(&sdkp->dev);
	del_gendisk(sdkp->disk);
	sd_shutdown(dev);

	free_opal_dev(sdkp->opal_dev);

	blk_register_region(devt, SD_MINORS, NULL,
			    sd_default_probe, NULL, NULL);

	mutex_lock(&sd_ref_mutex);
	dev_set_drvdata(dev, NULL);
	put_device(&sdkp->dev);
	mutex_unlock(&sd_ref_mutex);

	return 0;
}

/**
 *	scsi_disk_release - Called to free the scsi_disk structure
 *	@dev: pointer to embedded class device
 *
 *	sd_ref_mutex must be held entering this routine.  Because it is
 *	called on last put, you should always use the scsi_disk_get()
 *	scsi_disk_put() helpers which manipulate the semaphore directly
 *	and never do a direct put_device.
 **/
static void scsi_disk_release(struct device *dev)
{
	struct scsi_disk *sdkp = to_scsi_disk(dev);
	struct gendisk *disk = sdkp->disk;
	struct request_queue *q = disk->queue;

	ida_free(&sd_index_ida, sdkp->index);

	/*
	 * Wait until all requests that are in progress have completed.
	 * This is necessary to avoid that e.g. scsi_end_request() crashes
	 * due to clearing the disk->private_data pointer. Wait from inside
	 * scsi_disk_release() instead of from sd_release() to avoid that
	 * freezing and unfreezing the request queue affects user space I/O
	 * in case multiple processes open a /dev/sd... node concurrently.
	 */
	blk_mq_freeze_queue(q);
	blk_mq_unfreeze_queue(q);

	disk->private_data = NULL;
	put_disk(disk);
	put_device(&sdkp->device->sdev_gendev);

	kfree(sdkp);
}

static int sd_start_stop_device(struct scsi_disk *sdkp, int start)
{
	unsigned char cmd[6] = { START_STOP };	/* START_VALID */
	struct scsi_sense_hdr sshdr;
	struct scsi_device *sdp = sdkp->device;
	int res;

	if (start)
		cmd[4] |= 1;	/* START */

	if (sdp->start_stop_pwr_cond)
		cmd[4] |= start ? 1 << 4 : 3 << 4;	/* Active or Standby */

	if (!scsi_device_online(sdp))
		return -ENODEV;

	res = scsi_execute(sdp, cmd, DMA_NONE, NULL, 0, NULL, &sshdr,
			SD_TIMEOUT, SD_MAX_RETRIES, 0, RQF_PM, NULL);
	if (res) {
		sd_print_result(sdkp, "Start/Stop Unit failed", res);
		if (driver_byte(res) == DRIVER_SENSE)
			sd_print_sense_hdr(sdkp, &sshdr);
		if (scsi_sense_valid(&sshdr) &&
			/* 0x3a is medium not present */
			sshdr.asc == 0x3a)
			res = 0;
	}

	/* SCSI error codes must not go to the generic layer */
	if (res)
		return -EIO;

	return 0;
}

/*
 * Send a SYNCHRONIZE CACHE instruction down to the device through
 * the normal SCSI command structure.  Wait for the command to
 * complete.
 */
static void sd_shutdown(struct device *dev)
{
	struct scsi_disk *sdkp = dev_get_drvdata(dev);

	if (!sdkp)
		return;         /* this can happen */

	if (pm_runtime_suspended(dev))
		return;

	if (sdkp->WCE && sdkp->media_present) {
		sd_printk(KERN_NOTICE, sdkp, "Synchronizing SCSI cache\n");
		sd_sync_cache(sdkp, NULL);
	}

	if (system_state != SYSTEM_RESTART && sdkp->device->manage_start_stop) {
		sd_printk(KERN_NOTICE, sdkp, "Stopping disk\n");
		sd_start_stop_device(sdkp, 0);
	}
}

static int sd_suspend_common(struct device *dev, bool ignore_stop_errors)
{
	struct scsi_disk *sdkp = dev_get_drvdata(dev);
	struct scsi_sense_hdr sshdr;
	int ret = 0;

	if (!sdkp)	/* E.g.: runtime suspend following sd_remove() */
		return 0;

	if (sdkp->WCE && sdkp->media_present) {
		sd_printk(KERN_NOTICE, sdkp, "Synchronizing SCSI cache\n");
		ret = sd_sync_cache(sdkp, &sshdr);

		if (ret) {
			/* ignore OFFLINE device */
			if (ret == -ENODEV)
				return 0;

			if (!scsi_sense_valid(&sshdr) ||
			    sshdr.sense_key != ILLEGAL_REQUEST)
				return ret;

			/*
			 * sshdr.sense_key == ILLEGAL_REQUEST means this drive
			 * doesn't support sync. There's not much to do and
			 * suspend shouldn't fail.
			 */
			ret = 0;
		}
	}

	if (sdkp->device->manage_start_stop) {
		sd_printk(KERN_NOTICE, sdkp, "Stopping disk\n");
		/* an error is not worth aborting a system sleep */
		ret = sd_start_stop_device(sdkp, 0);
		if (ignore_stop_errors)
			ret = 0;
	}

	return ret;
}

static int sd_suspend_system(struct device *dev)
{
	return sd_suspend_common(dev, true);
}

static int sd_suspend_runtime(struct device *dev)
{
	return sd_suspend_common(dev, false);
}

static int sd_resume(struct device *dev)
{
	struct scsi_disk *sdkp = dev_get_drvdata(dev);
	int ret;

	if (!sdkp)	/* E.g.: runtime resume at the start of sd_probe() */
		return 0;

	if (!sdkp->device->manage_start_stop)
		return 0;

	sd_printk(KERN_NOTICE, sdkp, "Starting disk\n");
	ret = sd_start_stop_device(sdkp, 1);
	if (!ret)
		opal_unlock_from_suspend(sdkp->opal_dev);
	return ret;
}

/**
 *	init_sd - entry point for this driver (both when built in or when
 *	a module).
 *
 *	Note: this function registers this driver with the scsi mid-level.
 **/
static int __init init_sd(void)
{
	int majors = 0, i, err;

	SCSI_LOG_HLQUEUE(3, printk("init_sd: sd driver entry point\n"));

	for (i = 0; i < SD_MAJORS; i++) {
		if (register_blkdev(sd_major(i), "sd") != 0)
			continue;
		majors++;
		blk_register_region(sd_major(i), SD_MINORS, NULL,
				    sd_default_probe, NULL, NULL);
	}

	if (!majors)
		return -ENODEV;

	err = class_register(&sd_disk_class);
	if (err)
		goto err_out;

	sd_cdb_cache = kmem_cache_create("sd_ext_cdb", SD_EXT_CDB_SIZE,
					 0, 0, NULL);
	if (!sd_cdb_cache) {
		printk(KERN_ERR "sd: can't init extended cdb cache\n");
		err = -ENOMEM;
		goto err_out_class;
	}

	sd_cdb_pool = mempool_create_slab_pool(SD_MEMPOOL_SIZE, sd_cdb_cache);
	if (!sd_cdb_pool) {
		printk(KERN_ERR "sd: can't init extended cdb pool\n");
		err = -ENOMEM;
		goto err_out_cache;
	}

	sd_page_pool = mempool_create_page_pool(SD_MEMPOOL_SIZE, 0);
	if (!sd_page_pool) {
		printk(KERN_ERR "sd: can't init discard page pool\n");
		err = -ENOMEM;
		goto err_out_ppool;
	}

	err = scsi_register_driver(&sd_template.gendrv);
	if (err)
		goto err_out_driver;

	return 0;

err_out_driver:
	mempool_destroy(sd_page_pool);

err_out_ppool:
	mempool_destroy(sd_cdb_pool);

err_out_cache:
	kmem_cache_destroy(sd_cdb_cache);

err_out_class:
	class_unregister(&sd_disk_class);
err_out:
	for (i = 0; i < SD_MAJORS; i++)
		unregister_blkdev(sd_major(i), "sd");
	return err;
}

/**
 *	exit_sd - exit point for this driver (when it is a module).
 *
 *	Note: this function unregisters this driver from the scsi mid-level.
 **/
static void __exit exit_sd(void)
{
	int i;

	SCSI_LOG_HLQUEUE(3, printk("exit_sd: exiting sd driver\n"));

	scsi_unregister_driver(&sd_template.gendrv);
	mempool_destroy(sd_cdb_pool);
	mempool_destroy(sd_page_pool);
	kmem_cache_destroy(sd_cdb_cache);

	class_unregister(&sd_disk_class);

	for (i = 0; i < SD_MAJORS; i++) {
		blk_unregister_region(sd_major(i), SD_MINORS);
		unregister_blkdev(sd_major(i), "sd");
	}
}

module_init(init_sd);
module_exit(exit_sd);

static void sd_print_sense_hdr(struct scsi_disk *sdkp,
			       struct scsi_sense_hdr *sshdr)
{
	scsi_print_sense_hdr(sdkp->device,
			     sdkp->disk ? sdkp->disk->disk_name : NULL, sshdr);
}

static void sd_print_result(const struct scsi_disk *sdkp, const char *msg,
			    int result)
{
	const char *hb_string = scsi_hostbyte_string(result);
	const char *db_string = scsi_driverbyte_string(result);

	if (hb_string || db_string)
		sd_printk(KERN_INFO, sdkp,
			  "%s: Result: hostbyte=%s driverbyte=%s\n", msg,
			  hb_string ? hb_string : "invalid",
			  db_string ? db_string : "invalid");
	else
		sd_printk(KERN_INFO, sdkp,
			  "%s: Result: hostbyte=0x%02x driverbyte=0x%02x\n",
			  msg, host_byte(result), driver_byte(result));
}








======================================================================================================================================================

/*
 * SCSI hosts which support the Data Integrity Extensions must
 * indicate their capabilities by setting the prot_capabilities using
 * this call.
 */
static inline void scsi_host_set_prot(struct Scsi_Host *shost, unsigned int mask)
{
	shost->prot_capabilities = mask;
}

static inline unsigned int scsi_host_dif_capable(struct Scsi_Host *shost, unsigned int target_type)
{
	static unsigned char cap[] = { 0,
				       SHOST_DIF_TYPE1_PROTECTION,
				       SHOST_DIF_TYPE2_PROTECTION,
				       SHOST_DIF_TYPE3_PROTECTION };

	if (target_type >= ARRAY_SIZE(cap))
		return 0;

	return shost->prot_capabilities & cap[target_type] ? target_type : 0;
}

static inline unsigned scsi_prot_sg_count(struct scsi_cmnd *cmd)
{
	return cmd->prot_sdb ? cmd->prot_sdb->table.nents : 0;
}

#define rq_data_dir(rq)		(op_is_write(req_op(rq)) ? WRITE : READ)

#define req_op(req) \
	((req)->cmd_flags & REQ_OP_MASK)

static inline bool op_is_write(unsigned int op)
{
	return (op & 1);
}

static inline sector_t sectors_to_logical(struct scsi_device *sdev, sector_t sector)
{
	return sector >> (ilog2(sdev->sector_size) - 9);                                     // sector >> (ilog2(512) - 9) = sector >> (9 - 9) = sector
}

/*
 * blk_rq_pos()			: the current sector
 * blk_rq_bytes()		: bytes left in the entire request
 * blk_rq_cur_bytes()		: bytes left in the current segment
 * blk_rq_err_bytes()		: bytes left till the next error boundary
 * blk_rq_sectors()		: sectors left in the entire request
 * blk_rq_cur_sectors()		: sectors left in the current segment
 * blk_rq_stats_sectors()	: sectors of the entire request used for stats
 */
static inline sector_t blk_rq_pos(const struct request *rq)
{
	return rq->__sector;
}

static inline unsigned int blk_rq_bytes(const struct request *rq)
{
	return rq->__data_len;
}

static inline int blk_rq_cur_bytes(const struct request *rq)
{
	return rq->bio ? bio_cur_bytes(rq->bio) : 0;
}

static inline unsigned int blk_rq_sectors(const struct request *rq)
{
	return blk_rq_bytes(rq) >> SECTOR_SHIFT;                                             // rq->__data_len >> SECTOR_SHIFT
}

static inline unsigned int blk_rq_cur_sectors(const struct request *rq)
{
	return blk_rq_cur_bytes(rq) >> SECTOR_SHIFT;
}

static inline unsigned int blk_rq_stats_sectors(const struct request *rq)
{
	return rq->stats_sectors;
}

static inline struct scsi_disk *scsi_disk(struct gendisk *disk)
{
	return container_of(disk->private_data, struct scsi_disk, driver);
}

static inline bool blk_rq_is_scsi(struct request *rq)
{
	return blk_op_is_scsi(req_op(rq));
}

static inline bool blk_op_is_scsi(unsigned int op)
{
	return op == REQ_OP_SCSI_IN || op == REQ_OP_SCSI_OUT;
}

static inline struct scsi_request *scsi_req(struct request *rq)
{
	return blk_mq_rq_to_pdu(rq);                                                         // rq + 1
}

static inline void *blk_mq_rq_to_pdu(struct request *rq)
{
	return rq + 1;
}

/**
 * scsi_host_alloc - register a scsi host adapter instance.
 * @sht:	pointer to scsi host template
 * @privsize:	extra bytes to allocate for driver
 *
 * Note:
 * 	Allocate a new Scsi_Host and perform basic initialization.
 * 	The host is not published to the scsi midlayer until scsi_add_host
 * 	is called.
 *
 * Return value:
 * 	Pointer to a new Scsi_Host
 **/
struct Scsi_Host *scsi_host_alloc(struct scsi_host_template *sht, int privsize)
{
	struct Scsi_Host *shost;
	gfp_t gfp_mask = GFP_KERNEL;
	int index;

	if (sht->unchecked_isa_dma && privsize)                                              // 若sht->unchecked_isa_dma不为0，并且privsize不为0
		gfp_mask |= __GFP_DMA;                                                           // 添加__GFP_DMA到gfp_mask

	shost = kzalloc(sizeof(struct Scsi_Host) + privsize, gfp_mask);                      // 为shost分配内存空间
	if (!shost)                                                                          // 若shost为NULL
		return NULL;                                                                     // 返回NULL

	shost->host_lock = &shost->default_lock;                                             // 保存&shost->default_lock到shost->host_lock
	spin_lock_init(shost->host_lock);                                                    // 初始化shost->host_lock
	shost->shost_state = SHOST_CREATED;                                                  // 设置shost->shost_state为SHOST_CREATED
	INIT_LIST_HEAD(&shost->__devices);                                                   // 初始化shost->__devices
	INIT_LIST_HEAD(&shost->__targets);                                                   // 初始化shost->__targets
	INIT_LIST_HEAD(&shost->eh_cmd_q);                                                    // 初始化shost->eh_cmd_q
	INIT_LIST_HEAD(&shost->starved_list);                                                // 初始化shost->starved_list
	init_waitqueue_head(&shost->host_wait);                                              // 初始化shost->host_wait
	mutex_init(&shost->scan_mutex);                                                      // 初始化shost->scan_mutex

	index = ida_simple_get(&host_index_ida, 0, 0, GFP_KERNEL);                           // Allocate an unused ID
	if (index < 0)                                                                       // 若index < 0
		goto fail_kfree;                                                                 // 跳转至fail_kfree
	shost->host_no = index;                                                              // 保存index到shost->host_no

	shost->dma_channel = 0xff;                                                           // 设置shost->dma_channel为0xff

	/* These three are default values which can be overridden */
	shost->max_channel = 0;                                                              // 设置shost->max_channel为0
	shost->max_id = 8;                                                                   // 设置shost->max_id为8
	shost->max_lun = 8;                                                                  // 设置shost->max_lun为8

	/* Give each shost a default transportt */
	shost->transportt = &blank_transport_template;                                       // 设置shost->transportt为&blank_transport_template

	/*
	 * All drivers right now should be able to handle 12 byte
	 * commands.  Every so often there are requests for 16 byte
	 * commands, but individual low-level drivers need to certify that
	 * they actually do something sensible with such commands.
	 */
	shost->max_cmd_len = 12;                                                             // 重点：设置shost->max_cmd_len为12
	shost->hostt = sht;                                                                  // 重点：保存sht到shost->hostt
	shost->this_id = sht->this_id;                                                       // 设置shost->this_id为sht->this_id
	shost->can_queue = sht->can_queue;                                                   // 重点：设置shost->can_queue为sht->can_queue
	shost->sg_tablesize = sht->sg_tablesize;                                             // 重点：设置shost->sg_tablesize为sht->sg_tablesize
	shost->sg_prot_tablesize = sht->sg_prot_tablesize;                                   // 设置shost->sg_prot_tablesize为sht->sg_prot_tablesize
	shost->cmd_per_lun = sht->cmd_per_lun;                                               // 设置shost->cmd_per_lun为sht->cmd_per_lun
	shost->unchecked_isa_dma = sht->unchecked_isa_dma;                                   // 设置shost->unchecked_isa_dma为sht->unchecked_isa_dma
	shost->no_write_same = sht->no_write_same;                                           // 设置shost->no_write_same为sht->no_write_same

	if (shost_eh_deadline == -1 || !sht->eh_host_reset_handler)                          // 若shost_eh_deadline == -1为true，或sht->eh_host_reset_handler为NULL
		shost->eh_deadline = -1;                                                         // 设置shost->eh_deadline为-1
	else if ((ulong) shost_eh_deadline * HZ > INT_MAX) {                                 // 若(ulong) shost_eh_deadline * HZ > INT_MAX为true
		shost_printk(KERN_WARNING, shost,
			     "eh_deadline %u too large, setting to %u\n",
			     shost_eh_deadline, INT_MAX / HZ);                                       // 打印提示信息
		shost->eh_deadline = INT_MAX;                                                    // 设置shost->eh_deadline为INT_MAX
	} else
		shost->eh_deadline = shost_eh_deadline * HZ;                                     // 设置shost->eh_deadline为shost_eh_deadline * HZ

	if (sht->supported_mode == MODE_UNKNOWN)                                             // 若sht->supported_mode == MODE_UNKNOWN为true
		/* means we didn't set it ... default to INITIATOR */
		shost->active_mode = MODE_INITIATOR;                                             // 设置shost->active_mode为MODE_INITIATOR
	else
		shost->active_mode = sht->supported_mode;                                        // 设置shost->active_mode为sht->supported_mode

	if (sht->max_host_blocked)                                                           // 若sht->max_host_blocked不为0
		shost->max_host_blocked = sht->max_host_blocked;                                 // 设置shost->max_host_blocked为sht->max_host_blocked
	else
		shost->max_host_blocked = SCSI_DEFAULT_HOST_BLOCKED;                             // 设置shost->max_host_blocked为SCSI_DEFAULT_HOST_BLOCKED

	/*
	 * If the driver imposes no hard sector transfer limit, start at
	 * machine infinity initially.
	 */
	if (sht->max_sectors)                                                                // 若sht->max_sectors不为0
		shost->max_sectors = sht->max_sectors;                                           // 设置shost->max_sectors为sht->max_sectors
	else
		shost->max_sectors = SCSI_DEFAULT_MAX_SECTORS;                                   // 设置shost->max_sectors为SCSI_DEFAULT_MAX_SECTORS

	if (sht->max_segment_size)                                                           // 若sht->max_segment_size不为0
		shost->max_segment_size = sht->max_segment_size;                                 // 设置shost->max_segment_size为sht->max_segment_size，#define PRDT_DATA_BYTE_COUNT_MAX (256 * 1024)
	else
		shost->max_segment_size = BLK_MAX_SEGMENT_SIZE;                                  // 设置shost->max_segment_size为BLK_MAX_SEGMENT_SIZE，BLK_MAX_SEGMENT_SIZE	= 65536

	/*
	 * assume a 4GB boundary, if not set
	 */
	if (sht->dma_boundary)                                                               // 若sht->dma_boundary不为0
		shost->dma_boundary = sht->dma_boundary;                                         // 设置shost->dma_boundary为sht->dma_boundary
	else
		shost->dma_boundary = 0xffffffff;                                                // 设置shost->dma_boundary为0xffffffff

	if (sht->virt_boundary_mask)                                                         // 若sht->virt_boundary_mask不为0
		shost->virt_boundary_mask = sht->virt_boundary_mask;                             // 设置shost->virt_boundary_mask为sht->virt_boundary_mask

	device_initialize(&shost->shost_gendev);                                             // 初始化shost->shost_gendev
	dev_set_name(&shost->shost_gendev, "host%d", shost->host_no);                        // 设置shost->shost_gendev.kobj.name
	shost->shost_gendev.bus = &scsi_bus_type;                                            // 保存&scsi_bus_type到shost->shost_gendev.bus
	shost->shost_gendev.type = &scsi_host_type;                                          // 保存&scsi_host_type到shost->shost_gendev.type

	device_initialize(&shost->shost_dev);                                                // 初始化shost->shost_dev
	shost->shost_dev.parent = &shost->shost_gendev;                                      // 设置shost->shost_dev.parent为&shost->shost_gendev
	shost->shost_dev.class = &shost_class;                                               // 设置shost->shost_dev.class为&shost_class
	dev_set_name(&shost->shost_dev, "host%d", shost->host_no);                           // 设置shost->shost_dev.kobj.name
	shost->shost_dev.groups = scsi_sysfs_shost_attr_groups;                              // 设置shost->shost_dev.groups为scsi_sysfs_shost_attr_groups

	shost->ehandler = kthread_run(scsi_error_handler, shost,
			"scsi_eh_%d", shost->host_no);                                               // 创建错误处理task并唤醒，将task保存在shost->ehandler中
	if (IS_ERR(shost->ehandler)) {                                                       // 若IS_ERR(shost->ehandler)为true
		shost_printk(KERN_WARNING, shost,
			"error handler thread failed to spawn, error = %ld\n",
			PTR_ERR(shost->ehandler));                                                   // 打印提示信息
		goto fail_index_remove;                                                          // 跳转至fail_index_remove
	}

	shost->tmf_work_q = alloc_workqueue("scsi_tmf_%d",
					    WQ_UNBOUND | WQ_MEM_RECLAIM,
					   1, shost->host_no);                                               // 申请shost->tmf_work_q
	if (!shost->tmf_work_q) {                                                            // 若shost->tmf_work_q为NULL
		shost_printk(KERN_WARNING, shost,
			     "failed to create tmf workq\n");                                        // 打印提示信息
		goto fail_kthread;                                                               // 跳转至fail_kthread
	}
	scsi_proc_hostdir_add(shost->hostt);                                                 // *
	return shost;                                                                        // 返回shost

 fail_kthread:
	kthread_stop(shost->ehandler);
 fail_index_remove:
	ida_simple_remove(&host_index_ida, shost->host_no);
 fail_kfree:
	kfree(shost);
	return NULL;                                                                         // 返回NULL
}
EXPORT_SYMBOL(scsi_host_alloc);

static DEFINE_IDA(host_index_ida);

struct ufs_mtk_host {
	struct ufs_hba *hba;
	struct phy *mphy;
}

/**
 * ufshcd_pltfrm_init - probe routine of the driver
 * @pdev: pointer to Platform device handle
 * @vops: pointer to variant ops
 *
 * Returns 0 on success, non-zero value on failure
 */
int ufshcd_pltfrm_init(struct platform_device *pdev,
		       const struct ufs_hba_variant_ops *vops)
{
	struct ufs_hba *hba;
	void __iomem *mmio_base;
	int irq, err;
	struct device *dev = &pdev->dev;                                                     // 获取dev

	mmio_base = devm_platform_ioremap_resource(pdev, 0);                                 // 获取dts中定义的寄存器地址并映射
	if (IS_ERR(mmio_base)) {                                                             // 若IS_ERR(mmio_base)为true
		err = PTR_ERR(mmio_base);                                                        // 设置err为PTR_ERR(mmio_base)
		goto out;                                                                        // 跳转至out
	}

	irq = platform_get_irq(pdev, 0);                                                     // 获取dts中定义的中断号并映射
	if (irq < 0) {                                                                       // 若irq < 0
		dev_err(dev, "IRQ resource not available\n");                                    // 打印提示信息
		err = -ENODEV;                                                                   // 设置err为-ENODEV
		goto out;                                                                        // 跳转至out
	}

	err = ufshcd_alloc_host(dev, &hba);                                                  // allocate Host Bus Adapter (HBA)
	if (err) {                                                                           // 若err不为0
		dev_err(&pdev->dev, "Allocation failed\n");                                      // 打印提示信息
		goto out;                                                                        // 跳转至out
	}

	hba->vops = vops;                                                                    // 重点：保存vops到hba->vops

	err = ufshcd_parse_clock_info(hba);                                                  // 重点：*
	if (err) {                                                                           // 若err不为0
		dev_err(&pdev->dev, "%s: clock parse failed %d\n",
				__func__, err);                                                          // 打印提示信息
		goto dealloc_host;                                                               // 跳转至dealloc_host
	}
	err = ufshcd_parse_regulator_info(hba);                                              // 重点：*
	if (err) {                                                                           // 若err不为0
		dev_err(&pdev->dev, "%s: regulator init failed %d\n",
				__func__, err);                                                          // 打印提示信息
		goto dealloc_host;                                                               // 跳转至dealloc_host
	}

	ufshcd_init_lanes_per_dir(hba);                                                      // *

	err = ufshcd_init(hba, mmio_base, irq);                                              // 重点：Driver initialization routine
	if (err) {                                                                           // 若err不为0
		dev_err(dev, "Initialization failed\n");                                         // 打印提示信息
		goto dealloc_host;                                                               // 跳转至dealloc_host
	}

	platform_set_drvdata(pdev, hba);                                                     // pdev->dev.driver_data = data

	pm_runtime_set_active(&pdev->dev);                                                   // *
	pm_runtime_enable(&pdev->dev);                                                       // *

	return 0;                                                                            // 返回0

dealloc_host:
	ufshcd_dealloc_host(hba);
out:
	return err;                                                                          // 返回err
}
EXPORT_SYMBOL_GPL(ufshcd_pltfrm_init);

/**
 * ufs_mtk_init - find other essential mmio bases
 * @hba: host controller instance
 *
 * Binds PHY with controller and powers up PHY enabling clocks
 * and regulators.
 *
 * Returns -EPROBE_DEFER if binding fails, returns negative error
 * on phy power up failure and returns zero on success.
 */
static int ufs_mtk_init(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host;
	struct device *dev = hba->dev;                                                       // 取出dev
	int err = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);                                 // 为host申请内存
	if (!host) {                                                                         // 若host为NULL
		err = -ENOMEM;                                                                   // 设置err为-ENOMEM
		dev_info(dev, "%s: no memory for mtk ufs host\n", __func__);                     // 打印提示信息
		goto out;                                                                        // 跳转至out
	}

	host->hba = hba;                                                                     // 重点：将hba保存到host->hba中
	ufshcd_set_variant(hba, host);                                                       // hba->priv = host

	err = ufs_mtk_bind_mphy(hba);                                                        // *
	if (err)                                                                             // 若err不为0
		goto out_variant_clear;                                                          // 跳转至out_variant_clear

	/*
	 * ufshcd_vops_init() is invoked after
	 * ufshcd_setup_clock(true) in ufshcd_hba_init() thus
	 * phy clock setup is skipped.
	 *
	 * Enable phy clocks specifically here.
	 */
	ufs_mtk_setup_clocks(hba, true, POST_CHANGE);                                        // *

	goto out;                                                                            // 跳转至out

out_variant_clear:
	ufshcd_set_variant(hba, NULL);                                                       // hba->priv = NULL
out:
	return err;                                                                          // 返回err
}

/**
 * struct ufs_hba_mtk_vops - UFS MTK specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static struct ufs_hba_variant_ops ufs_hba_mtk_vops = {
	.name                = "mediatek.ufshci",                                            // variant name
	.init                = ufs_mtk_init,                                                 // 重点：called when the driver is initialized
	.setup_clocks        = ufs_mtk_setup_clocks,                                         // called before touching any of the controller registers
	.link_startup_notify = ufs_mtk_link_startup_notify,                                  // called before and after Link startup is carried out to allow variant specific Uni-Pro initialization
	.pwr_change_notify   = ufs_mtk_pwr_change_notify,                                    // called before and after a power mode change is carried out to allow vendor spesific capabilities to be set
	.suspend             = ufs_mtk_suspend,                                              // called during host controller PM callback
	.resume              = ufs_mtk_resume,                                               // called during host controller PM callback
};

/**
 * ufs_mtk_probe - probe routine of the driver
 * @pdev: pointer to Platform device handle
 *
 * Return zero for success and non-zero for failure
 */
static int ufs_mtk_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;                                                     // 获取dev

	/* perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_mtk_vops);                                   // probe routine of the driver
	if (err)                                                                             // 若err不为0
		dev_info(dev, "probe failed %d\n", err);                                         // 打印提示信息

	return err;                                                                          // 返回err
}

static const struct of_device_id ufs_mtk_of_match[] = {
	{ .compatible = "mediatek,mt8183-ufshci"},
	{},
};

static struct platform_driver ufs_mtk_pltform = {
	.probe      = ufs_mtk_probe,
	.remove     = ufs_mtk_remove,
	.shutdown   = ufshcd_pltfrm_shutdown,
	.driver = {
		.name   = "ufshcd-mtk",
		.pm     = &ufs_mtk_pm_ops,
		.of_match_table = ufs_mtk_of_match,
	},
};

module_platform_driver(ufs_mtk_pltform);                                                 // 重点

/**
 * scsi_scan_host - scan the given adapter
 * @shost:	adapter to scan
 **/
void scsi_scan_host(struct Scsi_Host *shost)
{
	struct async_scan_data *data;

	if (strncmp(scsi_scan_type, "none", 4) == 0 ||
	    strncmp(scsi_scan_type, "manual", 6) == 0)
		return;
	if (scsi_autopm_get_host(shost) < 0)
		return;

	data = scsi_prep_async_scan(shost);
	if (!data) {
		do_scsi_scan_host(shost);
		scsi_autopm_put_host(shost);
		return;
	}

	/* register with the async subsystem so wait_for_device_probe()
	 * will flush this work
	 */
	async_schedule(do_scan_async, data);

	/* scsi_autopm_put_host(shost) is called in scsi_finish_async_scan() */
}
EXPORT_SYMBOL(scsi_scan_host);

static void do_scan_async(void *_data, async_cookie_t c)
{
	struct async_scan_data *data = _data;
	struct Scsi_Host *shost = data->shost;

	do_scsi_scan_host(shost);
	scsi_finish_async_scan(data);
}

static void do_scsi_scan_host(struct Scsi_Host *shost)
{
	if (shost->hostt->scan_finished) {
		unsigned long start = jiffies;
		if (shost->hostt->scan_start)
			shost->hostt->scan_start(shost);

		while (!shost->hostt->scan_finished(shost, jiffies - start))
			msleep(10);
	} else {
		scsi_scan_host_selected(shost, SCAN_WILD_CARD, SCAN_WILD_CARD,
				SCAN_WILD_CARD, 0);
	}
}

static struct class sd_disk_class = {
	.name		= "scsi_disk",
	.owner		= THIS_MODULE,
	.dev_release	= scsi_disk_release,
	.dev_groups	= sd_disk_groups,
};

static const struct dev_pm_ops sd_pm_ops = {
	.suspend		= sd_suspend_system,
	.resume			= sd_resume,
	.poweroff		= sd_suspend_system,
	.restore		= sd_resume,
	.runtime_suspend	= sd_suspend_runtime,
	.runtime_resume		= sd_resume,
};

static struct scsi_driver sd_template = {
	.gendrv = {
		.name		= "sd",
		.owner		= THIS_MODULE,
		.probe		= sd_probe,
		.probe_type	= PROBE_PREFER_ASYNCHRONOUS,
		.remove		= sd_remove,
		.shutdown	= sd_shutdown,
		.pm		= &sd_pm_ops,
	},
	.rescan			= sd_rescan,
	.init_command		= sd_init_command,
	.uninit_command		= sd_uninit_command,
	.done			= sd_done,
	.eh_action		= sd_eh_action,
	.eh_reset		= sd_eh_reset,
};

/**
 *	sd_probe - called during driver initialization and whenever a
 *	new scsi device is attached to the system. It is called once
 *	for each scsi device (not just disks) present.
 *	@dev: pointer to device object
 *
 *	Returns 0 if successful (or not interested in this scsi device 
 *	(e.g. scanner)); 1 when there is an error.
 *
 *	Note: this function is invoked from the scsi mid-level.
 *	This function sets up the mapping between a given 
 *	<host,channel,id,lun> (found in sdp) and new device name 
 *	(e.g. /dev/sda). More precisely it is the block device major 
 *	and minor number that is chosen here.
 *
 *	Assume sd_probe is not re-entrant (for time being)
 *	Also think about sd_probe() and sd_remove() running coincidentally.
 **/
static int sd_probe(struct device *dev)
{
	struct scsi_device *sdp = to_scsi_device(dev);
	struct scsi_disk *sdkp;
	struct gendisk *gd;
	int index;
	int error;

	scsi_autopm_get_device(sdp);
	error = -ENODEV;
	if (sdp->type != TYPE_DISK &&
	    sdp->type != TYPE_ZBC &&
	    sdp->type != TYPE_MOD &&
	    sdp->type != TYPE_RBC)
		goto out;

#ifndef CONFIG_BLK_DEV_ZONED
	if (sdp->type == TYPE_ZBC)
		goto out;
#endif
	SCSI_LOG_HLQUEUE(3, sdev_printk(KERN_INFO, sdp,
					"sd_probe\n"));

	error = -ENOMEM;
	sdkp = kzalloc(sizeof(*sdkp), GFP_KERNEL);
	if (!sdkp)
		goto out;

	gd = alloc_disk(SD_MINORS);
	if (!gd)
		goto out_free;

	index = ida_alloc(&sd_index_ida, GFP_KERNEL);
	if (index < 0) {
		sdev_printk(KERN_WARNING, sdp, "sd_probe: memory exhausted.\n");
		goto out_put;
	}

	error = sd_format_disk_name("sd", index, gd->disk_name, DISK_NAME_LEN);
	if (error) {
		sdev_printk(KERN_WARNING, sdp, "SCSI disk (sd) name length exceeded.\n");
		goto out_free_index;
	}

	sdkp->device = sdp;
	sdkp->driver = &sd_template;
	sdkp->disk = gd;
	sdkp->index = index;
	atomic_set(&sdkp->openers, 0);
	atomic_set(&sdkp->device->ioerr_cnt, 0);

	if (!sdp->request_queue->rq_timeout) {
		if (sdp->type != TYPE_MOD)
			blk_queue_rq_timeout(sdp->request_queue, SD_TIMEOUT);
		else
			blk_queue_rq_timeout(sdp->request_queue,
					     SD_MOD_TIMEOUT);
	}

	device_initialize(&sdkp->dev);
	sdkp->dev.parent = dev;
	sdkp->dev.class = &sd_disk_class;
	dev_set_name(&sdkp->dev, "%s", dev_name(dev));

	error = device_add(&sdkp->dev);
	if (error)
		goto out_free_index;

	get_device(dev);
	dev_set_drvdata(dev, sdkp);

	gd->major = sd_major((index & 0xf0) >> 4);
	gd->first_minor = ((index & 0xf) << 4) | (index & 0xfff00);

	gd->fops = &sd_fops;                                                                 // passthrough
	gd->private_data = &sdkp->driver;                                                    // 重点：在scsi_setup_fs_cmnd通过scsi_cmd_to_driver访问gd->private_data
	gd->queue = sdkp->device->request_queue;                                             // 重点：保存sdkp->device->request_queue到gd->queue

	/* defaults, until the device tells us otherwise */
	sdp->sector_size = 512;                                                              // 设置sdp->sector_size为512
	sdkp->capacity = 0;
	sdkp->media_present = 1;
	sdkp->write_prot = 0;
	sdkp->cache_override = 0;
	sdkp->WCE = 0;
	sdkp->RCD = 0;
	sdkp->ATO = 0;
	sdkp->first_scan = 1;
	sdkp->max_medium_access_timeouts = SD_MAX_MEDIUM_TIMEOUTS;

	sd_revalidate_disk(gd);

	gd->flags = GENHD_FL_EXT_DEVT;
	if (sdp->removable) {
		gd->flags |= GENHD_FL_REMOVABLE;
		gd->events |= DISK_EVENT_MEDIA_CHANGE;
		gd->event_flags = DISK_EVENT_FLAG_POLL | DISK_EVENT_FLAG_UEVENT;
	}

	blk_pm_runtime_init(sdp->request_queue, dev);
	device_add_disk(dev, gd, NULL);
	if (sdkp->capacity)
		sd_dif_config_host(sdkp);

	sd_revalidate_disk(gd);

	if (sdkp->security) {
		sdkp->opal_dev = init_opal_dev(sdp, &sd_sec_submit);
		if (sdkp->opal_dev)
			sd_printk(KERN_NOTICE, sdkp, "supports TCG Opal\n");                         // 打印提示信息
	}

	sd_printk(KERN_NOTICE, sdkp, "Attached SCSI %sdisk\n",
		  sdp->removable ? "removable " : "");                                           // 打印提示信息
	scsi_autopm_put_device(sdp);

	return 0;                                                                            // 返回0

 out_free_index:
	ida_free(&sd_index_ida, index);
 out_put:
	put_disk(gd);
 out_free:
	kfree(sdkp);
 out:
	scsi_autopm_put_device(sdp);
	return error;                                                                        // 返回error
}

/**
 * scsi_add_lun - allocate and fully initialze a scsi_device
 * @sdev:	holds information to be stored in the new scsi_device
 * @inq_result:	holds the result of a previous INQUIRY to the LUN
 * @bflags:	black/white list flag
 * @async:	1 if this device is being scanned asynchronously
 *
 * Description:
 *     Initialize the scsi_device @sdev.  Optionally set fields based
 *     on values in *@bflags.
 *
 * Return:
 *     SCSI_SCAN_NO_RESPONSE: could not allocate or setup a scsi_device
 *     SCSI_SCAN_LUN_PRESENT: a new scsi_device was allocated and initialized
 **/
static int scsi_add_lun(struct scsi_device *sdev, unsigned char *inq_result,
		blist_flags_t *bflags, int async)
{
	int ret;

	/*
	 * XXX do not save the inquiry, since it can change underneath us,
	 * save just vendor/model/rev.
	 *
	 * Rather than save it and have an ioctl that retrieves the saved
	 * value, have an ioctl that executes the same INQUIRY code used
	 * in scsi_probe_lun, let user level programs doing INQUIRY
	 * scanning run at their own risk, or supply a user level program
	 * that can correctly scan.
	 */

	/*
	 * Copy at least 36 bytes of INQUIRY data, so that we don't
	 * dereference unallocated memory when accessing the Vendor,
	 * Product, and Revision strings.  Badly behaved devices may set
	 * the INQUIRY Additional Length byte to a small value, indicating
	 * these strings are invalid, but often they contain plausible data
	 * nonetheless.  It doesn't matter if the device sent < 36 bytes
	 * total, since scsi_probe_lun() initializes inq_result with 0s.
	 */
	sdev->inquiry = kmemdup(inq_result,
				max_t(size_t, sdev->inquiry_len, 36),
				GFP_KERNEL);
	if (sdev->inquiry == NULL)
		return SCSI_SCAN_NO_RESPONSE;

	sdev->vendor = (char *) (sdev->inquiry + 8);
	sdev->model = (char *) (sdev->inquiry + 16);
	sdev->rev = (char *) (sdev->inquiry + 32);

	if (strncmp(sdev->vendor, "ATA     ", 8) == 0) {
		/*
		 * sata emulation layer device.  This is a hack to work around
		 * the SATL power management specifications which state that
		 * when the SATL detects the device has gone into standby
		 * mode, it shall respond with NOT READY.
		 */
		sdev->allow_restart = 1;
	}

	if (*bflags & BLIST_ISROM) {
		sdev->type = TYPE_ROM;
		sdev->removable = 1;
	} else {
		sdev->type = (inq_result[0] & 0x1f);
		sdev->removable = (inq_result[1] & 0x80) >> 7;

		/*
		 * some devices may respond with wrong type for
		 * well-known logical units. Force well-known type
		 * to enumerate them correctly.
		 */
		if (scsi_is_wlun(sdev->lun) && sdev->type != TYPE_WLUN) {
			sdev_printk(KERN_WARNING, sdev,
				"%s: correcting incorrect peripheral device type 0x%x for W-LUN 0x%16xhN\n",
				__func__, sdev->type, (unsigned int)sdev->lun);
			sdev->type = TYPE_WLUN;
		}

	}

	if (sdev->type == TYPE_RBC || sdev->type == TYPE_ROM) {
		/* RBC and MMC devices can return SCSI-3 compliance and yet
		 * still not support REPORT LUNS, so make them act as
		 * BLIST_NOREPORTLUN unless BLIST_REPORTLUN2 is
		 * specifically set */
		if ((*bflags & BLIST_REPORTLUN2) == 0)
			*bflags |= BLIST_NOREPORTLUN;
	}

	/*
	 * For a peripheral qualifier (PQ) value of 1 (001b), the SCSI
	 * spec says: The device server is capable of supporting the
	 * specified peripheral device type on this logical unit. However,
	 * the physical device is not currently connected to this logical
	 * unit.
	 *
	 * The above is vague, as it implies that we could treat 001 and
	 * 011 the same. Stay compatible with previous code, and create a
	 * scsi_device for a PQ of 1
	 *
	 * Don't set the device offline here; rather let the upper
	 * level drivers eval the PQ to decide whether they should
	 * attach. So remove ((inq_result[0] >> 5) & 7) == 1 check.
	 */ 

	sdev->inq_periph_qual = (inq_result[0] >> 5) & 7;
	sdev->lockable = sdev->removable;
	sdev->soft_reset = (inq_result[7] & 1) && ((inq_result[3] & 7) == 2);

	if (sdev->scsi_level >= SCSI_3 ||
			(sdev->inquiry_len > 56 && inq_result[56] & 0x04))
		sdev->ppr = 1;
	if (inq_result[7] & 0x60)
		sdev->wdtr = 1;
	if (inq_result[7] & 0x10)
		sdev->sdtr = 1;

	sdev_printk(KERN_NOTICE, sdev, "%s %.8s %.16s %.4s PQ: %d "
			"ANSI: %d%s\n", scsi_device_type(sdev->type),
			sdev->vendor, sdev->model, sdev->rev,
			sdev->inq_periph_qual, inq_result[2] & 0x07,
			(inq_result[3] & 0x0f) == 1 ? " CCS" : "");

	if ((sdev->scsi_level >= SCSI_2) && (inq_result[7] & 2) &&
	    !(*bflags & BLIST_NOTQ)) {
		sdev->tagged_supported = 1;
		sdev->simple_tags = 1;
	}

	/*
	 * Some devices (Texel CD ROM drives) have handshaking problems
	 * when used with the Seagate controllers. borken is initialized
	 * to 1, and then set it to 0 here.
	 */
	if ((*bflags & BLIST_BORKEN) == 0)
		sdev->borken = 0;

	if (*bflags & BLIST_NO_ULD_ATTACH)
		sdev->no_uld_attach = 1;

	/*
	 * Apparently some really broken devices (contrary to the SCSI
	 * standards) need to be selected without asserting ATN
	 */
	if (*bflags & BLIST_SELECT_NO_ATN)
		sdev->select_no_atn = 1;

	/*
	 * Maximum 512 sector transfer length
	 * broken RA4x00 Compaq Disk Array
	 */
	if (*bflags & BLIST_MAX_512)
		blk_queue_max_hw_sectors(sdev->request_queue, 512);
	/*
	 * Max 1024 sector transfer length for targets that report incorrect
	 * max/optimal lengths and relied on the old block layer safe default
	 */
	else if (*bflags & BLIST_MAX_1024)
		blk_queue_max_hw_sectors(sdev->request_queue, 1024);

	/*
	 * Some devices may not want to have a start command automatically
	 * issued when a device is added.
	 */
	if (*bflags & BLIST_NOSTARTONADD)
		sdev->no_start_on_add = 1;

	if (*bflags & BLIST_SINGLELUN)
		scsi_target(sdev)->single_lun = 1;

	sdev->use_10_for_rw = 1;                                                             // 重点：支持WRITE_10和READ_10命令

	/* some devices don't like REPORT SUPPORTED OPERATION CODES
	 * and will simply timeout causing sd_mod init to take a very
	 * very long time */
	if (*bflags & BLIST_NO_RSOC)
		sdev->no_report_opcodes = 1;

	/* set the device running here so that slave configure
	 * may do I/O */
	mutex_lock(&sdev->state_mutex);
	ret = scsi_device_set_state(sdev, SDEV_RUNNING);
	if (ret)
		ret = scsi_device_set_state(sdev, SDEV_BLOCK);
	mutex_unlock(&sdev->state_mutex);

	if (ret) {
		sdev_printk(KERN_ERR, sdev,
			    "in wrong state %s to complete scan\n",
			    scsi_device_state_name(sdev->sdev_state));
		return SCSI_SCAN_NO_RESPONSE;
	}

	if (*bflags & BLIST_NOT_LOCKABLE)
		sdev->lockable = 0;

	if (*bflags & BLIST_RETRY_HWERROR)
		sdev->retry_hwerror = 1;

	if (*bflags & BLIST_NO_DIF)
		sdev->no_dif = 1;

	if (*bflags & BLIST_UNMAP_LIMIT_WS)
		sdev->unmap_limit_for_ws = 1;

	sdev->eh_timeout = SCSI_DEFAULT_EH_TIMEOUT;

	if (*bflags & BLIST_TRY_VPD_PAGES)
		sdev->try_vpd_pages = 1;
	else if (*bflags & BLIST_SKIP_VPD_PAGES)
		sdev->skip_vpd_pages = 1;

	transport_configure_device(&sdev->sdev_gendev);

	if (sdev->host->hostt->slave_configure) {
		ret = sdev->host->hostt->slave_configure(sdev);
		if (ret) {
			/*
			 * if LLDD reports slave not present, don't clutter
			 * console with alloc failure messages
			 */
			if (ret != -ENXIO) {
				sdev_printk(KERN_ERR, sdev,
					"failed to configure device\n");
			}
			return SCSI_SCAN_NO_RESPONSE;
		}
	}

	if (sdev->scsi_level >= SCSI_3)
		scsi_attach_vpd(sdev);

	sdev->max_queue_depth = sdev->queue_depth;
	sdev->sdev_bflags = *bflags;

	/*
	 * Ok, the device is now all set up, we can
	 * register it and tell the rest of the kernel
	 * about it.
	 */
	if (!async && scsi_sysfs_add_sdev(sdev) != 0)
		return SCSI_SCAN_NO_RESPONSE;

	return SCSI_SCAN_LUN_PRESENT;
}



===========================================================================================================================================

/**
 * scsi_eh_lock_door - Prevent medium removal for the specified device
 * @sdev:	SCSI device to prevent medium removal
 *
 * Locking:
 * 	We must be called from process context.
 *
 * Notes:
 * 	We queue up an asynchronous "ALLOW MEDIUM REMOVAL" request on the
 * 	head of the devices request queue, and continue.
 */
static void scsi_eh_lock_door(struct scsi_device *sdev)                                  // REQ_OP_SCSI_IN
{
	struct request *req;
	struct scsi_request *rq;

	req = blk_get_request(sdev->request_queue, REQ_OP_SCSI_IN, 0);
	if (IS_ERR(req))
		return;
	rq = scsi_req(req);                                                                 // rq + 1

	rq->cmd[0] = ALLOW_MEDIUM_REMOVAL;
	rq->cmd[1] = 0;
	rq->cmd[2] = 0;
	rq->cmd[3] = 0;
	rq->cmd[4] = SCSI_REMOVAL_PREVENT;
	rq->cmd[5] = 0;
	rq->cmd_len = COMMAND_SIZE(rq->cmd[0]);

	req->rq_flags |= RQF_QUIET;
	req->timeout = 10 * HZ;
	rq->retries = 5;

	blk_execute_rq_nowait(req->q, NULL, req, 1, eh_lock_door_done);
}

static const struct block_device_operations sd_fops = {
	.owner			= THIS_MODULE,
	.open			= sd_open,
	.release		= sd_release,
	.ioctl			= sd_ioctl,                                                          // REQ_OP_SCSI_IN
	.getgeo			= sd_getgeo,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= sd_compat_ioctl,
#endif
	.check_events		= sd_check_events,
	.revalidate_disk	= sd_revalidate_disk,
	.unlock_native_capacity	= sd_unlock_native_capacity,
	.report_zones		= sd_zbc_report_zones,
	.pr_ops			= &sd_pr_ops,
};

static const struct file_operations sg_fops = {
	.owner = THIS_MODULE,
	.read = sg_read,
	.write = sg_write,                                                                   // REQ_OP_SCSI_IN
	.poll = sg_poll,
	.unlocked_ioctl = sg_ioctl,                                                          // REQ_OP_SCSI_IN
#ifdef CONFIG_COMPAT
	.compat_ioctl = sg_compat_ioctl,
#endif
	.open = sg_open,
	.mmap = sg_mmap,
	.release = sg_release,
	.fasync = sg_fasync,
	.llseek = no_llseek,
};

/* Make sure any sense buffer is the correct size. */
#define scsi_execute(sdev, cmd, data_direction, buffer, bufflen, sense,	\
		     sshdr, timeout, retries, flags, rq_flags, resid)	\
({									\
	BUILD_BUG_ON((sense) != NULL &&					\
		     sizeof(sense) != SCSI_SENSE_BUFFERSIZE);		\
	__scsi_execute(sdev, cmd, data_direction, buffer, bufflen,	\
		       sense, sshdr, timeout, retries, flags, rq_flags,	\
		       resid);						\
})

/**
 * __scsi_execute - insert request and wait for the result
 * @sdev:	scsi device
 * @cmd:	scsi command
 * @data_direction: data direction
 * @buffer:	data buffer
 * @bufflen:	len of buffer
 * @sense:	optional sense buffer
 * @sshdr:	optional decoded sense header
 * @timeout:	request timeout in seconds
 * @retries:	number of times to retry request
 * @flags:	flags for ->cmd_flags
 * @rq_flags:	flags for ->rq_flags
 * @resid:	optional residual length
 *
 * Returns the scsi_cmnd result field if a command was executed, or a negative
 * Linux error code if we didn't get that far.
 */
int __scsi_execute(struct scsi_device *sdev, const unsigned char *cmd,
		 int data_direction, void *buffer, unsigned bufflen,
		 unsigned char *sense, struct scsi_sense_hdr *sshdr,
		 int timeout, int retries, u64 flags, req_flags_t rq_flags,
		 int *resid)                                                                     // REQ_OP_SCSI_IN
{
	struct request *req;
	struct scsi_request *rq;
	int ret = DRIVER_ERROR << 24;

	req = blk_get_request(sdev->request_queue,
			data_direction == DMA_TO_DEVICE ?
			REQ_OP_SCSI_OUT : REQ_OP_SCSI_IN, BLK_MQ_REQ_PREEMPT);                       // 重点
	if (IS_ERR(req))
		return ret;
	rq = scsi_req(req);                                                                  // rq + 1

	if (bufflen &&	blk_rq_map_kern(sdev->request_queue, req,
					buffer, bufflen, GFP_NOIO))
		goto out;

	rq->cmd_len = COMMAND_SIZE(cmd[0]);                                                  // 
	memcpy(rq->cmd, cmd, rq->cmd_len);
	rq->retries = retries;
	req->timeout = timeout;
	req->cmd_flags |= flags;
	req->rq_flags |= rq_flags | RQF_QUIET;

	/*
	 * head injection *required* here otherwise quiesce won't work
	 */
	blk_execute_rq(req->q, NULL, req, 1);

	/*
	 * Some devices (USB mass-storage in particular) may transfer
	 * garbage data together with a residue indicating that the data
	 * is invalid.  Prevent the garbage from being misinterpreted
	 * and prevent security leaks by zeroing out the excess data.
	 */
	if (unlikely(rq->resid_len > 0 && rq->resid_len <= bufflen))
		memset(buffer + (bufflen - rq->resid_len), 0, rq->resid_len);

	if (resid)
		*resid = rq->resid_len;
	if (sense && rq->sense_len)
		memcpy(sense, rq->sense, SCSI_SENSE_BUFFERSIZE);
	if (sshdr)
		scsi_normalize_sense(rq->sense, rq->sense_len, sshdr);
	ret = rq->result;
 out:
	blk_put_request(req);

	return ret;
}
EXPORT_SYMBOL(__scsi_execute);

static const struct blk_mq_ops scsi_mq_ops_no_commit = {                                 // 重点：参考scsi_mq_setup_tags，在ufshcd_driver_template中未定义commit_rqs
	.get_budget	= scsi_mq_get_budget,
	.put_budget	= scsi_mq_put_budget,
	.queue_rq	= scsi_queue_rq,                                                         // 重点
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

int scsi_mq_setup_tags(struct Scsi_Host *shost)
{
	unsigned int cmd_size, sgl_size;

	sgl_size = max_t(unsigned int, sizeof(struct scatterlist),
				scsi_mq_inline_sgl_size(shost));                                         // 重点：计算inline sgl_size为2 * sizeof(struct scatterlist)，inline sgl_size的含义是只在cmd中预分配的scatterlist空间大小
	cmd_size = sizeof(struct scsi_cmnd) + shost->hostt->cmd_size + sgl_size;             // 重点：计算在分配request时需要分配的额外空间，shost->hostt->cmd_size表示cmd中的私有数据大小，在ufshcd_driver_template中定义为0
	if (scsi_host_get_prot(shost))                                                       // 若shost->prot_capabilities不为0，Protection Information 
		cmd_size += sizeof(struct scsi_data_buffer) +
			sizeof(struct scatterlist) * SCSI_INLINE_PROT_SG_CNT;                        // 重点：计算在分配request时需要分配的额外空间，cmd->prot_sdb + 1就是cmd->prot_sdb->table.sgl

	memset(&shost->tag_set, 0, sizeof(shost->tag_set));
	if (shost->hostt->commit_rqs)                                                        // 重点：在ufshcd_driver_template中未定义commit_rqs
		shost->tag_set.ops = &scsi_mq_ops;
	else
		shost->tag_set.ops = &scsi_mq_ops_no_commit;                                     // 重点
	shost->tag_set.nr_hw_queues = shost->nr_hw_queues ? : 1;
	shost->tag_set.queue_depth = shost->can_queue;
	shost->tag_set.cmd_size = cmd_size;                                                  // 重点：在分配request时需要分配shost->tag_set.cmd_size大小的额外空间
	shost->tag_set.numa_node = NUMA_NO_NODE;
	shost->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	shost->tag_set.flags |=
		BLK_ALLOC_POLICY_TO_MQ_FLAG(shost->hostt->tag_alloc_policy);
	shost->tag_set.driver_data = shost;

	return blk_mq_alloc_tag_set(&shost->tag_set);
}

/* Size in bytes of the sg-list stored in the scsi-mq command-private data. */
static unsigned int scsi_mq_inline_sgl_size(struct Scsi_Host *shost)
{
	return min_t(unsigned int, shost->sg_tablesize, SCSI_INLINE_SG_CNT) *
		sizeof(struct scatterlist);                                                      // min_t(unsigned int, SG_CHUNK_SIZE, SCSI_INLINE_SG_CNT) * sizeof(struct scatterlist)
}

#define SG_ALL	SG_CHUNK_SIZE
/*
 * The maximum number of SG segments that we will put inside a
 * scatterlist (unless chaining is used). Should ideally fit inside a
 * single page, to avoid a higher order allocation.  We could define this
 * to SG_MAX_SINGLE_ALLOC to pack correctly at the highest order.  The
 * minimum value is 32
 */
#define SG_CHUNK_SIZE	128

/*
 * Size of integrity metadata is usually small, 1 inline sg should
 * cover normal cases.
 */
#ifdef CONFIG_ARCH_NO_SG_CHAIN
#define  SCSI_INLINE_PROT_SG_CNT  0
#define  SCSI_INLINE_SG_CNT  0
#else
#define  SCSI_INLINE_PROT_SG_CNT  1
#define  SCSI_INLINE_SG_CNT  2
#endif

static inline unsigned int scsi_host_get_prot(struct Scsi_Host *shost)
{
	return shost->prot_capabilities;                                                     // Protection Information 
}

static blk_status_t scsi_queue_rq(struct blk_mq_hw_ctx *hctx,
			 const struct blk_mq_queue_data *bd)
{
	struct request *req = bd->rq;                                                        // 取出req
	struct request_queue *q = req->q;                                                    // 取出q
	struct scsi_device *sdev = q->queuedata;                                             // 取出sdev
	struct Scsi_Host *shost = sdev->host;                                                // 取出shost
	struct scsi_cmnd *cmd = blk_mq_rq_to_pdu(req);                                       // 取出cmd，实际是取出预分配的cmd空间地址，在分配request时会额外分配cmd空间，参考scsi_mq_setup_tags和blk_mq_alloc_rqs
	blk_status_t ret;
	int reason;

	/*
	 * If the device is not in running state we will reject some or all
	 * commands.
	 */
	if (unlikely(sdev->sdev_state != SDEV_RUNNING)) {                                    // 若sdev->sdev_state不为SDEV_RUNNING
		ret = scsi_prep_state_check(sdev, req);                                          // 根据sdev->sdev_state和req->rq_flags确定返回值
		if (ret != BLK_STS_OK)                                                           // 若ret不为BLK_STS_OK
			goto out_put_budget;                                                         // 跳转至out_put_budget
	}

	ret = BLK_STS_RESOURCE;                                                              // 设置ret为BLK_STS_RESOURCE
	if (!scsi_target_queue_ready(shost, sdev))                                           // 
		goto out_put_budget;                                                             // 跳转至out_put_budget
	if (!scsi_host_queue_ready(q, shost, sdev))                                          // 
		goto out_dec_target_busy;                                                        // 跳转至out_dec_target_busy

	if (!(req->rq_flags & RQF_DONTPREP)) {                                               // 若!(req->rq_flags & RQF_DONTPREP)为true，表示需要prepare
		ret = scsi_mq_prep_fn(req);                                                      // 重点：构造scsi命令，包括sg table的分配、request和sg table的map、cmd成员的初始化等，并调用blk_mq_start_request
		if (ret != BLK_STS_OK)                                                           // 若ret不为BLK_STS_OK
			goto out_dec_host_busy;                                                      // 跳转至out_dec_host_busy
		req->rq_flags |= RQF_DONTPREP;                                                   // 添加RQF_DONTPREP到req->rq_flags，表示不再需要prepare cmd
	} else {                                                                             // 此分支不需要prepare
		clear_bit(SCMD_STATE_COMPLETE, &cmd->state);                                     // 清除cmd->state中的SCMD_STATE_COMPLETE位
		blk_mq_start_request(req);                                                       // 标记传输开始
	}

	cmd->flags &= SCMD_PRESERVED_FLAGS;                                                  // cmd->flags &= SCMD_PRESERVED_FLAGS
	if (sdev->simple_tags)                                                               // sdev->simple_tags
		cmd->flags |= SCMD_TAGGED;                                                       // cmd->flags |= SCMD_TAGGED
	if (bd->last)                                                                        // bd->last
		cmd->flags |= SCMD_LAST;                                                         // cmd->flags |= SCMD_LAST

	scsi_init_cmd_errh(cmd);                                                             // 
	cmd->scsi_done = scsi_mq_done;                                                       // cmd->scsi_done = scsi_mq_done

	reason = scsi_dispatch_cmd(cmd);                                                     // 
	if (reason) {                                                                        // 若reason不为0
		scsi_set_blocked(cmd, reason);                                                   // *
		ret = BLK_STS_RESOURCE;                                                          // 设置ret为BLK_STS_RESOURCE
		goto out_dec_host_busy;                                                          // 跳转至out_dec_host_busy
	}

	return BLK_STS_OK;                                                                   // 返回BLK_STS_OK

out_dec_host_busy:
	scsi_dec_host_busy(shost);
out_dec_target_busy:
	if (scsi_target(sdev)->can_queue > 0)
		atomic_dec(&scsi_target(sdev)->target_busy);
out_put_budget:
	scsi_mq_put_budget(hctx);
	switch (ret) {
	case BLK_STS_OK:
		break;
	case BLK_STS_RESOURCE:
		if (atomic_read(&sdev->device_busy) ||
		    scsi_device_blocked(sdev))
			ret = BLK_STS_DEV_RESOURCE;
		break;
	default:
		if (unlikely(!scsi_device_online(sdev)))
			scsi_req(req)->result = DID_NO_CONNECT << 16;
		else
			scsi_req(req)->result = DID_ERROR << 16;
		/*
		 * Make sure to release all allocated resources when
		 * we hit an error, as we will never see this command
		 * again.
		 */
		if (req->rq_flags & RQF_DONTPREP)
			scsi_mq_uninit_cmd(cmd);
		break;
	}
	return ret;                                                                          // 返回ret
}

static blk_status_t
scsi_prep_state_check(struct scsi_device *sdev, struct request *req)
{
	switch (sdev->sdev_state) {                                                          // 根据sdev->sdev_state确定返回值
	case SDEV_OFFLINE:                                                                   // SDEV_OFFLINE
	case SDEV_TRANSPORT_OFFLINE:                                                         // SDEV_TRANSPORT_OFFLINE
		/*
		 * If the device is offline we refuse to process any
		 * commands.  The device must be brought online
		 * before trying any recovery commands.
		 */
		sdev_printk(KERN_ERR, sdev,
			    "rejecting I/O to offline device\n");                                    // 打印提示信息
		return BLK_STS_IOERR;                                                            // 返回BLK_STS_IOERR
	case SDEV_DEL:                                                                       // SDEV_DEL
		/*
		 * If the device is fully deleted, we refuse to
		 * process any commands as well.
		 */
		sdev_printk(KERN_ERR, sdev,
			    "rejecting I/O to dead device\n");                                       // 打印提示信息
		return BLK_STS_IOERR;                                                            // 返回BLK_STS_IOERR
	case SDEV_BLOCK:                                                                     // SDEV_BLOCK
	case SDEV_CREATED_BLOCK:                                                             // SDEV_CREATED_BLOCK
		return BLK_STS_RESOURCE;
	case SDEV_QUIESCE:                                                                   // 返回SDEV_QUIESCE
		/*
		 * If the devices is blocked we defer normal commands.
		 */
		if (req && !(req->rq_flags & RQF_PREEMPT))                                       // 若req && !(req->rq_flags & RQF_PREEMPT)为true
			return BLK_STS_RESOURCE;                                                     // 返回BLK_STS_RESOURCE
		return BLK_STS_OK;                                                               // 返回BLK_STS_OK
	default:
		/*
		 * For any other not fully online state we only allow
		 * special commands.  In particular any user initiated
		 * command is not allowed.
		 */
		if (req && !(req->rq_flags & RQF_PREEMPT))                                       // 若req && !(req->rq_flags & RQF_PREEMPT)为true
			return BLK_STS_IOERR;                                                        // 返回BLK_STS_IOERR
		return BLK_STS_OK;                                                               // 返回BLK_STS_OK
	}
}

static blk_status_t scsi_mq_prep_fn(struct request *req)                                 // 构造scsi命令，包括sg table的分配、request和sg table的map、cmd成员的初始化等
{
	struct scsi_cmnd *cmd = blk_mq_rq_to_pdu(req);                                       // 取出cmd，实际是取出预分配的cmd空间地址，在分配request时会额外分配cmd空间，参考scsi_mq_setup_tags和blk_mq_alloc_rqs
	struct scsi_device *sdev = req->q->queuedata;                                        // 取出sdev
	struct Scsi_Host *shost = sdev->host;                                                // 取出shost
	struct scatterlist *sg;

	scsi_init_command(sdev, cmd);                                                        // 初始化cmd，最主要的工作是清空除cmd->req外的cmd空间（部分成员未清空），及初始化cmd->abort_work

	cmd->request = req;                                                                  // 保存req到cmd->request
	cmd->tag = req->tag;                                                                 // 设置cmd->tag为req->tag
	cmd->prot_op = SCSI_PROT_NORMAL;                                                     // 设置cmd->prot_op为SCSI_PROT_NORMAL

	sg = (void *)cmd + sizeof(struct scsi_cmnd) + shost->hostt->cmd_size;                // 重点：计算预分配的sg list地址
	cmd->sdb.table.sgl = sg;                                                             // 重点：保存sg到cmd->sdb.table.sgl，在scsi_setup_cmnd可能会重新分配sg table更新cmd->sdb.table.sgl

	if (scsi_host_get_prot(shost)) {                                                     // 若shost->prot_capabilities不为0，Protection Information 
		memset(cmd->prot_sdb, 0, sizeof(struct scsi_data_buffer));                       // 清空cmd->prot_sdb区域

		cmd->prot_sdb->table.sgl =
			(struct scatterlist *)(cmd->prot_sdb + 1);                                   // 获取cmd->prot_sdb->table.sgl
	}

	blk_mq_start_request(req);                                                           // 重点：标记传输的开始，这是必须的接口调用

	return scsi_setup_cmnd(sdev, req);                                                   // 构造scsi命令，包括sg table的分配、request和sg table的map、cmd成员的初始化等
}

/* Called after a request has been started. */
void scsi_init_command(struct scsi_device *dev, struct scsi_cmnd *cmd)
{
	void *buf = cmd->sense_buffer;                                                       // 备份cmd->sense_buffer到buf
	void *prot = cmd->prot_sdb;                                                          // 备份cmd->prot_sdb到prot
	struct request *rq = blk_mq_rq_from_pdu(cmd);                                        // 获取rq，在分配request时会额外分配cmd空间，因此根据cmd可以计算出rq地址，参考scsi_mq_setup_tags和blk_mq_alloc_rqs
	unsigned int flags = cmd->flags & SCMD_PRESERVED_FLAGS;                              // 保存cmd->flags & SCMD_PRESERVED_FLAGS到flags
	unsigned long jiffies_at_alloc;
	int retries;

	if (!blk_rq_is_scsi(rq) && !(flags & SCMD_INITIALIZED)) {
		flags |= SCMD_INITIALIZED;
		scsi_initialize_rq(rq);
	}

	jiffies_at_alloc = cmd->jiffies_at_alloc;                                            // 备份cmd->jiffies_at_alloc到jiffies_at_alloc
	retries = cmd->retries;                                                              // 备份cmd->retries到retries
	/* zero out the cmd, except for the embedded scsi_request */
	memset((char *)cmd + sizeof(cmd->req), 0,
		sizeof(*cmd) - sizeof(cmd->req) + dev->host->hostt->cmd_size);                   // 重点：清空除cmd->req外的cmd空间，dev->host->hostt->cmd_size表示cmd中的私有数据大小

	cmd->device = dev;                                                                   // 保存dev到cmd->device
	cmd->sense_buffer = buf;                                                             // 恢复cmd->sense_buffer
	cmd->prot_sdb = prot;                                                                // 恢复cmd->prot_sdb
	cmd->flags = flags;                                                                  // 设置cmd->flags为flags
	INIT_DELAYED_WORK(&cmd->abort_work, scmd_eh_abort_handler);                          // 初始化cmd->abort_work
	cmd->jiffies_at_alloc = jiffies_at_alloc;                                            // 恢复cmd->jiffies_at_alloc
	cmd->retries = retries;                                                              // 恢复cmd->retries

	scsi_add_cmd_to_list(cmd);                                                           // 
}

/*
 * Driver command data is immediately after the request. So subtract request
 * size to get back to the original request, add request size to get the PDU.
 */
static inline struct request *blk_mq_rq_from_pdu(void *pdu)
{
	return pdu - sizeof(struct request);
}

static blk_status_t scsi_setup_cmnd(struct scsi_device *sdev,
		struct request *req)
{
	struct scsi_cmnd *cmd = blk_mq_rq_to_pdu(req);                                       // 取出cmd，实际是取出预分配的cmd空间地址，在分配request时会额外分配cmd空间，参考scsi_mq_setup_tags和blk_mq_alloc_rqs

	if (!blk_rq_bytes(req))                                                              // 若rq->__data_len为0
		cmd->sc_data_direction = DMA_NONE;                                               // 重点：设置cmd->sc_data_direction为DMA_NONE
	else if (rq_data_dir(req) == WRITE)                                                  // (req->cmd_flags & REQ_OP_MASK) & 1 == WRITE
		cmd->sc_data_direction = DMA_TO_DEVICE;                                          // 重点：设置cmd->sc_data_direction为DMA_TO_DEVICE
	else
		cmd->sc_data_direction = DMA_FROM_DEVICE;                                        // 重点：设置cmd->sc_data_direction为DMA_FROM_DEVICE

	if (blk_rq_is_scsi(req))                                                             // req->cmd_flags & REQ_OP_MASK == REQ_OP_SCSI_IN || req->cmd_flags & REQ_OP_MASK == REQ_OP_SCSI_OUT
		return scsi_setup_scsi_cmnd(sdev, req);
	else
		return scsi_setup_fs_cmnd(sdev, req);
}

static blk_status_t scsi_setup_scsi_cmnd(struct scsi_device *sdev,
		struct request *req)
{
	struct scsi_cmnd *cmd = blk_mq_rq_to_pdu(req);                                       // 取出cmd，实际是取出预分配的cmd空间地址，在分配request时会额外分配cmd空间，参考scsi_mq_setup_tags和blk_mq_alloc_rqs

	/*
	 * Passthrough requests may transfer data, in which case they must
	 * a bio attached to them.  Or they might contain a SCSI command
	 * that does not transfer data, in which case they may optionally
	 * submit a request without an attached bio.
	 */
	if (req->bio) {                                                                      // 重点：注释有利于理解原理
		blk_status_t ret = scsi_init_io(cmd);                                            // SCSI I/O initialize function
		if (unlikely(ret != BLK_STS_OK))                                                 // 若ret不为BLK_STS_OK
			return ret;                                                                  // 返回ret
	} else {
		BUG_ON(blk_rq_bytes(req));                                                       // 自检，若rq->__data_len不为0，则报BUG

		memset(&cmd->sdb, 0, sizeof(cmd->sdb));                                          // 清空cmd->sdb区域
	}

	cmd->cmd_len = scsi_req(req)->cmd_len;                                               // 初始化cmd->cmd_len
	cmd->cmnd = scsi_req(req)->cmd;                                                      // 初始化cmd->cmnd
	cmd->transfersize = blk_rq_bytes(req);                                               // 初始化cmd->transfersize为req->__data_len，为所有bio的bio->bi_iter.bi_size总和
	cmd->allowed = scsi_req(req)->retries;                                               // 初始化cmd->allowed
	return BLK_STS_OK;                                                                   // 返回BLK_STS_OK
}

/*
 * Function:    scsi_init_io()
 *
 * Purpose:     SCSI I/O initialize function.
 *
 * Arguments:   cmd   - Command descriptor we wish to initialize
 *
 * Returns:     BLK_STS_OK on success
 *		BLK_STS_RESOURCE if the failure is retryable
 *		BLK_STS_IOERR if the failure is fatal
 */
blk_status_t scsi_init_io(struct scsi_cmnd *cmd)
{
	struct request *rq = cmd->request;                                                   // 取出rq
	blk_status_t ret;

	if (WARN_ON_ONCE(!blk_rq_nr_phys_segments(rq)))                                      // 自检警告，blk_rq_nr_phys_segments(rq)表示Number of physical segments as sent to the device
		return BLK_STS_IOERR;                                                            // 返回BLK_STS_IOERR

	ret = scsi_init_sgtable(rq, &cmd->sdb);                                              // 分配sg table保存到cmd->sdb.table.sgl中，并map a request to scatterlist, return number of sg entries setup
	if (ret)                                                                             // 若ret不为0
		return ret;                                                                      // 返回ret

	if (blk_integrity_rq(rq)) {                                                          // ？
		struct scsi_data_buffer *prot_sdb = cmd->prot_sdb;
		int ivecs, count;

		if (WARN_ON_ONCE(!prot_sdb)) {
			/*
			 * This can happen if someone (e.g. multipath)
			 * queues a command to a device on an adapter
			 * that does not support DIX.
			 */
			ret = BLK_STS_IOERR;
			goto out_free_sgtables;
		}

		ivecs = blk_rq_count_integrity_sg(rq->q, rq->bio);

		if (sg_alloc_table_chained(&prot_sdb->table, ivecs,
				prot_sdb->table.sgl,
				SCSI_INLINE_PROT_SG_CNT)) {                                              // 
			ret = BLK_STS_RESOURCE;
			goto out_free_sgtables;
		}

		count = blk_rq_map_integrity_sg(rq->q, rq->bio,
						prot_sdb->table.sgl);
		BUG_ON(count > ivecs);                                                           // 自检
		BUG_ON(count > queue_max_integrity_segments(rq->q));                             // 自检

		cmd->prot_sdb = prot_sdb;                                                        // 保存prot_sdb到cmd->prot_sdb中
		cmd->prot_sdb->table.nents = count;                                              // 更新cmd->prot_sdb->table.nents为count
	}

	return BLK_STS_OK;                                                                   // 返回BLK_STS_OK
out_free_sgtables:
	scsi_mq_free_sgtables(cmd);                                                          // 释放已分配的sg list
	return ret;                                                                          // 返回ret
}
EXPORT_SYMBOL(scsi_init_io);

/*
 * Number of physical segments as sent to the device.
 *
 * Normally this is the number of discontiguous data segments sent by the
 * submitter.  But for data-less command like discard we might have no
 * actual data segments submitted, but the driver might have to add it's
 * own special payload.  In that case we still return 1 here so that this
 * special payload will be mapped.
 */
static inline unsigned short blk_rq_nr_phys_segments(struct request *rq)
{
	if (rq->rq_flags & RQF_SPECIAL_PAYLOAD)                                              // 参考注释理解代码意图
		return 1;                                                                        // 返回1
	return rq->nr_phys_segments;                                                         // Number of physical segments as sent to the device，？在哪里设置的
}

static blk_status_t scsi_init_sgtable(struct request *req,
		struct scsi_data_buffer *sdb)
{
	int count;

	/*
	 * If sg table allocation fails, requeue request later.
	 */
	if (unlikely(sg_alloc_table_chained(&sdb->table,
			blk_rq_nr_phys_segments(req), sdb->table.sgl,
			SCSI_INLINE_SG_CNT)))                                                        // 重点：分配sg table，将地址将保存在sdb->table.sgl中，blk_rq_nr_phys_segments(req)表示Number of physical segments as sent to the device，此时的参数sdb->table.sgl为预分配的sg list，在分配后sdb->table.sgl将保存分配好的sg table地址，SCSI_INLINE_SG_CNT为预分配的sg list的sg entry数量，其值为2
		return BLK_STS_RESOURCE;                                                         // 返回BLK_STS_RESOURCE

	/* 
	 * Next, walk the list, and fill in the addresses and sizes of
	 * each segment.
	 */
	count = blk_rq_map_sg(req->q, req, sdb->table.sgl);                                  // map a request to scatterlist, return number of sg entries setup
	BUG_ON(count > sdb->table.nents);                                                    // 自检
	sdb->table.nents = count;                                                            // 更新sdb->table.nents为count
	sdb->length = blk_rq_payload_bytes(req);                                             // 参考接口注释理解接口意图
	return BLK_STS_OK;                                                                   // 返回BLK_STS_OK
}

/**
 * sg_alloc_table_chained - Allocate and chain SGLs in an sg table
 * @table:	The sg table header to use
 * @nents:	Number of entries in sg list
 * @first_chunk: first SGL
 * @nents_first_chunk: number of the SGL of @first_chunk
 *
 *  Description:
 *    Allocate and chain SGLs in an sg table. If @nents@ is larger than
 *    @nents_first_chunk a chained sg table will be setup. @first_chunk is
 *    ignored if nents_first_chunk <= 1 because user expects the SGL points
 *    non-chain SGL.
 *
 **/
int sg_alloc_table_chained(struct sg_table *table, int nents,
		struct scatterlist *first_chunk, unsigned nents_first_chunk)                     // 重点：为了更好地理解代码，需要区分sg table和sg list的概念，table->sgl用于存放数据的sg table，nents为要分配用于存放数据的sg entry的数量，first_chunk为预分配的sg list，nents_first_chunk为预分配的sg list的entry数量
{
	int ret;

	BUG_ON(!nents);                                                                      // 自检，要分配用于存放数据的sg entry数量为0则报BUG

	if (first_chunk && nents_first_chunk) {                                              // 若first_chunk不为NULL并且nents_first_chunk不为0，说明预分配的sg list是有效的
		if (nents <= nents_first_chunk) {                                                // 若nents <= nents_first_chunk为true，则将使用first_chunk作为sg table（table->sgl），不需要再分配sg entry了
			table->nents = table->orig_nents = nents;                                    // table->nents = table->orig_nents = nents，为用于存放数据的sg entry数量
			sg_init_table(table->sgl, nents);                                            // 清空table->sgl区域sizeof(*table->sgl) * nents大小空间，sgl[nents - 1]->page_link |= SG_END, sgl[nents - 1]->page_link &= ~SG_CHAIN
			return 0;                                                                    // 返回0
		}
	}

	/* User supposes that the 1st SGL includes real entry */
	if (nents_first_chunk <= 1) {                                                        // 重点：若nents > nents_first_chunk && nents_first_chunk <= 1为true则在接下来的分配中不使用预分配的sg list（因为chain entry要占用一个sg entry）
		first_chunk = NULL;                                                              // 设置first_chunk为NULL，这样在__sg_alloc_table将不会使用预分配的sg list
		nents_first_chunk = 0;                                                           // 设置nents_first_chunk为0，这样在__sg_alloc_table将不会使用预分配的sg list
	}

	ret = __sg_alloc_table(table, nents, SG_CHUNK_SIZE,
			       first_chunk, nents_first_chunk,
			       GFP_ATOMIC, sg_pool_alloc);                                           // 重点：分配sg table，将其保存到table->sgl，SG_CHUNK_SIZE（128）表示一个sg list最大容纳的sg entry数量，超过SG_CHUNK_SIZE的分配需要使用chain entry将sg list链接成sg table
	if (unlikely(ret))                                                                   // 若ret非0，表示分配sg table失败
		sg_free_table_chained(table, nents_first_chunk);                                 // 因为在分配sg table时可能有chain的情况，因此需要free已分配成功的sg list
	return ret;                                                                          // 返回ret
}
EXPORT_SYMBOL_GPL(sg_alloc_table_chained);

/**
 * sg_init_table - Initialize SG table
 * @sgl:	   The SG table
 * @nents:	   Number of entries in table
 *
 * Notes:
 *   If this is part of a chained sg table, sg_mark_end() should be
 *   used only on the last table part.
 *
 **/
void sg_init_table(struct scatterlist *sgl, unsigned int nents)
{
	memset(sgl, 0, sizeof(*sgl) * nents);                                                // 重点：清空sgl区域sizeof(*sgl) * nents大小空间
	sg_init_marker(sgl, nents);                                                          // sgl[nents - 1]->page_link |= SG_END, sgl[nents - 1]->page_link &= ~SG_CHAIN
}
EXPORT_SYMBOL(sg_init_table);

/**
 * __sg_alloc_table - Allocate and initialize an sg table with given allocator
 * @table:	The sg table header to use
 * @nents:	Number of entries in sg list
 * @max_ents:	The maximum number of entries the allocator returns per call
 * @nents_first_chunk: Number of entries int the (preallocated) first
 * 	scatterlist chunk, 0 means no such preallocated chunk provided by user
 * @gfp_mask:	GFP allocation mask
 * @alloc_fn:	Allocator to use
 *
 * Description:
 *   This function returns a @table @nents long. The allocator is
 *   defined to return scatterlist chunks of maximum size @max_ents.
 *   Thus if @nents is bigger than @max_ents, the scatterlists will be
 *   chained in units of @max_ents.
 *
 * Notes:
 *   If this function returns non-0 (eg failure), the caller must call
 *   __sg_free_table() to cleanup any leftover allocations.
 *
 **/
int __sg_alloc_table(struct sg_table *table, unsigned int nents,
		     unsigned int max_ents, struct scatterlist *first_chunk,
		     unsigned int nents_first_chunk, gfp_t gfp_mask,
		     sg_alloc_fn *alloc_fn)                                                      // 重点：table用于保存sg table信息，nents表示要分配的用于存储数据的sg entry数量，max_ents表示一个sg list支持的最大sg entry数量，first_chunk表示预分配的sg list，nents_first_chunk表示预分配的sg list的sg entry数量
{
	struct scatterlist *sg, *prv;                                                        // 重点：因为一个sg list的entry数量最多不能超过max_ents（128），如果nents超过max_ents，就需要申请多次，使用chain entry链接，前一次申请的sg list就用prv表示
	unsigned int left;                                                                   // 表示还需要分别配的sg entry数量
	unsigned curr_max_ents = nents_first_chunk ?: max_ents;                              // curr_max_ents表示当前（指这次循环）sg list支持的最大sg entry数量，nents_first_chunk不为0表示使用first_chunk作为第一个sg list，因此设置curr_max_ents为nents_first_chunk，否则将分配第一个sg list，因此要设置curr_max_ents为max_ents
	unsigned prv_max_ents;

	memset(table, 0, sizeof(*table));                                                    // 清空table

	if (nents == 0)                                                                      // 若nents为0
		return -EINVAL;                                                                  // 返回-EINVAL
#ifdef CONFIG_ARCH_NO_SG_CHAIN                                                           // 若定义了CONFIG_ARCH_NO_SG_CHAIN
	if (WARN_ON_ONCE(nents > max_ents))                                                  // 自检警告，CONFIG_ARCH_NO_SG_CHAIN表示不支持chain sg，因此nents不能超过max_ents
		return -EINVAL;                                                                  // 返回-EINVAL
#endif

	left = nents;                                                                        // 初始化left为nents，表示还要分配nents个sg
	prv = NULL;                                                                          // 初始化prv为NULL
	do {
		unsigned int sg_size, alloc_size = left;                                         // alloc_size表示分配的sg entry数量（包括chain entry，chain entry不存放数据），sg_size表示用于存放数据的sg entry数量

		if (alloc_size > curr_max_ents) {                                                // 要申请的sg entry数量大于本次循环支持的最大sg entry数量
			alloc_size = curr_max_ents;                                                  // 需要申请多次，本次申请curr_max_ents个sg entry
			sg_size = alloc_size - 1;                                                    // 考虑chain entry，存放数据的sg entry数量为alloc_size - 1，chain entry的page_link不再指向存储数据的页，而是指向下一个sg list
		} else
			sg_size = alloc_size;                                                        // 要申请的sg entry数量不大于本次循环支持的最大sg entry数量，表明一次可以分配完，不需要chain entry

		left -= sg_size;                                                                 // 更新还需要分配的sg entry数量，这是循环的退出条件

		if (first_chunk) {                                                               // 若first_chunk不为NULL
			sg = first_chunk;                                                            // 将使用first_chunk作为第一个sg表
			first_chunk = NULL;                                                          // 设置first_chunk为NULL，说明第一次循环使用预分配的first_chunk，后续循环将分配sg list
		} else {
			sg = alloc_fn(alloc_size, gfp_mask);                                         // 分配alloc_size个sg entry
		}
		if (unlikely(!sg)) {                                                             // 若sg为NULL，表示本次分配失败
			/*
			 * Adjust entry count to reflect that the last
			 * entry of the previous table won't be used for
			 * linkage.  Without this, sg_kfree() may get
			 * confused.
			 */
			if (prv)                                                                     // 若prv不为NULL，表示上次分配成功
				table->nents = ++table->orig_nents;                                      // table->nents和table->orig_nents的更新是每次sg_size，prv中最后一个用用chain的sg未被统计到这两个变量中，因此本次分配失败时要考虑进去，参考注释理解意图

 			return -ENOMEM;                                                              // 返回-ENOMEM
		}

		sg_init_table(sg, alloc_size);                                                   // 重点：清空sg区域sizeof(*sg) * nents大小空间，sg[nents - 1]->page_link |= SG_END, sgl[nents - 1]->page_link &= ~SG_CHAIN
		table->nents = table->orig_nents += sg_size;                                     // 重点：table->nents = table->orig_nents += sg_size，注意更新的大小为sg_size

		/*
		 * If this is the first mapping, assign the sg table header.
		 * If this is not the first mapping, chain previous part.
		 */
		if (prv)                                                                         // 若prv不为NULL
			sg_chain(prv, prv_max_ents, sg);                                             // 进行chain
		else
			table->sgl = sg;                                                             // 保存sg到table->sgl作为sg table，即若prv为NULL说明sg是第一个sg list，保存到到table->sgl作为sg table，其他sg list将通过chain entry链接到sg

		/*
		 * If no more entries after this one, mark the end
		 */
		if (!left)                                                                       // 若left为0说明不在需要分配，要标记sg[sg_size - 1]的flags
			sg_mark_end(&sg[sg_size - 1]);                                               // 重点：sg[nents - 1]->page_link |= SG_END, sg[nents - 1]->page_link &= ~SG_CHAIN

		prv = sg;                                                                        // 保存sg到prv
		prv_max_ents = curr_max_ents;                                                    // 保存curr_max_ents到prv_max_ents，用于计算下次循环中的alloc_size，也用于设置chain entry
		curr_max_ents = max_ents;                                                        // 除第一次循环使用预分配的sg list外，后续循环将分配sg list，因此设置curr_max_ents为max_ents为下次循环准备
	} while (left);                                                                      // left为0表示不需要继续分配，将结束循环

	return 0;                                                                            // 返回0
}
EXPORT_SYMBOL(__sg_alloc_table);

/**
 * sg_init_marker - Initialize markers in sg table
 * @sgl:	   The SG table
 * @nents:	   Number of entries in table
 *
 **/
static inline void sg_init_marker(struct scatterlist *sgl,
				  unsigned int nents)
{
	sg_mark_end(&sgl[nents - 1]);                                                        // sgl[nents - 1]->page_link |= SG_END, sgl[nents - 1]->page_link &= ~SG_CHAIN
}

/**
 * sg_mark_end - Mark the end of the scatterlist
 * @sg:		 SG entryScatterlist
 *
 * Description:
 *   Marks the passed in sg entry as the termination point for the sg
 *   table. A call to sg_next() on this entry will return NULL.
 *
 **/
static inline void sg_mark_end(struct scatterlist *sg)
{
	/*
	 * Set termination bit, clear potential chain bit
	 */
	sg->page_link |= SG_END;
	sg->page_link &= ~SG_CHAIN;
}

/**
 * sg_chain - Chain two sglists together
 * @prv:	First scatterlist
 * @prv_nents:	Number of entries in prv
 * @sgl:	Second scatterlist
 *
 * Description:
 *   Links @prv@ and @sgl@ together, to form a longer scatterlist.
 *
 **/
static inline void sg_chain(struct scatterlist *prv, unsigned int prv_nents,
			    struct scatterlist *sgl)
{
	/*
	 * offset and length are unused for chain entry.  Clear them.
	 */
	prv[prv_nents - 1].offset = 0;
	prv[prv_nents - 1].length = 0;

	/*
	 * Set lowest bit to indicate a link pointer, and make sure to clear
	 * the termination bit if it happens to be set.
	 */
	prv[prv_nents - 1].page_link = ((unsigned long) sgl | SG_CHAIN)
					& ~SG_END;
}

/*
 * map a request to scatterlist, return number of sg entries setup. Caller
 * must make sure sg can hold rq->nr_phys_segments entries
 */
int blk_rq_map_sg(struct request_queue *q, struct request *rq,
		  struct scatterlist *sglist)                                                    // 参考注释理解接口意图，在scsi_init_sgtable调用时传递进的sglist为sdb->table.sgl
{
	struct scatterlist *sg = NULL;                                                       // 重点：被初始化为NULL，sg用于迭代，记录迭代至的位置，sg为NULL表明这是第一次迭代
	int nsegs = 0;                                                                       // 初始化nsegs为0，number of sg entries setup

	if (rq->rq_flags & RQF_SPECIAL_PAYLOAD)                                              // 若rq->rq_flags & RQF_SPECIAL_PAYLOAD为true，则blk_rq_nr_phys_segments(rq)返回1
		nsegs = __blk_bvec_map_sg(rq->special_vec, sglist, &sg);                         // 重点：将rq->special_vec与sglist进行map，sg = blk_next_sg(&sg, sglist)，sg->page_link = sg->page_link & (SG_CHAIN | SG_END) | (unsigned long) rq->special_vec.bv_page，设置sg->offset为rq->special_vec.bv_offset，设置sg->length为rq->special_vec.bv_len，并返回1
	else if (rq->bio && bio_op(rq->bio) == REQ_OP_WRITE_SAME)                            // 若rq->bio && bio_op(rq->bio) == REQ_OP_WRITE_SAME为true
		nsegs = __blk_bvec_map_sg(bio_iovec(rq->bio), sglist, &sg);                      // 重点：sg = blk_next_sg(&sg, sglist)，sg->page_link = sg->page_link & (SG_CHAIN | SG_END) | (unsigned long) bio_iovec(rq->bio).bv_page，设置sg->offset为bio_iovec(rq->bio).bv_offset，设置sg->length为bio_iovec(rq->bio).bv_len，并返回1
		/*
		 * bio_iovec(rq->bio)宏展开后的结果：
		 * struct bio_vec {
		 * 	.bv_page	= (&bvec[rq->bio->bi_iter.bi_idx])->bv_page + ((&bvec[rq->bio->bi_iter.bi_idx])->bv_offset + rq->bio->bi_iter.bi_bvec_done) / PAGE_SIZE,
		 * 	.bv_len		= min_t(unsigned, min(rq->bio->bi_iter.bi_size, (&bvec[rq->bio->bi_iter.bi_idx])->bv_len - rq->bio->bi_iter.bi_bvec_done), PAGE_SIZE - (((&bvec[rq->bio->bi_iter.bi_idx])->bv_offset + rq->bio->bi_iter.bi_bvec_done) % PAGE_SIZE)),
		 * 	.bv_offset	= ((&bvec[rq->bio->bi_iter.bi_idx])->bv_offset + rq->bio->bi_iter.bi_bvec_done) % PAGE_SIZE,
		 * }
		 */
	else if (rq->bio)                                                                    // 若rq->bio不为NULL
		nsegs = __blk_bios_map_sg(q, rq->bio, sglist, &sg);                              // 迭代rq->bio单链表中的所有bvec，根据bvec信息设置sglist中的sg entry

	if (unlikely(rq->rq_flags & RQF_COPY_USER) &&
	    (blk_rq_bytes(rq) & q->dma_pad_mask)) {                                          // 若rq->rq_flags & RQF_COPY_USER && blk_rq_bytes(rq) & q->dma_pad_mask为true
		unsigned int pad_len =
			(q->dma_pad_mask & ~blk_rq_bytes(rq)) + 1;                                   // 计算pad_len

		sg->length += pad_len;                                                           // sg->length += pad_len
		rq->extra_len += pad_len;                                                        // rq->extra_len += pad_len
	}

	if (q->dma_drain_size && q->dma_drain_needed(rq)) {                                  // 若q->dma_drain_size && q->dma_drain_needed(rq)为true
		if (op_is_write(req_op(rq)))                                                     // 若op_is_write(req_op(rq))为true
			memset(q->dma_drain_buffer, 0, q->dma_drain_size);                           // 清空q->dma_drain_buffer空间

		sg_unmark_end(sg);                                                               // sg->page_link &= ~SG_END
		sg = sg_next(sg);                                                                // 返回sg下一个用于存储的sg entry
		sg_set_page(sg, virt_to_page(q->dma_drain_buffer),
			    q->dma_drain_size,
			    ((unsigned long)q->dma_drain_buffer) &
			    (PAGE_SIZE - 1));                                                        // 重点：sg->page_link = sg->page_link & (SG_CHAIN | SG_END) | (unsigned long) virt_to_page(q->dma_drain_buffer)，设置sg->offset为((unsigned long)q->dma_drain_buffer) & (PAGE_SIZE - 1)，设置sg->length为q->dma_drain_size
		nsegs++;
		rq->extra_len += q->dma_drain_size;                                              // 更新rq->extra_len
	}

	if (sg)                                                                              // 执行到这里，若sg不为NULL，表明这是最后一个sg
		sg_mark_end(sg);                                                                 // sg->page_link |= SG_END, sg->page_link &= ~SG_CHAIN

	/*
	 * Something must have been wrong if the figured number of
	 * segment is bigger than number of req's physical segments
	 */
	WARN_ON(nsegs > blk_rq_nr_phys_segments(rq));                                        // 若nsegs > the number of physical segments as sent to the device为true，则发出警告

	return nsegs;                                                                        // 返回nsegs，nsegs表示使用的用于存储数据的sg的数量，注意不包括chain sg，因为chain sg不存储数据，nsegs可能包含物理地址连续的段，这种情况是受max_segment_size的限制
}
EXPORT_SYMBOL(blk_rq_map_sg);

static int __blk_bios_map_sg(struct request_queue *q, struct bio *bio,
			     struct scatterlist *sglist,
			     struct scatterlist **sg)                                                // *sg用于迭代，记录迭代至的位置，bio表示bio单链表
{
	struct bio_vec uninitialized_var(bvec), bvprv = { NULL };                            // for gcc: #define uninitialized_var(x) x = x; it's a trick to suppress uninitialized variable warning without generating any code
	struct bvec_iter iter;                                                               // 用于迭代bio中的bvec
	int nsegs = 0;                                                                       // 初始化nsegs为0
	bool new_bio = false;                                                                // 初始化new_bio为false

	for_each_bio(bio) {                                                                  // for (; bio; bio = bio->bi_next)
		bio_for_each_bvec(bvec, bio, iter) {
			/*
			 * Only try to merge bvecs from two bios given we
			 * have done bio internal merge when adding pages
			 * to bio
			 */
			/* 
			 * 将bio_for_each_bvec(bvec, bio, iter)展开，结合bio_advance_iter的实现才比较好理解bvec和iter的更新原理：
			 *	for (iter = bio->bi_iter;
			 *		iter.bi_size &&
			 *		(bvec = struct bio_vec {
			 *				.bv_page	= (&bio->bi_io_vec[iter.bi_idx])->bv_page,
			 *				.bv_len		= min(iter.bi_size, (&bio->bi_io_vec[iter.bi_idx])->bv_len - iter.bi_bvec_done),
			 *				.bv_offset	= (&bio->bi_io_vec[iter.bi_idx])->bv_offset + iter.bi_bvec_done,
			 *			}, 1);
			 *		bio_advance_iter(bio, &iter, bvec.bv_len))
			 */

			if (new_bio &&
			    __blk_segment_map_sg_merge(q, &bvec, &bvprv, sg))                        // 若new_bio为true，则only try to merge bvecs into one sg if they are from two bios，merge成功返回true，失败返回false，进行普通处理
				goto next_bvec;                                                          // 跳转至next_bvec

			if (bvec.bv_offset + bvec.bv_len <= PAGE_SIZE)                               // 若bvec.bv_offset + bvec.bv_len <= PAGE_SIZE为true
				nsegs += __blk_bvec_map_sg(bvec, sglist, sg);                            // 重点：将bvec进行map，*sg = blk_next_sg(sg, sglist)，*sg->page_link = *sg->page_link & (SG_CHAIN | SG_END) | (unsigned long) bvec.bv_page，设置*sg->offset为bvec.bv_offset，设置*sg->length为bvec.bv_len，并返回1
			else
				nsegs += blk_bvec_map_sg(q, &bvec, sglist, sg);                          // 重点：对于多page要考虑max_segment_size，返回使用存放数据的sg entry数量
 next_bvec:
			new_bio = false;                                                             // 设置new_bio为false
		}
		if (likely(bio->bi_iter.bi_size)) {                                              // 若bio->bi_iter.bi_size不为0，可以看出会忽略不包含数据的bio
			bvprv = bvec;                                                                // 保存bvec到bvprv
			new_bio = true;                                                              // 设置new_bio为true
		}
	}

	return nsegs;                                                                        // 返回nsegs
}

static inline void bio_advance_iter(struct bio *bio, struct bvec_iter *iter,
				    unsigned bytes)
{
	iter->bi_sector += bytes >> 9;                                                       // bio数据在内存中可能不连续，单在磁盘中是连续的

	if (bio_no_advance_iter(bio))
		iter->bi_size -= bytes;
	else
		bvec_iter_advance(bio->bi_io_vec, iter, bytes);
		/* TODO: It is reasonable to complete bio with error here. */
}

static inline bool bio_no_advance_iter(struct bio *bio)
{
	return bio_op(bio) == REQ_OP_DISCARD ||
	       bio_op(bio) == REQ_OP_SECURE_ERASE ||
	       bio_op(bio) == REQ_OP_WRITE_SAME ||
	       bio_op(bio) == REQ_OP_WRITE_ZEROES;
}

static inline bool bvec_iter_advance(const struct bio_vec *bv,
		struct bvec_iter *iter, unsigned bytes)                                          // bytes: bvec.bv_len
{
	if (WARN_ONCE(bytes > iter->bi_size,
		     "Attempted to advance past end of bvec iter\n")) {                          // 自检警告
		iter->bi_size = 0;                                                               // 设置iter->bi_size为0
		return false;                                                                    // 返回false
	}

	while (bytes) {                                                                      // 若bytes不为0
		const struct bio_vec *cur = bv + iter->bi_idx;                                   // 计算当前的bvec指针，保存在cur中
		unsigned len = min3(bytes, iter->bi_size,
				    cur->bv_len - iter->bi_bvec_done);                                   // 

		bytes -= len;                                                                    // 重点：更新bytes，这是终止条件
		iter->bi_size -= len;                                                            // 重点：更新iter->bi_size，可以看出iter->bi_size的含义
		iter->bi_bvec_done += len;                                                       // 重点：更新iter->bi_bvec_done，可以看出iter->bi_bvec_done的含义

		if (iter->bi_bvec_done == cur->bv_len) {                                          // iter->bi_bvec_done与cur->bv_len相等表示cur处理结束，需要更新到下一个bvec
			iter->bi_bvec_done = 0;                                                       // 设置iter->bi_bvec_done为0
			iter->bi_idx++;                                                               // 更新到下一个bvec
		}
	}
	return true;                                                                          // 返回true
}

static inline int __blk_bvec_map_sg(struct bio_vec bv,
		struct scatterlist *sglist, struct scatterlist **sg)                             // *sg用于sg table迭代，记录迭代至的位置，*sg为NULL表明将进行第一步迭代
{
	*sg = blk_next_sg(sg, sglist);                                                       // 重点：返回*sg中下一个存放数据的sg entry，sg用于带回这个sg entry
	sg_set_page(*sg, bv.bv_page, bv.bv_len, bv.bv_offset);                               // 重点：*sg->page_link = *sg->page_link & (SG_CHAIN | SG_END) | (unsigned long) bv.bv_page，设置*sg->offset为bv.bv_offset，设置*sg->length为bv.bv_len
	return 1;                                                                            // 返回1
}

static inline struct scatterlist *blk_next_sg(struct scatterlist **sg,
		struct scatterlist *sglist)                                                      // sg用于迭代，在循环中记录迭代至的位置，为NULL表明迭代第一个g entry
{
	if (!*sg)                                                                            // 若*sg为NULL，表示迭代第一个sg entry
		return sglist;                                                                   // 返回sglist，表示第一个sg entry

	/*
	 * If the driver previously mapped a shorter list, we could see a
	 * termination bit prematurely unless it fully inits the sg table
	 * on each mapping. We KNOW that there must be more entries here
	 * or the driver would be buggy, so force clear the termination bit
	 * to avoid doing a full sg_init_table() in drivers for each command.
	 */
	sg_unmark_end(*sg);                                                                  // sg->page_link &= ~SG_END，参考注释理解代码意图
	return sg_next(*sg);                                                                 // 返回*sg下一个用于存储的sg entry
}

/**
 * sg_next - return the next scatterlist entry in a list
 * @sg:		The current sg entry
 *
 * Description:
 *   Usually the next entry will be @sg@ + 1, but if this sg element is part
 *   of a chained scatterlist, it could jump to the start of a new
 *   scatterlist array.
 *
 **/
struct scatterlist *sg_next(struct scatterlist *sg)
{
	if (sg_is_last(sg))                                                                  // 若sg为sg table中的最后一个sg
		return NULL;                                                                     // 返回NULL

	sg++;                                                                                // 取下一个sg
	if (unlikely(sg_is_chain(sg)))                                                       // 若sg是chain sg
		sg = sg_chain_ptr(sg);                                                           // 返回(struct scatterlist *) (sg->page_link & ~(SG_CHAIN | SG_END))，表示下一个sg，这个sg在下一个数组中

	return sg;                                                                           // 返回sg
}
EXPORT_SYMBOL(sg_next);

/**
 * sg_set_page - Set sg entry to point at given page
 * @sg:		 SG entry
 * @page:	 The page
 * @len:	 Length of data
 * @offset:	 Offset into page
 *
 * Description:
 *   Use this function to set an sg entry pointing at a page, never assign
 *   the page directly. We encode sg table information in the lower bits
 *   of the page pointer. See sg_page() for looking up the page belonging
 *   to an sg entry.
 *
 **/
static inline void sg_set_page(struct scatterlist *sg, struct page *page,
			       unsigned int len, unsigned int offset)
{
	sg_assign_page(sg, page);                                                            // sg->page_link = sg->page_link & (SG_CHAIN | SG_END) | (unsigned long) page
	sg->offset = offset;                                                                 // 设置sg->offset为offset
	sg->length = len;                                                                    // 设置sg->length为len
}

/**
 * sg_assign_page - Assign a given page to an SG entry
 * @sg:		    SG entry
 * @page:	    The page
 *
 * Description:
 *   Assign page to sg entry. Also see sg_set_page(), the most commonly used
 *   variant.
 *
 **/
static inline void sg_assign_page(struct scatterlist *sg, struct page *page)
{
	unsigned long page_link = sg->page_link & (SG_CHAIN | SG_END);                       // 保存sg->page_link的SG_CHAIN和SG_END位到page_link

	/*
	 * In order for the low bit stealing approach to work, pages
	 * must be aligned at a 32-bit boundary as a minimum.
	 */
	BUG_ON((unsigned long) page & (SG_CHAIN | SG_END));                                  // 自检，要求page必须32-bit（4-bytes）对齐
#ifdef CONFIG_DEBUG_SG
	BUG_ON(sg_is_chain(sg));                                                             // 自检，sg不应该是chain entry
#endif
	sg->page_link = page_link | (unsigned long) page;                                    // sg->page_link = sg->page_link & (SG_CHAIN | SG_END) | (unsigned long) page
}

static unsigned blk_bvec_map_sg(struct request_queue *q,
		struct bio_vec *bvec, struct scatterlist *sglist,
		struct scatterlist **sg)                                                         // 参考bvec_split_segs中计算*nsegs的方法，bvec_split_segs与blk_bvec_map_sg的计算方法一致
{
	unsigned nbytes = bvec->bv_len;                                                      // 设置nbytes为bvec->bv_len
	unsigned nsegs = 0, total = 0;                                                       // 设置nsegs为0，total为0

	while (nbytes > 0) {                                                                 // 若nbytes > 0为true
		unsigned offset = bvec->bv_offset + total;                                       // 重点：计算offset
		unsigned len = min(get_max_segment_size(q, offset), nbytes);                     // len = min(min((65536 - (0x10000 & offset)) + 1, (256 * 1024)), nbytes)
		struct page *page = bvec->bv_page;                                               // 取出page

		/*
		 * Unfortunately a fair number of drivers barf on scatterlists
		 * that have an offset larger than PAGE_SIZE, despite other
		 * subsystems dealing with that invariant just fine.  For now
		 * stick to the legacy format where we never present those from
		 * the block layer, but the code below should be removed once
		 * these offenders (mostly MMC/SD drivers) are fixed.
		 */
		page += (offset >> PAGE_SHIFT);                                                  // 重点：计算offset相对起始page所在的page地址
		offset &= ~PAGE_MASK;                                                            // 更新offset

		*sg = blk_next_sg(sg, sglist);                                                   // 重点：返回*sg中下一个存放数据的sg entry，sg用于带回这个sg entry
		sg_set_page(*sg, page, len, offset);                                             // 重点：*sg->page_link = *sg->page_link & (SG_CHAIN | SG_END) | (unsigned long) page，设置*sg->offset为offset，设置*sg->length为len

		total += len;                                                                    // 更新total
		nbytes -= len;                                                                   // 更新nbytes
		nsegs++;                                                                         // 更新nsegs
	}

	return nsegs;                                                                        // 返回nsegs
}

static unsigned get_max_segment_size(const struct request_queue *q,
				     unsigned offset)
{
	unsigned long mask = queue_segment_boundary(q);                                      // q->limits.max_segment_size, BLK_MAX_SEGMENT_SIZE = 65536

	/* default segment boundary mask means no boundary limit */
	if (mask == BLK_SEG_BOUNDARY_MASK)                                                   // 若mask为BLK_SEG_BOUNDARY_MASK，BLK_SEG_BOUNDARY_MASK = 0xFFFFFFFFUL
		return queue_max_segment_size(q);                                                // 返回q->limits.max_segment_size，#define PRDT_DATA_BYTE_COUNT_MAX (256 * 1024)

	return min_t(unsigned long, mask - (mask & offset) + 1,
		     queue_max_segment_size(q));                                                 // min((65536 - (0x10000 & offset)) + 1, (256 * 1024))
}

/**
 * blk_set_default_limits - reset limits to default values
 * @lim:  the queue_limits structure to reset
 *
 * Description:
 *   Returns a queue_limit struct to its default state.
 */
void blk_set_default_limits(struct queue_limits *lim)
{
	lim->max_segments = BLK_MAX_SEGMENTS;
	lim->max_discard_segments = 1;
	lim->max_integrity_segments = 0;
	lim->seg_boundary_mask = BLK_SEG_BOUNDARY_MASK;
	lim->virt_boundary_mask = 0;
	lim->max_segment_size = BLK_MAX_SEGMENT_SIZE;
	lim->max_sectors = lim->max_hw_sectors = BLK_SAFE_MAX_SECTORS;
	lim->max_dev_sectors = 0;
	lim->chunk_sectors = 0;
	lim->max_write_same_sectors = 0;
	lim->max_write_zeroes_sectors = 0;
	lim->max_discard_sectors = 0;
	lim->max_hw_discard_sectors = 0;
	lim->discard_granularity = 0;
	lim->discard_alignment = 0;
	lim->discard_misaligned = 0;
	lim->logical_block_size = lim->physical_block_size = lim->io_min = 512;
	lim->bounce_pfn = (unsigned long)(BLK_BOUNCE_ANY >> PAGE_SHIFT);
	lim->alignment_offset = 0;
	lim->io_opt = 0;
	lim->misaligned = 0;
	lim->zoned = BLK_ZONED_NONE;
}
EXPORT_SYMBOL(blk_set_default_limits);

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
void blk_queue_make_request(struct request_queue *q, make_request_fn *mfn)
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

static inline unsigned long queue_segment_boundary(const struct request_queue *q)
{
	return q->limits.seg_boundary_mask;
}

static inline unsigned int queue_max_segment_size(const struct request_queue *q)
{
	return q->limits.max_segment_size;                                                   // 返回q->limits.max_segment_size，#define PRDT_DATA_BYTE_COUNT_MAX (256 * 1024)
}

/* only try to merge bvecs into one sg if they are from two bios */
static inline bool
__blk_segment_map_sg_merge(struct request_queue *q, struct bio_vec *bvec,
			   struct bio_vec *bvprv, struct scatterlist **sg)
{

	int nbytes = bvec->bv_len;                                                           // 取出nbytes

	if (!*sg)                                                                            // 若*sg为NULL，表示迭代第一个sg entry
		return false;                                                                    // 返回false

	if ((*sg)->length + nbytes > queue_max_segment_size(q))                              // (*sg)->length + nbytes > q->limits.max_segment_size为true
		return false;                                                                    // 返回false

	if (!biovec_phys_mergeable(q, bvprv, bvec))                                          // 若不可合并
		return false;                                                                    // 返回false

	(*sg)->length += nbytes;                                                             // 重点：这里就是实际的合并操作，因为物理内存连续，因此只需要更新(*sg)->length即可

	return true;                                                                         // 返回true
}

/* The maximum length of the data byte count field in the PRDT is 256KB */
#define PRDT_DATA_BYTE_COUNT_MAX	(256 * 1024)

/*
 * Setup a normal block command.  These are simple request from filesystems
 * that still need to be translated to SCSI CDBs from the ULD.
 */
static blk_status_t scsi_setup_fs_cmnd(struct scsi_device *sdev,
		struct request *req)                                                             // 参考注释理解接口意图
{
	struct scsi_cmnd *cmd = blk_mq_rq_to_pdu(req);                                       // 取出cmd，实际是取出预分配的cmd空间地址，在分配request时会额外分配cmd空间，参考scsi_mq_setup_tags和blk_mq_alloc_rqs

	if (unlikely(sdev->handler && sdev->handler->prep_fn)) {                             // 若定义了sdev->handler->prep_fn
		blk_status_t ret = sdev->handler->prep_fn(sdev, req);                            // 调用sdev->handler->prep_fn
		if (ret != BLK_STS_OK)                                                           // 若ret不为BLK_STS_OK
			return ret;                                                                  // 返回ret
	}

	cmd->cmnd = scsi_req(req)->cmd = scsi_req(req)->__cmd;                               // 重点：cmd->cmnd = ((struct scsi_request *)(req + 1))->cmd = ((struct scsi_request *)(req + 1))->__cmd
	memset(cmd->cmnd, 0, BLK_MAX_CDB);                                                   // 清空cmd->cmnd空间，实际是((struct scsi_request *)(req + 1))->__cmd的空间
	return scsi_cmd_to_driver(cmd)->init_command(cmd);                                   // 调用(*(struct scsi_driver **)cmd->request->rq_disk->private_data)->init_command
}

/* make sure not to use it with passthrough commands */
static inline struct scsi_driver *scsi_cmd_to_driver(struct scsi_cmnd *cmd)
{
	return *(struct scsi_driver **)cmd->request->rq_disk->private_data;
}

static blk_status_t sd_init_command(struct scsi_cmnd *cmd)
{
	struct request *rq = cmd->request;                                                   // 取出rq

	switch (req_op(rq)) {                                                                // req->cmd_flags & REQ_OP_MASK
	case REQ_OP_DISCARD:                                                                 // REQ_OP_DISCARD
		switch (scsi_disk(rq->rq_disk)->provisioning_mode) {                             // (container_of(rq->rq_disk->private_data, struct scsi_disk, driver))->provisioning_mode
		case SD_LBP_UNMAP:                                                               // SD_LBP_UNMAP
			return sd_setup_unmap_cmnd(cmd);                                             // *
		case SD_LBP_WS16:                                                                // SD_LBP_WS16
			return sd_setup_write_same16_cmnd(cmd, true);                                // *
		case SD_LBP_WS10:                                                                // SD_LBP_WS10
			return sd_setup_write_same10_cmnd(cmd, true);                                // *
		case SD_LBP_ZERO:                                                                // SD_LBP_ZERO
			return sd_setup_write_same10_cmnd(cmd, false);                               // *
		default:
			return BLK_STS_TARGET;                                                       // 返回BLK_STS_TARGET
		}
	case REQ_OP_WRITE_ZEROES:                                                            // REQ_OP_WRITE_ZEROES
		return sd_setup_write_zeroes_cmnd(cmd);                                          // *
	case REQ_OP_WRITE_SAME:                                                              // REQ_OP_WRITE_SAME
		return sd_setup_write_same_cmnd(cmd);                                            // *
	case REQ_OP_FLUSH:                                                                   // REQ_OP_FLUSH
		return sd_setup_flush_cmnd(cmd);                                                 // *
	case REQ_OP_READ:                                                                    // REQ_OP_READ
	case REQ_OP_WRITE:                                                                   // REQ_OP_WRITE
		return sd_setup_read_write_cmnd(cmd);                                            // *
	case REQ_OP_ZONE_RESET:                                                              // REQ_OP_ZONE_RESET
		return sd_zbc_setup_reset_cmnd(cmd, false);                                      // *
	case REQ_OP_ZONE_RESET_ALL:                                                          // REQ_OP_ZONE_RESET_ALL
		return sd_zbc_setup_reset_cmnd(cmd, true);                                       // *
	default:
		WARN_ON_ONCE(1);                                                                 // 自检警告
		return BLK_STS_NOTSUPP;                                                          // 返回BLK_STS_NOTSUPP
	}
}

static blk_status_t sd_setup_unmap_cmnd(struct scsi_cmnd *cmd)
{
	struct scsi_device *sdp = cmd->device;
	struct request *rq = cmd->request;
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));                                   // rq->__sector >> (ilog2(sdp->sector_size) - 9) = blk_rq_pos(rq) >> 0 = blk_rq_pos(rq)
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));                         // rq->__data_len >> SECTOR_SHIFT >> (ilog2(sdp->sector_size) - 9) = blk_rq_sectors(rq) >> 0 = blk_rq_sectors(rq)
	unsigned int data_len = 24;
	char *buf;

	rq->special_vec.bv_page = mempool_alloc(sd_page_pool, GFP_ATOMIC);
	if (!rq->special_vec.bv_page)
		return BLK_STS_RESOURCE;
	clear_highpage(rq->special_vec.bv_page);
	rq->special_vec.bv_offset = 0;
	rq->special_vec.bv_len = data_len;
	rq->rq_flags |= RQF_SPECIAL_PAYLOAD;

	cmd->cmd_len = 10;
	cmd->cmnd[0] = UNMAP;
	cmd->cmnd[8] = 24;

	buf = page_address(rq->special_vec.bv_page);
	put_unaligned_be16(6 + 16, &buf[0]);
	put_unaligned_be16(16, &buf[2]);
	put_unaligned_be64(lba, &buf[8]);
	put_unaligned_be32(nr_blocks, &buf[16]);

	cmd->allowed = SD_MAX_RETRIES;
	cmd->transfersize = data_len;
	rq->timeout = SD_TIMEOUT;

	return scsi_init_io(cmd);
}

static blk_status_t sd_setup_write_same16_cmnd(struct scsi_cmnd *cmd,
		bool unmap)
{
	struct scsi_device *sdp = cmd->device;
	struct request *rq = cmd->request;
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));
	u32 data_len = sdp->sector_size;

	rq->special_vec.bv_page = mempool_alloc(sd_page_pool, GFP_ATOMIC);
	if (!rq->special_vec.bv_page)
		return BLK_STS_RESOURCE;
	clear_highpage(rq->special_vec.bv_page);
	rq->special_vec.bv_offset = 0;
	rq->special_vec.bv_len = data_len;
	rq->rq_flags |= RQF_SPECIAL_PAYLOAD;

	cmd->cmd_len = 16;
	cmd->cmnd[0] = WRITE_SAME_16;
	if (unmap)
		cmd->cmnd[1] = 0x8; /* UNMAP */
	put_unaligned_be64(lba, &cmd->cmnd[2]);
	put_unaligned_be32(nr_blocks, &cmd->cmnd[10]);

	cmd->allowed = SD_MAX_RETRIES;
	cmd->transfersize = data_len;
	rq->timeout = unmap ? SD_TIMEOUT : SD_WRITE_SAME_TIMEOUT;

	return scsi_init_io(cmd);
}

static blk_status_t sd_setup_write_same10_cmnd(struct scsi_cmnd *cmd,
		bool unmap)
{
	struct scsi_device *sdp = cmd->device;
	struct request *rq = cmd->request;
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));
	u32 data_len = sdp->sector_size;

	rq->special_vec.bv_page = mempool_alloc(sd_page_pool, GFP_ATOMIC);
	if (!rq->special_vec.bv_page)
		return BLK_STS_RESOURCE;
	clear_highpage(rq->special_vec.bv_page);
	rq->special_vec.bv_offset = 0;
	rq->special_vec.bv_len = data_len;
	rq->rq_flags |= RQF_SPECIAL_PAYLOAD;

	cmd->cmd_len = 10;
	cmd->cmnd[0] = WRITE_SAME;
	if (unmap)
		cmd->cmnd[1] = 0x8; /* UNMAP */
	put_unaligned_be32(lba, &cmd->cmnd[2]);
	put_unaligned_be16(nr_blocks, &cmd->cmnd[7]);

	cmd->allowed = SD_MAX_RETRIES;
	cmd->transfersize = data_len;
	rq->timeout = unmap ? SD_TIMEOUT : SD_WRITE_SAME_TIMEOUT;

	return scsi_init_io(cmd);
}

static blk_status_t sd_setup_write_zeroes_cmnd(struct scsi_cmnd *cmd)
{
	struct request *rq = cmd->request;
	struct scsi_device *sdp = cmd->device;
	struct scsi_disk *sdkp = scsi_disk(rq->rq_disk);
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));

	if (!(rq->cmd_flags & REQ_NOUNMAP)) {
		switch (sdkp->zeroing_mode) {
		case SD_ZERO_WS16_UNMAP:
			return sd_setup_write_same16_cmnd(cmd, true);
		case SD_ZERO_WS10_UNMAP:
			return sd_setup_write_same10_cmnd(cmd, true);
		}
	}

	if (sdp->no_write_same)
		return BLK_STS_TARGET;

	if (sdkp->ws16 || lba > 0xffffffff || nr_blocks > 0xffff)
		return sd_setup_write_same16_cmnd(cmd, false);

	return sd_setup_write_same10_cmnd(cmd, false);
}

/**
 * sd_setup_write_same_cmnd - write the same data to multiple blocks
 * @cmd: command to prepare
 *
 * Will set up either WRITE SAME(10) or WRITE SAME(16) depending on
 * the preference indicated by the target device.
 **/
static blk_status_t sd_setup_write_same_cmnd(struct scsi_cmnd *cmd)
{
	struct request *rq = cmd->request;
	struct scsi_device *sdp = cmd->device;
	struct scsi_disk *sdkp = scsi_disk(rq->rq_disk);
	struct bio *bio = rq->bio;
	u64 lba = sectors_to_logical(sdp, blk_rq_pos(rq));
	u32 nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));
	blk_status_t ret;

	if (sdkp->device->no_write_same)
		return BLK_STS_TARGET;

	BUG_ON(bio_offset(bio) || bio_iovec(bio).bv_len != sdp->sector_size);

	rq->timeout = SD_WRITE_SAME_TIMEOUT;

	if (sdkp->ws16 || lba > 0xffffffff || nr_blocks > 0xffff) {
		cmd->cmd_len = 16;
		cmd->cmnd[0] = WRITE_SAME_16;
		put_unaligned_be64(lba, &cmd->cmnd[2]);
		put_unaligned_be32(nr_blocks, &cmd->cmnd[10]);
	} else {
		cmd->cmd_len = 10;
		cmd->cmnd[0] = WRITE_SAME;
		put_unaligned_be32(lba, &cmd->cmnd[2]);
		put_unaligned_be16(nr_blocks, &cmd->cmnd[7]);
	}

	cmd->transfersize = sdp->sector_size;
	cmd->allowed = SD_MAX_RETRIES;

	/*
	 * For WRITE SAME the data transferred via the DATA OUT buffer is
	 * different from the amount of data actually written to the target.
	 *
	 * We set up __data_len to the amount of data transferred via the
	 * DATA OUT buffer so that blk_rq_map_sg sets up the proper S/G list
	 * to transfer a single sector of data first, but then reset it to
	 * the amount of data to be written right after so that the I/O path
	 * knows how much to actually write.
	 */
	rq->__data_len = sdp->sector_size;
	ret = scsi_init_io(cmd);
	rq->__data_len = blk_rq_bytes(rq);

	return ret;
}

static blk_status_t sd_setup_flush_cmnd(struct scsi_cmnd *cmd)
{
	struct request *rq = cmd->request;

	/* flush requests don't perform I/O, zero the S/G table */
	memset(&cmd->sdb, 0, sizeof(cmd->sdb));

	cmd->cmnd[0] = SYNCHRONIZE_CACHE;
	cmd->cmd_len = 10;
	cmd->transfersize = 0;
	cmd->allowed = SD_MAX_RETRIES;

	rq->timeout = rq->q->rq_timeout * SD_FLUSH_TIMEOUT_MULTIPLIER;
	return BLK_STS_OK;
}

static blk_status_t sd_setup_read_write_cmnd(struct scsi_cmnd *cmd)
{
	struct request *rq = cmd->request;                                                   // 取出rq
	struct scsi_device *sdp = cmd->device;                                               // 取出sdp
	struct scsi_disk *sdkp = scsi_disk(rq->rq_disk);                                     // 取出sdkp，sdkp = container_of(rq->rq_disk->private_data, struct scsi_disk, driver)
	sector_t lba = sectors_to_logical(sdp, blk_rq_pos(rq));                              // rq->__sector >> (ilog2(sdp->sector_size) - 9) = rq->__sector >> 0 = rq->__sector
	sector_t threshold;
	unsigned int nr_blocks = sectors_to_logical(sdp, blk_rq_sectors(rq));                // (rq->__data_len >> SECTOR_SHIFT) >> (ilog2(sdp->sector_size) - 9) = (rq->__data_len >> SECTOR_SHIFT) >> 0 = rq->__data_len >> SECTOR_SHIFT
	unsigned int mask = logical_to_sectors(sdp, 1) - 1;                                  // 计算mask
	bool write = rq_data_dir(rq) == WRITE;                                               // (req->cmd_flags & REQ_OP_MASK) & 1 == WRITE
	unsigned char protect, fua;
	blk_status_t ret;
	unsigned int dif;
	bool dix;

	ret = scsi_init_io(cmd);                                                             // 重点：分配sg table，并map bio中的bvec到sg table中的sg entry
	if (ret != BLK_STS_OK)                                                               // 若ret不为BLK_STS_OK
		return ret;                                                                      // 返回ret

	if (!scsi_device_online(sdp) || sdp->changed) {                                      // checks for positions of the SCSI state machine, etc
		scmd_printk(KERN_ERR, cmd, "device offline or changed\n");                       // 打印提示信息
		return BLK_STS_IOERR;                                                            // 返回BLK_STS_IOERR
	}

	if (blk_rq_pos(rq) + blk_rq_sectors(rq) > get_capacity(rq->rq_disk)) {               // rq->__sector + rq->__data_len >> SECTOR_SHIFT > rq->rq_disk->part0.nr_sects
		scmd_printk(KERN_ERR, cmd, "access beyond end of device\n");                     // 打印提示信息
		return BLK_STS_IOERR;                                                            // 返回BLK_STS_IOERR
	}

	if ((blk_rq_pos(rq) & mask) || (blk_rq_sectors(rq) & mask)) {                        // rq->__sector & mask || rq->__data_len & mask
		scmd_printk(KERN_ERR, cmd, "request not aligned to the logical block size\n");   // 打印提示信息
		return BLK_STS_IOERR;                                                            // BLK_STS_IOERR
	}

	/*
	 * Some SD card readers can't handle accesses which touch the
	 * last one or two logical blocks. Split accesses as needed.
	 */
	threshold = sdkp->capacity - SD_LAST_BUGGY_SECTORS;                                  // threshold = sdkp->capacity - SD_LAST_BUGGY_SECTORS

	if (unlikely(sdp->last_sector_bug && lba + nr_blocks > threshold)) {                 // sdp->last_sector_bug && lba + nr_blocks > threshold
		if (lba < threshold) {                                                           // lba < threshold
			/* Access up to the threshold but not beyond */
			nr_blocks = threshold - lba;                                                 // nr_blocks = threshold - lba
		} else {
			/* Access only a single logical block */
			nr_blocks = 1;                                                               // nr_blocks = 1
		}
	}

	fua = rq->cmd_flags & REQ_FUA ? 0x8 : 0;                                             // fua = rq->cmd_flags & REQ_FUA ? 0x8 : 0
	dix = scsi_prot_sg_count(cmd);                                                       // dix = cmd->prot_sdb ? cmd->prot_sdb->table.nents : 0
	dif = scsi_host_dif_capable(cmd->device->host, sdkp->protection_type);               // shost->prot_capabilities & cap[target_type] ? target_type : 0

	if (dif || dix)                                                                      // 若dif || dix为true
		protect = sd_setup_protect_cmnd(cmd, dix, dif);                                  // setup protect cmnd
	else
		protect = 0;                                                                     // 设置protect为0

	if (protect && sdkp->protection_type == T10_PI_TYPE2_PROTECTION) {                   // 有条件地使用READ_32/WRITE_32
		ret = sd_setup_rw32_cmnd(cmd, write, lba, nr_blocks,
					 protect | fua);                                                     // setup rw32 cmnd
	} else if (sdp->use_16_for_rw || (nr_blocks > 0xffff)) {                             // 重点：相比于READ_10/WRITE_10和READ_6/WRITE_6，优先考虑READ_16/WRITE_16
		ret = sd_setup_rw16_cmnd(cmd, write, lba, nr_blocks,
					 protect | fua);                                                     // setup rw16 cmnd
	} else if ((nr_blocks > 0xff) || (lba > 0x1fffff) ||
		   sdp->use_10_for_rw || protect) {                                              // 重点：相比于READ_6/WRITE_6，优先考虑READ_10/WRITE_10
		ret = sd_setup_rw10_cmnd(cmd, write, lba, nr_blocks,
					 protect | fua);                                                     // setup rw10 cmnd
	} else {
		ret = sd_setup_rw6_cmnd(cmd, write, lba, nr_blocks,
					protect | fua);                                                      // setup rw6 cmnd
	}

	if (unlikely(ret != BLK_STS_OK))                                                     // 若ret不为BLK_STS_OK
		return ret;                                                                      // 返回ret

	/*
	 * We shouldn't disconnect in the middle of a sector, so with a dumb
	 * host adapter, it's safe to assume that we can at least transfer
	 * this many bytes between each connect / disconnect.
	 */
	cmd->transfersize = sdp->sector_size;                                                // 设置cmd->transfersize为sdp->sector_size，512
	cmd->underflow = nr_blocks << 9;                                                     // 设置cmd->underflow为nr_blocks << 9
	cmd->allowed = SD_MAX_RETRIES;                                                       // 设置cmd->allowed为SD_MAX_RETRIES
	cmd->sdb.length = nr_blocks * sdp->sector_size;                                      // 设置cmd->sdb.length为nr_blocks * sdp->sector_size

	SCSI_LOG_HLQUEUE(1,
			 scmd_printk(KERN_INFO, cmd,
				     "%s: block=%llu, count=%d\n", __func__,
				     (unsigned long long)blk_rq_pos(rq),
				     blk_rq_sectors(rq)));                                               // 打印提示信息
	SCSI_LOG_HLQUEUE(2,
			 scmd_printk(KERN_INFO, cmd,
				     "%s %d/%u 512 byte blocks.\n",
				     write ? "writing" : "reading", nr_blocks,
				     blk_rq_sectors(rq)));                                               // 打印提示信息

	/*
	 * This indicates that the command is ready from our end to be
	 * queued.
	 */
	return BLK_STS_OK;                                                                   // 返回BLK_STS_OK
}

static blk_status_t sd_setup_rw32_cmnd(struct scsi_cmnd *cmd, bool write,
				       sector_t lba, unsigned int nr_blocks,
				       unsigned char flags)
{
	cmd->cmnd = mempool_alloc(sd_cdb_pool, GFP_ATOMIC);
	if (unlikely(cmd->cmnd == NULL))
		return BLK_STS_RESOURCE;

	cmd->cmd_len = SD_EXT_CDB_SIZE;
	memset(cmd->cmnd, 0, cmd->cmd_len);

	cmd->cmnd[0]  = VARIABLE_LENGTH_CMD;
	cmd->cmnd[7]  = 0x18; /* Additional CDB len */
	cmd->cmnd[9]  = write ? WRITE_32 : READ_32;
	cmd->cmnd[10] = flags;
	put_unaligned_be64(lba, &cmd->cmnd[12]);
	put_unaligned_be32(lba, &cmd->cmnd[20]); /* Expected Indirect LBA */
	put_unaligned_be32(nr_blocks, &cmd->cmnd[28]);

	return BLK_STS_OK;
}

static blk_status_t sd_setup_rw16_cmnd(struct scsi_cmnd *cmd, bool write,
				       sector_t lba, unsigned int nr_blocks,
				       unsigned char flags)
{
	cmd->cmd_len  = 16;
	cmd->cmnd[0]  = write ? WRITE_16 : READ_16;
	cmd->cmnd[1]  = flags;
	cmd->cmnd[14] = 0;
	cmd->cmnd[15] = 0;
	put_unaligned_be64(lba, &cmd->cmnd[2]);
	put_unaligned_be32(nr_blocks, &cmd->cmnd[10]);

	return BLK_STS_OK;
}

static blk_status_t sd_setup_rw10_cmnd(struct scsi_cmnd *cmd, bool write,
				       sector_t lba, unsigned int nr_blocks,
				       unsigned char flags)
{
	cmd->cmd_len = 10;                                                                   // command length
	cmd->cmnd[0] = write ? WRITE_10 : READ_10;                                           // OPERATION CODE
	cmd->cmnd[1] = flags;                                                                // Miscellaneous CDB information/SERVICE ACTION (if required)
	cmd->cmnd[6] = 0;                                                                    // Miscellaneous CDB information
	cmd->cmnd[9] = 0;                                                                    // CONTROL
	put_unaligned_be32(lba, &cmd->cmnd[2]);                                              // LOGICAL BLOCK ADDRESS (if required)
	put_unaligned_be16(nr_blocks, &cmd->cmnd[7]);                                        // TRANSFER LENGTH (if required)/PARAMETER LIST LENGTH (if required)/ALLOCATION LENGTH (if required)

	return BLK_STS_OK;                                                                   // 返回BLK_STS_OK
}

/**
 * sd_zbc_setup_reset_cmnd - Prepare a RESET WRITE POINTER scsi command.
 * @cmd: the command to setup
 * @all: Reset all zones control.
 *
 * Called from sd_init_command() for a REQ_OP_ZONE_RESET request.
 */
blk_status_t sd_zbc_setup_reset_cmnd(struct scsi_cmnd *cmd, bool all)                    // 该接口的实现受宏限制
{
	struct request *rq = cmd->request;
	struct scsi_disk *sdkp = scsi_disk(rq->rq_disk);
	sector_t sector = blk_rq_pos(rq);
	sector_t block = sectors_to_logical(sdkp->device, sector);

	if (!sd_is_zoned(sdkp))
		/* Not a zoned device */
		return BLK_STS_IOERR;

	if (sdkp->device->changed)
		return BLK_STS_IOERR;

	if (sector & (sd_zbc_zone_sectors(sdkp) - 1))
		/* Unaligned request */
		return BLK_STS_IOERR;

	cmd->cmd_len = 16;
	memset(cmd->cmnd, 0, cmd->cmd_len);
	cmd->cmnd[0] = ZBC_OUT;
	cmd->cmnd[1] = ZO_RESET_WRITE_POINTER;
	if (all)
		cmd->cmnd[14] = 0x1;
	else
		put_unaligned_be64(block, &cmd->cmnd[2]);

	rq->timeout = SD_TIMEOUT;
	cmd->sc_data_direction = DMA_NONE;
	cmd->transfersize = 0;
	cmd->allowed = 0;

	return BLK_STS_OK;
}















/**
 * scsi_dispatch_command - Dispatch a command to the low-level driver.
 * @cmd: command block we are dispatching.
 *
 * Return: nonzero return request was rejected and device's queue needs to be
 * plugged.
 */
static int scsi_dispatch_cmd(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host = cmd->device->host;                                          // 取出host
	int rtn = 0;

	atomic_inc(&cmd->device->iorequest_cnt);                                             // increase cmd->device->iorequest_cnt

	/* check if the device is still usable */
	if (unlikely(cmd->device->sdev_state == SDEV_DEL)) {                                 // 若cmd->device->sdev_state为SDEV_DEL
		/* in SDEV_DEL we error all commands. DID_NO_CONNECT
		 * returns an immediate error upwards, and signals
		 * that the device is no longer present */
		cmd->result = DID_NO_CONNECT << 16;                                              // 设置cmd->result为DID_NO_CONNECT << 16
		goto done;                                                                       // 跳转至done
	}

	/* Check to see if the scsi lld made this device blocked. */
	if (unlikely(scsi_device_blocked(cmd->device))) {                                    // 
		/*
		 * in blocked state, the command is just put back on
		 * the device queue.  The suspend state has already
		 * blocked the queue so future requests should not
		 * occur until the device transitions out of the
		 * suspend state.
		 */
		SCSI_LOG_MLQUEUE(3, scmd_printk(KERN_INFO, cmd,
			"queuecommand : device blocked\n"));                                         // 打印提示信息
		return SCSI_MLQUEUE_DEVICE_BUSY;                                                 // 返回SCSI_MLQUEUE_DEVICE_BUSY
	}

	/* Store the LUN value in cmnd, if needed. */
	if (cmd->device->lun_in_cdb)                                                         // 若cmd->device->lun_in_cdb不为0
		cmd->cmnd[1] = (cmd->cmnd[1] & 0x1f) |
			       (cmd->device->lun << 5 & 0xe0);                                       // 

	scsi_log_send(cmd);                                                                  // *

	/*
	 * Before we queue this command, check if the command
	 * length exceeds what the host adapter can handle.
	 */
	if (cmd->cmd_len > cmd->device->host->max_cmd_len) {                                 // 
		SCSI_LOG_MLQUEUE(3, scmd_printk(KERN_INFO, cmd,
			       "queuecommand : command too long. "
			       "cdb_size=%d host->max_cmd_len=%d\n",
			       cmd->cmd_len, cmd->device->host->max_cmd_len));                       // 打印提示信息
		cmd->result = (DID_ABORT << 16);                                                 // 
		goto done;                                                                       // 跳转至done
	}

	if (unlikely(host->shost_state == SHOST_DEL)) {                                      // 
		cmd->result = (DID_NO_CONNECT << 16);                                            // 
		goto done;                                                                       // 跳转至done

	}

	trace_scsi_dispatch_cmd_start(cmd);                                                  // trace
	rtn = host->hostt->queuecommand(host, cmd);                                          // 
	if (rtn) {                                                                           // 若rtn不为0
		trace_scsi_dispatch_cmd_error(cmd, rtn);                                         // trace
		if (rtn != SCSI_MLQUEUE_DEVICE_BUSY &&
		    rtn != SCSI_MLQUEUE_TARGET_BUSY)                                             // 
			rtn = SCSI_MLQUEUE_HOST_BUSY;                                                // 

		SCSI_LOG_MLQUEUE(3, scmd_printk(KERN_INFO, cmd,
			"queuecommand : request rejected\n"));                                       // 打印提示信息
	}

	return rtn;                                                                          // 返回rtn
 done:
	cmd->scsi_done(cmd);                                                                 // 调用scsi_mq_done
	return 0;                                                                            // 返回0
}

1 =======================================================================================================================================

/**
 * struct utp_transfer_cmd_desc - UFS Command Descriptor structure
 * @command_upiu: Command UPIU Frame address
 * @response_upiu: Response UPIU Frame address
 * @prd_table: Physical Region Descriptor
 */
struct utp_transfer_cmd_desc {
	u8 command_upiu[ALIGNED_UPIU_SIZE];
	u8 response_upiu[ALIGNED_UPIU_SIZE];
	struct ufshcd_sg_entry    prd_table[SG_ALL];
};


/**
 * struct ufshcd_sg_entry - UFSHCI PRD Entry
 * @base_addr: Lower 32bit physical address DW-0
 * @upper_addr: Upper 32bit physical address DW-1
 * @reserved: Reserved for future use DW-2
 * @size: size of physical segment DW-3
 */
struct ufshcd_sg_entry {
	__le32    base_addr;
	__le32    upper_addr;
	__le32    reserved;
	__le32    size;
};

/**
 * struct utp_cmd_rsp - Response UPIU structure
 * @residual_transfer_count: Residual transfer count DW-3
 * @reserved: Reserved double words DW-4 to DW-7
 * @sense_data_len: Sense data length DW-8 U16
 * @sense_data: Sense data field DW-8 to DW-12
 */
struct utp_cmd_rsp {
	__be32 residual_transfer_count;
	__be32 reserved[4];
	__be16 sense_data_len;
	u8 sense_data[UFS_SENSE_SIZE];
};

/**
 * struct utp_upiu_header - UPIU header structure
 * @dword_0: UPIU header DW-0
 * @dword_1: UPIU header DW-1
 * @dword_2: UPIU header DW-2
 */
struct utp_upiu_header {
	__be32 dword_0;
	__be32 dword_1;
	__be32 dword_2;
};

/**
 * struct utp_upiu_query - upiu request buffer structure for
 * query request.
 * @opcode: command to perform B-0
 * @idn: a value that indicates the particular type of data B-1
 * @index: Index to further identify data B-2
 * @selector: Index to further identify data B-3
 * @reserved_osf: spec reserved field B-4,5
 * @length: number of descriptor bytes to read/write B-6,7
 * @value: Attribute value to be written DW-5
 * @reserved: spec reserved DW-6,7
 */
struct utp_upiu_query {
	__u8 opcode;
	__u8 idn;
	__u8 index;
	__u8 selector;
	__be16 reserved_osf;
	__be16 length;
	__be32 value;
	__be32 reserved[2];
};

/**
 * struct utp_upiu_cmd - Command UPIU structure
 * @data_transfer_len: Data Transfer Length DW-3
 * @cdb: Command Descriptor Block CDB DW-4 to DW-7
 */
struct utp_upiu_cmd {
	__be32 exp_data_transfer_len;
	__u8 cdb[UFS_CDB_SIZE];
};

/**
 * struct utp_upiu_req - general upiu request structure
 * @header:UPIU header structure DW-0 to DW-2
 * @sc: fields structure for scsi command DW-3 to DW-7
 * @qr: fields structure for query request DW-3 to DW-7
 */
struct utp_upiu_req {
	struct utp_upiu_header header;
	union {
		struct utp_upiu_cmd		sc;
		struct utp_upiu_query		qr;
		struct utp_upiu_query		tr;
		/* use utp_upiu_query to host the 4 dwords of uic command */
		struct utp_upiu_query		uc;
	};
};

/**
 * struct request_desc_header - Descriptor Header common to both UTRD and UTMRD
 * @dword0: Descriptor Header DW0
 * @dword1: Descriptor Header DW1
 * @dword2: Descriptor Header DW2
 * @dword3: Descriptor Header DW3
 */
struct request_desc_header {
	__le32 dword_0;
	__le32 dword_1;
	__le32 dword_2;
	__le32 dword_3;
};

/**
 * struct utp_transfer_req_desc - UTRD structure
 * @header: UTRD header DW-0 to DW-3
 * @command_desc_base_addr_lo: UCD base address low DW-4
 * @command_desc_base_addr_hi: UCD base address high DW-5
 * @response_upiu_length: response UPIU length DW-6
 * @response_upiu_offset: response UPIU offset DW-6
 * @prd_table_length: Physical region descriptor length DW-7
 * @prd_table_offset: Physical region descriptor offset DW-7
 */
struct utp_transfer_req_desc {

	/* DW 0-3 */
	struct request_desc_header header;

	/* DW 4-5*/
	__le32  command_desc_base_addr_lo;
	__le32  command_desc_base_addr_hi;

	/* DW 6 */
	__le16  response_upiu_length;
	__le16  response_upiu_offset;

	/* DW 7 */
	__le16  prd_table_length;
	__le16  prd_table_offset;
};

struct scsi_device {
	struct Scsi_Host *host;
	struct request_queue *request_queue;

	/* the next two are protected by the host->host_lock */
	struct list_head    siblings;   /* list of all devices on this host */
	struct list_head    same_target_siblings; /* just the devices sharing same target id */

	atomic_t device_busy;		/* commands actually active on LLDD */
	atomic_t device_blocked;	/* Device returned QUEUE_FULL. */

	spinlock_t list_lock;
	struct list_head cmd_list;	/* queue of in use SCSI Command structures */
	struct list_head starved_entry;
	unsigned short queue_depth;	/* How deep of a queue we want */
	unsigned short max_queue_depth;	/* max queue depth */
	unsigned short last_queue_full_depth; /* These two are used by */
	unsigned short last_queue_full_count; /* scsi_track_queue_full() */
	unsigned long last_queue_full_time;	/* last queue full time */
	unsigned long queue_ramp_up_period;	/* ramp up period in jiffies */
#define SCSI_DEFAULT_RAMP_UP_PERIOD	(120 * HZ)

	unsigned long last_queue_ramp_up;	/* last queue ramp up time */

	unsigned int id, channel;
	u64 lun;
	unsigned int manufacturer;	/* Manufacturer of device, for using 
					 * vendor-specific cmd's */
	unsigned sector_size;	/* size in bytes */

	void *hostdata;		/* available to low-level driver */
	unsigned char type;
	char scsi_level;
	char inq_periph_qual;	/* PQ from INQUIRY data */	
	struct mutex inquiry_mutex;
	unsigned char inquiry_len;	/* valid bytes in 'inquiry' */
	unsigned char * inquiry;	/* INQUIRY response data */
	const char * vendor;		/* [back_compat] point into 'inquiry' ... */
	const char * model;		/* ... after scan; point to static string */
	const char * rev;		/* ... "nullnullnullnull" before scan */

#define SCSI_VPD_PG_LEN                255
	struct scsi_vpd __rcu *vpd_pg83;
	struct scsi_vpd __rcu *vpd_pg80;
	unsigned char current_tag;	/* current tag */
	struct scsi_target      *sdev_target;   /* used only for single_lun */

	blist_flags_t		sdev_bflags; /* black/white flags as also found in
				 * scsi_devinfo.[hc]. For now used only to
				 * pass settings from slave_alloc to scsi
				 * core. */
	unsigned int eh_timeout; /* Error handling timeout */
	unsigned removable:1;
	unsigned changed:1;	/* Data invalid due to media change */
	unsigned busy:1;	/* Used to prevent races */
	unsigned lockable:1;	/* Able to prevent media removal */
	unsigned locked:1;      /* Media removal disabled */
	unsigned borken:1;	/* Tell the Seagate driver to be 
				 * painfully slow on this device */
	unsigned disconnect:1;	/* can disconnect */
	unsigned soft_reset:1;	/* Uses soft reset option */
	unsigned sdtr:1;	/* Device supports SDTR messages */
	unsigned wdtr:1;	/* Device supports WDTR messages */
	unsigned ppr:1;		/* Device supports PPR messages */
	unsigned tagged_supported:1;	/* Supports SCSI-II tagged queuing */
	unsigned simple_tags:1;	/* simple queue tag messages are enabled */
	unsigned was_reset:1;	/* There was a bus reset on the bus for 
				 * this device */
	unsigned expecting_cc_ua:1; /* Expecting a CHECK_CONDITION/UNIT_ATTN
				     * because we did a bus reset. */
	unsigned use_10_for_rw:1; /* first try 10-byte read / write */
	unsigned use_10_for_ms:1; /* first try 10-byte mode sense/select */
	unsigned no_report_opcodes:1;	/* no REPORT SUPPORTED OPERATION CODES */
	unsigned no_write_same:1;	/* no WRITE SAME command */
	unsigned use_16_for_rw:1; /* Use read/write(16) over read/write(10) */
	unsigned skip_ms_page_8:1;	/* do not use MODE SENSE page 0x08 */
	unsigned skip_ms_page_3f:1;	/* do not use MODE SENSE page 0x3f */
	unsigned skip_vpd_pages:1;	/* do not read VPD pages */
	unsigned try_vpd_pages:1;	/* attempt to read VPD pages */
	unsigned use_192_bytes_for_3f:1; /* ask for 192 bytes from page 0x3f */
	unsigned no_start_on_add:1;	/* do not issue start on add */
	unsigned allow_restart:1; /* issue START_UNIT in error handler */
	unsigned manage_start_stop:1;	/* Let HLD (sd) manage start/stop */
	unsigned start_stop_pwr_cond:1;	/* Set power cond. in START_STOP_UNIT */
	unsigned no_uld_attach:1; /* disable connecting to upper level drivers */
	unsigned select_no_atn:1;
	unsigned fix_capacity:1;	/* READ_CAPACITY is too high by 1 */
	unsigned guess_capacity:1;	/* READ_CAPACITY might be too high by 1 */
	unsigned retry_hwerror:1;	/* Retry HARDWARE_ERROR */
	unsigned last_sector_bug:1;	/* do not use multisector accesses on
					   SD_LAST_BUGGY_SECTORS */
	unsigned no_read_disc_info:1;	/* Avoid READ_DISC_INFO cmds */
	unsigned no_read_capacity_16:1; /* Avoid READ_CAPACITY_16 cmds */
	unsigned try_rc_10_first:1;	/* Try READ_CAPACACITY_10 first */
	unsigned security_supported:1;	/* Supports Security Protocols */
	unsigned is_visible:1;	/* is the device visible in sysfs */
	unsigned wce_default_on:1;	/* Cache is ON by default */
	unsigned no_dif:1;	/* T10 PI (DIF) should be disabled */
	unsigned broken_fua:1;		/* Don't set FUA bit */
	unsigned lun_in_cdb:1;		/* Store LUN bits in CDB[1] */
	unsigned unmap_limit_for_ws:1;	/* Use the UNMAP limit for WRITE SAME */

	atomic_t disk_events_disable_depth; /* disable depth for disk events */

	DECLARE_BITMAP(supported_events, SDEV_EVT_MAXBITS); /* supported events */
	DECLARE_BITMAP(pending_events, SDEV_EVT_MAXBITS); /* pending events */
	struct list_head event_list;	/* asserted events */
	struct work_struct event_work;

	unsigned int max_device_blocked; /* what device_blocked counts down from  */
#define SCSI_DEFAULT_DEVICE_BLOCKED	3

	atomic_t iorequest_cnt;
	atomic_t iodone_cnt;
	atomic_t ioerr_cnt;

	struct device		sdev_gendev,
				sdev_dev;

	struct execute_work	ew; /* used to get process context on put */
	struct work_struct	requeue_work;

	struct scsi_device_handler *handler;
	void			*handler_data;

	unsigned char		access_state;
	struct mutex		state_mutex;
	enum scsi_device_state sdev_state;
	struct task_struct	*quiesced_by;
	unsigned long		sdev_data[0];
} __attribute__((aligned(sizeof(unsigned long))));

struct Scsi_Host {
	/*
	 * __devices is protected by the host_lock, but you should
	 * usually use scsi_device_lookup / shost_for_each_device
	 * to access it and don't care about locking yourself.
	 * In the rare case of being in irq context you can use
	 * their __ prefixed variants with the lock held. NEVER
	 * access this list directly from a driver.
	 */
	struct list_head	__devices;
	struct list_head	__targets;
	
	struct list_head	starved_list;

	spinlock_t		default_lock;
	spinlock_t		*host_lock;

	struct mutex		scan_mutex;/* serialize scanning activity */

	struct list_head	eh_cmd_q;
	struct task_struct    * ehandler;  /* Error recovery thread. */
	struct completion     * eh_action; /* Wait for specific actions on the
					      host. */
	wait_queue_head_t       host_wait;
	struct scsi_host_template *hostt;
	struct scsi_transport_template *transportt;

	/* Area to keep a shared tag map */
	struct blk_mq_tag_set	tag_set;

	atomic_t host_busy;		   /* commands actually active on low-level */
	atomic_t host_blocked;

	unsigned int host_failed;	   /* commands that failed.
					      protected by host_lock */
	unsigned int host_eh_scheduled;    /* EH scheduled without command */
    
	unsigned int host_no;  /* Used for IOCTL_GET_IDLUN, /proc/scsi et al. */

	/* next two fields are used to bound the time spent in error handling */
	int eh_deadline;
	unsigned long last_reset;


	/*
	 * These three parameters can be used to allow for wide scsi,
	 * and for host adapters that support multiple busses
	 * The last two should be set to 1 more than the actual max id
	 * or lun (e.g. 8 for SCSI parallel systems).
	 */
	unsigned int max_channel;
	unsigned int max_id;
	u64 max_lun;

	/*
	 * This is a unique identifier that must be assigned so that we
	 * have some way of identifying each detected host adapter properly
	 * and uniquely.  For hosts that do not support more than one card
	 * in the system at one time, this does not need to be set.  It is
	 * initialized to 0 in scsi_register.
	 */
	unsigned int unique_id;

	/*
	 * The maximum length of SCSI commands that this host can accept.
	 * Probably 12 for most host adapters, but could be 16 for others.
	 * or 260 if the driver supports variable length cdbs.
	 * For drivers that don't set this field, a value of 12 is
	 * assumed.
	 */
	unsigned short max_cmd_len;

	int this_id;
	int can_queue;
	short cmd_per_lun;
	short unsigned int sg_tablesize;
	short unsigned int sg_prot_tablesize;
	unsigned int max_sectors;
	unsigned int max_segment_size;
	unsigned long dma_boundary;
	unsigned long virt_boundary_mask;
	/*
	 * In scsi-mq mode, the number of hardware queues supported by the LLD.
	 *
	 * Note: it is assumed that each hardware queue has a queue depth of
	 * can_queue. In other words, the total queue depth per host
	 * is nr_hw_queues * can_queue.
	 */
	unsigned nr_hw_queues;
	unsigned active_mode:2;
	unsigned unchecked_isa_dma:1;

	/*
	 * Host has requested that no further requests come through for the
	 * time being.
	 */
	unsigned host_self_blocked:1;
    
	/*
	 * Host uses correct SCSI ordering not PC ordering. The bit is
	 * set for the minority of drivers whose authors actually read
	 * the spec ;).
	 */
	unsigned reverse_ordering:1;

	/* Task mgmt function in progress */
	unsigned tmf_in_progress:1;

	/* Asynchronous scan in progress */
	unsigned async_scan:1;

	/* Don't resume host in EH */
	unsigned eh_noresume:1;

	/* The controller does not support WRITE SAME */
	unsigned no_write_same:1;

	unsigned use_cmd_list:1;

	/* Host responded with short (<36 bytes) INQUIRY result */
	unsigned short_inquiry:1;

	/* The transport requires the LUN bits NOT to be stored in CDB[1] */
	unsigned no_scsi2_lun_in_cdb:1;

	/*
	 * Optional work queue to be utilized by the transport
	 */
	char work_q_name[20];
	struct workqueue_struct *work_q;

	/*
	 * Task management function work queue
	 */
	struct workqueue_struct *tmf_work_q;

	/*
	 * Value host_blocked counts down from
	 */
	unsigned int max_host_blocked;

	/* Protection Information */
	unsigned int prot_capabilities;
	unsigned char prot_guard_type;

	/* legacy crap */
	unsigned long base;
	unsigned long io_port;
	unsigned char n_io_port;
	unsigned char dma_channel;
	unsigned int  irq;
	

	enum scsi_host_state shost_state;

	/* ldm bits */
	struct device		shost_gendev, shost_dev;

	/*
	 * Points to the transport data (if any) which is allocated
	 * separately
	 */
	void *shost_data;

	/*
	 * Points to the physical bus device we'd use to do DMA
	 * Needed just in case we have virtual hosts.
	 */
	struct device *dma_dev;

	/*
	 * We should ensure that this is aligned, both for better performance
	 * and also because some compilers (m68k) don't automatically force
	 * alignment to a long boundary.
	 */
	unsigned long hostdata[0]  /* Used for storage of host specific stuff */
		__attribute__ ((aligned (sizeof(unsigned long))));
};

struct scsi_host_template {
	struct module *module;
	const char *name;

	/*
	 * The info function will return whatever useful information the
	 * developer sees fit.  If not provided, then the name field will
	 * be used instead.
	 *
	 * Status: OPTIONAL
	 */
	const char *(* info)(struct Scsi_Host *);

	/*
	 * Ioctl interface
	 *
	 * Status: OPTIONAL
	 */
	int (*ioctl)(struct scsi_device *dev, unsigned int cmd,
		     void __user *arg);


#ifdef CONFIG_COMPAT
	/* 
	 * Compat handler. Handle 32bit ABI.
	 * When unknown ioctl is passed return -ENOIOCTLCMD.
	 *
	 * Status: OPTIONAL
	 */
	int (*compat_ioctl)(struct scsi_device *dev, unsigned int cmd,
			    void __user *arg);
#endif

	/*
	 * The queuecommand function is used to queue up a scsi
	 * command block to the LLDD.  When the driver finished
	 * processing the command the done callback is invoked.
	 *
	 * If queuecommand returns 0, then the driver has accepted the
	 * command.  It must also push it to the HBA if the scsi_cmnd
	 * flag SCMD_LAST is set, or if the driver does not implement
	 * commit_rqs.  The done() function must be called on the command
	 * when the driver has finished with it. (you may call done on the
	 * command before queuecommand returns, but in this case you
	 * *must* return 0 from queuecommand).
	 *
	 * Queuecommand may also reject the command, in which case it may
	 * not touch the command and must not call done() for it.
	 *
	 * There are two possible rejection returns:
	 *
	 *   SCSI_MLQUEUE_DEVICE_BUSY: Block this device temporarily, but
	 *   allow commands to other devices serviced by this host.
	 *
	 *   SCSI_MLQUEUE_HOST_BUSY: Block all devices served by this
	 *   host temporarily.
	 *
         * For compatibility, any other non-zero return is treated the
         * same as SCSI_MLQUEUE_HOST_BUSY.
	 *
	 * NOTE: "temporarily" means either until the next command for#
	 * this device/host completes, or a period of time determined by
	 * I/O pressure in the system if there are no other outstanding
	 * commands.
	 *
	 * STATUS: REQUIRED
	 */
	int (* queuecommand)(struct Scsi_Host *, struct scsi_cmnd *);

	/*
	 * The commit_rqs function is used to trigger a hardware
	 * doorbell after some requests have been queued with
	 * queuecommand, when an error is encountered before sending
	 * the request with SCMD_LAST set.
	 *
	 * STATUS: OPTIONAL
	 */
	void (*commit_rqs)(struct Scsi_Host *, u16);

	/*
	 * This is an error handling strategy routine.  You don't need to
	 * define one of these if you don't want to - there is a default
	 * routine that is present that should work in most cases.  For those
	 * driver authors that have the inclination and ability to write their
	 * own strategy routine, this is where it is specified.  Note - the
	 * strategy routine is *ALWAYS* run in the context of the kernel eh
	 * thread.  Thus you are guaranteed to *NOT* be in an interrupt
	 * handler when you execute this, and you are also guaranteed to
	 * *NOT* have any other commands being queued while you are in the
	 * strategy routine. When you return from this function, operations
	 * return to normal.
	 *
	 * See scsi_error.c scsi_unjam_host for additional comments about
	 * what this function should and should not be attempting to do.
	 *
	 * Status: REQUIRED	(at least one of them)
	 */
	int (* eh_abort_handler)(struct scsi_cmnd *);
	int (* eh_device_reset_handler)(struct scsi_cmnd *);
	int (* eh_target_reset_handler)(struct scsi_cmnd *);
	int (* eh_bus_reset_handler)(struct scsi_cmnd *);
	int (* eh_host_reset_handler)(struct scsi_cmnd *);

	/*
	 * Before the mid layer attempts to scan for a new device where none
	 * currently exists, it will call this entry in your driver.  Should
	 * your driver need to allocate any structs or perform any other init
	 * items in order to send commands to a currently unused target/lun
	 * combo, then this is where you can perform those allocations.  This
	 * is specifically so that drivers won't have to perform any kind of
	 * "is this a new device" checks in their queuecommand routine,
	 * thereby making the hot path a bit quicker.
	 *
	 * Return values: 0 on success, non-0 on failure
	 *
	 * Deallocation:  If we didn't find any devices at this ID, you will
	 * get an immediate call to slave_destroy().  If we find something
	 * here then you will get a call to slave_configure(), then the
	 * device will be used for however long it is kept around, then when
	 * the device is removed from the system (or * possibly at reboot
	 * time), you will then get a call to slave_destroy().  This is
	 * assuming you implement slave_configure and slave_destroy.
	 * However, if you allocate memory and hang it off the device struct,
	 * then you must implement the slave_destroy() routine at a minimum
	 * in order to avoid leaking memory
	 * each time a device is tore down.
	 *
	 * Status: OPTIONAL
	 */
	int (* slave_alloc)(struct scsi_device *);

	/*
	 * Once the device has responded to an INQUIRY and we know the
	 * device is online, we call into the low level driver with the
	 * struct scsi_device *.  If the low level device driver implements
	 * this function, it *must* perform the task of setting the queue
	 * depth on the device.  All other tasks are optional and depend
	 * on what the driver supports and various implementation details.
	 * 
	 * Things currently recommended to be handled at this time include:
	 *
	 * 1.  Setting the device queue depth.  Proper setting of this is
	 *     described in the comments for scsi_change_queue_depth.
	 * 2.  Determining if the device supports the various synchronous
	 *     negotiation protocols.  The device struct will already have
	 *     responded to INQUIRY and the results of the standard items
	 *     will have been shoved into the various device flag bits, eg.
	 *     device->sdtr will be true if the device supports SDTR messages.
	 * 3.  Allocating command structs that the device will need.
	 * 4.  Setting the default timeout on this device (if needed).
	 * 5.  Anything else the low level driver might want to do on a device
	 *     specific setup basis...
	 * 6.  Return 0 on success, non-0 on error.  The device will be marked
	 *     as offline on error so that no access will occur.  If you return
	 *     non-0, your slave_destroy routine will never get called for this
	 *     device, so don't leave any loose memory hanging around, clean
	 *     up after yourself before returning non-0
	 *
	 * Status: OPTIONAL
	 */
	int (* slave_configure)(struct scsi_device *);

	/*
	 * Immediately prior to deallocating the device and after all activity
	 * has ceased the mid layer calls this point so that the low level
	 * driver may completely detach itself from the scsi device and vice
	 * versa.  The low level driver is responsible for freeing any memory
	 * it allocated in the slave_alloc or slave_configure calls. 
	 *
	 * Status: OPTIONAL
	 */
	void (* slave_destroy)(struct scsi_device *);

	/*
	 * Before the mid layer attempts to scan for a new device attached
	 * to a target where no target currently exists, it will call this
	 * entry in your driver.  Should your driver need to allocate any
	 * structs or perform any other init items in order to send commands
	 * to a currently unused target, then this is where you can perform
	 * those allocations.
	 *
	 * Return values: 0 on success, non-0 on failure
	 *
	 * Status: OPTIONAL
	 */
	int (* target_alloc)(struct scsi_target *);

	/*
	 * Immediately prior to deallocating the target structure, and
	 * after all activity to attached scsi devices has ceased, the
	 * midlayer calls this point so that the driver may deallocate
	 * and terminate any references to the target.
	 *
	 * Status: OPTIONAL
	 */
	void (* target_destroy)(struct scsi_target *);

	/*
	 * If a host has the ability to discover targets on its own instead
	 * of scanning the entire bus, it can fill in this function and
	 * call scsi_scan_host().  This function will be called periodically
	 * until it returns 1 with the scsi_host and the elapsed time of
	 * the scan in jiffies.
	 *
	 * Status: OPTIONAL
	 */
	int (* scan_finished)(struct Scsi_Host *, unsigned long);

	/*
	 * If the host wants to be called before the scan starts, but
	 * after the midlayer has set up ready for the scan, it can fill
	 * in this function.
	 *
	 * Status: OPTIONAL
	 */
	void (* scan_start)(struct Scsi_Host *);

	/*
	 * Fill in this function to allow the queue depth of this host
	 * to be changeable (on a per device basis).  Returns either
	 * the current queue depth setting (may be different from what
	 * was passed in) or an error.  An error should only be
	 * returned if the requested depth is legal but the driver was
	 * unable to set it.  If the requested depth is illegal, the
	 * driver should set and return the closest legal queue depth.
	 *
	 * Status: OPTIONAL
	 */
	int (* change_queue_depth)(struct scsi_device *, int);

	/*
	 * This functions lets the driver expose the queue mapping
	 * to the block layer.
	 *
	 * Status: OPTIONAL
	 */
	int (* map_queues)(struct Scsi_Host *shost);

	/*
	 * This function determines the BIOS parameters for a given
	 * harddisk.  These tend to be numbers that are made up by
	 * the host adapter.  Parameters:
	 * size, device, list (heads, sectors, cylinders)
	 *
	 * Status: OPTIONAL
	 */
	int (* bios_param)(struct scsi_device *, struct block_device *,
			sector_t, int []);

	/*
	 * This function is called when one or more partitions on the
	 * device reach beyond the end of the device.
	 *
	 * Status: OPTIONAL
	 */
	void (*unlock_native_capacity)(struct scsi_device *);

	/*
	 * Can be used to export driver statistics and other infos to the
	 * world outside the kernel ie. userspace and it also provides an
	 * interface to feed the driver with information.
	 *
	 * Status: OBSOLETE
	 */
	int (*show_info)(struct seq_file *, struct Scsi_Host *);
	int (*write_info)(struct Scsi_Host *, char *, int);

	/*
	 * This is an optional routine that allows the transport to become
	 * involved when a scsi io timer fires. The return value tells the
	 * timer routine how to finish the io timeout handling.
	 *
	 * Status: OPTIONAL
	 */
	enum blk_eh_timer_return (*eh_timed_out)(struct scsi_cmnd *);

	/* This is an optional routine that allows transport to initiate
	 * LLD adapter or firmware reset using sysfs attribute.
	 *
	 * Return values: 0 on success, -ve value on failure.
	 *
	 * Status: OPTIONAL
	 */

	int (*host_reset)(struct Scsi_Host *shost, int reset_type);
#define SCSI_ADAPTER_RESET	1
#define SCSI_FIRMWARE_RESET	2


	/*
	 * Name of proc directory
	 */
	const char *proc_name;

	/*
	 * Used to store the procfs directory if a driver implements the
	 * show_info method.
	 */
	struct proc_dir_entry *proc_dir;

	/*
	 * This determines if we will use a non-interrupt driven
	 * or an interrupt driven scheme.  It is set to the maximum number
	 * of simultaneous commands a given host adapter will accept.
	 */
	int can_queue;

	/*
	 * In many instances, especially where disconnect / reconnect are
	 * supported, our host also has an ID on the SCSI bus.  If this is
	 * the case, then it must be reserved.  Please set this_id to -1 if
	 * your setup is in single initiator mode, and the host lacks an
	 * ID.
	 */
	int this_id;

	/*
	 * This determines the degree to which the host adapter is capable
	 * of scatter-gather.
	 */
	unsigned short sg_tablesize;
	unsigned short sg_prot_tablesize;

	/*
	 * Set this if the host adapter has limitations beside segment count.
	 */
	unsigned int max_sectors;

	/*
	 * Maximum size in bytes of a single segment.
	 */
	unsigned int max_segment_size;

	/*
	 * DMA scatter gather segment boundary limit. A segment crossing this
	 * boundary will be split in two.
	 */
	unsigned long dma_boundary;

	unsigned long virt_boundary_mask;

	/*
	 * This specifies "machine infinity" for host templates which don't
	 * limit the transfer size.  Note this limit represents an absolute
	 * maximum, and may be over the transfer limits allowed for
	 * individual devices (e.g. 256 for SCSI-1).
	 */
#define SCSI_DEFAULT_MAX_SECTORS	1024

	/*
	 * True if this host adapter can make good use of linked commands.
	 * This will allow more than one command to be queued to a given
	 * unit on a given host.  Set this to the maximum number of command
	 * blocks to be provided for each device.  Set this to 1 for one
	 * command block per lun, 2 for two, etc.  Do not set this to 0.
	 * You should make sure that the host adapter will do the right thing
	 * before you try setting this above 1.
	 */
	short cmd_per_lun;

	/*
	 * present contains counter indicating how many boards of this
	 * type were found when we did the scan.
	 */
	unsigned char present;

	/* If use block layer to manage tags, this is tag allocation policy */
	int tag_alloc_policy;

	/*
	 * Track QUEUE_FULL events and reduce queue depth on demand.
	 */
	unsigned track_queue_depth:1;

	/*
	 * This specifies the mode that a LLD supports.
	 */
	unsigned supported_mode:2;

	/*
	 * True if this host adapter uses unchecked DMA onto an ISA bus.
	 */
	unsigned unchecked_isa_dma:1;

	/*
	 * True for emulated SCSI host adapters (e.g. ATAPI).
	 */
	unsigned emulated:1;

	/*
	 * True if the low-level driver performs its own reset-settle delays.
	 */
	unsigned skip_settle_delay:1;

	/* True if the controller does not support WRITE SAME */
	unsigned no_write_same:1;

	/* True if the low-level driver supports blk-mq only */
	unsigned force_blk_mq:1;

	/*
	 * Countdown for host blocking with no commands outstanding.
	 */
	unsigned int max_host_blocked;

	/*
	 * Default value for the blocking.  If the queue is empty,
	 * host_blocked counts down in the request_fn until it restarts
	 * host operations as zero is reached.  
	 *
	 * FIXME: This should probably be a value in the template
	 */
#define SCSI_DEFAULT_HOST_BLOCKED	7

	/*
	 * Pointer to the sysfs class properties for this host, NULL terminated.
	 */
	struct device_attribute **shost_attrs;

	/*
	 * Pointer to the SCSI device properties for this host, NULL terminated.
	 */
	struct device_attribute **sdev_attrs;

	/*
	 * Pointer to the SCSI device attribute groups for this host,
	 * NULL terminated.
	 */
	const struct attribute_group **sdev_groups;

	/*
	 * Vendor Identifier associated with the host
	 *
	 * Note: When specifying vendor_id, be sure to read the
	 *   Vendor Type and ID formatting requirements specified in
	 *   scsi_netlink.h
	 */
	u64 vendor_id;

	/*
	 * Additional per-command data allocated for the driver.
	 */
	unsigned int cmd_size;
	struct scsi_host_cmd_pool *cmd_pool;
};

struct scsi_cmnd {
	struct scsi_request req;
	struct scsi_device *device;
	struct list_head list;  /* scsi_cmnd participates in queue lists */
	struct list_head eh_entry; /* entry for the host eh_cmd_q */
	struct delayed_work abort_work;

	struct rcu_head rcu;

	int eh_eflags;		/* Used by error handlr */

	/*
	 * This is set to jiffies as it was when the command was first
	 * allocated.  It is used to time how long the command has
	 * been outstanding
	 */
	unsigned long jiffies_at_alloc;

	int retries;
	int allowed;

	unsigned char prot_op;
	unsigned char prot_type;
	unsigned char prot_flags;

	unsigned short cmd_len;
	enum dma_data_direction sc_data_direction;

	/* These elements define the operation we are about to perform */
	unsigned char *cmnd;                                                                 // These elements define the operation we are about to perform


	/* These elements define the operation we ultimately want to perform */
	struct scsi_data_buffer sdb;                                                         // 重点
	struct scsi_data_buffer *prot_sdb;

	unsigned underflow;	/* Return error if less than
				   this amount is transferred */

	unsigned transfersize;	/* How much we are guaranteed to
				   transfer with each SCSI transfer
				   (ie, between disconnect / 
				   reconnects.   Probably == sector
				   size */

	struct request *request;	/* The command we are
				   	   working on */

	unsigned char *sense_buffer;
				/* obtained by REQUEST SENSE when
				 * CHECK CONDITION is received on original
				 * command (auto-sense). Length must be
				 * SCSI_SENSE_BUFFERSIZE bytes. */

	/* Low-level done function - can be used by low-level driver to point
	 *        to completion function.  Not used by mid/upper level code. */
	void (*scsi_done) (struct scsi_cmnd *);

	/*
	 * The following fields can be written to by the host specific code. 
	 * Everything else should be left alone. 
	 */
	struct scsi_pointer SCp;	/* Scratchpad used by some host adapters */

	unsigned char *host_scribble;	/* The host adapter is allowed to
					 * call scsi_malloc and get some memory
					 * and hang it here.  The host adapter
					 * is also expected to call scsi_free
					 * to release this memory.  (The memory
					 * obtained by scsi_malloc is guaranteed
					 * to be at an address < 16Mb). */

	int result;		/* Status code from lower level driver */
	int flags;		/* Command flags */
	unsigned long state;	/* Command completion state */

	unsigned char tag;	/* SCSI-II queued command tag */
};

struct scsi_request {
	unsigned char	__cmd[BLK_MAX_CDB];                                                  // #define BLK_MAX_CDB	16
	unsigned char	*cmd;
	unsigned short	cmd_len;
	int		result;
	unsigned int	sense_len;
	unsigned int	resid_len;	/* residual count */
	int		retries;
	void		*sense;
};

/**
 * struct utp_transfer_cmd_desc - UFS Command Descriptor structure
 * @command_upiu: Command UPIU Frame address
 * @response_upiu: Response UPIU Frame address
 * @prd_table: Physical Region Descriptor
 */
struct utp_transfer_cmd_desc {
	u8 command_upiu[ALIGNED_UPIU_SIZE];                                                  // Command UPIU Frame address, ALIGNED_UPIU_SIZE = 512
	u8 response_upiu[ALIGNED_UPIU_SIZE];                                                 // Response UPIU Frame address, ALIGNED_UPIU_SIZE = 512
	struct ufshcd_sg_entry    prd_table[SG_ALL];                                         // Physical Region Descriptor, #define SG_ALL SG_CHUNK_SIZE, #define SG_CHUNK_SIZE	128
};

struct scsi_device *__scsi_add_device(struct Scsi_Host *shost, uint channel,
				      uint id, u64 lun, void *hostdata)
{
	struct scsi_device *sdev = ERR_PTR(-ENODEV);                                         // 设置sdev为ERR_PTR(-ENODEV)
	struct device *parent = &shost->shost_gendev;                                        // 重点：设置parent为&shost->shost_gendev
	struct scsi_target *starget;

	if (strncmp(scsi_scan_type, "none", 4) == 0)                                         // 
		return ERR_PTR(-ENODEV);                                                         // 返回ERR_PTR(-ENODEV)

	starget = scsi_alloc_target(parent, channel, id);                                    // 
	if (!starget)                                                                        // 
		return ERR_PTR(-ENOMEM);                                                         // 
	scsi_autopm_get_target(starget);                                                     // 

	mutex_lock(&shost->scan_mutex);                                                      // 
	if (!shost->async_scan)                                                              // 
		scsi_complete_async_scans();                                                     // 

	if (scsi_host_scan_allowed(shost) && scsi_autopm_get_host(shost) == 0) {             // 
		scsi_probe_and_add_lun(starget, lun, NULL, &sdev, 1, hostdata);                  // 
		scsi_autopm_put_host(shost);                                                     // 
	}
	mutex_unlock(&shost->scan_mutex);                                                    // 
	scsi_autopm_put_target(starget);                                                     // 
	/*
	 * paired with scsi_alloc_target().  Target will be destroyed unless
	 * scsi_probe_and_add_lun made an underlying device visible
	 */
	scsi_target_reap(starget);                                                           // 
	put_device(&starget->dev);                                                           // 

	return sdev;                                                                         // 
}
EXPORT_SYMBOL(__scsi_add_device);

/**
 * scsi_alloc_target - allocate a new or find an existing target
 * @parent:	parent of the target (need not be a scsi host)
 * @channel:	target channel number (zero if no channels)
 * @id:		target id number
 *
 * Return an existing target if one exists, provided it hasn't already
 * gone into STARGET_DEL state, otherwise allocate a new target.
 *
 * The target is returned with an incremented reference, so the caller
 * is responsible for both reaping and doing a last put
 */
static struct scsi_target *scsi_alloc_target(struct device *parent,
					     int channel, uint id)
{
	struct Scsi_Host *shost = dev_to_shost(parent);                                      // 获取shost
	struct device *dev = NULL;
	unsigned long flags;
	const int size = sizeof(struct scsi_target)
		+ shost->transportt->target_size;                                                // 
	struct scsi_target *starget;
	struct scsi_target *found_target;
	int error, ref_got;

	starget = kzalloc(size, GFP_KERNEL);                                                 // 申请starget
	if (!starget) {
		printk(KERN_ERR "%s: allocation failure\n", __func__);
		return NULL;
	}
	dev = &starget->dev;
	device_initialize(dev);
	kref_init(&starget->reap_ref);
	dev->parent = get_device(parent);
	dev_set_name(dev, "target%d:%d:%d", shost->host_no, channel, id);
	dev->bus = &scsi_bus_type;
	dev->type = &scsi_target_type;
	starget->id = id;
	starget->channel = channel;
	starget->can_queue = 0;
	INIT_LIST_HEAD(&starget->siblings);
	INIT_LIST_HEAD(&starget->devices);
	starget->state = STARGET_CREATED;
	starget->scsi_level = SCSI_2;
	starget->max_target_blocked = SCSI_DEFAULT_TARGET_BLOCKED;
 retry:
	spin_lock_irqsave(shost->host_lock, flags);

	found_target = __scsi_find_target(parent, channel, id);
	if (found_target)
		goto found;

	list_add_tail(&starget->siblings, &shost->__targets);
	spin_unlock_irqrestore(shost->host_lock, flags);
	/* allocate and add */
	transport_setup_device(dev);
	if (shost->hostt->target_alloc) {
		error = shost->hostt->target_alloc(starget);

		if(error) {
			dev_printk(KERN_ERR, dev, "target allocation failed, error %d\n", error);
			/* don't want scsi_target_reap to do the final
			 * put because it will be under the host lock */
			scsi_target_destroy(starget);
			return NULL;
		}
	}
	get_device(dev);

	return starget;

 found:
	/*
	 * release routine already fired if kref is zero, so if we can still
	 * take the reference, the target must be alive.  If we can't, it must
	 * be dying and we need to wait for a new target
	 */
	ref_got = kref_get_unless_zero(&found_target->reap_ref);

	spin_unlock_irqrestore(shost->host_lock, flags);
	if (ref_got) {
		put_device(dev);
		return found_target;
	}
	/*
	 * Unfortunately, we found a dying target; need to wait until it's
	 * dead before we can get a new one.  There is an anomaly here.  We
	 * *should* call scsi_target_reap() to balance the kref_get() of the
	 * reap_ref above.  However, since the target being released, it's
	 * already invisible and the reap_ref is irrelevant.  If we call
	 * scsi_target_reap() we might spuriously do another device_del() on
	 * an already invisible target.
	 */
	put_device(&found_target->dev);
	/*
	 * length of time is irrelevant here, we just want to yield the CPU
	 * for a tick to avoid busy waiting for the target to die.
	 */
	msleep(1);
	goto retry;
}

void scsi_sysfs_device_initialize(struct scsi_device *sdev)
{
	unsigned long flags;
	struct Scsi_Host *shost = sdev->host;                                                // 取出shost
	struct scsi_target  *starget = sdev->sdev_target;                                    // 取出target

	device_initialize(&sdev->sdev_gendev);                                               // 重点：初始化gendisk基类对象，这个基类对象参与ufs设备的match和probe
	sdev->sdev_gendev.bus = &scsi_bus_type;                                              // 重点：设置gendisk基类对象的bus
	sdev->sdev_gendev.type = &scsi_dev_type;                                             // 重点：设置gendisk基类对象的type
	dev_set_name(&sdev->sdev_gendev, "%d:%d:%d:%llu",
		     sdev->host->host_no, sdev->channel, sdev->id, sdev->lun);                   // 重点：设置gendisk基类对象的kobj->name

	device_initialize(&sdev->sdev_dev);                                                  // 
	sdev->sdev_dev.parent = get_device(&sdev->sdev_gendev);                              // 
	sdev->sdev_dev.class = &sdev_class;                                                  // 
	dev_set_name(&sdev->sdev_dev, "%d:%d:%d:%llu",
		     sdev->host->host_no, sdev->channel, sdev->id, sdev->lun);                   // 
	/*
	 * Get a default scsi_level from the target (derived from sibling
	 * devices).  This is the best we can do for guessing how to set
	 * sdev->lun_in_cdb for the initial INQUIRY command.  For LUN 0 the
	 * setting doesn't matter, because all the bits are zero anyway.
	 * But it does matter for higher LUNs.
	 */
	sdev->scsi_level = starget->scsi_level;                                              // 
	if (sdev->scsi_level <= SCSI_2 &&
			sdev->scsi_level != SCSI_UNKNOWN &&
			!shost->no_scsi2_lun_in_cdb)                                                 // 
		sdev->lun_in_cdb = 1;                                                            // 

	transport_setup_device(&sdev->sdev_gendev);                                          // common，忽略
	spin_lock_irqsave(shost->host_lock, flags);
	list_add_tail(&sdev->same_target_siblings, &starget->devices);                       // 
	list_add_tail(&sdev->siblings, &shost->__devices);                                   // 
	spin_unlock_irqrestore(shost->host_lock, flags);
	/*
	 * device can now only be removed via __scsi_remove_device() so hold
	 * the target.  Target will be held in CREATED state until something
	 * beneath it becomes visible (in which case it moves to RUNNING)
	 */
	kref_get(&starget->reap_ref);                                                        // 
}

/**
 * scsi_probe_and_add_lun - probe a LUN, if a LUN is found add it
 * @starget:	pointer to target device structure
 * @lun:	LUN of target device
 * @bflagsp:	store bflags here if not NULL
 * @sdevp:	probe the LUN corresponding to this scsi_device
 * @rescan:     if not equal to SCSI_SCAN_INITIAL skip some code only
 *              needed on first scan
 * @hostdata:	passed to scsi_alloc_sdev()
 *
 * Description:
 *     Call scsi_probe_lun, if a LUN with an attached device is found,
 *     allocate and set it up by calling scsi_add_lun.
 *
 * Return:
 *
 *   - SCSI_SCAN_NO_RESPONSE: could not allocate or setup a scsi_device
 *   - SCSI_SCAN_TARGET_PRESENT: target responded, but no device is
 *         attached at the LUN
 *   - SCSI_SCAN_LUN_PRESENT: a new scsi_device was allocated and initialized
 **/
static int scsi_probe_and_add_lun(struct scsi_target *starget,
				  u64 lun, blist_flags_t *bflagsp,
				  struct scsi_device **sdevp,
				  enum scsi_scan_mode rescan,
				  void *hostdata)
{
	struct scsi_device *sdev;
	unsigned char *result;
	blist_flags_t bflags;
	int res = SCSI_SCAN_NO_RESPONSE, result_len = 256;
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);

	/*
	 * The rescan flag is used as an optimization, the first scan of a
	 * host adapter calls into here with rescan == 0.
	 */
	sdev = scsi_device_lookup_by_target(starget, lun);
	if (sdev) {
		if (rescan != SCSI_SCAN_INITIAL || !scsi_device_created(sdev)) {
			SCSI_LOG_SCAN_BUS(3, sdev_printk(KERN_INFO, sdev,
				"scsi scan: device exists on %s\n",
				dev_name(&sdev->sdev_gendev)));
			if (sdevp)
				*sdevp = sdev;
			else
				scsi_device_put(sdev);

			if (bflagsp)
				*bflagsp = scsi_get_device_flags(sdev,
								 sdev->vendor,
								 sdev->model);
			return SCSI_SCAN_LUN_PRESENT;
		}
		scsi_device_put(sdev);
	} else
		sdev = scsi_alloc_sdev(starget, lun, hostdata);                                  // 
	if (!sdev)
		goto out;

	result = kmalloc(result_len, GFP_KERNEL |
			((shost->unchecked_isa_dma) ? __GFP_DMA : 0));                               // 
	if (!result)
		goto out_free_sdev;

	if (scsi_probe_lun(sdev, result, result_len, &bflags))                               // 
		goto out_free_result;

	if (bflagsp)
		*bflagsp = bflags;
	/*
	 * result contains valid SCSI INQUIRY data.
	 */
	if ((result[0] >> 5) == 3) {
		/*
		 * For a Peripheral qualifier 3 (011b), the SCSI
		 * spec says: The device server is not capable of
		 * supporting a physical device on this logical
		 * unit.
		 *
		 * For disks, this implies that there is no
		 * logical disk configured at sdev->lun, but there
		 * is a target id responding.
		 */
		SCSI_LOG_SCAN_BUS(2, sdev_printk(KERN_INFO, sdev, "scsi scan:"
				   " peripheral qualifier of 3, device not"
				   " added\n"))
		if (lun == 0) {
			SCSI_LOG_SCAN_BUS(1, {
				unsigned char vend[9];
				unsigned char mod[17];

				sdev_printk(KERN_INFO, sdev,
					"scsi scan: consider passing scsi_mod."
					"dev_flags=%s:%s:0x240 or 0x1000240\n",
					scsi_inq_str(vend, result, 8, 16),
					scsi_inq_str(mod, result, 16, 32));
			});

		}

		res = SCSI_SCAN_TARGET_PRESENT;
		goto out_free_result;
	}

	/*
	 * Some targets may set slight variations of PQ and PDT to signal
	 * that no LUN is present, so don't add sdev in these cases.
	 * Two specific examples are:
	 * 1) NetApp targets: return PQ=1, PDT=0x1f
	 * 2) IBM/2145 targets: return PQ=1, PDT=0
	 * 3) USB UFI: returns PDT=0x1f, with the PQ bits being "reserved"
	 *    in the UFI 1.0 spec (we cannot rely on reserved bits).
	 *
	 * References:
	 * 1) SCSI SPC-3, pp. 145-146
	 * PQ=1: "A peripheral device having the specified peripheral
	 * device type is not connected to this logical unit. However, the
	 * device server is capable of supporting the specified peripheral
	 * device type on this logical unit."
	 * PDT=0x1f: "Unknown or no device type"
	 * 2) USB UFI 1.0, p. 20
	 * PDT=00h Direct-access device (floppy)
	 * PDT=1Fh none (no FDD connected to the requested logical unit)
	 */
	if (((result[0] >> 5) == 1 ||
	    (starget->pdt_1f_for_no_lun && (result[0] & 0x1f) == 0x1f)) &&
	    !scsi_is_wlun(lun)) {
		SCSI_LOG_SCAN_BUS(3, sdev_printk(KERN_INFO, sdev,
					"scsi scan: peripheral device type"
					" of 31, no device added\n"));
		res = SCSI_SCAN_TARGET_PRESENT;
		goto out_free_result;
	}

	res = scsi_add_lun(sdev, result, &bflags, shost->async_scan);
	if (res == SCSI_SCAN_LUN_PRESENT) {
		if (bflags & BLIST_KEY) {
			sdev->lockable = 0;
			scsi_unlock_floptical(sdev, result);
		}
	}

 out_free_result:
	kfree(result);
 out_free_sdev:
	if (res == SCSI_SCAN_LUN_PRESENT) {
		if (sdevp) {
			if (scsi_device_get(sdev) == 0) {
				*sdevp = sdev;
			} else {
				__scsi_remove_device(sdev);
				res = SCSI_SCAN_NO_RESPONSE;
			}
		}
	} else
		__scsi_remove_device(sdev);
 out:
	return res;                                                                          // 返回res
}

/**
 * scsi_alloc_sdev - allocate and setup a scsi_Device
 * @starget: which target to allocate a &scsi_device for
 * @lun: which lun
 * @hostdata: usually NULL and set by ->slave_alloc instead
 *
 * Description:
 *     Allocate, initialize for io, and return a pointer to a scsi_Device.
 *     Stores the @shost, @channel, @id, and @lun in the scsi_Device, and
 *     adds scsi_Device to the appropriate list.
 *
 * Return value:
 *     scsi_Device pointer, or NULL on failure.
 **/
static struct scsi_device *scsi_alloc_sdev(struct scsi_target *starget,
					   u64 lun, void *hostdata)
{
	struct scsi_device *sdev;
	int display_failure_msg = 1, ret;
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);                         // 取出shost

	sdev = kzalloc(sizeof(*sdev) + shost->transportt->device_size,
		       GFP_KERNEL);                                                              // 申请sdev空间
	if (!sdev)                                                                           // 若sdev为NULL
		goto out;                                                                        // 跳转至out

	sdev->vendor = scsi_null_device_strs;                                                // 
	sdev->model = scsi_null_device_strs;                                                 // 
	sdev->rev = scsi_null_device_strs;                                                   // 
	sdev->host = shost;                                                                  // 
	sdev->queue_ramp_up_period = SCSI_DEFAULT_RAMP_UP_PERIOD;                            // 
	sdev->id = starget->id;                                                              // 
	sdev->lun = lun;                                                                     // 
	sdev->channel = starget->channel;                                                    // 
	mutex_init(&sdev->state_mutex);                                                      //
	sdev->sdev_state = SDEV_CREATED;                                                     // 
	INIT_LIST_HEAD(&sdev->siblings);                                                     // 
	INIT_LIST_HEAD(&sdev->same_target_siblings);                                         // 
	INIT_LIST_HEAD(&sdev->cmd_list);                                                     // 
	INIT_LIST_HEAD(&sdev->starved_entry);                                                // 
	INIT_LIST_HEAD(&sdev->event_list);                                                   // 
	spin_lock_init(&sdev->list_lock);                                                    // 
	mutex_init(&sdev->inquiry_mutex);                                                    // 
	INIT_WORK(&sdev->event_work, scsi_evt_thread);                                       // 
	INIT_WORK(&sdev->requeue_work, scsi_requeue_run_queue);                              // 

	sdev->sdev_gendev.parent = get_device(&starget->dev);                                // 
	sdev->sdev_target = starget;                                                         // 

	/* usually NULL and set by ->slave_alloc instead */
	sdev->hostdata = hostdata;                                                           // 

	/* if the device needs this changing, it may do so in the
	 * slave_configure function */
	sdev->max_device_blocked = SCSI_DEFAULT_DEVICE_BLOCKED;                              // 

	/*
	 * Some low level driver could use device->type
	 */
	sdev->type = -1;                                                                     // 

	/*
	 * Assume that the device will have handshaking problems,
	 * and then fix this field later if it turns out it
	 * doesn't
	 */
	sdev->borken = 1;                                                                    // 

	sdev->request_queue = scsi_mq_alloc_queue(sdev);                                     // 重点：
	if (!sdev->request_queue) {
		/* release fn is set up in scsi_sysfs_device_initialise, so
		 * have to free and put manually here */
		put_device(&starget->dev);                                                       // 
		kfree(sdev);                                                                     // 
		goto out;                                                                        // 
	}
	WARN_ON_ONCE(!blk_get_queue(sdev->request_queue));                                   // 
	sdev->request_queue->queuedata = sdev;                                               // 重点

	scsi_change_queue_depth(sdev, sdev->host->cmd_per_lun ?
					sdev->host->cmd_per_lun : 1);                                        // 

	scsi_sysfs_device_initialize(sdev);                                                  // 

	if (shost->hostt->slave_alloc) {                                                     // 
		ret = shost->hostt->slave_alloc(sdev);                                           // 
		if (ret) {                                                                       // 
			/*
			 * if LLDD reports slave not present, don't clutter
			 * console with alloc failure messages
			 */
			if (ret == -ENXIO)                                                           // 
				display_failure_msg = 0;                                                 // 
			goto out_device_destroy;                                                     // 
		}
	}

	return sdev;                                                                         // 返回sdev

out_device_destroy:
	__scsi_remove_device(sdev);
out:
	if (display_failure_msg)
		printk(ALLOC_FAILURE_MSG, __func__);
	return NULL;
}

/**
 * scsi_probe_lun - probe a single LUN using a SCSI INQUIRY
 * @sdev:	scsi_device to probe
 * @inq_result:	area to store the INQUIRY result
 * @result_len: len of inq_result
 * @bflags:	store any bflags found here
 *
 * Description:
 *     Probe the lun associated with @req using a standard SCSI INQUIRY;
 *
 *     If the INQUIRY is successful, zero is returned and the
 *     INQUIRY data is in @inq_result; the scsi_level and INQUIRY length
 *     are copied to the scsi_device any flags value is stored in *@bflags.
 **/
static int scsi_probe_lun(struct scsi_device *sdev, unsigned char *inq_result,
			  int result_len, blist_flags_t *bflags)
{
	unsigned char scsi_cmd[MAX_COMMAND_SIZE];
	int first_inquiry_len, try_inquiry_len, next_inquiry_len;
	int response_len = 0;
	int pass, count, result;
	struct scsi_sense_hdr sshdr;

	*bflags = 0;

	/* Perform up to 3 passes.  The first pass uses a conservative
	 * transfer length of 36 unless sdev->inquiry_len specifies a
	 * different value. */
	first_inquiry_len = sdev->inquiry_len ? sdev->inquiry_len : 36;
	try_inquiry_len = first_inquiry_len;
	pass = 1;

 next_pass:
	SCSI_LOG_SCAN_BUS(3, sdev_printk(KERN_INFO, sdev,
				"scsi scan: INQUIRY pass %d length %d\n",
				pass, try_inquiry_len));

	/* Each pass gets up to three chances to ignore Unit Attention */
	for (count = 0; count < 3; ++count) {
		int resid;

		memset(scsi_cmd, 0, 6);
		scsi_cmd[0] = INQUIRY;
		scsi_cmd[4] = (unsigned char) try_inquiry_len;

		memset(inq_result, 0, try_inquiry_len);

		result = scsi_execute_req(sdev,  scsi_cmd, DMA_FROM_DEVICE,
					  inq_result, try_inquiry_len, &sshdr,
					  HZ / 2 + HZ * scsi_inq_timeout, 3,
					  &resid);

		SCSI_LOG_SCAN_BUS(3, sdev_printk(KERN_INFO, sdev,
				"scsi scan: INQUIRY %s with code 0x%x\n",
				result ? "failed" : "successful", result));

		if (result) {
			/*
			 * not-ready to ready transition [asc/ascq=0x28/0x0]
			 * or power-on, reset [asc/ascq=0x29/0x0], continue.
			 * INQUIRY should not yield UNIT_ATTENTION
			 * but many buggy devices do so anyway. 
			 */
			if (driver_byte(result) == DRIVER_SENSE &&
			    scsi_sense_valid(&sshdr)) {
				if ((sshdr.sense_key == UNIT_ATTENTION) &&
				    ((sshdr.asc == 0x28) ||
				     (sshdr.asc == 0x29)) &&
				    (sshdr.ascq == 0))
					continue;
			}
		} else {
			/*
			 * if nothing was transferred, we try
			 * again. It's a workaround for some USB
			 * devices.
			 */
			if (resid == try_inquiry_len)
				continue;
		}
		break;
	}

	if (result == 0) {
		scsi_sanitize_inquiry_string(&inq_result[8], 8);
		scsi_sanitize_inquiry_string(&inq_result[16], 16);
		scsi_sanitize_inquiry_string(&inq_result[32], 4);

		response_len = inq_result[4] + 5;
		if (response_len > 255)
			response_len = first_inquiry_len;	/* sanity */

		/*
		 * Get any flags for this device.
		 *
		 * XXX add a bflags to scsi_device, and replace the
		 * corresponding bit fields in scsi_device, so bflags
		 * need not be passed as an argument.
		 */
		*bflags = scsi_get_device_flags(sdev, &inq_result[8],
				&inq_result[16]);

		/* When the first pass succeeds we gain information about
		 * what larger transfer lengths might work. */
		if (pass == 1) {
			if (BLIST_INQUIRY_36 & *bflags)
				next_inquiry_len = 36;
			else if (sdev->inquiry_len)
				next_inquiry_len = sdev->inquiry_len;
			else
				next_inquiry_len = response_len;

			/* If more data is available perform the second pass */
			if (next_inquiry_len > try_inquiry_len) {
				try_inquiry_len = next_inquiry_len;
				pass = 2;
				goto next_pass;
			}
		}

	} else if (pass == 2) {
		sdev_printk(KERN_INFO, sdev,
			    "scsi scan: %d byte inquiry failed.  "
			    "Consider BLIST_INQUIRY_36 for this device\n",
			    try_inquiry_len);

		/* If this pass failed, the third pass goes back and transfers
		 * the same amount as we successfully got in the first pass. */
		try_inquiry_len = first_inquiry_len;
		pass = 3;
		goto next_pass;
	}

	/* If the last transfer attempt got an error, assume the
	 * peripheral doesn't exist or is dead. */
	if (result)
		return -EIO;

	/* Don't report any more data than the device says is valid */
	sdev->inquiry_len = min(try_inquiry_len, response_len);

	/*
	 * XXX Abort if the response length is less than 36? If less than
	 * 32, the lookup of the device flags (above) could be invalid,
	 * and it would be possible to take an incorrect action - we do
	 * not want to hang because of a short INQUIRY. On the flip side,
	 * if the device is spun down or becoming ready (and so it gives a
	 * short INQUIRY), an abort here prevents any further use of the
	 * device, including spin up.
	 *
	 * On the whole, the best approach seems to be to assume the first
	 * 36 bytes are valid no matter what the device says.  That's
	 * better than copying < 36 bytes to the inquiry-result buffer
	 * and displaying garbage for the Vendor, Product, or Revision
	 * strings.
	 */
	if (sdev->inquiry_len < 36) {
		if (!sdev->host->short_inquiry) {
			shost_printk(KERN_INFO, sdev->host,
				    "scsi scan: INQUIRY result too short (%d),"
				    " using 36\n", sdev->inquiry_len);
			sdev->host->short_inquiry = 1;
		}
		sdev->inquiry_len = 36;
	}

	/*
	 * Related to the above issue:
	 *
	 * XXX Devices (disk or all?) should be sent a TEST UNIT READY,
	 * and if not ready, sent a START_STOP to start (maybe spin up) and
	 * then send the INQUIRY again, since the INQUIRY can change after
	 * a device is initialized.
	 *
	 * Ideally, start a device if explicitly asked to do so.  This
	 * assumes that a device is spun up on power on, spun down on
	 * request, and then spun up on request.
	 */

	/*
	 * The scanning code needs to know the scsi_level, even if no
	 * device is attached at LUN 0 (SCSI_SCAN_TARGET_PRESENT) so
	 * non-zero LUNs can be scanned.
	 */
	sdev->scsi_level = inq_result[2] & 0x07;
	if (sdev->scsi_level >= 2 ||
	    (sdev->scsi_level == 1 && (inq_result[3] & 0x0f) == 1))
		sdev->scsi_level++;
	sdev->sdev_target->scsi_level = sdev->scsi_level;

	/*
	 * If SCSI-2 or lower, and if the transport requires it,
	 * store the LUN value in CDB[1].
	 */
	sdev->lun_in_cdb = 0;
	if (sdev->scsi_level <= SCSI_2 &&
	    sdev->scsi_level != SCSI_UNKNOWN &&
	    !sdev->host->no_scsi2_lun_in_cdb)
		sdev->lun_in_cdb = 1;

	return 0;
}

/**
 * scsi_add_lun - allocate and fully initialze a scsi_device
 * @sdev:	holds information to be stored in the new scsi_device
 * @inq_result:	holds the result of a previous INQUIRY to the LUN
 * @bflags:	black/white list flag
 * @async:	1 if this device is being scanned asynchronously
 *
 * Description:
 *     Initialize the scsi_device @sdev.  Optionally set fields based
 *     on values in *@bflags.
 *
 * Return:
 *     SCSI_SCAN_NO_RESPONSE: could not allocate or setup a scsi_device
 *     SCSI_SCAN_LUN_PRESENT: a new scsi_device was allocated and initialized
 **/
static int scsi_add_lun(struct scsi_device *sdev, unsigned char *inq_result,
		blist_flags_t *bflags, int async)
{
	int ret;

	/*
	 * XXX do not save the inquiry, since it can change underneath us,
	 * save just vendor/model/rev.
	 *
	 * Rather than save it and have an ioctl that retrieves the saved
	 * value, have an ioctl that executes the same INQUIRY code used
	 * in scsi_probe_lun, let user level programs doing INQUIRY
	 * scanning run at their own risk, or supply a user level program
	 * that can correctly scan.
	 */

	/*
	 * Copy at least 36 bytes of INQUIRY data, so that we don't
	 * dereference unallocated memory when accessing the Vendor,
	 * Product, and Revision strings.  Badly behaved devices may set
	 * the INQUIRY Additional Length byte to a small value, indicating
	 * these strings are invalid, but often they contain plausible data
	 * nonetheless.  It doesn't matter if the device sent < 36 bytes
	 * total, since scsi_probe_lun() initializes inq_result with 0s.
	 */
	sdev->inquiry = kmemdup(inq_result,
				max_t(size_t, sdev->inquiry_len, 36),
				GFP_KERNEL);                                                             // 
	if (sdev->inquiry == NULL)                                                           // 
		return SCSI_SCAN_NO_RESPONSE;                                                    // 

	sdev->vendor = (char *) (sdev->inquiry + 8);                                         // 重点
	sdev->model = (char *) (sdev->inquiry + 16);                                         // 重点
	sdev->rev = (char *) (sdev->inquiry + 32);                                           // 重点

	if (strncmp(sdev->vendor, "ATA     ", 8) == 0) {
		/*
		 * sata emulation layer device.  This is a hack to work around
		 * the SATL power management specifications which state that
		 * when the SATL detects the device has gone into standby
		 * mode, it shall respond with NOT READY.
		 */
		sdev->allow_restart = 1;
	}

	if (*bflags & BLIST_ISROM) {
		sdev->type = TYPE_ROM;
		sdev->removable = 1;
	} else {
		sdev->type = (inq_result[0] & 0x1f);
		sdev->removable = (inq_result[1] & 0x80) >> 7;

		/*
		 * some devices may respond with wrong type for
		 * well-known logical units. Force well-known type
		 * to enumerate them correctly.
		 */
		if (scsi_is_wlun(sdev->lun) && sdev->type != TYPE_WLUN) {
			sdev_printk(KERN_WARNING, sdev,
				"%s: correcting incorrect peripheral device type 0x%x for W-LUN 0x%16xhN\n",
				__func__, sdev->type, (unsigned int)sdev->lun);
			sdev->type = TYPE_WLUN;
		}

	}

	if (sdev->type == TYPE_RBC || sdev->type == TYPE_ROM) {
		/* RBC and MMC devices can return SCSI-3 compliance and yet
		 * still not support REPORT LUNS, so make them act as
		 * BLIST_NOREPORTLUN unless BLIST_REPORTLUN2 is
		 * specifically set */
		if ((*bflags & BLIST_REPORTLUN2) == 0)
			*bflags |= BLIST_NOREPORTLUN;
	}

	/*
	 * For a peripheral qualifier (PQ) value of 1 (001b), the SCSI
	 * spec says: The device server is capable of supporting the
	 * specified peripheral device type on this logical unit. However,
	 * the physical device is not currently connected to this logical
	 * unit.
	 *
	 * The above is vague, as it implies that we could treat 001 and
	 * 011 the same. Stay compatible with previous code, and create a
	 * scsi_device for a PQ of 1
	 *
	 * Don't set the device offline here; rather let the upper
	 * level drivers eval the PQ to decide whether they should
	 * attach. So remove ((inq_result[0] >> 5) & 7) == 1 check.
	 */ 

	sdev->inq_periph_qual = (inq_result[0] >> 5) & 7;
	sdev->lockable = sdev->removable;
	sdev->soft_reset = (inq_result[7] & 1) && ((inq_result[3] & 7) == 2);

	if (sdev->scsi_level >= SCSI_3 ||
			(sdev->inquiry_len > 56 && inq_result[56] & 0x04))
		sdev->ppr = 1;
	if (inq_result[7] & 0x60)
		sdev->wdtr = 1;
	if (inq_result[7] & 0x10)
		sdev->sdtr = 1;

	sdev_printk(KERN_NOTICE, sdev, "%s %.8s %.16s %.4s PQ: %d "
			"ANSI: %d%s\n", scsi_device_type(sdev->type),
			sdev->vendor, sdev->model, sdev->rev,
			sdev->inq_periph_qual, inq_result[2] & 0x07,
			(inq_result[3] & 0x0f) == 1 ? " CCS" : "");

	if ((sdev->scsi_level >= SCSI_2) && (inq_result[7] & 2) &&
	    !(*bflags & BLIST_NOTQ)) {
		sdev->tagged_supported = 1;
		sdev->simple_tags = 1;
	}

	/*
	 * Some devices (Texel CD ROM drives) have handshaking problems
	 * when used with the Seagate controllers. borken is initialized
	 * to 1, and then set it to 0 here.
	 */
	if ((*bflags & BLIST_BORKEN) == 0)
		sdev->borken = 0;

	if (*bflags & BLIST_NO_ULD_ATTACH)
		sdev->no_uld_attach = 1;

	/*
	 * Apparently some really broken devices (contrary to the SCSI
	 * standards) need to be selected without asserting ATN
	 */
	if (*bflags & BLIST_SELECT_NO_ATN)
		sdev->select_no_atn = 1;

	/*
	 * Maximum 512 sector transfer length
	 * broken RA4x00 Compaq Disk Array
	 */
	if (*bflags & BLIST_MAX_512)
		blk_queue_max_hw_sectors(sdev->request_queue, 512);
	/*
	 * Max 1024 sector transfer length for targets that report incorrect
	 * max/optimal lengths and relied on the old block layer safe default
	 */
	else if (*bflags & BLIST_MAX_1024)
		blk_queue_max_hw_sectors(sdev->request_queue, 1024);

	/*
	 * Some devices may not want to have a start command automatically
	 * issued when a device is added.
	 */
	if (*bflags & BLIST_NOSTARTONADD)
		sdev->no_start_on_add = 1;

	if (*bflags & BLIST_SINGLELUN)
		scsi_target(sdev)->single_lun = 1;

	sdev->use_10_for_rw = 1;

	/* some devices don't like REPORT SUPPORTED OPERATION CODES
	 * and will simply timeout causing sd_mod init to take a very
	 * very long time */
	if (*bflags & BLIST_NO_RSOC)
		sdev->no_report_opcodes = 1;

	/* set the device running here so that slave configure
	 * may do I/O */
	mutex_lock(&sdev->state_mutex);
	ret = scsi_device_set_state(sdev, SDEV_RUNNING);
	if (ret)
		ret = scsi_device_set_state(sdev, SDEV_BLOCK);
	mutex_unlock(&sdev->state_mutex);

	if (ret) {
		sdev_printk(KERN_ERR, sdev,
			    "in wrong state %s to complete scan\n",
			    scsi_device_state_name(sdev->sdev_state));
		return SCSI_SCAN_NO_RESPONSE;
	}

	if (*bflags & BLIST_NOT_LOCKABLE)
		sdev->lockable = 0;

	if (*bflags & BLIST_RETRY_HWERROR)
		sdev->retry_hwerror = 1;

	if (*bflags & BLIST_NO_DIF)
		sdev->no_dif = 1;

	if (*bflags & BLIST_UNMAP_LIMIT_WS)
		sdev->unmap_limit_for_ws = 1;

	sdev->eh_timeout = SCSI_DEFAULT_EH_TIMEOUT;

	if (*bflags & BLIST_TRY_VPD_PAGES)
		sdev->try_vpd_pages = 1;
	else if (*bflags & BLIST_SKIP_VPD_PAGES)
		sdev->skip_vpd_pages = 1;

	transport_configure_device(&sdev->sdev_gendev);

	if (sdev->host->hostt->slave_configure) {
		ret = sdev->host->hostt->slave_configure(sdev);                                  // 重点
		if (ret) {
			/*
			 * if LLDD reports slave not present, don't clutter
			 * console with alloc failure messages
			 */
			if (ret != -ENXIO) {
				sdev_printk(KERN_ERR, sdev,
					"failed to configure device\n");
			}
			return SCSI_SCAN_NO_RESPONSE;
		}
	}

	if (sdev->scsi_level >= SCSI_3)
		scsi_attach_vpd(sdev);

	sdev->max_queue_depth = sdev->queue_depth;
	sdev->sdev_bflags = *bflags;

	/*
	 * Ok, the device is now all set up, we can
	 * register it and tell the rest of the kernel
	 * about it.
	 */
	if (!async && scsi_sysfs_add_sdev(sdev) != 0)                                        // 重点
		return SCSI_SCAN_NO_RESPONSE;

	return SCSI_SCAN_LUN_PRESENT;
}

/*
 * INQ PERIPHERAL QUALIFIERS
 */
#define SCSI_INQ_PQ_CON         0x00
#define SCSI_INQ_PQ_NOT_CON     0x01
#define SCSI_INQ_PQ_NOT_CAP     0x03

/* all probing is done in the individual ->probe routines */
static int scsi_bus_match(struct device *dev, struct device_driver *gendrv)
{
	struct scsi_device *sdp;

	if (dev->type != &scsi_dev_type)                                                     // 若dev->type不为&scsi_dev_type
		return 0;                                                                        // 返回0

	sdp = to_scsi_device(dev);                                                           // 获取sdp
	if (sdp->no_uld_attach)                                                              // 若sdp->no_uld_attach不为0
		return 0;                                                                        // 返回0
	return (sdp->inq_periph_qual == SCSI_INQ_PQ_CON)? 1: 0;                              // 若sdp->inq_periph_qual为SCSI_INQ_PQ_CON则返回1，表示匹配到，否则返回0
}

static struct device_type scsi_dev_type = {
	.name =		"scsi_device",
	.release =	scsi_device_dev_release,
	.groups =	scsi_sdev_attr_groups,
};

static int __driver_attach(struct device *dev, void *data)
{
	struct device_driver *drv = data;
	int ret;

	/*
	 * Lock device and try to bind to it. We drop the error
	 * here and always return 0, because we need to keep trying
	 * to bind to devices and some drivers will return an error
	 * simply if it didn't support the device.
	 *
	 * driver_probe_device() will spit a warning if there
	 * is an error.
	 */

	ret = driver_match_device(drv, dev);
	if (ret == 0) {
		/* no match */
		return 0;
	} else if (ret == -EPROBE_DEFER) {
		dev_dbg(dev, "Device match requests probe deferral\n");
		driver_deferred_probe_add(dev);
	} else if (ret < 0) {
		dev_dbg(dev, "Bus failed to match device: %d", ret);
		return ret;
	} /* ret > 0 means positive match */

	if (driver_allows_async_probing(drv)) {
		/*
		 * Instead of probing the device synchronously we will
		 * probe it asynchronously to allow for more parallelism.
		 *
		 * We only take the device lock here in order to guarantee
		 * that the dev->driver and async_driver fields are protected
		 */
		dev_dbg(dev, "probing driver %s asynchronously\n", drv->name);
		device_lock(dev);
		if (!dev->driver) {
			get_device(dev);
			dev->p->async_driver = drv;
			async_schedule_dev(__driver_attach_async_helper, dev);
		}
		device_unlock(dev);
		return 0;
	}

	device_driver_attach(drv, dev);

	return 0;
}

/**
 * bus_probe_device - probe drivers for a new device
 * @dev: device to probe
 *
 * - Automatically probe for a driver if the bus allows it.
 */
void bus_probe_device(struct device *dev)
{
	struct bus_type *bus = dev->bus;
	struct subsys_interface *sif;

	if (!bus)
		return;

	if (bus->p->drivers_autoprobe)
		device_initial_probe(dev);

	mutex_lock(&bus->p->mutex);
	list_for_each_entry(sif, &bus->p->interfaces, node)
		if (sif->add_dev)
			sif->add_dev(dev, sif);
	mutex_unlock(&bus->p->mutex);
}

void device_initial_probe(struct device *dev)
{
	__device_attach(dev, true);
}

static int __device_attach(struct device *dev, bool allow_async)
{
	int ret = 0;

	device_lock(dev);
	if (dev->driver) {
		if (device_is_bound(dev)) {
			ret = 1;
			goto out_unlock;
		}
		ret = device_bind_driver(dev);
		if (ret == 0)
			ret = 1;
		else {
			dev->driver = NULL;
			ret = 0;
		}
	} else {
		struct device_attach_data data = {
			.dev = dev,
			.check_async = allow_async,
			.want_async = false,
		};

		if (dev->parent)
			pm_runtime_get_sync(dev->parent);

		ret = bus_for_each_drv(dev->bus, NULL, &data,
					__device_attach_driver);
		if (!ret && allow_async && data.have_async) {
			/*
			 * If we could not find appropriate driver
			 * synchronously and we are allowed to do
			 * async probes and there are drivers that
			 * want to probe asynchronously, we'll
			 * try them.
			 */
			dev_dbg(dev, "scheduling asynchronous probe\n");
			get_device(dev);
			async_schedule_dev(__device_attach_async_helper, dev);
		} else {
			pm_request_idle(dev);
		}

		if (dev->parent)
			pm_runtime_put(dev->parent);
	}
out_unlock:
	device_unlock(dev);
	return ret;
}

static int __device_attach_driver(struct device_driver *drv, void *_data)
{
	struct device_attach_data *data = _data;
	struct device *dev = data->dev;
	bool async_allowed;
	int ret;

	ret = driver_match_device(drv, dev);
	if (ret == 0) {
		/* no match */
		return 0;
	} else if (ret == -EPROBE_DEFER) {
		dev_dbg(dev, "Device match requests probe deferral\n");
		driver_deferred_probe_add(dev);
	} else if (ret < 0) {
		dev_dbg(dev, "Bus failed to match device: %d", ret);
		return ret;
	} /* ret > 0 means positive match */

	async_allowed = driver_allows_async_probing(drv);

	if (async_allowed)
		data->have_async = true;

	if (data->check_async && async_allowed != data->want_async)
		return 0;

	return driver_probe_device(drv, dev);
}

static inline int driver_match_device(struct device_driver *drv,
				      struct device *dev)
{
	return drv->bus->match ? drv->bus->match(dev, drv) : 1;
}

/**
 * scsi_sysfs_add_sdev - add scsi device to sysfs
 * @sdev:	scsi_device to add
 *
 * Return value:
 * 	0 on Success / non-zero on Failure
 **/
int scsi_sysfs_add_sdev(struct scsi_device *sdev)
{
	int error, i;
	struct request_queue *rq = sdev->request_queue;
	struct scsi_target *starget = sdev->sdev_target;

	error = scsi_target_add(starget);                                                    // 重点
	if (error)
		return error;

	transport_configure_device(&starget->dev);

	device_enable_async_suspend(&sdev->sdev_gendev);
	scsi_autopm_get_target(starget);
	pm_runtime_set_active(&sdev->sdev_gendev);
	pm_runtime_forbid(&sdev->sdev_gendev);
	pm_runtime_enable(&sdev->sdev_gendev);
	scsi_autopm_put_target(starget);

	scsi_autopm_get_device(sdev);

	scsi_dh_add_device(sdev);

	error = device_add(&sdev->sdev_gendev);                                              // 重点：gendisk的基类
	if (error) {
		sdev_printk(KERN_INFO, sdev,
				"failed to add device: %d\n", error);
		return error;
	}

	device_enable_async_suspend(&sdev->sdev_dev);
	error = device_add(&sdev->sdev_dev);                                                 // 重点：
	if (error) {
		sdev_printk(KERN_INFO, sdev,
				"failed to add class device: %d\n", error);
		device_del(&sdev->sdev_gendev);
		return error;
	}
	transport_add_device(&sdev->sdev_gendev);                                            // 
	sdev->is_visible = 1;                                                                // 

	error = bsg_scsi_register_queue(rq, &sdev->sdev_gendev);                             // 
	if (error)
		/* we're treating error on bsg register as non-fatal,
		 * so pretend nothing went wrong */
		sdev_printk(KERN_INFO, sdev,
			    "Failed to register bsg queue, errno=%d\n", error);

	/* add additional host specific attributes */
	if (sdev->host->hostt->sdev_attrs) {
		for (i = 0; sdev->host->hostt->sdev_attrs[i]; i++) {
			error = device_create_file(&sdev->sdev_gendev,
					sdev->host->hostt->sdev_attrs[i]);
			if (error)
				return error;
		}
	}

	if (sdev->host->hostt->sdev_groups) {
		error = sysfs_create_groups(&sdev->sdev_gendev.kobj,
				sdev->host->hostt->sdev_groups);                                         // 
		if (error)
			return error;
	}

	scsi_autopm_put_device(sdev);
	return error;
}

static int scsi_target_add(struct scsi_target *starget)
{
	int error;

	if (starget->state != STARGET_CREATED)
		return 0;

	error = device_add(&starget->dev);                                                   // 重点
	if (error) {
		dev_err(&starget->dev, "target device_add failed, error %d\n", error);
		return error;
	}
	transport_add_device(&starget->dev);
	starget->state = STARGET_RUNNING;

	pm_runtime_set_active(&starget->dev);
	pm_runtime_enable(&starget->dev);
	device_enable_async_suspend(&starget->dev);

	return 0;
}










store_scan
	|--- scsi_scan
			|-- scsi_scan_host_selected
					|-- scsi_scan_channel
							|-- __scsi_scan_target
									|-- scsi_probe_and_add_lun
									|		|-- scsi_add_lun
									|				|-- scsi_sysfs_add_sdev
									|
									|-- scsi_report_lun_scan
									|		|-- scsi_probe_and_add_lun
									|				|-- scsi_add_lun
									|						|-- scsi_sysfs_add_sdev
									|
									|-- scsi_sequential_lun_scan 
									 		|-- scsi_probe_and_add_lun
									 				|-- scsi_add_lun
									 						|-- scsi_sysfs_add_sdev

sdebug_driver_probe                                                                      // 用于debug？
	|-- scsi_scan_host
			|-- do_scsi_scan_host
			|		|-- scsi_scan_host_selected
			|				|-- scsi_scan_channel
			|						|-- __scsi_scan_target
			|								|-- scsi_probe_and_add_lun
			|								|		|-- scsi_add_lun
			|								|				|-- scsi_sysfs_add_sdev
			|								|
			|								|-- scsi_report_lun_scan
			|								|		|-- scsi_probe_and_add_lun
			|								|				|-- scsi_add_lun
			|								|						|-- scsi_sysfs_add_sdev
			|								|
			|								|-- scsi_sequential_lun_scan 
			|								 		|-- scsi_probe_and_add_lun
			|								 				|-- scsi_add_lun
			|								 						|-- scsi_sysfs_add_sdev
			|
			|-- async_schedule(do_scan_async, data)
					>>> do_scan_async
							|-- do_scsi_scan_host
									|-- scsi_scan_host_selected
											|-- scsi_scan_channel
													|-- __scsi_scan_target
															|-- scsi_probe_and_add_lun
															|		|-- scsi_add_lun
															|				|-- scsi_sysfs_add_sdev
															|
															|-- scsi_report_lun_scan
															|		|-- scsi_probe_and_add_lun
															|				|-- scsi_add_lun
															|						|-- scsi_sysfs_add_sdev
															|
															|-- scsi_sequential_lun_scan 
																	|-- scsi_probe_and_add_lun
																			|-- scsi_add_lun
																					|-- scsi_sysfs_add_sdev


proc_scsi_write
	|-- scsi_add_single_device
			|-- scsi_scan_host_selected
					|-- scsi_scan_channel
							|-- __scsi_scan_target
									|-- scsi_probe_and_add_lun
									|		|-- scsi_add_lun
									|				|-- scsi_sysfs_add_sdev
									|
									|-- scsi_report_lun_scan
									|		|-- scsi_probe_and_add_lun
									|				|-- scsi_add_lun
									|						|-- scsi_sysfs_add_sdev
									|
									|-- scsi_sequential_lun_scan 
									 		|-- scsi_probe_and_add_lun
									 				|-- scsi_add_lun
									 						|-- scsi_sysfs_add_sdev


ufshcd_init
	|-- async_schedule(ufshcd_async_scan, hba)
			>>> ufshcd_async_scan
					|-- ufshcd_probe_hba
							|-- ufshcd_scsi_add_wlus
							|		|-- __scsi_add_device
							|		 		|-- scsi_probe_and_add_lun
							|		 				|-- scsi_add_lun
							|		 						|-- scsi_sysfs_add_sdev
							|-- scsi_scan_host
									|-- do_scsi_scan_host
									|		|-- scsi_scan_host_selected
									|				|-- scsi_scan_channel
									|						|-- __scsi_scan_target
									|								|-- scsi_probe_and_add_lun
									|								|		|-- scsi_add_lun
									|								|				|-- scsi_sysfs_add_sdev
									|								|
									|								|-- scsi_report_lun_scan
									|								|		|-- scsi_probe_and_add_lun
									|								|				|-- scsi_add_lun
									|								|						|-- scsi_sysfs_add_sdev
									|								|
									|								|-- scsi_sequential_lun_scan 
									|								 		|-- scsi_probe_and_add_lun
									|								 				|-- scsi_add_lun
									|								 						|-- scsi_sysfs_add_sdev
									|
									|-- async_schedule(do_scan_async, data)
											>>> do_scan_async
													|-- do_scsi_scan_host
															|-- scsi_scan_host_selected
																	|-- scsi_scan_channel
																			|-- __scsi_scan_target
																					|-- scsi_probe_and_add_lun
																					|		|-- scsi_add_lun
																					|				|-- scsi_sysfs_add_sdev
																					|
																					|-- scsi_report_lun_scan
																					|		|-- scsi_probe_and_add_lun
																					|				|-- scsi_add_lun
																					|						|-- scsi_sysfs_add_sdev
																					|
																					|-- scsi_sequential_lun_scan 
																							|-- scsi_probe_and_add_lun
																									|-- scsi_add_lun
																											|-- scsi_sysfs_add_sdev

ufshcd_host_reset_and_restore
	|-- ufshcd_probe_hba
			|-- ufshcd_scsi_add_wlus
			|		|-- __scsi_add_device
			|		 		|-- scsi_probe_and_add_lun
			|		 				|-- scsi_add_lun
			|		 						|-- scsi_sysfs_add_sdev
			|-- scsi_scan_host
			 		|-- do_scsi_scan_host
		 			|		|-- scsi_scan_host_selected
			 		|				|-- scsi_scan_channel
			 		|						|-- __scsi_scan_target
			 		|								|-- scsi_probe_and_add_lun
			 		|								|		|-- scsi_add_lun
			 		|								|				|-- scsi_sysfs_add_sdev
			 		|								|
			 		|								|-- scsi_report_lun_scan
			 		|								|		|-- scsi_probe_and_add_lun
			 		|								|				|-- scsi_add_lun
					|								|						|-- scsi_sysfs_add_sdev
			 		|								|
			 		|								|-- scsi_sequential_lun_scan 
			 		|								 		|-- scsi_probe_and_add_lun
			 		|								 				|-- scsi_add_lun
			 		|								 						|-- scsi_sysfs_add_sdev
					|
			 		|-- async_schedule(do_scan_async, data)
			 				>>> do_scan_async
			 						|-- do_scsi_scan_host
			 								|-- scsi_scan_host_selected
			 										|-- scsi_scan_channel
			 												|-- __scsi_scan_target
			 														|-- scsi_probe_and_add_lun
			 														|		|-- scsi_add_lun
			 														|				|-- scsi_sysfs_add_sdev
			 														|
			 														|-- scsi_report_lun_scan
			 														|		|-- scsi_probe_and_add_lun
			 														|				|-- scsi_add_lun
																	|						|-- scsi_sysfs_add_sdev
			 														|
			 														|-- scsi_sequential_lun_scan 
			 														 		|-- scsi_probe_and_add_lun
			 														 				|-- scsi_add_lun
			 														 						|-- scsi_sysfs_add_sdev




scsi_add_device                                                                          // 导出接口，ufs子系统未查到使用之处
	|-- __scsi_add_device
	 		|-- scsi_probe_and_add_lun
	 				|-- scsi_add_lun
	 						|-- scsi_sysfs_add_sdev

scsi_scan_target                                                                         // 导出接口，ufs子系统未查到使用之处
	|-- __scsi_scan_target
			|-- scsi_probe_and_add_lun
			|		|-- scsi_add_lun
			|				|-- scsi_sysfs_add_sdev
			|
			|-- scsi_report_lun_scan
			|		|-- scsi_probe_and_add_lun
			|				|-- scsi_add_lun
			|						|-- scsi_sysfs_add_sdev
			|
			|-- scsi_sequential_lun_scan 
			 		|-- scsi_probe_and_add_lun
			 				|-- scsi_add_lun
			 						|-- scsi_sysfs_add_sdev


scsi_scan_host                                                                           // 导出接口
	|-- do_scan_async
			|-- scsi_finish_async_scan
					|-- scsi_sysfs_add_devices
							|-- scsi_sysfs_add_sdev

#define	to_scsi_device(d)	\
	container_of(d, struct scsi_device, sdev_gendev)





/**
 *	sd_probe - called during driver initialization and whenever a
 *	new scsi device is attached to the system. It is called once
 *	for each scsi device (not just disks) present.
 *	@dev: pointer to device object
 *
 *	Returns 0 if successful (or not interested in this scsi device 
 *	(e.g. scanner)); 1 when there is an error.
 *
 *	Note: this function is invoked from the scsi mid-level.
 *	This function sets up the mapping between a given 
 *	<host,channel,id,lun> (found in sdp) and new device name 
 *	(e.g. /dev/sda). More precisely it is the block device major 
 *	and minor number that is chosen here.
 *
 *	Assume sd_probe is not re-entrant (for time being)
 *	Also think about sd_probe() and sd_remove() running coincidentally.
 **/
static int sd_probe(struct device *dev)
{
	struct scsi_device *sdp = to_scsi_device(dev);                                       // 重点：container_of(dev, struct scsi_device, sdev_gendev)，表明在添加scsi->sdev_gendev时会触发sd_probe的调用
	struct scsi_disk *sdkp;
	struct gendisk *gd;
	int index;
	int error;

	scsi_autopm_get_device(sdp);                                                         // pm_runtime_get_sync(&sdp->sdev_gendev)，目的为何？
	error = -ENODEV;                                                                     // 设置error为-ENODEV
	if (sdp->type != TYPE_DISK &&
	    sdp->type != TYPE_ZBC &&
	    sdp->type != TYPE_MOD &&
	    sdp->type != TYPE_RBC)                                                           // 若sdp->type不为TYPE_DISK，并且不为TYPE_ZBC，并且不为TYPE_MOD，并且不为TYPE_RBC
		goto out;                                                                        // 跳转至out

#ifndef CONFIG_BLK_DEV_ZONED
	if (sdp->type == TYPE_ZBC)                                                           // 若sdp->type为TYPE_ZBC
		goto out;                                                                        // 跳转至out
#endif
	SCSI_LOG_HLQUEUE(3, sdev_printk(KERN_INFO, sdp,
					"sd_probe\n"));                                                      // 打印提示信息

	error = -ENOMEM;                                                                     // 设置error为-ENOMEM
	sdkp = kzalloc(sizeof(*sdkp), GFP_KERNEL);                                           // 重点：分配sdkp，这是scsi disk的抽象
	if (!sdkp)                                                                           // 若sdkp为NULL
		goto out;                                                                        // 跳转至out

	gd = alloc_disk(SD_MINORS);                                                          // 分配gd
	if (!gd)                                                                             // 若gd为NULL
		goto out_free;                                                                   // 跳转至out_free

	index = ida_alloc(&sd_index_ida, GFP_KERNEL);                                        // 申请index
	if (index < 0) {                                                                     // 若index < 0为true
		sdev_printk(KERN_WARNING, sdp, "sd_probe: memory exhausted.\n");                 // 打印提示信息
		goto out_put;                                                                    // 跳转至out_put
	}

	error = sd_format_disk_name("sd", index, gd->disk_name, DISK_NAME_LEN);              // 重点：
	if (error) {                                                                         // 若error不为0
		sdev_printk(KERN_WARNING, sdp, "SCSI disk (sd) name length exceeded.\n");        // 打印提示信息
		goto out_free_index;                                                             // 跳转至out_free_index
	}

	sdkp->device = sdp;                                                                  // 重点：设置sdkp->device为sdp
	sdkp->driver = &sd_template;                                                         // 重点：设置sdkp->driver为&sd_template
	sdkp->disk = gd;                                                                     // 重点：设置sdkp->disk为gd
	sdkp->index = index;                                                                 // 设置sdkp->index为index
	atomic_set(&sdkp->openers, 0);                                                       // 设置sdkp->openers为0
	atomic_set(&sdkp->device->ioerr_cnt, 0);                                             // 设置sdkp->device->ioerr_cnt为0

	if (!sdp->request_queue->rq_timeout) {                                               // 若sdp->request_queue->rq_timeout为0
		if (sdp->type != TYPE_MOD)                                                       // 若sdp->type不为TYPE_MOD
			blk_queue_rq_timeout(sdp->request_queue, SD_TIMEOUT);                        // 
		else
			blk_queue_rq_timeout(sdp->request_queue,
					     SD_MOD_TIMEOUT);                                                // 
	}

	device_initialize(&sdkp->dev);                                                       // 重点：初始化sdkp->dev，这是sdkp在设备模型中的代表
	sdkp->dev.parent = dev;                                                              // 设置sdkp->dev.parent为dev
	sdkp->dev.class = &sd_disk_class;                                                    // 重点：设置sdkp->dev.class为&sd_disk_class
	dev_set_name(&sdkp->dev, "%s", dev_name(dev));                                       // 重点：分配sdkp->dev.kobj.name空间并设置为dev_name(dev)

	error = device_add(&sdkp->dev);                                                      // 重点：将sdkp->dev添加到sysfs（设备模型）中
	if (error)                                                                           // 若error不为0
		goto out_free_index;                                                             // 跳转至out_free_index

	get_device(dev);                                                                     // refcount_inc(&dev->kobj.kref.refcount)
	dev_set_drvdata(dev, sdkp);                                                          // dev->driver_data = sdkp

	gd->major = sd_major((index & 0xf0) >> 4);                                           // 
	gd->first_minor = ((index & 0xf) << 4) | (index & 0xfff00);                          // 

	gd->fops = &sd_fops;                                                                 // 设置gd->fops为&sd_fops
	gd->private_data = &sdkp->driver;                                                    // 设置gd->private_data为&sdkp->driver
	gd->queue = sdkp->device->request_queue;                                             // 重点：设置gd->queue为sdkp->device->request_queue

	/* defaults, until the device tells us otherwise */
	sdp->sector_size = 512;                                                              // 设置sdp->sector_size为512
	sdkp->capacity = 0;                                                                  // 设置sdkp->capacity为0
	sdkp->media_present = 1;                                                             // 设置sdkp->media_present为1
	sdkp->write_prot = 0;                                                                // 设置sdkp->write_prot为0
	sdkp->cache_override = 0;                                                            // 设置sdkp->cache_override为0
	sdkp->WCE = 0;                                                                       // 设置sdkp->WCE为0
	sdkp->RCD = 0;                                                                       // 设置sdkp->RCD为0
	sdkp->ATO = 0;                                                                       // 设置sdkp->ATO为0
	sdkp->first_scan = 1;                                                                // 设置sdkp->first_scan为1
	sdkp->max_medium_access_timeouts = SD_MAX_MEDIUM_TIMEOUTS;                           // 设置sdkp->max_medium_access_timeouts为SD_MAX_MEDIUM_TIMEOUTS

	sd_revalidate_disk(gd);                                                              // 

	gd->flags = GENHD_FL_EXT_DEVT;                                                       // 
	if (sdp->removable) {                                                                //
		gd->flags |= GENHD_FL_REMOVABLE;                                                 // 
		gd->events |= DISK_EVENT_MEDIA_CHANGE;                                           // 
		gd->event_flags = DISK_EVENT_FLAG_POLL | DISK_EVENT_FLAG_UEVENT;                 // 
	}

	blk_pm_runtime_init(sdp->request_queue, dev);                                        // 
	device_add_disk(dev, gd, NULL);                                                      // 重点：注册gendisk，将生成设备节点
	if (sdkp->capacity)                                                                  // 若sdkp->capacity不为0
		sd_dif_config_host(sdkp);                                                        // 

	sd_revalidate_disk(gd);                                                              // 

	if (sdkp->security) {                                                                // 若sdkp->security不为0
		sdkp->opal_dev = init_opal_dev(sdp, &sd_sec_submit);                             // 
		if (sdkp->opal_dev)                                                              // 若sdkp->opal_dev不为NULL
			sd_printk(KERN_NOTICE, sdkp, "supports TCG Opal\n");                         // 打印提示信息
	}

	sd_printk(KERN_NOTICE, sdkp, "Attached SCSI %sdisk\n",
		  sdp->removable ? "removable " : "");                                           // 打印提示信息
	scsi_autopm_put_device(sdp);                                                         // pm_runtime_put_sync(&sdp->sdev_gendev);

	return 0;                                                                            // 返回0

 out_free_index:
	ida_free(&sd_index_ida, index);
 out_put:
	put_disk(gd);
 out_free:
	kfree(sdkp);
 out:
	scsi_autopm_put_device(sdp);
	return error;
}

/**
 *	sd_revalidate_disk - called the first time a new disk is seen,
 *	performs disk spin up, read_capacity, etc.
 *	@disk: struct gendisk we care about
 **/
static int sd_revalidate_disk(struct gendisk *disk)
{
	struct scsi_disk *sdkp = scsi_disk(disk);                                            // 重点：container_of(dev, struct scsi_device, sdev_gendev)，表明在添加scsi->sdev_gendev时会触发sd_probe的调用
	struct scsi_device *sdp = sdkp->device;                                              // 取出sdp
	struct request_queue *q = sdkp->disk->queue;                                         // 取出q
	sector_t old_capacity = sdkp->capacity;                                              // 取出old_capacity
	unsigned char *buffer;
	unsigned int dev_max, rw_max;

	SCSI_LOG_HLQUEUE(3, sd_printk(KERN_INFO, sdkp,
				      "sd_revalidate_disk\n"));                                          // 打印提示信息

	/*
	 * If the device is offline, don't try and read capacity or any
	 * of the other niceties.
	 */
	if (!scsi_device_online(sdp))                                                        // 
		goto out;                                                                        // 跳转至out

	buffer = kmalloc(SD_BUF_SIZE, GFP_KERNEL);                                           // 
	if (!buffer) {                                                                       // 
		sd_printk(KERN_WARNING, sdkp, "sd_revalidate_disk: Memory "
			  "allocation failure.\n");                                                  // 
		goto out;                                                                        // 跳转至out
	}

	sd_spinup_disk(sdkp);                                                                // 

	/*
	 * Without media there is no reason to ask; moreover, some devices
	 * react badly if we do.
	 */
	if (sdkp->media_present) {                                                           // 
		sd_read_capacity(sdkp, buffer);                                                  // 

		/*
		 * set the default to rotational.  All non-rotational devices
		 * support the block characteristics VPD page, which will
		 * cause this to be updated correctly and any device which
		 * doesn't support it should be treated as rotational.
		 */
		blk_queue_flag_clear(QUEUE_FLAG_NONROT, q);                                      // 
		blk_queue_flag_set(QUEUE_FLAG_ADD_RANDOM, q);                                    // 

		if (scsi_device_supports_vpd(sdp)) {                                             // 
			sd_read_block_provisioning(sdkp);                                            // 
			sd_read_block_limits(sdkp);                                                  // 
			sd_read_block_characteristics(sdkp);                                         // 
			sd_zbc_read_zones(sdkp, buffer);                                             // 
		}

		sd_print_capacity(sdkp, old_capacity);                                           // 

		sd_read_write_protect_flag(sdkp, buffer);                                        // 
		sd_read_cache_type(sdkp, buffer);                                                // 
		sd_read_app_tag_own(sdkp, buffer);                                               // 
		sd_read_write_same(sdkp, buffer);                                                // 
		sd_read_security(sdkp, buffer);                                                  // 
	}

	/*
	 * We now have all cache related info, determine how we deal
	 * with flush requests.
	 */
	sd_set_flush_flag(sdkp);                                                             // 

	/* Initial block count limit based on CDB TRANSFER LENGTH field size. */
	dev_max = sdp->use_16_for_rw ? SD_MAX_XFER_BLOCKS : SD_DEF_XFER_BLOCKS;              // 

	/* Some devices report a maximum block count for READ/WRITE requests. */
	dev_max = min_not_zero(dev_max, sdkp->max_xfer_blocks);                              // 
	q->limits.max_dev_sectors = logical_to_sectors(sdp, dev_max);                        // 

	if (sd_validate_opt_xfer_size(sdkp, dev_max)) {                                      // 
		q->limits.io_opt = logical_to_bytes(sdp, sdkp->opt_xfer_blocks);                 // 
		rw_max = logical_to_sectors(sdp, sdkp->opt_xfer_blocks);                         // 
	} else
		rw_max = min_not_zero(logical_to_sectors(sdp, dev_max),
				      (sector_t)BLK_DEF_MAX_SECTORS);                                    // 

	/* Do not exceed controller limit */
	rw_max = min(rw_max, queue_max_hw_sectors(q));                                       // 

	/*
	 * Only update max_sectors if previously unset or if the current value
	 * exceeds the capabilities of the hardware.
	 */
	if (sdkp->first_scan ||
	    q->limits.max_sectors > q->limits.max_dev_sectors ||
	    q->limits.max_sectors > q->limits.max_hw_sectors)                                // 
		q->limits.max_sectors = rw_max;                                                  // 

	sdkp->first_scan = 0;                                                                // 

	set_capacity(disk, logical_to_sectors(sdp, sdkp->capacity));                         // 
	sd_config_write_same(sdkp);                                                          // 
	kfree(buffer);                                                                       // 

 out:
	return 0;                                                                            // 返回0
}







int scsi_autopm_get_device(struct scsi_device *sdev)
{
	int	err;

	err = pm_runtime_get_sync(&sdev->sdev_gendev);
	if (err < 0 && err !=-EACCES)
		pm_runtime_put_sync(&sdev->sdev_gendev);
	else
		err = 0;
	return err;
}
EXPORT_SYMBOL_GPL(scsi_autopm_get_device);

/**
 *	sd_format_disk_name - format disk name
 *	@prefix: name prefix - ie. "sd" for SCSI disks
 *	@index: index of the disk to format name for
 *	@buf: output buffer
 *	@buflen: length of the output buffer
 *
 *	SCSI disk names starts at sda.  The 26th device is sdz and the
 *	27th is sdaa.  The last one for two lettered suffix is sdzz
 *	which is followed by sdaaa.
 *
 *	This is basically 26 base counting with one extra 'nil' entry
 *	at the beginning from the second digit on and can be
 *	determined using similar method as 26 base conversion with the
 *	index shifted -1 after each digit is computed.
 *
 *	CONTEXT:
 *	Don't care.
 *
 *	RETURNS:
 *	0 on success, -errno on failure.
 */
static int sd_format_disk_name(char *prefix, int index, char *buf, int buflen)
{
	const int base = 'z' - 'a' + 1;
	char *begin = buf + strlen(prefix);
	char *end = buf + buflen;
	char *p;
	int unit;

	p = end - 1;
	*p = '\0';
	unit = base;
	do {
		if (p == begin)
			return -EINVAL;
		*--p = 'a' + (index % unit);
		index = (index / unit) - 1;
	} while (index >= 0);

	memmove(begin, p, end - p);
	memcpy(buf, prefix, strlen(prefix));

	return 0;
}

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

struct request_queue *scsi_mq_alloc_queue(struct scsi_device *sdev)
{
	sdev->request_queue = blk_mq_init_queue(&sdev->host->tag_set);
	if (IS_ERR(sdev->request_queue))
		return NULL;

	sdev->request_queue->queuedata = sdev;
	__scsi_init_queue(sdev->host, sdev->request_queue);
	blk_queue_flag_set(QUEUE_FLAG_SCSI_PASSTHROUGH, sdev->request_queue);
	return sdev->request_queue;
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
	q = blk_mq_init_allocated_queue(set, uninit_q, false);                               // 重点：参考注释，elevator将在device_add_disk中初始化
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

	blk_queue_make_request(q, blk_mq_make_request);                                      // 重点

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







































