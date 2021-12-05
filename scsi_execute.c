

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
static inline int scsi_execute_req(struct scsi_device *sdev,
	const unsigned char *cmd, int data_direction, void *buffer,
	unsigned bufflen, struct scsi_sense_hdr *sshdr, int timeout,
	int retries, int *resid)
{
	return scsi_execute(sdev, cmd, data_direction, buffer,
		bufflen, NULL, sshdr, timeout, retries,  0, 0, resid);
}

struct scsi_request {
	unsigned char	__cmd[BLK_MAX_CDB];
	unsigned char	*cmd;
	unsigned short	cmd_len;
	int		result;
	unsigned int	sense_len;
	unsigned int	resid_len;	/* residual count */
	int		retries;
	void		*sense;
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
	unsigned char *cmnd;


	/* These elements define the operation we ultimately want to perform */
	struct scsi_data_buffer sdb;
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
		 int *resid)
{
	struct request *req;
	struct scsi_request *rq;
	int ret = DRIVER_ERROR << 24;

	req = blk_get_request(sdev->request_queue,
			data_direction == DMA_TO_DEVICE ?
			REQ_OP_SCSI_OUT : REQ_OP_SCSI_IN, BLK_MQ_REQ_PREEMPT);                       // 重点：通过initialize_rq_fn引用scsi_mq_init_request中完成了scsi特有的设置
	if (IS_ERR(req))                                                                     // 若IS_ERR(req)为true
		return ret;                                                                      // 返回ret
	rq = scsi_req(req);                                                                  // 重点：这和scsi_queue_rq中取struct scsi_cmnd对应起来了，原理是struct scsi_cmnd的第一个成员是struct scsi_request类型

	if (bufflen &&	blk_rq_map_kern(sdev->request_queue, req,
					buffer, bufflen, GFP_NOIO))                                          // 
		goto out;                                                                        // 跳转至out

	rq->cmd_len = COMMAND_SIZE(cmd[0]);                                                  // 
	memcpy(rq->cmd, cmd, rq->cmd_len);                                                   // 重点：将在scsi_queue_rq中解析
	rq->retries = retries;                                                               // 
	req->timeout = timeout;                                                              // 
	req->cmd_flags |= flags;                                                             // 
	req->rq_flags |= rq_flags | RQF_QUIET;                                               // 

	/*
	 * head injection *required* here otherwise quiesce won't work
	 */
	blk_execute_rq(req->q, NULL, req, 1);                                                // 

	/*
	 * Some devices (USB mass-storage in particular) may transfer
	 * garbage data together with a residue indicating that the data
	 * is invalid.  Prevent the garbage from being misinterpreted
	 * and prevent security leaks by zeroing out the excess data.
	 */
	if (unlikely(rq->resid_len > 0 && rq->resid_len <= bufflen))                         // 
		memset(buffer + (bufflen - rq->resid_len), 0, rq->resid_len);                    // 

	if (resid)                                                                           // 若resid为true
		*resid = rq->resid_len;                                                          // 
	if (sense && rq->sense_len)                                                          // 若sense不为NULL，并且rq->sense_len不为0
		memcpy(sense, rq->sense, SCSI_SENSE_BUFFERSIZE);                                 // 重点：拷贝rq->sense到sense
	if (sshdr)                                                                           // 若sshdr为true
		scsi_normalize_sense(rq->sense, rq->sense_len, sshdr);                           // 重点：解析rq->sense到sshdr中
	ret = rq->result;                                                                    // 重点：result是在__ufshcd_transfer_req_compl中设置的
 out:
	blk_put_request(req);                                                                // 

	return ret;                                                                          // 返回ret
}
EXPORT_SYMBOL(__scsi_execute);

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
	if (!IS_ERR(req) && q->mq_ops->initialize_rq_fn)                                     // 若IS_ERR(req)为0，并且q->mq_ops->initialize_rq_fn不为NULL
		q->mq_ops->initialize_rq_fn(req);                                                // 重点：scsi_mq_init_request，在其中设置设置scsi req的cmd为scsi req的__cmd，设置scsi req的sense为cmd的sense_buffer

	return req;                                                                          // 返回req
}
EXPORT_SYMBOL(blk_get_request);

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
	cmd->req.sense = cmd->sense_buffer;                                                  // 重点：设置cmd->req.sense为cmd->sense_buffer

	if (scsi_host_get_prot(shost)) {
		sg = (void *)cmd + sizeof(struct scsi_cmnd) +
			shost->hostt->cmd_size;
		cmd->prot_sdb = (void *)sg + scsi_mq_inline_sgl_size(shost);
	}

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

/**
 * scsi_initialize_rq - initialize struct scsi_cmnd partially
 * @rq: Request associated with the SCSI command to be initialized.
 *
 * This function initializes the members of struct scsi_cmnd that must be
 * initialized before request processing starts and that won't be
 * reinitialized if a SCSI command is requeued.
 *
 * Called from inside blk_get_request() for pass-through requests and from
 * inside scsi_init_command() for filesystem requests.
 */
static void scsi_initialize_rq(struct request *rq)
{
	struct scsi_cmnd *cmd = blk_mq_rq_to_pdu(rq);

	scsi_req_init(&cmd->req);
	init_rcu_head(&cmd->rcu);
	cmd->jiffies_at_alloc = jiffies;
	cmd->retries = 0;
}

/**
 * scsi_req_init - initialize certain fields of a scsi_request structure
 * @req: Pointer to a scsi_request structure.
 * Initializes .__cmd[], .cmd, .cmd_len and .sense_len but no other members
 * of struct scsi_request.
 */
void scsi_req_init(struct scsi_request *req)
{
	memset(req->__cmd, 0, sizeof(req->__cmd));
	req->cmd = req->__cmd;                                                               // 重点：设置req->cmd为req->__cmd
	req->cmd_len = BLK_MAX_CDB;
	req->sense_len = 0;
}
EXPORT_SYMBOL(scsi_req_init);

/**
 * blk_rq_map_kern - map kernel data to a request, for passthrough requests
 * @q:		request queue where request should be inserted
 * @rq:		request to fill
 * @kbuf:	the kernel buffer
 * @len:	length of user data
 * @gfp_mask:	memory allocation flags
 *
 * Description:
 *    Data will be mapped directly if possible. Otherwise a bounce
 *    buffer is used. Can be called multiple times to append multiple
 *    buffers.
 */
int blk_rq_map_kern(struct request_queue *q, struct request *rq, void *kbuf,
		    unsigned int len, gfp_t gfp_mask)
{
	int reading = rq_data_dir(rq) == READ;                                               // 判断是否为read
	unsigned long addr = (unsigned long) kbuf;                                           // 定义并初始化addr为kbuf
	int do_copy = 0;                                                                     // 设置do_copy为0
	struct bio *bio, *orig_bio;
	int ret;

	if (len > (queue_max_hw_sectors(q) << 9))                                            // 若len > (queue_max_hw_sectors(q) << 9)为true
		return -EINVAL;                                                                  // 返回-EINVAL
	if (!len || !kbuf)                                                                   // 若len为0，或kbuf为NULL
		return -EINVAL;                                                                  // 返回-EINVAL

	do_copy = !blk_rq_aligned(q, addr, len) || object_is_on_stack(kbuf);                 // 
	if (do_copy)
		bio = bio_copy_kern(q, kbuf, len, gfp_mask, reading);                            // 
	else
		bio = bio_map_kern(q, kbuf, len, gfp_mask);                                      // 重点：创建bio并与kbuf映射

	if (IS_ERR(bio))                                                                     // 若IS_ERR(bio)为true
		return PTR_ERR(bio);                                                             // 返回PTR_ERR(bio)

	bio->bi_opf &= ~REQ_OP_MASK;                                                         // 
	bio->bi_opf |= req_op(rq);                                                           // 

	if (do_copy)                                                                         // 若do_copy为true
		rq->rq_flags |= RQF_COPY_USER;                                                   // 添加RQF_COPY_USER到rq->rq_flags

	orig_bio = bio;                                                                      // 设置orig_bio为bio
	ret = blk_rq_append_bio(rq, &bio);                                                   // 
	if (unlikely(ret)) {                                                                 // 若ret不为0
		/* request is too big */
		bio_put(orig_bio);                                                               // 
		return ret;                                                                      // 返回ret
	}

	return 0;                                                                            // 返回0
}
EXPORT_SYMBOL(blk_rq_map_kern);

static inline int blk_rq_aligned(struct request_queue *q, unsigned long addr,
				 unsigned int len)
{
	unsigned int alignment = queue_dma_alignment(q) | q->dma_pad_mask;                   // 
	return !(addr & alignment) && !(len & alignment);
}

static inline int object_is_on_stack(const void *obj)
{
	void *stack = task_stack_page(current);                                              // 获取current的stack

	return (obj >= stack) && (obj < (stack + THREAD_SIZE));                              // 返回(obj >= stack) && (obj < (stack + THREAD_SIZE))
}

/**
 *	bio_map_kern	-	map kernel address into bio
 *	@q: the struct request_queue for the bio
 *	@data: pointer to buffer to map
 *	@len: length in bytes
 *	@gfp_mask: allocation flags for bio allocation
 *
 *	Map the kernel address into a bio suitable for io to a block
 *	device. Returns an error pointer in case of error.
 */
struct bio *bio_map_kern(struct request_queue *q, void *data, unsigned int len,
			 gfp_t gfp_mask)
{
	unsigned long kaddr = (unsigned long)data;                                           // 定义并初始化kaddr为data
	unsigned long end = (kaddr + len + PAGE_SIZE - 1) >> PAGE_SHIFT;                     // 定义并初始化end为(kaddr + len + PAGE_SIZE - 1) >> PAGE_SHIFT
	unsigned long start = kaddr >> PAGE_SHIFT;                                           // 定义并初始化start为kaddr >> PAGE_SHIFT
	const int nr_pages = end - start;                                                    // 定义并初始化nr_pages为end - start
	bool is_vmalloc = is_vmalloc_addr(data);                                             // 
	struct page *page;
	int offset, i;
	struct bio *bio;

	bio = bio_kmalloc(gfp_mask, nr_pages);                                                // 
	if (!bio)                                                                             // 
		return ERR_PTR(-ENOMEM);                                                          // 

	if (is_vmalloc) {                                                                     // 
		flush_kernel_vmap_range(data, len);                                               // 
		bio->bi_private = data;                                                           // 
	}

	offset = offset_in_page(kaddr);                                                       // 
	for (i = 0; i < nr_pages; i++) {                                                      // 
		unsigned int bytes = PAGE_SIZE - offset;                                          // 

		if (len <= 0)                                                                     // 
			break;                                                                        // 

		if (bytes > len)                                                                  // 
			bytes = len;                                                                  // 

		if (!is_vmalloc)                                                                  // 
			page = virt_to_page(data);                                                    // 
		else
			page = vmalloc_to_page(data);                                                 // 
		if (bio_add_pc_page(q, bio, page, bytes,
				    offset) < bytes) {                                                    // 
			/* we don't support partial mappings */
			bio_put(bio);                                                                 // 
			return ERR_PTR(-EINVAL);                                                      // 返回ERR_PTR(-EINVAL)
		}

		data += bytes;                                                                    // data自加bytes
		len -= bytes;                                                                     // len自减bytes
		offset = 0;                                                                       // 设置offset为0
	}

	bio->bi_end_io = bio_map_kern_endio;                                                  // 设置bio->bi_end_io为bio_map_kern_endio
	return bio;                                                                           // 返回bio
}

static void bio_map_kern_endio(struct bio *bio)
{
	bio_invalidate_vmalloc_pages(bio);
	bio_put(bio);
}

int bio_add_pc_page(struct request_queue *q, struct bio *bio,
		struct page *page, unsigned int len, unsigned int offset)
{
	bool same_page = false;
	return __bio_add_pc_page(q, bio, page, len, offset, &same_page);
}
EXPORT_SYMBOL(bio_add_pc_page);

/**
 *	__bio_add_pc_page	- attempt to add page to passthrough bio
 *	@q: the target queue
 *	@bio: destination bio
 *	@page: page to add
 *	@len: vec entry length
 *	@offset: vec entry offset
 *	@same_page: return if the merge happen inside the same page
 *
 *	Attempt to add a page to the bio_vec maplist. This can fail for a
 *	number of reasons, such as the bio being full or target block device
 *	limitations. The target block device must allow bio's up to PAGE_SIZE,
 *	so it is always possible to add a single page to an empty bio.
 *
 *	This should only be used by passthrough bios.
 */
static int __bio_add_pc_page(struct request_queue *q, struct bio *bio,
		struct page *page, unsigned int len, unsigned int offset,
		bool *same_page)
{
	struct bio_vec *bvec;

	/*
	 * cloned bio must not modify vec list
	 */
	if (unlikely(bio_flagged(bio, BIO_CLONED)))
		return 0;

	if (((bio->bi_iter.bi_size + len) >> 9) > queue_max_hw_sectors(q))
		return 0;

	if (bio->bi_vcnt > 0) {
		if (bio_try_merge_pc_page(q, bio, page, len, offset, same_page))
			return len;

		/*
		 * If the queue doesn't support SG gaps and adding this segment
		 * would create a gap, disallow it.
		 */
		bvec = &bio->bi_io_vec[bio->bi_vcnt - 1];
		if (bvec_gap_to_prev(q, bvec, offset))
			return 0;
	}

	if (bio_full(bio, len))
		return 0;

	if (bio->bi_vcnt >= queue_max_segments(q))
		return 0;

	bvec = &bio->bi_io_vec[bio->bi_vcnt];
	bvec->bv_page = page;
	bvec->bv_len = len;
	bvec->bv_offset = offset;
	bio->bi_vcnt++;
	bio->bi_iter.bi_size += len;
	return len;
}

static inline struct scsi_request *scsi_req(struct request *rq)
{
	return blk_mq_rq_to_pdu(rq);
}

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

/**
 * blk_execute_rq_nowait - insert a request into queue for execution
 * @q:		queue to insert the request in
 * @bd_disk:	matching gendisk
 * @rq:		request to insert
 * @at_head:    insert request at head or tail of queue
 * @done:	I/O completion handler
 *
 * Description:
 *    Insert a fully prepared request at the back of the I/O scheduler queue
 *    for execution.  Don't wait for completion.
 *
 * Note:
 *    This function will invoke @done directly if the queue is dead.
 */
void blk_execute_rq_nowait(struct request_queue *q, struct gendisk *bd_disk,
			   struct request *rq, int at_head,
			   rq_end_io_fn *done)
{
	WARN_ON(irqs_disabled());
	WARN_ON(!blk_rq_is_passthrough(rq));

	rq->rq_disk = bd_disk;
	rq->end_io = done;

	/*
	 * don't check dying flag for MQ because the request won't
	 * be reused after dying flag is set
	 */
	blk_mq_sched_insert_request(rq, at_head, true, false);
}
EXPORT_SYMBOL_GPL(blk_execute_rq_nowait);

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

void __blk_mq_insert_request(struct blk_mq_hw_ctx *hctx, struct request *rq,
			     bool at_head)
{
	struct blk_mq_ctx *ctx = rq->mq_ctx;

	lockdep_assert_held(&ctx->lock);

	__blk_mq_insert_req_list(hctx, rq, at_head);
	blk_mq_hctx_mark_pending(hctx, ctx);
}

static inline void __blk_mq_insert_req_list(struct blk_mq_hw_ctx *hctx,
					    struct request *rq,
					    bool at_head)
{
	struct blk_mq_ctx *ctx = rq->mq_ctx;
	enum hctx_type type = hctx->type;

	lockdep_assert_held(&ctx->lock);

	trace_block_rq_insert(hctx->queue, rq);

	if (at_head)
		list_add(&rq->queuelist, &ctx->rq_lists[type]);
	else
		list_add_tail(&rq->queuelist, &ctx->rq_lists[type]);
}

bool blk_mq_run_hw_queue(struct blk_mq_hw_ctx *hctx, bool async)
{
	int srcu_idx;
	bool need_run;

	/*
	 * When queue is quiesced, we may be switching io scheduler, or
	 * updating nr_hw_queues, or other things, and we can't run queue
	 * any more, even __blk_mq_hctx_has_pending() can't be called safely.
	 *
	 * And queue will be rerun in blk_mq_unquiesce_queue() if it is
	 * quiesced.
	 */
	hctx_lock(hctx, &srcu_idx);
	need_run = !blk_queue_quiesced(hctx->queue) &&
		blk_mq_hctx_has_pending(hctx);
	hctx_unlock(hctx, srcu_idx);

	if (need_run) {
		__blk_mq_delay_run_hw_queue(hctx, async, 0);
		return true;
	}

	return false;
}
EXPORT_SYMBOL(blk_mq_run_hw_queue);

static void __blk_mq_delay_run_hw_queue(struct blk_mq_hw_ctx *hctx, bool async,
					unsigned long msecs)
{
	if (unlikely(blk_mq_hctx_stopped(hctx)))
		return;

	if (!async && !(hctx->flags & BLK_MQ_F_BLOCKING)) {
		int cpu = get_cpu();
		if (cpumask_test_cpu(cpu, hctx->cpumask)) {
			__blk_mq_run_hw_queue(hctx);
			put_cpu();
			return;
		}

		put_cpu();
	}

	kblockd_mod_delayed_work_on(blk_mq_hctx_next_cpu(hctx), &hctx->run_work,
				    msecs_to_jiffies(msecs));
}

static void __blk_mq_run_hw_queue(struct blk_mq_hw_ctx *hctx)
{
	int srcu_idx;

	/*
	 * We should be running this queue from one of the CPUs that
	 * are mapped to it.
	 *
	 * There are at least two related races now between setting
	 * hctx->next_cpu from blk_mq_hctx_next_cpu() and running
	 * __blk_mq_run_hw_queue():
	 *
	 * - hctx->next_cpu is found offline in blk_mq_hctx_next_cpu(),
	 *   but later it becomes online, then this warning is harmless
	 *   at all
	 *
	 * - hctx->next_cpu is found online in blk_mq_hctx_next_cpu(),
	 *   but later it becomes offline, then the warning can't be
	 *   triggered, and we depend on blk-mq timeout handler to
	 *   handle dispatched requests to this hctx
	 */
	if (!cpumask_test_cpu(raw_smp_processor_id(), hctx->cpumask) &&
		cpu_online(hctx->next_cpu)) {
		printk(KERN_WARNING "run queue from wrong CPU %d, hctx %s\n",
			raw_smp_processor_id(),
			cpumask_empty(hctx->cpumask) ? "inactive": "active");
		dump_stack();
	}

	/*
	 * We can't run the queue inline with ints disabled. Ensure that
	 * we catch bad users of this early.
	 */
	WARN_ON_ONCE(in_interrupt());

	might_sleep_if(hctx->flags & BLK_MQ_F_BLOCKING);

	hctx_lock(hctx, &srcu_idx);
	blk_mq_sched_dispatch_requests(hctx);
	hctx_unlock(hctx, srcu_idx);
}

void blk_mq_sched_dispatch_requests(struct blk_mq_hw_ctx *hctx)
{
	struct request_queue *q = hctx->queue;
	struct elevator_queue *e = q->elevator;
	const bool has_sched_dispatch = e && e->type->ops.dispatch_request;
	LIST_HEAD(rq_list);

	/* RCU or SRCU read lock is needed before checking quiesced flag */
	if (unlikely(blk_mq_hctx_stopped(hctx) || blk_queue_quiesced(q)))
		return;

	hctx->run++;

	/*
	 * If we have previous entries on our dispatch list, grab them first for
	 * more fair dispatch.
	 */
	if (!list_empty_careful(&hctx->dispatch)) {
		spin_lock(&hctx->lock);
		if (!list_empty(&hctx->dispatch))
			list_splice_init(&hctx->dispatch, &rq_list);
		spin_unlock(&hctx->lock);
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
	if (!list_empty(&rq_list)) {
		blk_mq_sched_mark_restart_hctx(hctx);
		if (blk_mq_dispatch_rq_list(q, &rq_list, false)) {
			if (has_sched_dispatch)
				blk_mq_do_dispatch_sched(hctx);
			else
				blk_mq_do_dispatch_ctx(hctx);
		}
	} else if (has_sched_dispatch) {
		blk_mq_do_dispatch_sched(hctx);
	} else if (hctx->dispatch_busy) {
		/* dequeue request one by one from sw queue if queue is busy */
		blk_mq_do_dispatch_ctx(hctx);
	} else {
		blk_mq_flush_busy_ctxs(hctx, &rq_list);
		blk_mq_dispatch_rq_list(q, &rq_list, false);
	}
}

/*
 * Process software queues that have been marked busy, splicing them
 * to the for-dispatch
 */
void blk_mq_flush_busy_ctxs(struct blk_mq_hw_ctx *hctx, struct list_head *list)
{
	struct flush_busy_ctx_data data = {
		.hctx = hctx,
		.list = list,
	};

	sbitmap_for_each_set(&hctx->ctx_map, flush_busy_ctx, &data);
}
EXPORT_SYMBOL_GPL(blk_mq_flush_busy_ctxs);

/**
 * sbitmap_for_each_set() - Iterate over each set bit in a &struct sbitmap.
 * @sb: Bitmap to iterate over.
 * @fn: Callback. Should return true to continue or false to break early.
 * @data: Pointer to pass to callback.
 */
static inline void sbitmap_for_each_set(struct sbitmap *sb, sb_for_each_fn fn,
					void *data)
{
	__sbitmap_for_each_set(sb, 0, fn, data);
}

/**
 * __sbitmap_for_each_set() - Iterate over each set bit in a &struct sbitmap.
 * @start: Where to start the iteration.
 * @sb: Bitmap to iterate over.
 * @fn: Callback. Should return true to continue or false to break early.
 * @data: Pointer to pass to callback.
 *
 * This is inline even though it's non-trivial so that the function calls to the
 * callback will hopefully get optimized away.
 */
static inline void __sbitmap_for_each_set(struct sbitmap *sb,
					  unsigned int start,
					  sb_for_each_fn fn, void *data)
{
	unsigned int index;
	unsigned int nr;
	unsigned int scanned = 0;

	if (start >= sb->depth)
		start = 0;
	index = SB_NR_TO_INDEX(sb, start);
	nr = SB_NR_TO_BIT(sb, start);

	while (scanned < sb->depth) {
		unsigned long word;
		unsigned int depth = min_t(unsigned int,
					   sb->map[index].depth - nr,
					   sb->depth - scanned);

		scanned += depth;
		word = sb->map[index].word & ~sb->map[index].cleared;
		if (!word)
			goto next;

		/*
		 * On the first iteration of the outer loop, we need to add the
		 * bit offset back to the size of the word for find_next_bit().
		 * On all other iterations, nr is zero, so this is a noop.
		 */
		depth += nr;
		while (1) {
			nr = find_next_bit(&word, depth, nr);
			if (nr >= depth)
				break;
			if (!fn(sb, (index << sb->shift) + nr, data))
				return;

			nr++;
		}
next:
		nr = 0;
		if (++index >= sb->map_nr)
			index = 0;
	}
}

static bool flush_busy_ctx(struct sbitmap *sb, unsigned int bitnr, void *data)
{
	struct flush_busy_ctx_data *flush_data = data;
	struct blk_mq_hw_ctx *hctx = flush_data->hctx;
	struct blk_mq_ctx *ctx = hctx->ctxs[bitnr];
	enum hctx_type type = hctx->type;

	spin_lock(&ctx->lock);
	list_splice_tail_init(&ctx->rq_lists[type], flush_data->list);
	sbitmap_clear_bit(sb, bitnr);
	spin_unlock(&ctx->lock);
	return true;
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

	if (list_empty(list))
		return false;

	WARN_ON(!list_is_singular(list) && got_budget);

	/*
	 * Now process all the entries, sending them to the driver.
	 */
	errors = queued = 0;
	do {
		struct blk_mq_queue_data bd;

		rq = list_first_entry(list, struct request, queuelist);

		hctx = rq->mq_hctx;
		if (!got_budget && !blk_mq_get_dispatch_budget(hctx))
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

		list_del_init(&rq->queuelist);

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

		ret = q->mq_ops->queue_rq(hctx, &bd);
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

static blk_status_t scsi_queue_rq(struct blk_mq_hw_ctx *hctx,
			 const struct blk_mq_queue_data *bd)
{
	struct request *req = bd->rq;                                                        // 取出req
	struct request_queue *q = req->q;                                                    // 取出q
	struct scsi_device *sdev = q->queuedata;                                             // 取出sdev
	struct Scsi_Host *shost = sdev->host;                                                // 取出shost
	struct scsi_cmnd *cmd = blk_mq_rq_to_pdu(req);                                       // 取出cmd，原理是取出预分配的cmd空间地址，在分配request时会额外分配cmd空间，参考scsi_mq_setup_tags和blk_mq_alloc_rqs
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
	cmd->scsi_done = scsi_mq_done;                                                       // 重点：cmd->scsi_done = scsi_mq_done

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

static inline void *blk_mq_rq_to_pdu(struct request *rq)
{
	return rq + 1;
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
	lrbp->sense_bufflen = UFS_SENSE_SIZE;                                                // 重点：设置lrbp->sense_bufflen为UFS_SENSE_SIZE
	lrbp->sense_buffer = cmd->sense_buffer;                                              // 重点：在ufshcd_intr的调用路径下的ufshcd_copy_sense_data中将填充sense_buffer
	lrbp->task_tag = tag;                                                                // 重点：spec规定要UAP layer维护task tag
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

/**
 * struct utp_upiu_rsp - general upiu response structure
 * @header: UPIU header structure DW-0 to DW-2
 * @sr: fields structure for scsi command DW-3 to DW-12
 * @qr: fields structure for query request DW-3 to DW-7
 */
struct utp_upiu_rsp {
	struct utp_upiu_header header;
	union {
		struct utp_cmd_rsp sr;
		struct utp_upiu_query qr;
	};
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
			cmd->result = result;                                                        // 重点
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
 * ufshcd_transfer_rsp_status - Get overall status of the response
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block of completed command
 *
 * Returns result of the command to notify SCSI midlayer
 */
static inline int
ufshcd_transfer_rsp_status(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	int result = 0;                                                                      // 重点：默认result为0
	int scsi_status;
	int ocs;

	/* overall command status of utrd */
	ocs = ufshcd_get_tr_ocs(lrbp);                                                       // 

	switch (ocs) {                                                                       // 
	case OCS_SUCCESS:
		result = ufshcd_get_req_rsp(lrbp->ucd_rsp_ptr);                                  // 
		hba->ufs_stats.last_hibern8_exit_tstamp = ktime_set(0, 0);                       // 
		switch (result) {                                                                // 
		case UPIU_TRANSACTION_RESPONSE:
			/*
			 * get the response UPIU result to extract
			 * the SCSI command status
			 */
			result = ufshcd_get_rsp_upiu_result(lrbp->ucd_rsp_ptr);                      // 

			/*
			 * get the result based on SCSI status response
			 * to notify the SCSI midlayer of the command status
			 */
			scsi_status = result & MASK_SCSI_STATUS;
			result = ufshcd_scsi_cmd_status(lrbp, scsi_status);                          // 

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
	return result;                                                                       // 返回result
}

/* Overall command status values */
enum {
	OCS_SUCCESS			= 0x0,
	OCS_INVALID_CMD_TABLE_ATTR	= 0x1,
	OCS_INVALID_PRDT_ATTR		= 0x2,
	OCS_MISMATCH_DATA_BUF_SIZE	= 0x3,
	OCS_MISMATCH_RESP_UPIU_SIZE	= 0x4,
	OCS_PEER_COMM_FAILURE		= 0x5,
	OCS_ABORTED			= 0x6,
	OCS_FATAL_ERROR			= 0x7,
	OCS_INVALID_COMMAND_STATUS	= 0x0F,
	MASK_OCS			= 0x0F,
};

/**
 * ufshcd_get_tr_ocs - Get the UTRD Overall Command Status
 * @lrbp: pointer to local command reference block
 *
 * This function is used to get the OCS field from UTRD
 * Returns the OCS field in the UTRD
 */
static inline int ufshcd_get_tr_ocs(struct ufshcd_lrb *lrbp)
{
	return le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS;             // 重点
}

/**
 * ufshcd_get_req_rsp - returns the TR response transaction type
 * @ucd_rsp_ptr: pointer to response UPIU
 */
static inline int
ufshcd_get_req_rsp(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return be32_to_cpu(ucd_rsp_ptr->header.dword_0) >> 24;                               // 重点：这是scsi cmd执行的结果
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
	return be32_to_cpu(ucd_rsp_ptr->header.dword_1) & MASK_RSP_UPIU_RESULT;             // 重点：这是scsi_execute_req的返回值
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

static void scsi_mq_done(struct scsi_cmnd *cmd)
{
	if (unlikely(test_and_set_bit(SCMD_STATE_COMPLETE, &cmd->state)))
		return;
	trace_scsi_dispatch_cmd_done(cmd);

	/*
	 * If the block layer didn't complete the request due to a timeout
	 * injection, scsi must clear its internal completed state so that the
	 * timeout handler will see it needs to escalate its own error
	 * recovery.
	 */
	if (unlikely(!blk_mq_complete_request(cmd->request)))
		clear_bit(SCMD_STATE_COMPLETE, &cmd->state);
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

static void __blk_mq_complete_request_remote(void *data)
{
	struct request *rq = data;
	struct request_queue *q = rq->q;

	q->mq_ops->complete(rq);
}

static void scsi_softirq_done(struct request *rq)
{
	struct scsi_cmnd *cmd = blk_mq_rq_to_pdu(rq);
	unsigned long wait_for = (cmd->allowed + 1) * rq->timeout;
	int disposition;

	INIT_LIST_HEAD(&cmd->eh_entry);

	atomic_inc(&cmd->device->iodone_cnt);
	if (cmd->result)
		atomic_inc(&cmd->device->ioerr_cnt);

	disposition = scsi_decide_disposition(cmd);
	if (disposition != SUCCESS &&
	    time_before(cmd->jiffies_at_alloc + wait_for, jiffies)) {
		scmd_printk(KERN_ERR, cmd,
			    "timing out command, waited %lus\n",
			    wait_for/HZ);
		disposition = SUCCESS;
	}

	scsi_log_completion(cmd, disposition);

	switch (disposition) {
		case SUCCESS:
			scsi_finish_command(cmd);
			break;
		case NEEDS_RETRY:
			scsi_queue_insert(cmd, SCSI_MLQUEUE_EH_RETRY);
			break;
		case ADD_TO_MLQUEUE:
			scsi_queue_insert(cmd, SCSI_MLQUEUE_DEVICE_BUSY);
			break;
		default:
			scsi_eh_scmd_add(cmd);
			break;
	}
}

/**
 * scsi_finish_command - cleanup and pass command back to upper layer
 * @cmd: the command
 *
 * Description: Pass command off to upper layer for finishing of I/O
 *              request, waking processes that are waiting on results,
 *              etc.
 */
void scsi_finish_command(struct scsi_cmnd *cmd)
{
	struct scsi_device *sdev = cmd->device;
	struct scsi_target *starget = scsi_target(sdev);
	struct Scsi_Host *shost = sdev->host;
	struct scsi_driver *drv;
	unsigned int good_bytes;

	scsi_device_unbusy(sdev);

	/*
	 * Clear the flags that say that the device/target/host is no longer
	 * capable of accepting new commands.
	 */
	if (atomic_read(&shost->host_blocked))
		atomic_set(&shost->host_blocked, 0);
	if (atomic_read(&starget->target_blocked))
		atomic_set(&starget->target_blocked, 0);
	if (atomic_read(&sdev->device_blocked))
		atomic_set(&sdev->device_blocked, 0);

	/*
	 * If we have valid sense information, then some kind of recovery
	 * must have taken place.  Make a note of this.
	 */
	if (SCSI_SENSE_VALID(cmd))
		cmd->result |= (DRIVER_SENSE << 24);

	SCSI_LOG_MLCOMPLETE(4, sdev_printk(KERN_INFO, sdev,
				"Notifying upper driver of completion "
				"(result %x)\n", cmd->result));

	good_bytes = scsi_bufflen(cmd);
	if (!blk_rq_is_passthrough(cmd->request)) {
		int old_good_bytes = good_bytes;
		drv = scsi_cmd_to_driver(cmd);
		if (drv->done)
			good_bytes = drv->done(cmd);
		/*
		 * USB may not give sense identifying bad sector and
		 * simply return a residue instead, so subtract off the
		 * residue if drv->done() error processing indicates no
		 * change to the completion length.
		 */
		if (good_bytes == old_good_bytes)
			good_bytes -= scsi_get_resid(cmd);
	}
	scsi_io_completion(cmd, good_bytes);
}

/*
 * Function:    scsi_io_completion()
 *
 * Purpose:     Completion processing for block device I/O requests.
 *
 * Arguments:   cmd   - command that is finished.
 *
 * Lock status: Assumed that no lock is held upon entry.
 *
 * Returns:     Nothing
 *
 * Notes:       We will finish off the specified number of sectors.  If we
 *		are done, the command block will be released and the queue
 *		function will be goosed.  If we are not done then we have to
 *		figure out what to do next:
 *
 *		a) We can call scsi_requeue_command().  The request
 *		   will be unprepared and put back on the queue.  Then
 *		   a new command will be created for it.  This should
 *		   be used if we made forward progress, or if we want
 *		   to switch from READ(10) to READ(6) for example.
 *
 *		b) We can call __scsi_queue_insert().  The request will
 *		   be put back on the queue and retried using the same
 *		   command as before, possibly after a delay.
 *
 *		c) We can call scsi_end_request() with blk_stat other than
 *		   BLK_STS_OK, to fail the remainder of the request.
 */
void scsi_io_completion(struct scsi_cmnd *cmd, unsigned int good_bytes)
{
	int result = cmd->result;
	struct request_queue *q = cmd->device->request_queue;
	struct request *req = cmd->request;
	blk_status_t blk_stat = BLK_STS_OK;

	if (unlikely(result))	/* a nz result may or may not be an error */
		result = scsi_io_completion_nz_result(cmd, result, &blk_stat);

	if (unlikely(blk_rq_is_passthrough(req))) {
		/*
		 * scsi_result_to_blk_status may have reset the host_byte
		 */
		scsi_req(req)->result = cmd->result;                                             // 重点result是在__ufshcd_transfer_req_compl中设置的
	}

	/*
	 * Next deal with any sectors which we were able to correctly
	 * handle.
	 */
	SCSI_LOG_HLCOMPLETE(1, scmd_printk(KERN_INFO, cmd,
		"%u sectors total, %d bytes done.\n",
		blk_rq_sectors(req), good_bytes));

	/*
	 * Next deal with any sectors which we were able to correctly
	 * handle. Failed, zero length commands always need to drop down
	 * to retry code. Fast path should return in this block.
	 */
	if (likely(blk_rq_bytes(req) > 0 || blk_stat == BLK_STS_OK)) {
		if (likely(!scsi_end_request(req, blk_stat, good_bytes)))
			return; /* no bytes remaining */
	}

	/* Kill remainder if no retries. */
	if (unlikely(blk_stat && scsi_noretry_cmd(cmd))) {
		if (scsi_end_request(req, blk_stat, blk_rq_bytes(req)))
			WARN_ONCE(true,
			    "Bytes remaining after failed, no-retry command");
		return;
	}

	/*
	 * If there had been no error, but we have leftover bytes in the
	 * requeues just queue the command up again.
	 */
	if (likely(result == 0))
		scsi_io_completion_reprep(cmd, q);
	else
		scsi_io_completion_action(cmd, result);
}

/**
 * scsi_normalize_sense - normalize main elements from either fixed or
 *			descriptor sense data format into a common format.
 *
 * @sense_buffer:	byte array containing sense data returned by device
 * @sb_len:		number of valid bytes in sense_buffer
 * @sshdr:		pointer to instance of structure that common
 *			elements are written to.
 *
 * Notes:
 *	The "main elements" from sense data are: response_code, sense_key,
 *	asc, ascq and additional_length (only for descriptor format).
 *
 *	Typically this function can be called after a device has
 *	responded to a SCSI command with the CHECK_CONDITION status.
 *
 * Return value:
 *	true if valid sense data information found, else false;
 */
bool scsi_normalize_sense(const u8 *sense_buffer, int sb_len,
			  struct scsi_sense_hdr *sshdr)
{
	memset(sshdr, 0, sizeof(struct scsi_sense_hdr));

	if (!sense_buffer || !sb_len)
		return false;

	sshdr->response_code = (sense_buffer[0] & 0x7f);

	if (!scsi_sense_valid(sshdr))
		return false;

	if (sshdr->response_code >= 0x72) {
		/*
		 * descriptor format
		 */
		if (sb_len > 1)
			sshdr->sense_key = (sense_buffer[1] & 0xf);
		if (sb_len > 2)
			sshdr->asc = sense_buffer[2];
		if (sb_len > 3)
			sshdr->ascq = sense_buffer[3];
		if (sb_len > 7)
			sshdr->additional_length = sense_buffer[7];
	} else {
		/*
		 * fixed format
		 */
		if (sb_len > 2)
			sshdr->sense_key = (sense_buffer[2] & 0xf);
		if (sb_len > 7) {
			sb_len = (sb_len < (sense_buffer[7] + 8)) ?
					 sb_len : (sense_buffer[7] + 8);
			if (sb_len > 12)
				sshdr->asc = sense_buffer[12];
			if (sb_len > 13)
				sshdr->ascq = sense_buffer[13];
		}
	}

	return true;
}
EXPORT_SYMBOL(scsi_normalize_sense);

/**
 * get_device_flags - get device specific flags from the dynamic device list.
 * @sdev:       &scsi_device to get flags for
 * @vendor:	vendor name
 * @model:	model name
 *
 * Description:
 *     Search the global scsi_dev_info_list (specified by list zero)
 *     for an entry matching @vendor and @model, if found, return the
 *     matching flags value, else return the host or global default
 *     settings.  Called during scan time.
 **/
blist_flags_t scsi_get_device_flags(struct scsi_device *sdev,
				    const unsigned char *vendor,
				    const unsigned char *model)
{
	return scsi_get_device_flags_keyed(sdev, vendor, model,
					   SCSI_DEVINFO_GLOBAL);
}

/**
 * scsi_get_device_flags_keyed - get device specific flags from the dynamic device list
 * @sdev:       &scsi_device to get flags for
 * @vendor:	vendor name
 * @model:	model name
 * @key:	list to look up
 *
 * Description:
 *     Search the scsi_dev_info_list specified by @key for an entry
 *     matching @vendor and @model, if found, return the matching
 *     flags value, else return the host or global default settings.
 *     Called during scan time.
 **/
blist_flags_t scsi_get_device_flags_keyed(struct scsi_device *sdev,
				const unsigned char *vendor,
				const unsigned char *model,
				enum scsi_devinfo_key key)
{
	struct scsi_dev_info_list *devinfo;

	devinfo = scsi_dev_info_list_find(vendor, model, key);
	if (!IS_ERR(devinfo))
		return devinfo->flags;

	/* key or device not found: return nothing */
	if (key != SCSI_DEVINFO_GLOBAL)
		return 0;

	/* except for the global list, where we have an exception */
	if (sdev->sdev_bflags)
		return sdev->sdev_bflags;

	return scsi_default_dev_flags;
}
EXPORT_SYMBOL(scsi_get_device_flags_keyed);

/*
 * scsi_dev_info_list: structure to hold black/white listed devices.
 */
struct scsi_dev_info_list {
	struct list_head dev_info_list;
	char vendor[8];
	char model[16];
	blist_flags_t flags;
	unsigned compatible; /* for use with scsi_static_device_list entries */
};

static LIST_HEAD(scsi_dev_info_list);

/**
 * scsi_dev_info_list_find - find a matching dev_info list entry.
 * @vendor:	full vendor string
 * @model:	full model (product) string
 * @key:	specify list to use
 *
 * Description:
 *	Finds the first dev_info entry matching @vendor, @model
 *	in list specified by @key.
 *
 * Returns: pointer to matching entry, or ERR_PTR on failure.
 **/
static struct scsi_dev_info_list *scsi_dev_info_list_find(const char *vendor,
		const char *model, enum scsi_devinfo_key key)
{
	struct scsi_dev_info_list *devinfo;
	struct scsi_dev_info_list_table *devinfo_table =
		scsi_devinfo_lookup_by_key(key);                                                 // 
	size_t vmax, mmax, mlen;
	const char *vskip, *mskip;

	if (IS_ERR(devinfo_table))                                                           // 若IS_ERR(devinfo_table)为true
		return (struct scsi_dev_info_list *) devinfo_table;                              // 返回devinfo_table

	/* Prepare for "compatible" matches */

	/*
	 * XXX why skip leading spaces? If an odd INQUIRY
	 * value, that should have been part of the
	 * scsi_static_device_list[] entry, such as "  FOO"
	 * rather than "FOO". Since this code is already
	 * here, and we don't know what device it is
	 * trying to work with, leave it as-is.
	 */
	vmax = sizeof(devinfo->vendor);
	vskip = vendor;
	while (vmax > 0 && *vskip == ' ') {
		vmax--;
		vskip++;
	}
	/* Also skip trailing spaces */
	while (vmax > 0 && vskip[vmax - 1] == ' ')
		--vmax;

	mmax = sizeof(devinfo->model);
	mskip = model;
	while (mmax > 0 && *mskip == ' ') {
		mmax--;
		mskip++;
	}
	while (mmax > 0 && mskip[mmax - 1] == ' ')
		--mmax;

	list_for_each_entry(devinfo, &devinfo_table->scsi_dev_info_list,
			    dev_info_list) {
		if (devinfo->compatible) {
			/*
			 * vendor strings must be an exact match
			 */
			if (vmax != strnlen(devinfo->vendor,
					    sizeof(devinfo->vendor)) ||
			    memcmp(devinfo->vendor, vskip, vmax))
				continue;

			/*
			 * @model specifies the full string, and
			 * must be larger or equal to devinfo->model
			 */
			mlen = strnlen(devinfo->model, sizeof(devinfo->model));
			if (mmax < mlen || memcmp(devinfo->model, mskip, mlen))
				continue;
			return devinfo;
		} else {
			if (!memcmp(devinfo->vendor, vendor,
				    sizeof(devinfo->vendor)) &&
			    !memcmp(devinfo->model, model,
				    sizeof(devinfo->model)))
				return devinfo;
		}
	}

	return ERR_PTR(-ENOENT);
}

static struct scsi_dev_info_list_table *scsi_devinfo_lookup_by_key(int key)
{
	struct scsi_dev_info_list_table *devinfo_table;
	int found = 0;

	list_for_each_entry(devinfo_table, &scsi_dev_info_list, node)                        // 遍历scsi_dev_info_list
		if (devinfo_table->key == key) {
			found = 1;
			break;
		}
	if (!found)
		return ERR_PTR(-EINVAL);

	return devinfo_table;
}













