/**
 * blk_mq_submit_bio - Create and send a request to block device.
 * @bio: Bio pointer.
 *
 * Builds up a request structure from @q and @bio and send to the device. The
 * request may not be queued directly to hardware if:
 * * This request can be merged with another one
 * * We want to place request at plug queue for possible future merging
 * * There is an IO scheduler active at this queue
 *
 * It will not queue the request if there is an error with the bio, or at the
 * request creation.
 *
 * Returns: Request queue cookie.
 */
blk_qc_t blk_mq_submit_bio(struct bio *bio)
{
	struct request_queue *q = bio->bi_disk->queue;
	const int is_sync = op_is_sync(bio->bi_opf);
	const int is_flush_fua = op_is_flush(bio->bi_opf);
	struct blk_mq_alloc_data data = {
		.q		= q,
	};
	struct request *rq;
	struct blk_plug *plug;
	struct request *same_queue_rq = NULL;
	unsigned int nr_segs;
	blk_qc_t cookie;
	blk_status_t ret;

	blk_queue_bounce(q, &bio);                                                           // 
	__blk_queue_split(&bio, &nr_segs);                                                   // 

	if (!bio_integrity_prep(bio))                                                        // 
		goto queue_exit;

	if (!is_flush_fua && !blk_queue_nomerges(q) &&
	    blk_attempt_plug_merge(q, bio, nr_segs, &same_queue_rq))                         // 
		goto queue_exit;

	if (blk_mq_sched_bio_merge(q, bio, nr_segs))                                         // 
		goto queue_exit;

	rq_qos_throttle(q, bio);                                                             // 

	data.cmd_flags = bio->bi_opf;                                                        // 重点
	rq = __blk_mq_alloc_request(&data);                                                  // 
	if (unlikely(!rq)) {
		rq_qos_cleanup(q, bio);                                                          // 
		if (bio->bi_opf & REQ_NOWAIT)                                                    // 
			bio_wouldblock_error(bio);                                                   // 
		goto queue_exit;
	}

	trace_block_getrq(q, bio, bio->bi_opf);                                              // 

	rq_qos_track(q, rq, bio);                                                            // 

	cookie = request_to_qc_t(data.hctx, rq);                                             // 

	blk_mq_bio_to_request(rq, bio, nr_segs);                                             // 

	ret = blk_crypto_init_request(rq);                                                   // 
	if (ret != BLK_STS_OK) {                                                             // 
		bio->bi_status = ret;                                                            // 
		bio_endio(bio);                                                                  // 
		blk_mq_free_request(rq);                                                         // 
		return BLK_QC_T_NONE;                                                            // 
	}

	plug = blk_mq_plug(q, bio);                                                          // 
	if (unlikely(is_flush_fua)) {                                                        // 
		/* Bypass scheduler for flush requests */
		blk_insert_flush(rq);                                                            // 
		blk_mq_run_hw_queue(data.hctx, true);                                            // 
	} else if (plug && (q->nr_hw_queues == 1 || q->mq_ops->commit_rqs ||
				!blk_queue_nonrot(q))) {                                                 // 
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
			last = list_entry_rq(plug->mq_list.prev);                                    // 

		if (request_count >= BLK_MAX_REQUEST_COUNT || (last &&
		    blk_rq_bytes(last) >= BLK_PLUG_FLUSH_SIZE)) {
			blk_flush_plug_list(plug, false);
			trace_block_plug(q);
		}

		blk_add_rq_to_plug(plug, rq);
	} else if (q->elevator) {
		/* Insert the request at the IO scheduler queue */
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
		/*
		 * There is no scheduler and we can try to send directly
		 * to the hardware.
		 */
		blk_mq_try_issue_directly(data.hctx, rq, &cookie);
	} else {
		/* Default case. */
		blk_mq_sched_insert_request(rq, false, true, true);
	}

	return cookie;
queue_exit:
	blk_queue_exit(q);
	return BLK_QC_T_NONE;
}

void blk_queue_bounce(struct request_queue *q, struct bio **bio_orig)
{
	mempool_t *pool;

	/*
	 * Data-less bio, nothing to bounce
	 */
	if (!bio_has_data(*bio_orig))                                                        // bio->bi_iter.bi_size && bio_op(bio) != REQ_OP_DISCARD && bio_op(bio) != REQ_OP_SECURE_ERASE && bio_op(bio) != REQ_OP_WRITE_ZEROES
		return;

	/*
	 * for non-isa bounce case, just check if the bounce pfn is equal
	 * to or bigger than the highest pfn in the system -- in that case,
	 * don't waste time iterating over bio segments
	 */
	if (!(q->bounce_gfp & GFP_DMA)) {                                                    // 
		if (q->limits.bounce_pfn >= blk_max_pfn)                                         // 
			return;
		pool = &page_pool;                                                               // 
	} else {
		BUG_ON(!mempool_initialized(&isa_page_pool));                                    // 
		pool = &isa_page_pool;                                                           // 
	}

	/*
	 * slow path
	 */
	__blk_queue_bounce(q, bio_orig, pool);                                               // 
}

static void __blk_queue_bounce(struct request_queue *q, struct bio **bio_orig,
			       mempool_t *pool)
{
	struct bio *bio;
	int rw = bio_data_dir(*bio_orig);                                                    // op_is_write(bio_op(*bio_orig)) ? WRITE : READ
	struct bio_vec *to, from;
	struct bvec_iter iter;
	unsigned i = 0;
	bool bounce = false;
	int sectors = 0;
	bool passthrough = bio_is_passthrough(*bio_orig);                                    // blk_op_is_scsi(bio_op(*bio_orig)) || blk_op_is_private(bio_op(*bio_orig))

	bio_for_each_segment(from, *bio_orig, iter) {
		if (i++ < BIO_MAX_PAGES)
			sectors += from.bv_len >> 9;
		if (page_to_pfn(from.bv_page) > q->limits.bounce_pfn)
			bounce = true;
	}
	if (!bounce)
		return;

	if (!passthrough && sectors < bio_sectors(*bio_orig)) {
		bio = bio_split(*bio_orig, sectors, GFP_NOIO, &bounce_bio_split);
		bio_chain(bio, *bio_orig);
		submit_bio_noacct(*bio_orig);
		*bio_orig = bio;
	}
	bio = bounce_clone_bio(*bio_orig, GFP_NOIO, passthrough ? NULL :
			&bounce_bio_set);

	/*
	 * Bvec table can't be updated by bio_for_each_segment_all(),
	 * so retrieve bvec from the table directly. This way is safe
	 * because the 'bio' is single-page bvec.
	 */
	for (i = 0, to = bio->bi_io_vec; i < bio->bi_vcnt; to++, i++) {
		struct page *page = to->bv_page;

		if (page_to_pfn(page) <= q->limits.bounce_pfn)
			continue;

		to->bv_page = mempool_alloc(pool, q->bounce_gfp);
		inc_zone_page_state(to->bv_page, NR_BOUNCE);

		if (rw == WRITE) {
			char *vto, *vfrom;

			flush_dcache_page(page);

			vto = page_address(to->bv_page) + to->bv_offset;
			vfrom = kmap_atomic(page) + to->bv_offset;
			memcpy(vto, vfrom, to->bv_len);
			kunmap_atomic(vfrom);
		}
	}

	trace_block_bio_bounce(q, *bio_orig);

	bio->bi_flags |= (1 << BIO_BOUNCED);

	if (pool == &page_pool) {
		bio->bi_end_io = bounce_end_io_write;
		if (rw == READ)
			bio->bi_end_io = bounce_end_io_read;
	} else {
		bio->bi_end_io = bounce_end_io_write_isa;
		if (rw == READ)
			bio->bi_end_io = bounce_end_io_read_isa;
	}

	bio->bi_private = *bio_orig;
	*bio_orig = bio;
}

/**
 * __blk_queue_split - split a bio and submit the second half
 * @bio:     [in, out] bio to be split
 * @nr_segs: [out] number of segments in the first bio
 *
 * Split a bio into two bios, chain the two bios, submit the second half and
 * store a pointer to the first half in *@bio. If the second bio is still too
 * big it will be split by a recursive call to this function. Since this
 * function may allocate a new bio from @bio->bi_disk->queue->bio_split, it is
 * the responsibility of the caller to ensure that
 * @bio->bi_disk->queue->bio_split is only released after processing of the
 * split bio has finished.
 */
void __blk_queue_split(struct bio **bio, unsigned int *nr_segs)
{
	struct request_queue *q = (*bio)->bi_disk->queue;
	struct bio *split = NULL;

	switch (bio_op(*bio)) {
	case REQ_OP_DISCARD:
	case REQ_OP_SECURE_ERASE:
		split = blk_bio_discard_split(q, *bio, &q->bio_split, nr_segs);                  // 
		break;
	case REQ_OP_WRITE_ZEROES:
		split = blk_bio_write_zeroes_split(q, *bio, &q->bio_split,
				nr_segs);                                                                // 
		break;
	case REQ_OP_WRITE_SAME:
		split = blk_bio_write_same_split(q, *bio, &q->bio_split,
				nr_segs);                                                                // 
		break;
	default:
		/*
		 * All drivers must accept single-segments bios that are <=
		 * PAGE_SIZE.  This is a quick and dirty check that relies on
		 * the fact that bi_io_vec[0] is always valid if a bio has data.
		 * The check might lead to occasional false negatives when bios
		 * are cloned, but compared to the performance impact of cloned
		 * bios themselves the loop below doesn't matter anyway.
		 */
		if (!q->limits.chunk_sectors &&
		    (*bio)->bi_vcnt == 1 &&
		    ((*bio)->bi_io_vec[0].bv_len +
		     (*bio)->bi_io_vec[0].bv_offset) <= PAGE_SIZE) {
			*nr_segs = 1;
			break;
		}
		split = blk_bio_segment_split(q, *bio, &q->bio_split, nr_segs);                  // 
		break;
	}

	if (split) {
		/* there isn't chance to merge the splitted bio */
		split->bi_opf |= REQ_NOMERGE;                                                    // 

		bio_chain(split, *bio);                                                          // 
		trace_block_split(q, split, (*bio)->bi_iter.bi_sector);                          // 重点
		submit_bio_noacct(*bio);                                                         // 
		*bio = split;                                                                    // 
	}
}

/**
 * blk_bio_segment_split - split a bio in two bios
 * @q:    [in] request queue pointer
 * @bio:  [in] bio to be split
 * @bs:	  [in] bio set to allocate the clone from
 * @segs: [out] number of segments in the bio with the first half of the sectors
 *
 * Clone @bio, update the bi_iter of the clone to represent the first sectors
 * of @bio and update @bio->bi_iter to represent the remaining sectors. The
 * following is guaranteed for the cloned bio:
 * - That it has at most get_max_io_size(@q, @bio) sectors.
 * - That it has at most queue_max_segments(@q) segments.
 *
 * Except for discard requests the cloned bio will point at the bi_io_vec of
 * the original bio. It is the responsibility of the caller to ensure that the
 * original bio is not freed before the cloned bio. The caller is also
 * responsible for ensuring that @bs is only destroyed after processing of the
 * split bio has finished.
 */
static struct bio *blk_bio_segment_split(struct request_queue *q,
					 struct bio *bio,
					 struct bio_set *bs,
					 unsigned *segs)
{
	struct bio_vec bv, bvprv, *bvprvp = NULL;
	struct bvec_iter iter;
	unsigned nsegs = 0, sectors = 0;
	const unsigned max_sectors = get_max_io_size(q, bio);                                // the maximum number of sectors from the start of a bio that may be submitted as a single request to a block device
	const unsigned max_segs = queue_max_segments(q);                                     // QUEUE_RO_ENTRY(queue_max_segments, "max_segments");

	bio_for_each_bvec(bv, bio, iter) {
		/*
		 * If the queue doesn't support SG gaps and adding this
		 * offset would create a gap, disallow it.
		 */
		if (bvprvp && bvec_gap_to_prev(q, bvprvp, bv.bv_offset))
			goto split;

		if (nsegs < max_segs &&
		    sectors + (bv.bv_len >> 9) <= max_sectors &&
		    bv.bv_offset + bv.bv_len <= PAGE_SIZE) {                                     // 重点：是否split的条件
			nsegs++;                                                                     // 自加
			sectors += bv.bv_len >> 9;                                                   // 自加
		} else if (bvec_split_segs(q, &bv, &nsegs, &sectors, max_segs,
					 max_sectors)) {
			goto split;
		}

		bvprv = bv;
		bvprvp = &bvprv;
	}

	*segs = nsegs;
	return NULL;
split:
	*segs = nsegs;
	return bio_split(bio, sectors, GFP_NOIO, bs);
}

/*
 * Return the maximum number of sectors from the start of a bio that may be
 * submitted as a single request to a block device. If enough sectors remain,
 * align the end to the physical block size. Otherwise align the end to the
 * logical block size. This approach minimizes the number of non-aligned
 * requests that are submitted to a block device if the start of a bio is not
 * aligned to a physical block boundary.
 */
static inline unsigned get_max_io_size(struct request_queue *q,
				       struct bio *bio)
{
	unsigned sectors = blk_max_size_offset(q, bio->bi_iter.bi_sector, 0);                // maximum size of a request at given offset
	unsigned max_sectors = sectors;                                                      // 
	unsigned pbs = queue_physical_block_size(q) >> SECTOR_SHIFT;                         // QUEUE_RO_ENTRY(queue_physical_block_size, "physical_block_size");
	unsigned lbs = queue_logical_block_size(q) >> SECTOR_SHIFT;                          // QUEUE_RO_ENTRY(queue_logical_block_size, "logical_block_size");
	unsigned start_offset = bio->bi_iter.bi_sector & (pbs - 1);                          // 

	max_sectors += start_offset;                                                         // 
	max_sectors &= ~(pbs - 1);                                                           // 
	if (max_sectors > start_offset)                                                      // 
		return max_sectors - start_offset;                                               // 

	return sectors & ~(lbs - 1);                                                         // 
}

/*
 * The basic unit of block I/O is a sector. It is used in a number of contexts
 * in Linux (blk, bio, genhd). The size of one sector is 512 = 2**9
 * bytes. Variables of type sector_t represent an offset or size that is a
 * multiple of 512 bytes. Hence these two constants.
 */
#ifndef SECTOR_SHIFT
#define SECTOR_SHIFT 9
#endif
#ifndef SECTOR_SIZE
#define SECTOR_SIZE (1 << SECTOR_SHIFT)
#endif

static inline unsigned int queue_physical_block_size(const struct request_queue *q)
{
	return q->limits.physical_block_size;
}

static inline unsigned queue_logical_block_size(const struct request_queue *q)
{
	int retval = 512;

	if (q && q->limits.logical_block_size)
		retval = q->limits.logical_block_size;

	return retval;
}

/*
 * Return maximum size of a request at given offset. Only valid for
 * file system requests.
 */
static inline unsigned int blk_max_size_offset(struct request_queue *q,
					       sector_t offset,
					       unsigned int chunk_sectors)
{
	if (!chunk_sectors) {
		if (q->limits.chunk_sectors)
			chunk_sectors = q->limits.chunk_sectors;                                     // QUEUE_RO_ENTRY(queue_chunk_sectors, "chunk_sectors");
		else
			return q->limits.max_sectors;                                                // QUEUE_RW_ENTRY(queue_max_sectors, "max_sectors_kb");
	}

	if (likely(is_power_of_2(chunk_sectors)))
		chunk_sectors -= offset & (chunk_sectors - 1);
	else
		chunk_sectors -= sector_div(offset, chunk_sectors);

	return min(q->limits.max_sectors, chunk_sectors);
}


bio_for_each_segment




