
static int __init init_scsi(void)
{
	int error;

	error = scsi_init_queue();
	if (error)
		return error;
	error = scsi_init_procfs();
	if (error)
		goto cleanup_queue;
	error = scsi_init_devinfo();                                                         // set up the dynamic device list
	if (error)                                                                           // 若error不为0
		goto cleanup_procfs;                                                             // 跳转至cleanup_procfs
	error = scsi_init_hosts();
	if (error)
		goto cleanup_devlist;
	error = scsi_init_sysctl();
	if (error)
		goto cleanup_hosts;
	error = scsi_sysfs_register();
	if (error)
		goto cleanup_sysctl;

	scsi_netlink_init();

	printk(KERN_NOTICE "SCSI subsystem initialized\n");
	return 0;

cleanup_sysctl:
	scsi_exit_sysctl();
cleanup_hosts:
	scsi_exit_hosts();
cleanup_devlist:
	scsi_exit_devinfo();
cleanup_procfs:
	scsi_exit_procfs();
cleanup_queue:
	scsi_exit_queue();
	printk(KERN_ERR "SCSI subsystem failed to initialize, error = %d\n",
	       -error);
	return error;
}

/*
 * scsi_static_device_list: deprecated list of devices that require
 * settings that differ from the default, includes black-listed (broken)
 * devices. The entries here are added to the tail of scsi_dev_info_list
 * via scsi_dev_info_list_init.
 *
 * Do not add to this list, use the command line or proc interface to add
 * to the scsi_dev_info_list. This table will eventually go away.
 */
static struct {
	char *vendor;
	char *model;
	char *revision;	/* revision known to be bad, unused */
	blist_flags_t flags;
} scsi_static_device_list[] __initdata = {
	/*
	 * The following devices are known not to tolerate a lun != 0 scan
	 * for one reason or another. Some will respond to all luns,
	 * others will lock up.
	 */
	{"Aashima", "IMAGERY 2400SP", "1.03", BLIST_NOLUN},	/* locks up */
	{"CHINON", "CD-ROM CDS-431", "H42", BLIST_NOLUN},	/* locks up */
	{"CHINON", "CD-ROM CDS-535", "Q14", BLIST_NOLUN},	/* locks up */
	{"DENON", "DRD-25X", "V", BLIST_NOLUN},			/* locks up */
	{"HITACHI", "DK312C", "CM81", BLIST_NOLUN},	/* responds to all lun */
	{"HITACHI", "DK314C", "CR21", BLIST_NOLUN},	/* responds to all lun */
	{"IBM", "2104-DU3", NULL, BLIST_NOLUN},		/* locks up */
	{"IBM", "2104-TU3", NULL, BLIST_NOLUN},		/* locks up */
	{"IMS", "CDD521/10", "2.06", BLIST_NOLUN},	/* locks up */
	{"MAXTOR", "XT-3280", "PR02", BLIST_NOLUN},	/* locks up */
	{"MAXTOR", "XT-4380S", "B3C", BLIST_NOLUN},	/* locks up */
	{"MAXTOR", "MXT-1240S", "I1.2", BLIST_NOLUN},	/* locks up */
	{"MAXTOR", "XT-4170S", "B5A", BLIST_NOLUN},	/* locks up */
	{"MAXTOR", "XT-8760S", "B7B", BLIST_NOLUN},	/* locks up */
	{"MEDIAVIS", "RENO CD-ROMX2A", "2.03", BLIST_NOLUN},	/* responds to all lun */
	{"MICROTEK", "ScanMakerIII", "2.30", BLIST_NOLUN},	/* responds to all lun */
	{"NEC", "CD-ROM DRIVE:841", "1.0", BLIST_NOLUN},/* locks up */
	{"PHILIPS", "PCA80SC", "V4-2", BLIST_NOLUN},	/* responds to all lun */
	{"RODIME", "RO3000S", "2.33", BLIST_NOLUN},	/* locks up */
	{"SUN", "SENA", NULL, BLIST_NOLUN},		/* responds to all luns */
	/*
	 * The following causes a failed REQUEST SENSE on lun 1 for
	 * aha152x controller, which causes SCSI code to reset bus.
	 */
	{"SANYO", "CRD-250S", "1.20", BLIST_NOLUN},
	/*
	 * The following causes a failed REQUEST SENSE on lun 1 for
	 * aha152x controller, which causes SCSI code to reset bus.
	 */
	{"SEAGATE", "ST157N", "\004|j", BLIST_NOLUN},
	{"SEAGATE", "ST296", "921", BLIST_NOLUN},	/* responds to all lun */
	{"SEAGATE", "ST1581", "6538", BLIST_NOLUN},	/* responds to all lun */
	{"SONY", "CD-ROM CDU-541", "4.3d", BLIST_NOLUN},
	{"SONY", "CD-ROM CDU-55S", "1.0i", BLIST_NOLUN},
	{"SONY", "CD-ROM CDU-561", "1.7x", BLIST_NOLUN},
	{"SONY", "CD-ROM CDU-8012", NULL, BLIST_NOLUN},
	{"SONY", "SDT-5000", "3.17", BLIST_SELECT_NO_ATN},
	{"TANDBERG", "TDC 3600", "U07", BLIST_NOLUN},	/* locks up */
	{"TEAC", "CD-R55S", "1.0H", BLIST_NOLUN},	/* locks up */
	/*
	 * The following causes a failed REQUEST SENSE on lun 1 for
	 * seagate controller, which causes SCSI code to reset bus.
	 */
	{"TEAC", "CD-ROM", "1.06", BLIST_NOLUN},
	{"TEAC", "MT-2ST/45S2-27", "RV M", BLIST_NOLUN},	/* responds to all lun */
	/*
	 * The following causes a failed REQUEST SENSE on lun 1 for
	 * seagate controller, which causes SCSI code to reset bus.
	 */
	{"HP", "C1750A", "3226", BLIST_NOLUN},		/* scanjet iic */
	{"HP", "C1790A", NULL, BLIST_NOLUN},		/* scanjet iip */
	{"HP", "C2500A", NULL, BLIST_NOLUN},		/* scanjet iicx */
	{"MEDIAVIS", "CDR-H93MV", "1.31", BLIST_NOLUN},	/* locks up */
	{"MICROTEK", "ScanMaker II", "5.61", BLIST_NOLUN},	/* responds to all lun */
	{"MITSUMI", "CD-R CR-2201CS", "6119", BLIST_NOLUN},	/* locks up */
	{"NEC", "D3856", "0009", BLIST_NOLUN},
	{"QUANTUM", "LPS525S", "3110", BLIST_NOLUN},	/* locks up */
	{"QUANTUM", "PD1225S", "3110", BLIST_NOLUN},	/* locks up */
	{"QUANTUM", "FIREBALL ST4.3S", "0F0C", BLIST_NOLUN},	/* locks up */
	{"RELISYS", "Scorpio", NULL, BLIST_NOLUN},	/* responds to all lun */
	{"SANKYO", "CP525", "6.64", BLIST_NOLUN},	/* causes failed REQ SENSE, extra reset */
	{"TEXEL", "CD-ROM", "1.06", BLIST_NOLUN | BLIST_BORKEN},
	{"transtec", "T5008", "0001", BLIST_NOREPORTLUN },
	{"YAMAHA", "CDR100", "1.00", BLIST_NOLUN},	/* locks up */
	{"YAMAHA", "CDR102", "1.00", BLIST_NOLUN},	/* locks up */
	{"YAMAHA", "CRW8424S", "1.0", BLIST_NOLUN},	/* locks up */
	{"YAMAHA", "CRW6416S", "1.0c", BLIST_NOLUN},	/* locks up */
	{"", "Scanner", "1.80", BLIST_NOLUN},	/* responds to all lun */

	/*
	 * Other types of devices that have special flags.
	 * Note that all USB devices should have the BLIST_INQUIRY_36 flag.
	 */
	{"3PARdata", "VV", NULL, BLIST_REPORTLUN2},
	{"ADAPTEC", "AACRAID", NULL, BLIST_FORCELUN},
	{"ADAPTEC", "Adaptec 5400S", NULL, BLIST_FORCELUN},
	{"AIX", "VDASD", NULL, BLIST_TRY_VPD_PAGES},
	{"AFT PRO", "-IX CF", "0.0>", BLIST_FORCELUN},
	{"BELKIN", "USB 2 HS-CF", "1.95",  BLIST_FORCELUN | BLIST_INQUIRY_36},
	{"BROWNIE", "1200U3P", NULL, BLIST_NOREPORTLUN},
	{"BROWNIE", "1600U3P", NULL, BLIST_NOREPORTLUN},
	{"CANON", "IPUBJD", NULL, BLIST_SPARSELUN},
	{"CBOX3", "USB Storage-SMC", "300A", BLIST_FORCELUN | BLIST_INQUIRY_36},
	{"CMD", "CRA-7280", NULL, BLIST_SPARSELUN},	/* CMD RAID Controller */
	{"CNSI", "G7324", NULL, BLIST_SPARSELUN},	/* Chaparral G7324 RAID */
	{"CNSi", "G8324", NULL, BLIST_SPARSELUN},	/* Chaparral G8324 RAID */
	{"COMPAQ", "ARRAY CONTROLLER", NULL, BLIST_SPARSELUN | BLIST_LARGELUN |
		BLIST_MAX_512 | BLIST_REPORTLUN2},	/* Compaq RA4x00 */
	{"COMPAQ", "LOGICAL VOLUME", NULL, BLIST_FORCELUN | BLIST_MAX_512}, /* Compaq RA4x00 */
	{"COMPAQ", "CR3500", NULL, BLIST_FORCELUN},
	{"COMPAQ", "MSA1000", NULL, BLIST_SPARSELUN | BLIST_NOSTARTONADD},
	{"COMPAQ", "MSA1000 VOLUME", NULL, BLIST_SPARSELUN | BLIST_NOSTARTONADD},
	{"COMPAQ", "HSV110", NULL, BLIST_REPORTLUN2 | BLIST_NOSTARTONADD},
	{"DDN", "SAN DataDirector", "*", BLIST_SPARSELUN},
	{"DEC", "HSG80", NULL, BLIST_REPORTLUN2 | BLIST_NOSTARTONADD},
	{"DELL", "PV660F", NULL, BLIST_SPARSELUN},
	{"DELL", "PV660F   PSEUDO", NULL, BLIST_SPARSELUN},
	{"DELL", "PSEUDO DEVICE .", NULL, BLIST_SPARSELUN},	/* Dell PV 530F */
	{"DELL", "PV530F", NULL, BLIST_SPARSELUN},
	{"DELL", "PERCRAID", NULL, BLIST_FORCELUN},
	{"DGC", "RAID", NULL, BLIST_SPARSELUN},	/* EMC CLARiiON, storage on LUN 0 */
	{"DGC", "DISK", NULL, BLIST_SPARSELUN},	/* EMC CLARiiON, no storage on LUN 0 */
	{"EMC",  "Invista", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"EMC", "SYMMETRIX", NULL, BLIST_SPARSELUN | BLIST_LARGELUN |
	 BLIST_REPORTLUN2 | BLIST_RETRY_ITF},
	{"EMULEX", "MD21/S2     ESDI", NULL, BLIST_SINGLELUN},
	{"easyRAID", "16P", NULL, BLIST_NOREPORTLUN},
	{"easyRAID", "X6P", NULL, BLIST_NOREPORTLUN},
	{"easyRAID", "F8", NULL, BLIST_NOREPORTLUN},
	{"FSC", "CentricStor", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"FUJITSU", "ETERNUS_DXM", "*", BLIST_RETRY_ASC_C1},
	{"Generic", "USB SD Reader", "1.00", BLIST_FORCELUN | BLIST_INQUIRY_36},
	{"Generic", "USB Storage-SMC", NULL, BLIST_FORCELUN | BLIST_INQUIRY_36}, /* FW: 0180 and 0207 */
	{"HITACHI", "DF400", "*", BLIST_REPORTLUN2},
	{"HITACHI", "DF500", "*", BLIST_REPORTLUN2},
	{"HITACHI", "DISK-SUBSYSTEM", "*", BLIST_REPORTLUN2},
	{"HITACHI", "HUS1530", "*", BLIST_NO_DIF},
	{"HITACHI", "OPEN-", "*", BLIST_REPORTLUN2 | BLIST_TRY_VPD_PAGES},
	{"HP", "A6189A", NULL, BLIST_SPARSELUN | BLIST_LARGELUN},	/* HP VA7400 */
	{"HP", "OPEN-", "*", BLIST_REPORTLUN2 | BLIST_TRY_VPD_PAGES}, /* HP XP Arrays */
	{"HP", "NetRAID-4M", NULL, BLIST_FORCELUN},
	{"HP", "HSV100", NULL, BLIST_REPORTLUN2 | BLIST_NOSTARTONADD},
	{"HP", "C1557A", NULL, BLIST_FORCELUN},
	{"HP", "C3323-300", "4269", BLIST_NOTQ},
	{"HP", "C5713A", NULL, BLIST_NOREPORTLUN},
	{"HP", "DISK-SUBSYSTEM", "*", BLIST_REPORTLUN2},
	{"IBM", "AuSaV1S2", NULL, BLIST_FORCELUN},
	{"IBM", "ProFibre 4000R", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"IBM", "2105", NULL, BLIST_RETRY_HWERROR},
	{"iomega", "jaz 1GB", "J.86", BLIST_NOTQ | BLIST_NOLUN},
	{"IOMEGA", "ZIP", NULL, BLIST_NOTQ | BLIST_NOLUN},
	{"IOMEGA", "Io20S         *F", NULL, BLIST_KEY},
	{"INSITE", "Floptical   F*8I", NULL, BLIST_KEY},
	{"INSITE", "I325VM", NULL, BLIST_KEY},
	{"Intel", "Multi-Flex", NULL, BLIST_NO_RSOC},
	{"iRiver", "iFP Mass Driver", NULL, BLIST_NOT_LOCKABLE | BLIST_INQUIRY_36},
	{"LASOUND", "CDX7405", "3.10", BLIST_MAX5LUN | BLIST_SINGLELUN},
	{"Marvell", "Console", NULL, BLIST_SKIP_VPD_PAGES},
	{"Marvell", "91xx Config", "1.01", BLIST_SKIP_VPD_PAGES},
	{"MATSHITA", "PD-1", NULL, BLIST_FORCELUN | BLIST_SINGLELUN},
	{"MATSHITA", "DMC-LC5", NULL, BLIST_NOT_LOCKABLE | BLIST_INQUIRY_36},
	{"MATSHITA", "DMC-LC40", NULL, BLIST_NOT_LOCKABLE | BLIST_INQUIRY_36},
	{"Medion", "Flash XL  MMC/SD", "2.6D", BLIST_FORCELUN},
	{"MegaRAID", "LD", NULL, BLIST_FORCELUN},
	{"MICROP", "4110", NULL, BLIST_NOTQ},
	{"MSFT", "Virtual HD", NULL, BLIST_MAX_1024 | BLIST_NO_RSOC},
	{"MYLEX", "DACARMRB", "*", BLIST_REPORTLUN2},
	{"nCipher", "Fastness Crypto", NULL, BLIST_FORCELUN},
	{"NAKAMICH", "MJ-4.8S", NULL, BLIST_FORCELUN | BLIST_SINGLELUN},
	{"NAKAMICH", "MJ-5.16S", NULL, BLIST_FORCELUN | BLIST_SINGLELUN},
	{"NEC", "PD-1 ODX654P", NULL, BLIST_FORCELUN | BLIST_SINGLELUN},
	{"NEC", "iStorage", NULL, BLIST_REPORTLUN2},
	{"NRC", "MBR-7", NULL, BLIST_FORCELUN | BLIST_SINGLELUN},
	{"NRC", "MBR-7.4", NULL, BLIST_FORCELUN | BLIST_SINGLELUN},
	{"PIONEER", "CD-ROM DRM-600", NULL, BLIST_FORCELUN | BLIST_SINGLELUN},
	{"PIONEER", "CD-ROM DRM-602X", NULL, BLIST_FORCELUN | BLIST_SINGLELUN},
	{"PIONEER", "CD-ROM DRM-604X", NULL, BLIST_FORCELUN | BLIST_SINGLELUN},
	{"PIONEER", "CD-ROM DRM-624X", NULL, BLIST_FORCELUN | BLIST_SINGLELUN},
	{"Promise", "VTrak E610f", NULL, BLIST_SPARSELUN | BLIST_NO_RSOC},
	{"Promise", "", NULL, BLIST_SPARSELUN},
	{"QEMU", "QEMU CD-ROM", NULL, BLIST_SKIP_VPD_PAGES},
	{"QNAP", "iSCSI Storage", NULL, BLIST_MAX_1024},
	{"SYNOLOGY", "iSCSI Storage", NULL, BLIST_MAX_1024},
	{"QUANTUM", "XP34301", "1071", BLIST_NOTQ},
	{"REGAL", "CDC-4X", NULL, BLIST_MAX5LUN | BLIST_SINGLELUN},
	{"SanDisk", "ImageMate CF-SD1", NULL, BLIST_FORCELUN},
	{"SEAGATE", "ST34555N", "0930", BLIST_NOTQ},	/* Chokes on tagged INQUIRY */
	{"SEAGATE", "ST3390N", "9546", BLIST_NOTQ},
	{"SEAGATE", "ST900MM0006", NULL, BLIST_SKIP_VPD_PAGES},
	{"SGI", "RAID3", "*", BLIST_SPARSELUN},
	{"SGI", "RAID5", "*", BLIST_SPARSELUN},
	{"SGI", "TP9100", "*", BLIST_REPORTLUN2},
	{"SGI", "Universal Xport", "*", BLIST_NO_ULD_ATTACH},
	{"IBM", "Universal Xport", "*", BLIST_NO_ULD_ATTACH},
	{"SUN", "Universal Xport", "*", BLIST_NO_ULD_ATTACH},
	{"DELL", "Universal Xport", "*", BLIST_NO_ULD_ATTACH},
	{"STK", "Universal Xport", "*", BLIST_NO_ULD_ATTACH},
	{"NETAPP", "Universal Xport", "*", BLIST_NO_ULD_ATTACH},
	{"LSI", "Universal Xport", "*", BLIST_NO_ULD_ATTACH},
	{"ENGENIO", "Universal Xport", "*", BLIST_NO_ULD_ATTACH},
	{"LENOVO", "Universal Xport", "*", BLIST_NO_ULD_ATTACH},
	{"SanDisk", "Cruzer Blade", NULL, BLIST_TRY_VPD_PAGES |
		BLIST_INQUIRY_36},
	{"SMSC", "USB 2 HS-CF", NULL, BLIST_SPARSELUN | BLIST_INQUIRY_36},
	{"SONY", "CD-ROM CDU-8001", NULL, BLIST_BORKEN},
	{"SONY", "TSL", NULL, BLIST_FORCELUN},		/* DDS3 & DDS4 autoloaders */
	{"ST650211", "CF", NULL, BLIST_RETRY_HWERROR},
	{"SUN", "T300", "*", BLIST_SPARSELUN},
	{"SUN", "T4", "*", BLIST_SPARSELUN},
	{"Tornado-", "F4", "*", BLIST_NOREPORTLUN},
	{"TOSHIBA", "CDROM", NULL, BLIST_ISROM},
	{"TOSHIBA", "CD-ROM", NULL, BLIST_ISROM},
	{"Traxdata", "CDR4120", NULL, BLIST_NOLUN},	/* locks up */
	{"USB2.0", "SMARTMEDIA/XD", NULL, BLIST_FORCELUN | BLIST_INQUIRY_36},
	{"WangDAT", "Model 2600", "01.7", BLIST_SELECT_NO_ATN},
	{"WangDAT", "Model 3200", "02.2", BLIST_SELECT_NO_ATN},
	{"WangDAT", "Model 1300", "02.4", BLIST_SELECT_NO_ATN},
	{"WDC WD25", "00JB-00FUA0", NULL, BLIST_NOREPORTLUN},
	{"XYRATEX", "RS", "*", BLIST_SPARSELUN | BLIST_LARGELUN},
	{"Zzyzx", "RocketStor 500S", NULL, BLIST_SPARSELUN},
	{"Zzyzx", "RocketStor 2000", NULL, BLIST_SPARSELUN},
	{ NULL, NULL, NULL, 0 },
};

/* list of keys for the lists */
enum scsi_devinfo_key {
	SCSI_DEVINFO_GLOBAL = 0,
	SCSI_DEVINFO_SPI,
};

static blist_flags_t scsi_default_dev_flags;
static LIST_HEAD(scsi_dev_info_list);
static char scsi_dev_flags[256];

/**
 * scsi_init_devinfo - set up the dynamic device list.
 *
 * Description:
 *	Add command line entries from scsi_dev_flags, then add
 *	scsi_static_device_list entries to the scsi device info list.
 */
int __init scsi_init_devinfo(void)
{
#ifdef CONFIG_SCSI_PROC_FS
	struct proc_dir_entry *p;
#endif
	int error, i;

	error = scsi_dev_info_add_list(SCSI_DEVINFO_GLOBAL, NULL);                           // 添加key为SCSI_DEVINFO_GLOBAL的struct scsi_dev_info_list_table实例到scsi_dev_info_list
	if (error)                                                                           // 若error为true
		return error;                                                                    // 返回error

	error = scsi_dev_info_list_add_str(scsi_dev_flags);                                  // 解析scsi_dev_flags并将解析结果添加到key为SCSI_DEVINFO_GLOBAL的struct scsi_dev_info_list_table实例中
	if (error)                                                                           // 若error不为0
		goto out;                                                                        // 跳转至out

	for (i = 0; scsi_static_device_list[i].vendor; i++) {                                // 遍历数组scsi_static_device_list
		error = scsi_dev_info_list_add(1 /* compatibile */,
				scsi_static_device_list[i].vendor,
				scsi_static_device_list[i].model,
				NULL,
				scsi_static_device_list[i].flags);                                       // 添加scsi_static_device_list[i]定义的信息到key为SCSI_DEVINFO_GLOBAL的struct scsi_dev_info_list_table实例中
		if (error)                                                                       // 若error为true
			goto out;                                                                    // 跳转至out
	}

#ifdef CONFIG_SCSI_PROC_FS
	p = proc_create("scsi/device_info", 0, NULL, &scsi_devinfo_proc_fops);               // 创建/proc/scsi/device_info节点
	if (!p) {                                                                            // 若p为NULL
		error = -ENOMEM;                                                                 // 设置error为-ENOMEM
		goto out;                                                                        // 跳转至out
	}
#endif /* CONFIG_SCSI_PROC_FS */

 out:
	if (error)
		scsi_exit_devinfo();
	return error;                                                                        // 返回error
}

/**
 * scsi_dev_info_add_list - add a new devinfo list
 * @key:	key of the list to add
 * @name:	Name of the list to add (for /proc/scsi/device_info)
 *
 * Adds the requested list, returns zero on success, -EEXIST if the
 * key is already registered to a list, or other error on failure.
 */
int scsi_dev_info_add_list(enum scsi_devinfo_key key, const char *name)
{
	struct scsi_dev_info_list_table *devinfo_table =
		scsi_devinfo_lookup_by_key(key);

	if (!IS_ERR(devinfo_table))                                                          // 重点：若IS_ERR(devinfo_table)为false
		/* list already exists */
		return -EEXIST;                                                                  // 返回-EEXIST

	devinfo_table = kmalloc(sizeof(*devinfo_table), GFP_KERNEL);                         // 申请devinfo_table

	if (!devinfo_table)                                                                  // 若devinfo_table为NULL
		return -ENOMEM;                                                                  // 返回-ENOMEM

	INIT_LIST_HEAD(&devinfo_table->node);                                                // 初始化devinfo_table->node
	INIT_LIST_HEAD(&devinfo_table->scsi_dev_info_list);                                  // 初始化devinfo_table->scsi_dev_info_list
	devinfo_table->name = name;                                                          // 设置devinfo_table->name为name
	devinfo_table->key = key;                                                            // 设置devinfo_table->key为key
	list_add_tail(&devinfo_table->node, &scsi_dev_info_list);                            // 添加devinfo_table->node到scsi_dev_info_list

	return 0;                                                                            // 返回0
}
EXPORT_SYMBOL(scsi_dev_info_add_list);

/**
 * scsi_dev_info_list_add - add one dev_info list entry.
 * @compatible: if true, null terminate short strings.  Otherwise space pad.
 * @vendor:	vendor string
 * @model:	model (product) string
 * @strflags:	integer string
 * @flags:	if strflags NULL, use this flag value
 *
 * Description:
 *	Create and add one dev_info entry for @vendor, @model, @strflags or
 *	@flag. If @compatible, add to the tail of the list, do not space
 *	pad, and set devinfo->compatible. The scsi_static_device_list entries
 *	are added with @compatible 1 and @clfags NULL.
 *
 * Returns: 0 OK, -error on failure.
 **/
static int scsi_dev_info_list_add(int compatible, char *vendor, char *model,
			    char *strflags, blist_flags_t flags)
{
	return scsi_dev_info_list_add_keyed(compatible, vendor, model,
					    strflags, flags,
					    SCSI_DEVINFO_GLOBAL);
}

static struct scsi_dev_info_list_table *scsi_devinfo_lookup_by_key(int key)
{
	struct scsi_dev_info_list_table *devinfo_table;
	int found = 0;                                                                       // 设置found为0

	list_for_each_entry(devinfo_table, &scsi_dev_info_list, node)                        // 遍历scsi_dev_info_list
		if (devinfo_table->key == key) {                                                 // 若devinfo_table->key为key
			found = 1;                                                                   // 设置found为1
			break;                                                                       // 退出遍历
		}
	if (!found)                                                                          // 若found为0
		return ERR_PTR(-EINVAL);                                                         // 返回ERR_PTR(-EINVAL)

	return devinfo_table;                                                                // 返回devinfo_table
}

/**
 * scsi_dev_info_list_add_str - parse dev_list and add to the scsi_dev_info_list.
 * @dev_list:	string of device flags to add
 *
 * Description:
 *	Parse dev_list, and add entries to the scsi_dev_info_list.
 *	dev_list is of the form "vendor:product:flag,vendor:product:flag".
 *	dev_list is modified via strsep. Can be called for command line
 *	addition, for proc or mabye a sysfs interface.
 *
 * Returns: 0 if OK, -error on failure.
 **/
static int scsi_dev_info_list_add_str(char *dev_list)
{
	char *vendor, *model, *strflags, *next;
	char *next_check;
	int res = 0;

	next = dev_list;
	if (next && next[0] == '"') {
		/*
		 * Ignore both the leading and trailing quote.
		 */
		next++;
		next_check = ",\"";
	} else {
		next_check = ",";
	}

	/*
	 * For the leading and trailing '"' case, the for loop comes
	 * through the last time with vendor[0] == '\0'.
	 */
	for (vendor = strsep(&next, ":"); vendor && (vendor[0] != '\0')
	     && (res == 0); vendor = strsep(&next, ":")) {
		strflags = NULL;
		model = strsep(&next, ":");
		if (model)
			strflags = strsep(&next, next_check);
		if (!model || !strflags) {
			printk(KERN_ERR "%s: bad dev info string '%s' '%s'"
			       " '%s'\n", __func__, vendor, model,
			       strflags);
			res = -EINVAL;
		} else
			res = scsi_dev_info_list_add(0 /* compatible */, vendor,
						     model, strflags, 0);
	}
	return res;
}








