/*
 * get_page_from_freelist goes through the zonelist trying to allocate
 * a page.
 */
static struct page *
get_page_from_freelist(gfp_t gfp_mask, unsigned int order, int alloc_flags,
						const struct alloc_context *ac)
{
	struct zoneref *z;
	struct zone *zone;
	struct pglist_data *last_pgdat_dirty_limit = NULL;
	bool no_fallback;

retry:
	/*
	 * Scan zonelist, looking for a zone with enough free.
	 * See also __cpuset_node_allowed() comment in kernel/cpuset.c.
	 */
	no_fallback = alloc_flags & ALLOC_NOFRAGMENT;                                    // 取出ALLOC_NOFRAGMENT位的结果
	z = ac->preferred_zoneref;
	for_next_zone_zonelist_nodemask(zone, z, ac->zonelist, ac->high_zoneidx,         // 遍历符合条件的zone
								ac->nodemask) {
		struct page *page;
		unsigned long mark;

		if (cpusets_enabled() &&
			(alloc_flags & ALLOC_CPUSET) &&
			!__cpuset_zone_allowed(zone, gfp_mask))
				continue;
		/*
		 * When allocating a page cache page for writing, we
		 * want to get it from a node that is within its dirty
		 * limit, such that no single node holds more than its
		 * proportional share of globally allowed dirty pages.
		 * The dirty limits take into account the node's
		 * lowmem reserves and high watermark so that kswapd
		 * should be able to balance it without having to
		 * write pages from its LRU list.
		 *
		 * XXX: For now, allow allocations to potentially
		 * exceed the per-node dirty limit in the slowpath
		 * (spread_dirty_pages unset) before going into reclaim,
		 * which is important when on a NUMA setup the allowed
		 * nodes are together not big enough to reach the
		 * global limit.  The proper fix for these situations
		 * will require awareness of nodes in the
		 * dirty-throttling and the flusher threads.
		 */
		if (ac->spread_dirty_pages) {
			if (last_pgdat_dirty_limit == zone->zone_pgdat)
				continue;

			if (!node_dirty_ok(zone->zone_pgdat)) {
				last_pgdat_dirty_limit = zone->zone_pgdat;
				continue;
			}
		}

		if (no_fallback && nr_online_nodes > 1 &&
		    zone != ac->preferred_zoneref->zone) {
			int local_nid;

			/*
			 * If moving to a remote node, retry but allow
			 * fragmenting fallbacks. Locality is more important
			 * than fragmentation avoidance.
			 */
			local_nid = zone_to_nid(ac->preferred_zoneref->zone);
			if (zone_to_nid(zone) != local_nid) {
				alloc_flags &= ~ALLOC_NOFRAGMENT;
				goto retry;
			}
		}

		mark = wmark_pages(zone, alloc_flags & ALLOC_WMARK_MASK);
		if (!zone_watermark_fast(zone, order, mark,
				       ac_classzone_idx(ac), alloc_flags)) {
			int ret;

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
			/*
			 * Watermark failed for this zone, but see if we can
			 * grow this zone if it contains deferred pages.
			 */
			if (static_branch_unlikely(&deferred_pages)) {
				if (_deferred_grow_zone(zone, order))
					goto try_this_zone;
			}
#endif
			/* Checked here to keep the fast path fast */
			BUILD_BUG_ON(ALLOC_NO_WATERMARKS < NR_WMARK);
			if (alloc_flags & ALLOC_NO_WATERMARKS)
				goto try_this_zone;

			if (node_reclaim_mode == 0 ||
			    !zone_allows_reclaim(ac->preferred_zoneref->zone, zone))
				continue;

			ret = node_reclaim(zone->zone_pgdat, gfp_mask, order);
			switch (ret) {
			case NODE_RECLAIM_NOSCAN:
				/* did not scan */
				continue;
			case NODE_RECLAIM_FULL:
				/* scanned but unreclaimable */
				continue;
			default:
				/* did we reclaim enough */
				if (zone_watermark_ok(zone, order, mark,
						ac_classzone_idx(ac), alloc_flags))
					goto try_this_zone;

				continue;
			}
		}

try_this_zone:
		page = rmqueue(ac->preferred_zoneref->zone, zone, order,
				gfp_mask, alloc_flags, ac->migratetype);
		if (page) {
			prep_new_page(page, order, gfp_mask, alloc_flags);

			/*
			 * If this is a high-order atomic allocation then check
			 * if the pageblock should be reserved for the future
			 */
			if (unlikely(order && (alloc_flags & ALLOC_HARDER)))
				reserve_highatomic_pageblock(page, zone, order);

			return page;
		} else {
#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
			/* Try again if zone has deferred pages */
			if (static_branch_unlikely(&deferred_pages)) {
				if (_deferred_grow_zone(zone, order))
					goto try_this_zone;
			}
#endif
		}
	}

	/*
	 * It's possible on a UMA machine to get through all zones that are
	 * fragmented. If avoiding fragmentation, reset and try again.
	 */
	if (no_fallback) {
		alloc_flags &= ~ALLOC_NOFRAGMENT;
		goto retry;
	}

	return NULL;
}




#define cond_resched() ({			\
	___might_sleep(__FILE__, __LINE__, 0);	\
	_cond_resched();			\
})

int __sched _cond_resched(void)
{
	if (should_resched(0)) {
		preempt_schedule_common();
		return 1;
	}
	rcu_all_qs();
	return 0;
}
EXPORT_SYMBOL(_cond_resched);

static inline bool should_resched(int preempt_offset)
{
	u64 pc = READ_ONCE(current_thread_info()->preempt_count);
	return pc == preempt_offset;
}

static void __sched notrace preempt_schedule_common(void)
{
	do {
		/*
		 * Because the function tracer can trace preempt_count_sub()
		 * and it also uses preempt_enable/disable_notrace(), if
		 * NEED_RESCHED is set, the preempt_enable_notrace() called
		 * by the function tracer will call this function again and
		 * cause infinite recursion.
		 *
		 * Preemption must be disabled here before the function
		 * tracer can trace. Break up preempt_disable() into two
		 * calls. One to disable preemption without fear of being
		 * traced. The other to still record the preemption latency,
		 * which can also be traced by the function tracer.
		 */
		preempt_disable_notrace();
		preempt_latency_start(1);
		__schedule(true);
		preempt_latency_stop(1);
		preempt_enable_no_resched_notrace();

		/*
		 * Check again in case we missed a preemption opportunity
		 * between schedule and now.
		 */
	} while (need_resched());
}

/*
 * __schedule() is the main scheduler function.
 *
 * The main means of driving the scheduler and thus entering this function are:
 *
 *   1. Explicit blocking: mutex, semaphore, waitqueue, etc.
 *
 *   2. TIF_NEED_RESCHED flag is checked on interrupt and userspace return
 *      paths. For example, see arch/x86/entry_64.S.
 *
 *      To drive preemption between tasks, the scheduler sets the flag in timer
 *      interrupt handler scheduler_tick().
 *
 *   3. Wakeups don't really cause entry into schedule(). They add a
 *      task to the run-queue and that's it.
 *
 *      Now, if the new task added to the run-queue preempts the current
 *      task, then the wakeup sets TIF_NEED_RESCHED and schedule() gets
 *      called on the nearest possible occasion:
 *
 *       - If the kernel is preemptible (CONFIG_PREEMPTION=y):
 *
 *         - in syscall or exception context, at the next outmost
 *           preempt_enable(). (this might be as soon as the wake_up()'s
 *           spin_unlock()!)
 *
 *         - in IRQ context, return from interrupt-handler to
 *           preemptible context
 *
 *       - If the kernel is not preemptible (CONFIG_PREEMPTION is not set)
 *         then at the next:
 *
 *          - cond_resched() call
 *          - explicit schedule() call
 *          - return from syscall or exception to user-space
 *          - return from interrupt-handler to user-space
 *
 * WARNING: must be called with preemption disabled!
 */
static void __sched notrace __schedule(bool preempt)
{
	struct task_struct *prev, *next;
	unsigned long *switch_count;
	struct rq_flags rf;
	struct rq *rq;
	int cpu;

	cpu = smp_processor_id();
	rq = cpu_rq(cpu);
	prev = rq->curr;

	schedule_debug(prev, preempt);

	if (sched_feat(HRTICK))
		hrtick_clear(rq);

	local_irq_disable();
	rcu_note_context_switch(preempt);

	/*
	 * Make sure that signal_pending_state()->signal_pending() below
	 * can't be reordered with __set_current_state(TASK_INTERRUPTIBLE)
	 * done by the caller to avoid the race with signal_wake_up().
	 *
	 * The membarrier system call requires a full memory barrier
	 * after coming from user-space, before storing to rq->curr.
	 */
	rq_lock(rq, &rf);
	smp_mb__after_spinlock();

	/* Promote REQ to ACT */
	rq->clock_update_flags <<= 1;
	update_rq_clock(rq);

	switch_count = &prev->nivcsw;
	if (!preempt && prev->state) {
		if (signal_pending_state(prev->state, prev)) {
			prev->state = TASK_RUNNING;
		} else {
			deactivate_task(rq, prev, DEQUEUE_SLEEP | DEQUEUE_NOCLOCK);

			if (prev->in_iowait) {
				atomic_inc(&rq->nr_iowait);
				delayacct_blkio_start();
			}
		}
		switch_count = &prev->nvcsw;
	}

	next = pick_next_task(rq, prev, &rf);
	clear_tsk_need_resched(prev);
	clear_preempt_need_resched();

	if (likely(prev != next)) {
		rq->nr_switches++;
		/*
		 * RCU users of rcu_dereference(rq->curr) may not see
		 * changes to task_struct made by pick_next_task().
		 */
		RCU_INIT_POINTER(rq->curr, next);
		/*
		 * The membarrier system call requires each architecture
		 * to have a full memory barrier after updating
		 * rq->curr, before returning to user-space.
		 *
		 * Here are the schemes providing that barrier on the
		 * various architectures:
		 * - mm ? switch_mm() : mmdrop() for x86, s390, sparc, PowerPC.
		 *   switch_mm() rely on membarrier_arch_switch_mm() on PowerPC.
		 * - finish_lock_switch() for weakly-ordered
		 *   architectures where spin_unlock is a full barrier,
		 * - switch_to() for arm64 (weakly-ordered, spin_unlock
		 *   is a RELEASE barrier),
		 */
		++*switch_count;

		trace_sched_switch(preempt, prev, next);

		/* Also unlocks the rq: */
		rq = context_switch(rq, prev, next, &rf);
	} else {
		rq->clock_update_flags &= ~(RQCF_ACT_SKIP|RQCF_REQ_SKIP);
		rq_unlock_irq(rq, &rf);
	}

	balance_callback(rq);
}

/*
 * The inactive anon list should be small enough that the VM never has
 * to do too much work.
 *
 * The inactive file list should be small enough to leave most memory
 * to the established workingset on the scan-resistant active list,
 * but large enough to avoid thrashing the aggregate readahead window.
 *
 * Both inactive lists should also be large enough that each inactive
 * page has a chance to be referenced again before it is reclaimed.
 *
 * If that fails and refaulting is observed, the inactive list grows.
 *
 * The inactive_ratio is the target ratio of ACTIVE to INACTIVE pages
 * on this LRU, maintained by the pageout code. An inactive_ratio
 * of 3 means 3:1 or 25% of the pages are kept on the inactive list.
 *
 * total     target    max
 * memory    ratio     inactive
 * -------------------------------------
 *   10MB       1         5MB
 *  100MB       1        50MB
 *    1GB       3       250MB
 *   10GB      10       0.9GB
 *  100GB      31         3GB
 *    1TB     101        10GB
 *   10TB     320        32GB
 */
static bool inactive_list_is_low(struct lruvec *lruvec, bool file,
				 struct scan_control *sc, bool trace)
{
	enum lru_list active_lru = file * LRU_FILE + LRU_ACTIVE;
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);
	enum lru_list inactive_lru = file * LRU_FILE;
	unsigned long inactive, active;
	unsigned long inactive_ratio;
	unsigned long refaults;
	unsigned long gb;

	/*
	 * If we don't have swap space, anonymous page deactivation
	 * is pointless.
	 */
	if (!file && !total_swap_pages)
		return false;

	inactive = lruvec_lru_size(lruvec, inactive_lru, sc->reclaim_idx);
	active = lruvec_lru_size(lruvec, active_lru, sc->reclaim_idx);

	/*
	 * When refaults are being observed, it means a new workingset
	 * is being established. Disable active list protection to get
	 * rid of the stale workingset quickly.
	 */
	refaults = lruvec_page_state_local(lruvec, WORKINGSET_ACTIVATE);
	if (file && lruvec->refaults != refaults) {
		inactive_ratio = 0;
	} else {
		gb = (inactive + active) >> (30 - PAGE_SHIFT);
		if (gb)
			inactive_ratio = int_sqrt(10 * gb);
		else
			inactive_ratio = 1;
	}

	if (trace)
		trace_mm_vmscan_inactive_list_is_low(pgdat->node_id, sc->reclaim_idx,
			lruvec_lru_size(lruvec, inactive_lru, MAX_NR_ZONES), inactive,
			lruvec_lru_size(lruvec, active_lru, MAX_NR_ZONES), active,
			inactive_ratio, file);

	return inactive * inactive_ratio < active;
}

static void shrink_active_list(unsigned long nr_to_scan,
			       struct lruvec *lruvec,
			       struct scan_control *sc,
			       enum lru_list lru)
{
	unsigned long nr_taken;
	unsigned long nr_scanned;
	unsigned long vm_flags;
	LIST_HEAD(l_hold);                                                         // 临时保持不动页的链表
	LIST_HEAD(l_active);                                                       // 临时保存活跃页的链表
	LIST_HEAD(l_inactive);                                                     // 临时保存非活跃页的链表
	struct page *page;
	struct zone_reclaim_stat *reclaim_stat = &lruvec->reclaim_stat;
	/*
		struct lruvec {
			struct list_head		lists[NR_LRU_LISTS];
			struct zone_reclaim_stat	reclaim_stat;
			// Evictions & activations on the inactive file list
			atomic_long_t			inactive_age;
			// Refaults at the time of last reclaim cycle
			unsigned long			refaults;
		#ifdef CONFIG_MEMCG
			struct pglist_data *pgdat;
		#endif
		};
	 */
	unsigned nr_deactivate, nr_activate;
	unsigned nr_rotated = 0;
	int file = is_file_lru(lru);
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);
	/*
		static inline struct pglist_data *lruvec_pgdat(struct lruvec *lruvec)
		{
		#ifdef CONFIG_MEMCG
			return lruvec->pgdat;
		#else
			return container_of(lruvec, struct pglist_data, lruvec);
		#endif
		}
	 */

	lru_add_drain();                                                              // 把仍留在pagevec中的所有页移入活动与非活动链表，单独分析
	/*
		void lru_add_drain(void)
		{
			lru_add_drain_cpu(get_cpu());
			put_cpu();
		}

		#define get_cpu()		({ preempt_disable(); __smp_processor_id(); })
		#define put_cpu()		preempt_enable()
	 */


	spin_lock_irq(&pgdat->lru_lock);

	nr_taken = isolate_lru_pages(nr_to_scan, lruvec, &l_hold,
				     &nr_scanned, sc, lru);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, nr_taken);
	reclaim_stat->recent_scanned[file] += nr_taken;

	__count_vm_events(PGREFILL, nr_scanned);
	__count_memcg_events(lruvec_memcg(lruvec), PGREFILL, nr_scanned);

	spin_unlock_irq(&pgdat->lru_lock);

	while (!list_empty(&l_hold)) {
		cond_resched();
		page = lru_to_page(&l_hold);
		list_del(&page->lru);

		if (unlikely(!page_evictable(page))) {
			putback_lru_page(page);
			continue;
		}

		if (unlikely(buffer_heads_over_limit)) {
			if (page_has_private(page) && trylock_page(page)) {
				if (page_has_private(page))
					try_to_release_page(page, 0);
				unlock_page(page);
			}
		}

		if (page_referenced(page, 0, sc->target_mem_cgroup,
				    &vm_flags)) {
			nr_rotated += hpage_nr_pages(page);
			/*
			 * Identify referenced, file-backed active pages and
			 * give them one more trip around the active list. So
			 * that executable code get better chances to stay in
			 * memory under moderate memory pressure.  Anon pages
			 * are not likely to be evicted by use-once streaming
			 * IO, plus JVM can create lots of anon VM_EXEC pages,
			 * so we ignore them here.
			 */
			if ((vm_flags & VM_EXEC) && page_is_file_cache(page)) {
				list_add(&page->lru, &l_active);
				continue;
			}
		}

		ClearPageActive(page);	/* we are de-activating */
		SetPageWorkingset(page);
		list_add(&page->lru, &l_inactive);
	}

	/*
	 * Move pages back to the lru list.
	 */
	spin_lock_irq(&pgdat->lru_lock);
	/*
	 * Count referenced pages from currently used mappings as rotated,
	 * even though only some of them are actually re-activated.  This
	 * helps balance scan pressure between file and anonymous pages in
	 * get_scan_count.
	 */
	reclaim_stat->recent_rotated[file] += nr_rotated;

	nr_activate = move_pages_to_lru(lruvec, &l_active);
	nr_deactivate = move_pages_to_lru(lruvec, &l_inactive);
	/* Keep all free pages in l_active list */
	list_splice(&l_inactive, &l_active);

	__count_vm_events(PGDEACTIVATE, nr_deactivate);
	__count_memcg_events(lruvec_memcg(lruvec), PGDEACTIVATE, nr_deactivate);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, -nr_taken);
	spin_unlock_irq(&pgdat->lru_lock);

	mem_cgroup_uncharge_list(&l_active);
	free_unref_page_list(&l_active);
	trace_mm_vmscan_lru_shrink_active(pgdat->node_id, nr_taken, nr_activate,
			nr_deactivate, nr_rotated, sc->priority, file);
}

/*
 * Drain pages out of the cpu's pagevecs.
 * Either "cpu" is the current CPU, and preemption has already been
 * disabled; or "cpu" is being hot-unplugged, and is already dead.
 */
void lru_add_drain_cpu(int cpu)
{
	struct pagevec *pvec = &per_cpu(lru_add_pvec, cpu);

	if (pagevec_count(pvec))
		__pagevec_lru_add(pvec);

	pvec = &per_cpu(lru_rotate_pvecs, cpu);
	if (pagevec_count(pvec)) {
		unsigned long flags;

		/* No harm done if a racing interrupt already did this */
		local_irq_save(flags);
		pagevec_move_tail(pvec);
		local_irq_restore(flags);
	}

	pvec = &per_cpu(lru_deactivate_file_pvecs, cpu);
	if (pagevec_count(pvec))
		pagevec_lru_move_fn(pvec, lru_deactivate_file_fn, NULL);

	pvec = &per_cpu(lru_deactivate_pvecs, cpu);
	if (pagevec_count(pvec))
		pagevec_lru_move_fn(pvec, lru_deactivate_fn, NULL);

	pvec = &per_cpu(lru_lazyfree_pvecs, cpu);
	if (pagevec_count(pvec))
		pagevec_lru_move_fn(pvec, lru_lazyfree_fn, NULL);

	activate_page_drain(cpu);
}

/**
 * pgdat->lru_lock is heavily contended.  Some of the functions that
 * shrink the lists perform better by taking out a batch of pages
 * and working on them outside the LRU lock.
 *
 * For pagecache intensive workloads, this function is the hottest
 * spot in the kernel (apart from copy_*_user functions).
 *
 * Appropriate locks must be held before calling this function.
 *
 * @nr_to_scan:	The number of eligible pages to look through on the list.
 * @lruvec:	The LRU vector to pull pages from.
 * @dst:	The temp list to put pages on to.
 * @nr_scanned:	The number of pages that were scanned.
 * @sc:		The scan_control struct for this reclaim session
 * @mode:	One of the LRU isolation modes
 * @lru:	LRU list id for isolating
 *
 * returns how many pages were moved onto *@dst.
 */
static unsigned long isolate_lru_pages(unsigned long nr_to_scan,
		struct lruvec *lruvec, struct list_head *dst,
		unsigned long *nr_scanned, struct scan_control *sc,
		enum lru_list lru)
{
	struct list_head *src = &lruvec->lists[lru];
	unsigned long nr_taken = 0;
	unsigned long nr_zone_taken[MAX_NR_ZONES] = { 0 };
	unsigned long nr_skipped[MAX_NR_ZONES] = { 0, };
	unsigned long skipped = 0;
	unsigned long scan, total_scan, nr_pages;
	LIST_HEAD(pages_skipped);
	isolate_mode_t mode = (sc->may_unmap ? 0 : ISOLATE_UNMAPPED);

	total_scan = 0;
	scan = 0;
	while (scan < nr_to_scan && !list_empty(src)) {
		struct page *page;

		page = lru_to_page(src);
		prefetchw_prev_lru_page(page, src, flags);

		VM_BUG_ON_PAGE(!PageLRU(page), page);

		nr_pages = compound_nr(page);
		total_scan += nr_pages;

		if (page_zonenum(page) > sc->reclaim_idx) {
			list_move(&page->lru, &pages_skipped);
			nr_skipped[page_zonenum(page)] += nr_pages;
			continue;
		}

		/*
		 * Do not count skipped pages because that makes the function
		 * return with no isolated pages if the LRU mostly contains
		 * ineligible pages.  This causes the VM to not reclaim any
		 * pages, triggering a premature OOM.
		 *
		 * Account all tail pages of THP.  This would not cause
		 * premature OOM since __isolate_lru_page() returns -EBUSY
		 * only when the page is being freed somewhere else.
		 */
		scan += nr_pages;
		switch (__isolate_lru_page(page, mode)) {
		case 0:
			nr_taken += nr_pages;
			nr_zone_taken[page_zonenum(page)] += nr_pages;
			list_move(&page->lru, dst);
			break;

		case -EBUSY:
			/* else it is being freed elsewhere */
			list_move(&page->lru, src);
			continue;

		default:
			BUG();
		}
	}

	/*
	 * Splice any skipped pages to the start of the LRU list. Note that
	 * this disrupts the LRU order when reclaiming for lower zones but
	 * we cannot splice to the tail. If we did then the SWAP_CLUSTER_MAX
	 * scanning would soon rescan the same pages to skip and put the
	 * system at risk of premature OOM.
	 */
	if (!list_empty(&pages_skipped)) {
		int zid;

		list_splice(&pages_skipped, src);
		for (zid = 0; zid < MAX_NR_ZONES; zid++) {
			if (!nr_skipped[zid])
				continue;

			__count_zid_vm_events(PGSCAN_SKIP, zid, nr_skipped[zid]);
			skipped += nr_skipped[zid];
		}
	}
	*nr_scanned = total_scan;
	trace_mm_vmscan_lru_isolate(sc->reclaim_idx, sc->order, nr_to_scan,
				    total_scan, skipped, nr_taken, mode, lru);
	update_lru_sizes(lruvec, lru, nr_zone_taken);
	return nr_taken;
}

SetPageSwapCache(page);

/*
 * Various page->flags bits:
 *
 * PG_reserved is set for special pages. The "struct page" of such a page
 * should in general not be touched (e.g. set dirty) except by its owner.
 * Pages marked as PG_reserved include:
 * - Pages part of the kernel image (including vDSO) and similar (e.g. BIOS,
 *   initrd, HW tables)
 * - Pages reserved or allocated early during boot (before the page allocator
 *   was initialized). This includes (depending on the architecture) the
 *   initial vmemmap, initial page tables, crashkernel, elfcorehdr, and much
 *   much more. Once (if ever) freed, PG_reserved is cleared and they will
 *   be given to the page allocator.
 * - Pages falling into physical memory gaps - not IORESOURCE_SYSRAM. Trying
 *   to read/write these pages might end badly. Don't touch!
 * - The zero page(s)
 * - Pages not added to the page allocator when onlining a section because
 *   they were excluded via the online_page_callback() or because they are
 *   PG_hwpoison.
 * - Pages allocated in the context of kexec/kdump (loaded kernel image,
 *   control pages, vmcoreinfo)
 * - MMIO/DMA pages. Some architectures don't allow to ioremap pages that are
 *   not marked PG_reserved (as they might be in use by somebody else who does
 *   not respect the caching strategy).
 * - Pages part of an offline section (struct pages of offline sections should
 *   not be trusted as they will be initialized when first onlined).
 * - MCA pages on ia64
 * - Pages holding CPU notes for POWER Firmware Assisted Dump
 * - Device memory (e.g. PMEM, DAX, HMM)
 * Some PG_reserved pages will be excluded from the hibernation image.
 * PG_reserved does in general not hinder anybody from dumping or swapping
 * and is no longer required for remap_pfn_range(). ioremap might require it.
 * Consequently, PG_reserved for a page mapped into user space can indicate
 * the zero page, the vDSO, MMIO pages or device memory.
 *
 * The PG_private bitflag is set on pagecache pages if they contain filesystem
 * specific data (which is normally at page->private). It can be used by
 * private allocations for its own usage.
 *
 * During initiation of disk I/O, PG_locked is set. This bit is set before I/O
 * and cleared when writeback _starts_ or when read _completes_. PG_writeback
 * is set before writeback starts and cleared when it finishes.
 *
 * PG_locked also pins a page in pagecache, and blocks truncation of the file
 * while it is held.
 *
 * page_waitqueue(page) is a wait queue of all tasks waiting for the page
 * to become unlocked.
 *
 * PG_uptodate tells whether the page's contents is valid.  When a read
 * completes, the page becomes uptodate, unless a disk I/O error happened.
 *
 * PG_referenced, PG_reclaim are used for page reclaim for anonymous and
 * file-backed pagecache (see mm/vmscan.c).
 *
 * PG_error is set to indicate that an I/O error occurred on this page.
 *
 * PG_arch_1 is an architecture specific page state bit.  The generic code
 * guarantees that this bit is cleared for a page when it first is entered into
 * the page cache.
 *
 * PG_hwpoison indicates that a page got corrupted in hardware and contains
 * data with incorrect ECC bits that triggered a machine check. Accessing is
 * not safe since it may cause another machine check. Don't touch!
 */

/*
 * Don't use the *_dontuse flags.  Use the macros.  Otherwise you'll break
 * locked- and dirty-page accounting.
 *
 * The page flags field is split into two parts, the main flags area
 * which extends from the low bits upwards, and the fields area which
 * extends from the high bits downwards.
 *
 *  | FIELD | ... | FLAGS |
 *  N-1           ^       0
 *               (NR_PAGEFLAGS)
 *
 * The fields area is reserved for fields mapping zone, node (for NUMA) and
 * SPARSEMEM section (for variants of SPARSEMEM that require section ids like
 * SPARSEMEM_EXTREME with !SPARSEMEM_VMEMMAP).
 */
enum pageflags {
	PG_locked,		/* Page is locked. Don't touch. */
	PG_referenced,
	PG_uptodate,
	PG_dirty,
	PG_lru,
	PG_active,
	PG_workingset,
	PG_waiters,		/* Page has waiters, check its waitqueue. Must be bit #7 and in the same byte as "PG_locked" */
	PG_error,
	PG_slab,
	PG_owner_priv_1,	/* Owner use. If pagecache, fs may use*/
	PG_arch_1,
	PG_reserved,
	PG_private,		/* If pagecache, has fs-private data */
	PG_private_2,		/* If pagecache, has fs aux data */
	PG_writeback,		/* Page is under writeback */
	PG_head,		/* A head page */
	PG_mappedtodisk,	/* Has blocks allocated on-disk */
	PG_reclaim,		/* To be reclaimed asap */
	PG_swapbacked,		/* Page is backed by RAM/swap */
	PG_unevictable,		/* Page is "unevictable"  */
#ifdef CONFIG_MMU
	PG_mlocked,		/* Page is vma mlocked */
#endif
#ifdef CONFIG_ARCH_USES_PG_UNCACHED
	PG_uncached,		/* Page has been mapped as uncached */
#endif
#ifdef CONFIG_MEMORY_FAILURE
	PG_hwpoison,		/* hardware poisoned page. Don't touch */
#endif
#if defined(CONFIG_IDLE_PAGE_TRACKING) && defined(CONFIG_64BIT)
	PG_young,
	PG_idle,
#endif
	__NR_PAGEFLAGS,

	/* Filesystems */
	PG_checked = PG_owner_priv_1,

	/* SwapBacked */
	PG_swapcache = PG_owner_priv_1,	/* Swap page: swp_entry_t in private */

	/* Two page bits are conscripted by FS-Cache to maintain local caching
	 * state.  These bits are set on pages belonging to the netfs's inodes
	 * when those inodes are being locally cached.
	 */
	PG_fscache = PG_private_2,	/* page backed by cache */

	/* XEN */
	/* Pinned in Xen as a read-only pagetable page. */
	PG_pinned = PG_owner_priv_1,
	/* Pinned as part of domain save (see xen_mm_pin_all()). */
	PG_savepinned = PG_dirty,
	/* Has a grant mapping of another (foreign) domain's page. */
	PG_foreign = PG_owner_priv_1,
	/* Remapped by swiotlb-xen. */
	PG_xen_remapped = PG_owner_priv_1,

	/* SLOB */
	PG_slob_free = PG_private,

	/* Compound pages. Stored in first tail page's flags */
	PG_double_map = PG_private_2,

	/* non-lru isolated movable page */
	PG_isolated = PG_reclaim,
};


/*
 * shrink_inactive_list() is a helper for shrink_node().  It returns the number
 * of reclaimed pages
 */
static noinline_for_stack unsigned long
shrink_inactive_list(unsigned long nr_to_scan, struct lruvec *lruvec,
		     struct scan_control *sc, enum lru_list lru)
{
	LIST_HEAD(page_list);
	unsigned long nr_scanned;
	unsigned long nr_reclaimed = 0;
	unsigned long nr_taken;
	struct reclaim_stat stat;
	int file = is_file_lru(lru);
	enum vm_event_item item;
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);
	struct zone_reclaim_stat *reclaim_stat = &lruvec->reclaim_stat;
	bool stalled = false;

	while (unlikely(too_many_isolated(pgdat, file, sc))) {
		if (stalled)
			return 0;

		/* wait a bit for the reclaimer. */
		msleep(100);
		stalled = true;

		/* We are about to die and free our memory. Return now. */
		if (fatal_signal_pending(current))
			return SWAP_CLUSTER_MAX;
	}

	lru_add_drain();

	spin_lock_irq(&pgdat->lru_lock);

	nr_taken = isolate_lru_pages(nr_to_scan, lruvec, &page_list,
				     &nr_scanned, sc, lru);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, nr_taken);
	reclaim_stat->recent_scanned[file] += nr_taken;

	item = current_is_kswapd() ? PGSCAN_KSWAPD : PGSCAN_DIRECT;
	if (global_reclaim(sc))
		__count_vm_events(item, nr_scanned);
	__count_memcg_events(lruvec_memcg(lruvec), item, nr_scanned);
	spin_unlock_irq(&pgdat->lru_lock);

	if (nr_taken == 0)
		return 0;

	nr_reclaimed = shrink_page_list(&page_list, pgdat, sc, 0,
				&stat, false);

	spin_lock_irq(&pgdat->lru_lock);

	item = current_is_kswapd() ? PGSTEAL_KSWAPD : PGSTEAL_DIRECT;
	if (global_reclaim(sc))
		__count_vm_events(item, nr_reclaimed);
	__count_memcg_events(lruvec_memcg(lruvec), item, nr_reclaimed);
	reclaim_stat->recent_rotated[0] += stat.nr_activate[0];
	reclaim_stat->recent_rotated[1] += stat.nr_activate[1];

	move_pages_to_lru(lruvec, &page_list);

	__mod_node_page_state(pgdat, NR_ISOLATED_ANON + file, -nr_taken);

	spin_unlock_irq(&pgdat->lru_lock);

	mem_cgroup_uncharge_list(&page_list);
	free_unref_page_list(&page_list);

	/*
	 * If dirty pages are scanned that are not queued for IO, it
	 * implies that flushers are not doing their job. This can
	 * happen when memory pressure pushes dirty pages to the end of
	 * the LRU before the dirty limits are breached and the dirty
	 * data has expired. It can also happen when the proportion of
	 * dirty pages grows not through writes but through memory
	 * pressure reclaiming all the clean cache. And in some cases,
	 * the flushers simply cannot keep up with the allocation
	 * rate. Nudge the flusher threads in case they are asleep.
	 */
	if (stat.nr_unqueued_dirty == nr_taken)
		wakeup_flusher_threads(WB_REASON_VMSCAN);

	sc->nr.dirty += stat.nr_dirty;
	sc->nr.congested += stat.nr_congested;
	sc->nr.unqueued_dirty += stat.nr_unqueued_dirty;
	sc->nr.writeback += stat.nr_writeback;
	sc->nr.immediate += stat.nr_immediate;
	sc->nr.taken += nr_taken;
	if (file)
		sc->nr.file_taken += nr_taken;

	trace_mm_vmscan_lru_shrink_inactive(pgdat->node_id,
			nr_scanned, nr_reclaimed, &stat, sc->priority, file);
	return nr_reclaimed;
}

#define TESTPAGEFLAG_FALSE(uname)					\
static inline int Page##uname(const struct page *page) { return 0; }

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
/*
 * PageHuge() only returns true for hugetlbfs pages, but not for
 * normal or transparent huge pages.
 *
 * PageTransHuge() returns true for both transparent huge and
 * hugetlbfs pages, but not normal pages. PageTransHuge() can only be
 * called only in the core VM paths where hugetlbfs pages can't exist.
 */
static inline int PageTransHuge(struct page *page)
{
	VM_BUG_ON_PAGE(PageTail(page), page);
	return PageHead(page);
}

/*
 * PageTransCompound returns true for both transparent huge pages
 * and hugetlbfs pages, so it should only be called when it's known
 * that hugetlbfs pages aren't involved.
 */
static inline int PageTransCompound(struct page *page)
{
	return PageCompound(page);
}

/*
 * PageTransCompoundMap is the same as PageTransCompound, but it also
 * guarantees the primary MMU has the entire compound page mapped
 * through pmd_trans_huge, which in turn guarantees the secondary MMUs
 * can also map the entire compound page. This allows the secondary
 * MMUs to call get_user_pages() only once for each compound page and
 * to immediately map the entire compound page with a single secondary
 * MMU fault. If there will be a pmd split later, the secondary MMUs
 * will get an update through the MMU notifier invalidation through
 * split_huge_pmd().
 *
 * Unlike PageTransCompound, this is safe to be called only while
 * split_huge_pmd() cannot run from under us, like if protected by the
 * MMU notifier, otherwise it may result in page->_mapcount check false
 * positives.
 *
 * We have to treat page cache THP differently since every subpage of it
 * would get _mapcount inc'ed once it is PMD mapped.  But, it may be PTE
 * mapped in the current process so comparing subpage's _mapcount to
 * compound_mapcount to filter out PTE mapped case.
 */
static inline int PageTransCompoundMap(struct page *page)
{
	struct page *head;

	if (!PageTransCompound(page))
		return 0;

	if (PageAnon(page))
		return atomic_read(&page->_mapcount) < 0;

	head = compound_head(page);
	/* File THP is PMD mapped and not PTE mapped */
	return atomic_read(&page->_mapcount) ==
	       atomic_read(compound_mapcount_ptr(head));
}

/*
 * PageTransTail returns true for both transparent huge pages
 * and hugetlbfs pages, so it should only be called when it's known
 * that hugetlbfs pages aren't involved.
 */
static inline int PageTransTail(struct page *page)
{
	return PageTail(page);
}

/*
 * PageDoubleMap indicates that the compound page is mapped with PTEs as well
 * as PMDs.
 *
 * This is required for optimization of rmap operations for THP: we can postpone
 * per small page mapcount accounting (and its overhead from atomic operations)
 * until the first PMD split.
 *
 * For the page PageDoubleMap means ->_mapcount in all sub-pages is offset up
 * by one. This reference will go away with last compound_mapcount.
 *
 * See also __split_huge_pmd_locked() and page_remove_anon_compound_rmap().
 */
static inline int PageDoubleMap(struct page *page)
{
	return PageHead(page) && test_bit(PG_double_map, &page[1].flags);
}

static inline void SetPageDoubleMap(struct page *page)
{
	VM_BUG_ON_PAGE(!PageHead(page), page);
	set_bit(PG_double_map, &page[1].flags);
}

static inline void ClearPageDoubleMap(struct page *page)
{
	VM_BUG_ON_PAGE(!PageHead(page), page);
	clear_bit(PG_double_map, &page[1].flags);
}
static inline int TestSetPageDoubleMap(struct page *page)
{
	VM_BUG_ON_PAGE(!PageHead(page), page);
	return test_and_set_bit(PG_double_map, &page[1].flags);
}

static inline int TestClearPageDoubleMap(struct page *page)
{
	VM_BUG_ON_PAGE(!PageHead(page), page);
	return test_and_clear_bit(PG_double_map, &page[1].flags);
}

#else
TESTPAGEFLAG_FALSE(TransHuge)
TESTPAGEFLAG_FALSE(TransCompound)
TESTPAGEFLAG_FALSE(TransCompoundMap)
TESTPAGEFLAG_FALSE(TransTail)
PAGEFLAG_FALSE(DoubleMap)
	TESTSETFLAG_FALSE(DoubleMap)
	TESTCLEARFLAG_FALSE(DoubleMap)
#endif


#ifdef CONFIG_TRANSPARENT_HUGEPAGE
#else /* CONFIG_TRANSPARENT_HUGEPAGE */
static inline int
split_huge_page_to_list(struct page *page, struct list_head *list)
{
	return 0;
}
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */




















/*
 * shrink_page_list() returns the number of reclaimed pages
 */
static unsigned long shrink_page_list(struct list_head *page_list,
				      struct pglist_data *pgdat,
				      struct scan_control *sc,
				      enum ttu_flags ttu_flags,
				      struct reclaim_stat *stat,
				      bool ignore_references)
{
	LIST_HEAD(ret_pages);
	LIST_HEAD(free_pages);
	unsigned nr_reclaimed = 0;
	unsigned pgactivate = 0;

	memset(stat, 0, sizeof(*stat));
	cond_resched();

	while (!list_empty(page_list)) {
		struct address_space *mapping;
		struct page *page;
		int may_enter_fs;
		enum page_references references = PAGEREF_RECLAIM;
		bool dirty, writeback;
		unsigned int nr_pages;

		cond_resched();

		page = lru_to_page(page_list);
		list_del(&page->lru);

		if (!trylock_page(page))
			goto keep;

		VM_BUG_ON_PAGE(PageActive(page), page);

		nr_pages = compound_nr(page);

		/* Account the number of base pages even though THP */
		sc->nr_scanned += nr_pages;

		if (unlikely(!page_evictable(page)))
			goto activate_locked;

		if (!sc->may_unmap && page_mapped(page))
			goto keep_locked;

		may_enter_fs = (sc->gfp_mask & __GFP_FS) ||
			(PageSwapCache(page) && (sc->gfp_mask & __GFP_IO));

		/*
		 * The number of dirty pages determines if a node is marked
		 * reclaim_congested which affects wait_iff_congested. kswapd
		 * will stall and start writing pages if the tail of the LRU
		 * is all dirty unqueued pages.
		 */
		page_check_dirty_writeback(page, &dirty, &writeback);
		if (dirty || writeback)
			stat->nr_dirty++;

		if (dirty && !writeback)
			stat->nr_unqueued_dirty++;

		/*
		 * Treat this page as congested if the underlying BDI is or if
		 * pages are cycling through the LRU so quickly that the
		 * pages marked for immediate reclaim are making it to the
		 * end of the LRU a second time.
		 */
		mapping = page_mapping(page);
		if (((dirty || writeback) && mapping &&
		     inode_write_congested(mapping->host)) ||
		    (writeback && PageReclaim(page)))
			stat->nr_congested++;

		/*
		 * If a page at the tail of the LRU is under writeback, there
		 * are three cases to consider.
		 *
		 * 1) If reclaim is encountering an excessive number of pages
		 *    under writeback and this page is both under writeback and
		 *    PageReclaim then it indicates that pages are being queued
		 *    for IO but are being recycled through the LRU before the
		 *    IO can complete. Waiting on the page itself risks an
		 *    indefinite stall if it is impossible to writeback the
		 *    page due to IO error or disconnected storage so instead
		 *    note that the LRU is being scanned too quickly and the
		 *    caller can stall after page list has been processed.
		 *
		 * 2) Global or new memcg reclaim encounters a page that is
		 *    not marked for immediate reclaim, or the caller does not
		 *    have __GFP_FS (or __GFP_IO if it's simply going to swap,
		 *    not to fs). In this case mark the page for immediate
		 *    reclaim and continue scanning.
		 *
		 *    Require may_enter_fs because we would wait on fs, which
		 *    may not have submitted IO yet. And the loop driver might
		 *    enter reclaim, and deadlock if it waits on a page for
		 *    which it is needed to do the write (loop masks off
		 *    __GFP_IO|__GFP_FS for this reason); but more thought
		 *    would probably show more reasons.
		 *
		 * 3) Legacy memcg encounters a page that is already marked
		 *    PageReclaim. memcg does not have any dirty pages
		 *    throttling so we could easily OOM just because too many
		 *    pages are in writeback and there is nothing else to
		 *    reclaim. Wait for the writeback to complete.
		 *
		 * In cases 1) and 2) we activate the pages to get them out of
		 * the way while we continue scanning for clean pages on the
		 * inactive list and refilling from the active list. The
		 * observation here is that waiting for disk writes is more
		 * expensive than potentially causing reloads down the line.
		 * Since they're marked for immediate reclaim, they won't put
		 * memory pressure on the cache working set any longer than it
		 * takes to write them to disk.
		 */
		if (PageWriteback(page)) {
			/* Case 1 above */
			if (current_is_kswapd() &&
			    PageReclaim(page) &&
			    test_bit(PGDAT_WRITEBACK, &pgdat->flags)) {
				stat->nr_immediate++;
				goto activate_locked;

			/* Case 2 above */
			} else if (sane_reclaim(sc) ||
			    !PageReclaim(page) || !may_enter_fs) {
				/*
				 * This is slightly racy - end_page_writeback()
				 * might have just cleared PageReclaim, then
				 * setting PageReclaim here end up interpreted
				 * as PageReadahead - but that does not matter
				 * enough to care.  What we do want is for this
				 * page to have PageReclaim set next time memcg
				 * reclaim reaches the tests above, so it will
				 * then wait_on_page_writeback() to avoid OOM;
				 * and it's also appropriate in global reclaim.
				 */
				SetPageReclaim(page);
				stat->nr_writeback++;
				goto activate_locked;

			/* Case 3 above */
			} else {
				unlock_page(page);
				wait_on_page_writeback(page);
				/* then go back and try same page again */
				list_add_tail(&page->lru, page_list);
				continue;
			}
		}

		if (!ignore_references)
			references = page_check_references(page, sc);

		switch (references) {
		case PAGEREF_ACTIVATE:
			goto activate_locked;
		case PAGEREF_KEEP:
			stat->nr_ref_keep += nr_pages;
			goto keep_locked;
		case PAGEREF_RECLAIM:
		case PAGEREF_RECLAIM_CLEAN:
			; /* try to reclaim the page below */
		}

		/*
		 * Anonymous process memory has backing store?
		 * Try to allocate it some swap space here.
		 * Lazyfree page could be freed directly
		 */
		if (PageAnon(page) && PageSwapBacked(page)) {
			if (!PageSwapCache(page)) {
				if (!(sc->gfp_mask & __GFP_IO))
					goto keep_locked;
				if (PageTransHuge(page)) {
					/* cannot split THP, skip it */
					if (!can_split_huge_page(page, NULL))
						goto activate_locked;
					/*
					 * Split pages without a PMD map right
					 * away. Chances are some or all of the
					 * tail pages can be freed without IO.
					 */
					if (!compound_mapcount(page) &&
					    split_huge_page_to_list(page,
								    page_list))
						goto activate_locked;
				}
				if (!add_to_swap(page)) {
					if (!PageTransHuge(page))
						goto activate_locked_split;
					/* Fallback to swap normal pages */
					if (split_huge_page_to_list(page,
								    page_list))
						goto activate_locked;
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
					count_vm_event(THP_SWPOUT_FALLBACK);
#endif
					if (!add_to_swap(page))
						goto activate_locked_split;
				}

				may_enter_fs = 1;

				/* Adding to swap updated mapping */
				mapping = page_mapping(page);
			}
		} else if (unlikely(PageTransHuge(page))) {
			/* Split file THP */
			if (split_huge_page_to_list(page, page_list))
				goto keep_locked;
		}

		/*
		 * THP may get split above, need minus tail pages and update
		 * nr_pages to avoid accounting tail pages twice.
		 *
		 * The tail pages that are added into swap cache successfully
		 * reach here.
		 */
		if ((nr_pages > 1) && !PageTransHuge(page)) {
			sc->nr_scanned -= (nr_pages - 1);
			nr_pages = 1;
		}

		/*
		 * The page is mapped into the page tables of one or more
		 * processes. Try to unmap it here.
		 */
		if (page_mapped(page)) {
			enum ttu_flags flags = ttu_flags | TTU_BATCH_FLUSH;                 // 从shrink_inactive_list传递进的ttu_flags为0

			if (unlikely(PageTransHuge(page)))
				flags |= TTU_SPLIT_HUGE_PMD;
			if (!try_to_unmap(page, flags)) {
				stat->nr_unmap_fail += nr_pages;
				goto activate_locked;
			}
		}

		if (PageDirty(page)) {                                                  // 如果页是脏页
			/*
			 * Only kswapd can writeback filesystem pages
			 * to avoid risk of stack overflow. But avoid
			 * injecting inefficient single-page IO into
			 * flusher writeback as much as possible: only
			 * write pages when we've encountered many
			 * dirty pages, and when we've already scanned
			 * the rest of the LRU for clean pages and see
			 * the same dirty pages again (PageReclaim).
			 */
			if (page_is_file_cache(page) &&                                      // 一个前提：页是file cache
			    (!current_is_kswapd() || !PageReclaim(page) ||                   // 另一个前提是一下任意成立：不是kswapd线程，PG_reclaim标志为0，PGDAT_DIRTY为0
			     !test_bit(PGDAT_DIRTY, &pgdat->flags))) {                       // 阅读注释
				/*
				 * Immediately reclaim when written back.
				 * Similar in principal to deactivate_page()
				 * except we already have the page isolated
				 * and know it's dirty
				 */
				inc_node_page_state(page, NR_VMSCAN_IMMEDIATE);                  // 待分析
				SetPageReclaim(page);                                            // 设置PG_reclaim标志

				goto activate_locked;                                            // 跳转至activate_locked
			}

			if (references == PAGEREF_RECLAIM_CLEAN)                             // 待分析
				goto keep_locked;                                                // 跳转至keep_locked
			if (!may_enter_fs)                                                   // 不允许文件系统操作
				goto keep_locked;                                                // 跳转至keep_locked
			if (!sc->may_writepage)                                              // 不允许回写操作
				goto keep_locked;                                                // 跳转至keep_locked

			/*
			 * Page is dirty. Flush the TLB if a writable entry
			 * potentially exists to avoid CPU writes after IO
			 * starts and then write it out here.
			 */
			try_to_unmap_flush_dirty();
			switch (pageout(page, mapping, sc)) {
			case PAGE_KEEP:
				goto keep_locked;
			case PAGE_ACTIVATE:
				goto activate_locked;
			case PAGE_SUCCESS:
				if (PageWriteback(page))
					goto keep;
				if (PageDirty(page))
					goto keep;

				/*
				 * A synchronous write - probably a ramdisk.  Go
				 * ahead and try to reclaim the page.
				 */
				if (!trylock_page(page))
					goto keep;
				if (PageDirty(page) || PageWriteback(page))
					goto keep_locked;
				mapping = page_mapping(page);
			case PAGE_CLEAN:
				; /* try to free the page below */
			}
		}

		/*
		 * If the page has buffers, try to free the buffer mappings
		 * associated with this page. If we succeed we try to free
		 * the page as well.
		 *
		 * We do this even if the page is PageDirty().
		 * try_to_release_page() does not perform I/O, but it is
		 * possible for a page to have PageDirty set, but it is actually
		 * clean (all its buffers are clean).  This happens if the
		 * buffers were written out directly, with submit_bh(). ext3
		 * will do this, as well as the blockdev mapping.
		 * try_to_release_page() will discover that cleanness and will
		 * drop the buffers and mark the page clean - it can be freed.
		 *
		 * Rarely, pages can have buffers and no ->mapping.  These are
		 * the pages which were not successfully invalidated in
		 * truncate_complete_page().  We try to drop those buffers here
		 * and if that worked, and the page is no longer mapped into
		 * process address space (page_count == 1) it can be freed.
		 * Otherwise, leave the page on the LRU so it is swappable.
		 */
		if (page_has_private(page)) {
			if (!try_to_release_page(page, sc->gfp_mask))
				goto activate_locked;
			if (!mapping && page_count(page) == 1) {
				unlock_page(page);
				if (put_page_testzero(page))
					goto free_it;
				else {
					/*
					 * rare race with speculative reference.
					 * the speculative reference will free
					 * this page shortly, so we may
					 * increment nr_reclaimed here (and
					 * leave it off the LRU).
					 */
					nr_reclaimed++;
					continue;
				}
			}
		}

		if (PageAnon(page) && !PageSwapBacked(page)) {
			/* follow __remove_mapping for reference */
			if (!page_ref_freeze(page, 1))
				goto keep_locked;
			if (PageDirty(page)) {
				page_ref_unfreeze(page, 1);
				goto keep_locked;
			}

			count_vm_event(PGLAZYFREED);
			count_memcg_page_event(page, PGLAZYFREED);
		} else if (!mapping || !__remove_mapping(mapping, page, true))
			goto keep_locked;

		unlock_page(page);
free_it:
		/*
		 * THP may get swapped out in a whole, need account
		 * all base pages.
		 */
		nr_reclaimed += nr_pages;

		/*
		 * Is there need to periodically free_page_list? It would
		 * appear not as the counts should be low
		 */
		if (unlikely(PageTransHuge(page)))
			(*get_compound_page_dtor(page))(page);
		else
			list_add(&page->lru, &free_pages);
		continue;

activate_locked_split:
		/*
		 * The tail pages that are failed to add into swap cache
		 * reach here.  Fixup nr_scanned and nr_pages.
		 */
		if (nr_pages > 1) {
			sc->nr_scanned -= (nr_pages - 1);
			nr_pages = 1;
		}
activate_locked:
		/* Not a candidate for swapping, so reclaim swap space. */
		if (PageSwapCache(page) && (mem_cgroup_swap_full(page) ||
						PageMlocked(page)))
			try_to_free_swap(page);
		VM_BUG_ON_PAGE(PageActive(page), page);
		if (!PageMlocked(page)) {
			int type = page_is_file_cache(page);
			SetPageActive(page);
			stat->nr_activate[type] += nr_pages;
			count_memcg_page_event(page, PGACTIVATE);
		}
keep_locked:
		unlock_page(page);
keep:
		list_add(&page->lru, &ret_pages);
		VM_BUG_ON_PAGE(PageLRU(page) || PageUnevictable(page), page);
	}

	pgactivate = stat->nr_activate[0] + stat->nr_activate[1];

	mem_cgroup_uncharge_list(&free_pages);
	try_to_unmap_flush();
	free_unref_page_list(&free_pages);

	list_splice(&ret_pages, page_list);
	count_vm_events(PGACTIVATE, pgactivate);

	return nr_reclaimed;
}

/*
 * pageout is called by shrink_page_list() for each dirty page.
 * Calls ->writepage().
 */
static pageout_t pageout(struct page *page, struct address_space *mapping,
			 struct scan_control *sc)
{
	/*
	 * If the page is dirty, only perform writeback if that write
	 * will be non-blocking.  To prevent this allocation from being
	 * stalled by pagecache activity.  But note that there may be
	 * stalls if we need to run get_block().  We could test
	 * PagePrivate for that.
	 *
	 * If this process is currently in __generic_file_write_iter() against
	 * this page's queue, we can perform writeback even if that
	 * will block.
	 *
	 * If the page is swapcache, write it back even if that would
	 * block, for some throttling. This happens by accident, because
	 * swap_backing_dev_info is bust: it doesn't reflect the
	 * congestion state of the swapdevs.  Easy to fix, if needed.
	 */
	if (!is_page_cache_freeable(page))                                    // 如果页不是可free的
		return PAGE_KEEP;                                                 // 返回PAGE_KEEP
	if (!mapping) {                                                       // 如果mapping不存在，什么情况mapping不存在？
		/*
		 * Some data journaling orphaned pages can have
		 * page->mapping == NULL while being dirty with clean buffers.
		 */
		if (page_has_private(page)) {                                     // 如果PG_private*标志被置位
			if (try_to_free_buffers(page)) {                              // try_to_free_buffers
				ClearPageDirty(page);                                     // 清楚PG_dirty标志
				pr_info("%s: orphaned page\n", __func__);
				return PAGE_CLEAN;                                        // 返回PAGE_CLEAN
			}
		}
		return PAGE_KEEP;                                                 // 返回PAGE_KEEP
	}
	if (mapping->a_ops->writepage == NULL)
		return PAGE_ACTIVATE;
	if (!may_write_to_inode(mapping->host, sc))
		return PAGE_KEEP;

	if (clear_page_dirty_for_io(page)) {
		int res;
		struct writeback_control wbc = {
			.sync_mode = WB_SYNC_NONE,
			.nr_to_write = SWAP_CLUSTER_MAX,
			.range_start = 0,
			.range_end = LLONG_MAX,
			.for_reclaim = 1,
		};

		SetPageReclaim(page);
		res = mapping->a_ops->writepage(page, &wbc);
		if (res < 0)
			handle_write_error(mapping, page, res);
		if (res == AOP_WRITEPAGE_ACTIVATE) {
			ClearPageReclaim(page);
			return PAGE_ACTIVATE;
		}

		if (!PageWriteback(page)) {
			/* synchronous write or broken a_ops? */
			ClearPageReclaim(page);
		}
		trace_mm_vmscan_writepage(page);
		inc_node_page_state(page, NR_VMSCAN_WRITE);
		return PAGE_SUCCESS;
	}

	return PAGE_CLEAN;
}

u64 __section(".mmuoff.data.write") vabits_actual;
EXPORT_SYMBOL(vabits_actual);


#define VA_BITS			(CONFIG_ARM64_VA_BITS)
#if VA_BITS > 48
#define VA_BITS_MIN		(48)
#else
#define VA_BITS_MIN		(VA_BITS)
#endif

#define _PAGE_END(va)		(-(UL(1) << ((va) - 1)))                       // 0xffffffffffffffff << (39 - 1) = 0xffffffc000000000
#define _PAGE_OFFSET(va)	(-(UL(1) << (va)))                             // 0xffffffffffffffff << 39       = 0xffffff8000000000


/*
 * Generic and tag-based KASAN require 1/8th and 1/16th of the kernel virtual
 * address space for the shadow region respectively. They can bloat the stack
 * significantly, so double the (minimum) stack size when they are in use.
 */
#if defined(CONFIG_KASAN_GENERIC) || defined(CONFIG_KASAN_SW_TAGS)
#define KASAN_SHADOW_OFFSET	_AC(CONFIG_KASAN_SHADOW_OF FSET, UL)
#define KASAN_SHADOW_END	((UL(1) << (64 - KASAN_SHADOW_SCALE_SHIFT)) \
					+ KASAN_SHADOW_OFFSET)
#define PAGE_END		(KASAN_SHADOW_END - (1UL << (vabits_actual - KASAN_SHADOW_SCALE_SHIFT)))
#define KASAN_THREAD_SHIFT	1
#else
#define KASAN_THREAD_SHIFT	0
#define PAGE_END		(_PAGE_END(VA_BITS_MIN))
#endif /* CONFIG_KASAN */

/**
 * of_get_flat_dt_prop - Given a node in the flat blob, return the property ptr
 *
 * This function can be used within scan_flattened_dt callback to get
 * access to properties
 */
const void *__init of_get_flat_dt_prop(unsigned long node, const char *name,
				       int *size)
{
	return fdt_getprop(initial_boot_params, node, name, size);
}

u64 __init dt_mem_next_cell(int s, const __be32 **cellp)
{
	const __be32 *p = *cellp;

	*cellp = p + s;
	return of_read_number(p, s);
}

/*
linux,usable-memory-range
-------------------------

This property (arm64 only) holds a base address and size, describing a
limited region in which memory may be considered available for use by
the kernel. Memory outside of this range is not available for use.

This property describes a limitation: memory within this range is only
valid when also described through another mechanism that the kernel
would otherwise use to determine available memory (e.g. memory nodes
or the EFI memory map). Valid memory may be sparse within the range.
e.g.

/ {
	chosen {
		linux,usable-memory-range = <0x9 0xf0000000 0x0 0x10000000>;
	};
};

The main usage is for crash dump kernel to identify its own usable
memory and exclude, at its boot time, any other memory areas that are
part of the panicked kernel's memory.
*/
static int __init early_init_dt_scan_usablemem(unsigned long node,
		const char *uname, int depth, void *data)
{
	struct memblock_region *usablemem = data;
	const __be32 *reg;
	int len;

	if (depth != 1 || strcmp(uname, "chosen") != 0)                         // depth = 1表示是root节点的子节点，如果节点不是chosen则返回
		return 0;

	reg = of_get_flat_dt_prop(node, "linux,usable-memory-range", &len);
	if (!reg || (len < (dt_root_addr_cells + dt_root_size_cells)))          // 属性解析结果合法性判断
		return 1;

	usablemem->base = dt_mem_next_cell(dt_root_addr_cells, &reg);           // 获取base
	usablemem->size = dt_mem_next_cell(dt_root_size_cells, &reg);           // 获取size

	return 1;
}

/**
 * of_scan_flat_dt - scan flattened tree blob and call callback on each.
 * @it: callback function
 * @data: context data pointer
 *
 * This function is used to scan the flattened device-tree, it is
 * used to extract the memory information at boot before we can
 * unflatten the tree
 */
int __init of_scan_flat_dt(int (*it)(unsigned long node,
				     const char *uname, int depth,
				     void *data),
			   void *data)
{
	const void *blob = initial_boot_params;
	const char *pathp;
	int offset, rc = 0, depth = -1;

	if (!blob)
		return 0;

	for (offset = fdt_next_node(blob, -1, &depth);
	     offset >= 0 && depth >= 0 && !rc;
	     offset = fdt_next_node(blob, offset, &depth)) {

		pathp = fdt_get_name(blob, offset, NULL);
		if (*pathp == '/')
			pathp = kbasename(pathp);
		rc = it(offset, pathp, depth, data);
	}
	return rc;
}

static void __init fdt_enforce_memory_region(void)
{
	struct memblock_region reg = {
		.size = 0,
	};

	of_scan_flat_dt(early_init_dt_scan_usablemem, &reg);

	if (reg.size)
		memblock_cap_memory_range(reg.base, reg.size);
}

static inline bool memblock_is_nomap(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_NOMAP;
}

static void __init_memblock memblock_remove_region(struct memblock_type *type, unsigned long r)
{
	type->total_size -= type->regions[r].size;
	memmove(&type->regions[r], &type->regions[r + 1],
		(type->cnt - (r + 1)) * sizeof(type->regions[r]));
	type->cnt--;

	/* Special case for empty arrays */
	if (type->cnt == 0) {
		WARN_ON(type->total_size != 0);
		type->cnt = 1;
		type->regions[0].base = 0;
		type->regions[0].size = 0;
		type->regions[0].flags = 0;
		memblock_set_region_node(&type->regions[0], MAX_NUMNODES);
	}
}

void __init memblock_cap_memory_range(phys_addr_t base, phys_addr_t size)
{
	int start_rgn, end_rgn;
	int i, ret;

	if (!size)                                                            // size为0则直接返回
		return;

	ret = memblock_isolate_range(&memblock.memory, base, size,            // 隔离指定的区域
						&start_rgn, &end_rgn);
	if (ret)                                                              // 返回0表示已隔离指定区域
		return;

	/* remove all the MAP regions */
	for (i = memblock.memory.cnt - 1; i >= end_rgn; i--)                  // 从memory中移除end_rgn及之后的所有MAP区域
		if (!memblock_is_nomap(&memblock.memory.regions[i]))              // 如果是MAP的区域
			memblock_remove_region(&memblock.memory, i);                  // 从memory中移除区域

	for (i = start_rgn - 1; i >= 0; i--)                                  // 从memory中移除start_rgn及之前的所有MAP区域
		if (!memblock_is_nomap(&memblock.memory.regions[i]))              // 如果是MAP的区域
			memblock_remove_region(&memblock.memory, i);                  // 从memory中移除区域

	/* truncate the reserved regions */
	memblock_remove_range(&memblock.reserved, 0, base);                   // 从reserved中移除base之前的range
	memblock_remove_range(&memblock.reserved,                             // 从reserved中移除base + size之后的range
			base + size, PHYS_ADDR_MAX);
}

/* lowest address */
phys_addr_t __init_memblock memblock_start_of_DRAM(void)
{
	return memblock.memory.regions[0].base;
}

phys_addr_t __init_memblock memblock_end_of_DRAM(void)
{
	int idx = memblock.memory.cnt - 1;

	return (memblock.memory.regions[idx].base + memblock.memory.regions[idx].size);
}

#define MAX_RESERVED_REGIONS	32
static struct reserved_mem reserved_mem[MAX_RESERVED_REGIONS];
static int reserved_mem_count;

/**
 * res_mem_save_node() - save fdt node for second pass initialization
 */
void __init fdt_reserved_mem_save_node(unsigned long node, const char *uname,
				      phys_addr_t base, phys_addr_t size)
{
	struct reserved_mem *rmem = &reserved_mem[reserved_mem_count];        // 取数组成员地址

	if (reserved_mem_count == ARRAY_SIZE(reserved_mem)) {                 // #define MAX_RESERVED_REGIONS	32
		pr_err("not enough space all defined regions.\n");
		return;
	}

	rmem->fdt_node = node;                                                // 记录node
	rmem->name = uname;                                                   // 记录uname
	rmem->base = base;                                                    // 记录base
	rmem->size = size;                                                    // 记录size

	reserved_mem_count++;                                                 // 更新reserved_mem_count
	return;
}

/**
 * res_mem_reserve_reg() - reserve all memory described in 'reg' property
 */
static int __init __reserved_mem_reserve_reg(unsigned long node,
					     const char *uname)
{
	int t_len = (dt_root_addr_cells + dt_root_size_cells) * sizeof(__be32);// 计算描述一个range需要的信息长度
	phys_addr_t base, size;
	int len;
	const __be32 *prop;
	int first = 1;
	bool nomap;

	prop = of_get_flat_dt_prop(node, "reg", &len);                         // 提取reg属性
	if (!prop)                                                             // 若无reg属性，动态指定reserve内存不需要指定reg
		return -ENOENT;                                                    // 返回错误

	if (len && len % t_len != 0) {                                         // 若描述reg的长度不是描述一个range需要的信息长度的倍数说明reg无效
		pr_err("Reserved memory: invalid reg property in '%s', skipping node.\n",
		       uname);
		return -EINVAL;                                                    // 返回错误码
	}

	nomap = of_get_flat_dt_prop(node, "no-map", NULL) != NULL;             // 若存在no-map属性，则返回ture，不存在no-map节点则返回false
                                                                           // no-map这个属性很重要，在网上有使用的实例，可以参考
	while (len >= t_len) {                                                 // 遍历reg中的range，reg下可能定义多于一个range
		base = dt_mem_next_cell(dt_root_addr_cells, &prop);                // 取出base
		size = dt_mem_next_cell(dt_root_size_cells, &prop);                // 取出size

		if (size &&                                                        // size不为0
		    early_init_dt_reserve_memory_arch(base, size, nomap) == 0)     // reserve或remove指定range，reserve还是remove取决于nomap
			pr_debug("Reserved memory: reserved region for node '%s': base %pa, size %ld MiB\n",
				uname, &base, (unsigned long)size / SZ_1M);
		else                                                               // 若不为0意味着range无效
			pr_info("Reserved memory: failed to reserve memory for node '%s': base %pa, size %ld MiB\n",
				uname, &base, (unsigned long)size / SZ_1M);

		len -= t_len;                                                      // 计算循环结束条件
		if (first) {                                                       // 第一轮循环
			fdt_reserved_mem_save_node(node, uname, base, size);           // 记录节点等信息
			first = 0;                                                     // 不记录除第一轮循环之外的信息
		}
	}
	return 0;
}

static bool of_fdt_device_is_available(const void *blob, unsigned long node)
{
	const char *status = fdt_getprop(blob, node, "status", NULL);

	if (!status)
		return true;

	if (!strcmp(status, "ok") || !strcmp(status, "okay"))
		return true;

	return false;
}

/ {
	#address-cells = <1>;
	#size-cells = <1>;

	memory {
		reg = <0x40000000 0x40000000>;
	};

	reserved-memory {
		#address-cells = <1>;
		#size-cells = <1>;
		ranges;

		/* global autoconfigured region for contiguous allocations */
		linux,cma {
			compatible = "shared-dma-pool";
			reusable;
			size = <0x4000000>;
			alignment = <0x2000>;
			linux,cma-default;
		};

		display_reserved: framebuffer@78000000 {
			reg = <0x78000000 0x800000>;
		};

		multimedia_reserved: multimedia@77000000 {
			compatible = "acme,multimedia-memory";
			reg = <0x77000000 0x4000000>;
		};
	};

	/* ... */

	fb0: video@12300000 {
		memory-region = <&display_reserved>;
		/* ... */
	};

	scaler: scaler@12500000 {
		memory-region = <&multimedia_reserved>;
		/* ... */
	};

	codec: codec@12600000 {
		memory-region = <&multimedia_reserved>;
		/* ... */
	};
};

/**
 * fdt_scan_reserved_mem() - scan a single FDT node for reserved memory
 */
static int __init __fdt_scan_reserved_mem(unsigned long node, const char *uname,
					  int depth, void *data)
{
	static int found;                                                     // 这个变量标志着reserved节点是否查找到，注意，这是指reserved节点，当找到reserved节点后才会对其中的子节点操作
	int err;

	if (!found && depth == 1 && strcmp(uname, "reserved-memory") == 0) {  // depth为1表示
		if (__reserved_mem_check_root(node) != 0) {
			pr_err("Reserved memory: unsupported node format, ignoring\n");
			/* break scan */
			return 1;
		}
		found = 1;                                                        // 已查找到reserved节点，设置found
		/* scan next node */
		return 0;                                                         // 查找到reserved节点，返回
	} else if (!found) {                                                  // 若已查找到reserved节点则继续下面的操作，若未查找到reserved节点则返回继续查找
		/* scan next node */
		return 0;                                                         // 未查找到reserved节点则继续查找
	} else if (found && depth < 2) {                                      // 若found并且depth < 2表示
		/* scanning of /reserved-memory has been finished */
		return 1;
	}
                                                                          // 以下代码处理reserved-memory下的子节点
	if (!of_fdt_device_is_available(initial_boot_params, node))           // 若无status或status值为okay或ok，则返回true，否则返回false
		return 0;                                                         // 若节点not available则返回

	err = __reserved_mem_reserve_reg(node, uname);                        // reserve或remove reg描述的range
	if (err == -ENOENT && of_get_flat_dt_prop(node, "size", NULL))        // 意味着动态reserve memory，可参考https://www.kernel.org/doc/Documentation/devicetree/bindings/reserved-memory/reserved-memory.txt
		fdt_reserved_mem_save_node(node, uname, 0, 0);                    // 记录节点等信息

	/* scan next node */
	return 0;
}

/**
 * early_init_fdt_scan_reserved_mem() - create reserved memory regions
 *
 * This function grabs memory from early allocator for device exclusive use
 * defined in device tree structures. It should be called by arch specific code
 * once the early allocator (i.e. memblock) has been fully activated.
 */
void __init early_init_fdt_scan_reserved_mem(void)
{
	int n;
	u64 base, size;

	if (!initial_boot_params)
		return;

	/* Process header /memreserve/ fields */
	for (n = 0; ; n++) {
		fdt_get_mem_rsv(initial_boot_params, n, &base, &size);
		if (!size)
			break;
		early_init_dt_reserve_memory_arch(base, size, false);
	}

	of_scan_flat_dt(__fdt_scan_reserved_mem, NULL);                         // 扫描并reserve或remove range，no-map决定reserve或remove
	fdt_init_reserved_mem();
}

void __init arm64_memblock_init(void)
{
	const s64 linear_region_size = PAGE_END - _PAGE_OFFSET(vabits_actual);  // 0xffffffc000000000 - 0xffffff8000000000 = 0x4000000000

	/* Handle linux,usable-memory-range property */
	fdt_enforce_memory_region();                                            // 处理linux,usable-memory-range属性，限定可用的内存区间

	/* Remove memory above our supported physical address size */
	memblock_remove(1ULL << PHYS_MASK_SHIFT, ULLONG_MAX);                   // 移除不支持的区域

	/*
	 * Select a suitable value for the base of physical memory.
	 */
	memstart_addr = round_down(memblock_start_of_DRAM(),                    // 以ARM64_MEMSTART_ALIGN将memblock.memory.regions[0].base向下对齐
				   ARM64_MEMSTART_ALIGN);                                   // 在创建线性映射时是依据memblock.memory中region，因此向下对齐不会造成没有物理内存支持的虚拟内存被映射

	if ((memblock_end_of_DRAM() - memstart_addr) > linear_region_size)      // 参考打印提示理解
		pr_warn("Memory doesn't fit in the linear mapping, VA_BITS too small\n");

	/*
	 * Remove the memory that we will not be able to cover with the
	 * linear mapping. Take care not to clip the kernel which may be
	 * high in memory.
	 */
	memblock_remove(max_t(u64, memstart_addr + linear_region_size,           // 参考注释理解
			__pa_symbol(_end)), ULLONG_MAX);
	if (memstart_addr + linear_region_size < memblock_end_of_DRAM()) {
		/* ensure that memstart_addr remains sufficiently aligned */
		memstart_addr = round_up(memblock_end_of_DRAM() - linear_region_size,// 参考注释理解，如果向上对齐，会导致未对其部分从memblock管理中被移除
					 ARM64_MEMSTART_ALIGN);
		memblock_remove(0, memstart_addr);                                   // 移除不在管理范围的range
	}

	/*
	 * If we are running with a 52-bit kernel VA config on a system that
	 * does not support it, we have to place the available physical
	 * memory in the 48-bit addressable part of the linear region, i.e.,
	 * we have to move it upward. Since memstart_addr represents the
	 * physical address of PAGE_OFFSET, we have to *subtract* from it.
	 */
	if (IS_ENABLED(CONFIG_ARM64_VA_BITS_52) && (vabits_actual != 52))
		memstart_addr -= _PAGE_OFFSET(48) - _PAGE_OFFSET(52);

	/*
	 * Apply the memory limit if it was set. Since the kernel may be loaded
	 * high up in memory, add back the kernel region that must be accessible
	 * via the linear mapping.
	 */
	if (memory_limit != PHYS_ADDR_MAX) {
		memblock_mem_limit_remove_map(memory_limit);
		memblock_add(__pa_symbol(_text), (u64)(_end - _text));
	}

	if (IS_ENABLED(CONFIG_BLK_DEV_INITRD) && phys_initrd_size) {            // 忽略
		/*
		 * Add back the memory we just removed if it results in the
		 * initrd to become inaccessible via the linear mapping.
		 * Otherwise, this is a no-op
		 */
		u64 base = phys_initrd_start & PAGE_MASK;
		u64 size = PAGE_ALIGN(phys_initrd_start + phys_initrd_size) - base;

		/*
		 * We can only add back the initrd memory if we don't end up
		 * with more memory than we can address via the linear mapping.
		 * It is up to the bootloader to position the kernel and the
		 * initrd reasonably close to each other (i.e., within 32 GB of
		 * each other) so that all granule/#levels combinations can
		 * always access both.
		 */
		if (WARN(base < memblock_start_of_DRAM() ||
			 base + size > memblock_start_of_DRAM() +
				       linear_region_size,
			"initrd not fully accessible via the linear mapping -- please check your bootloader ...\n")) {
			phys_initrd_size = 0;
		} else {
			memblock_remove(base, size); /* clear MEMBLOCK_ flags */
			memblock_add(base, size);
			memblock_reserve(base, size);
		}
	}

	if (IS_ENABLED(CONFIG_RANDOMIZE_BASE)) {                                // 忽略
		extern u16 memstart_offset_seed;
		u64 mmfr0 = read_cpuid(ID_AA64MMFR0_EL1);
		int parange = cpuid_feature_extract_unsigned_field(
					mmfr0, ID_AA64MMFR0_PARANGE_SHIFT);
		s64 range = linear_region_size -
			    BIT(id_aa64mmfr0_parange_to_phys_shift(parange));

		/*
		 * If the size of the linear region exceeds, by a sufficient
		 * margin, the size of the region that the physical memory can
		 * span, randomize the linear region as well.
		 */
		if (memstart_offset_seed > 0 && range >= (s64)ARM64_MEMSTART_ALIGN) {
			range /= ARM64_MEMSTART_ALIGN;
			memstart_addr -= ARM64_MEMSTART_ALIGN *
					 ((range * memstart_offset_seed) >> 16);
		}
	}

	/*
	 * Register the kernel text, kernel data, initrd, and initial
	 * pagetables with memblock.
	 */
	memblock_reserve(__pa_symbol(_stext), _end - _stext);                    // 添加kernel image到reserve
	if (IS_ENABLED(CONFIG_BLK_DEV_INITRD) && phys_initrd_size) {             // 若定义CONFIG_BLK_DEV_INITRD并且phys_initrd_size不为0
		/* the generic initrd code expects virtual addresses */
		initrd_start = __phys_to_virt(phys_initrd_start);                    // 获得虚拟地址initrd_start
		initrd_end = initrd_start + phys_initrd_size;                        // 获取虚拟地址initrd_end
	}

	early_init_fdt_scan_reserved_mem();                                      // 处理reserved-memory和/memreserve/

	reserve_elfcorehdr();

	high_memory = __va(memblock_end_of_DRAM() - 1) + 1;                      // 获取虚拟地址high_memory
}


/*
 * Size mapped by an entry at level n ( 0 <= n <= 3)
 * We map (PAGE_SHIFT - 3) at all translation levels and PAGE_SHIFT bits
 * in the final page. The maximum number of translation levels supported by
 * the architecture is 4. Hence, starting at at level n, we have further
 * ((4 - n) - 1) levels of translation excluding the offset within the page.
 * So, the total number of bits mapped by an entry at level n is :
 *
 *  ((4 - n) - 1) * (PAGE_SHIFT - 3) + PAGE_SHIFT
 *
 * Rearranging it a bit we get :
 *   (4 - n) * (PAGE_SHIFT - 3) + 3
 */
#define ARM64_HW_PGTABLE_LEVEL_SHIFT(n)	((PAGE_SHIFT - 3) * (4 - (n)) + 3) // ((12 - 3) * (4 - (1)) + 3) = 9 * 3 + 3 = 30
#define pgd_offset_raw(pgd, addr)	((pgd) + pgd_index(addr))
#define PAGE_MASK		(~(PAGE_SIZE-1))
/*
 * PGDIR_SHIFT determines the size a top-level page table entry can map
 * (depending on the configuration, this level can be 0, 1 or 2).
 */
#define PGDIR_SHIFT		ARM64_HW_PGTABLE_LEVEL_SHIFT(4 - CONFIG_PGTABLE_LEVELS)  // ARM64_HW_PGTABLE_LEVEL_SHIFT(1) assume CONFIG_PGTABLE_LEVELS = 3
#define PGDIR_SIZE		(_AC(1, UL) << PGDIR_SHIFT)                        // 1 << 30 = 0x40000000 = 1GB
#define PGDIR_MASK		(~(PGDIR_SIZE-1))                                  // 0xffffffffc0000000

/*
 * When walking page tables, get the address of the next boundary,
 * or the end address of the range if that comes earlier.  Although no
 * vma end wraps to 0, rounded up __boundary may wrap to 0 throughout.
 */

#define pgd_addr_end(addr, end)						\
({	unsigned long __boundary = ((addr) + PGDIR_SIZE) & PGDIR_MASK;	\      // __boundary = (addr + 0x40000000) & 0xffffffffc0000000
	(__boundary - 1 < (end) - 1)? __boundary: (end);		\              // __boundary = (addr + 0b100 0000 0000 0000 0000 0000 0000 0000) & 0b1111 1111 1111 1111 1111 1111 1111 1111 1100 0000 0000 0000 0000 0000 0000 0000
})                                                                         // 0b1111 1111 1111 1111 1111 1111 1111 1111 1100 0000 0000 0000 0000 0000 0000 0000
                                                                           // 0b                                         100 0000 0000 0000 0000 0000 0000 0000
static void __create_pgd_mapping(pgd_t *pgdir, phys_addr_t phys,           // 创建线性映射
				 unsigned long virt, phys_addr_t size,
				 pgprot_t prot,
				 phys_addr_t (*pgtable_alloc)(int),
				 int flags)
{
	unsigned long addr, length, end, next;
	pgd_t *pgdp = pgd_offset_raw(pgdir, virt);                               // ((pgd) + pgd_index(addr))

	/*
	 * If the virtual and physical address don't have the same offset
	 * within a page, we cannot map the region as the caller expects.
	 */
	if (WARN_ON((phys ^ virt) & ~PAGE_MASK))
		return;

	phys &= PAGE_MASK;                                                        // phys &= 0xfffffffffffff000
	addr = virt & PAGE_MASK;                                                  // addr = virt & 0xfffffffffffff000
	length = PAGE_ALIGN(size + (virt & ~PAGE_MASK));                          // length = PAGE_ALIGN(size + (virt & 0xfff))

	end = addr + length;                                                      // 计算虚拟地址end
	do {                                                                      // if CONFIG_PGTABLE_LEVELS = 3, PGDIR_SIZE = 1GB
		next = pgd_addr_end(addr, end);                                       // next = addr > PGDIR_SIZE? align(addr + PGDIR_SIZE, PGDIR_SIZE) : end
		alloc_init_pud(pgdp, addr, next, phys, prot, pgtable_alloc,
			       flags);
		phys += next - addr;
	} while (pgdp++, addr = next, addr != end);
}

static void __init __map_memblock(pgd_t *pgdp, phys_addr_t start,
				  phys_addr_t end, pgprot_t prot, int flags)
{
	__create_pgd_mapping(pgdp, start, __phys_to_virt(start), end - start,      // __phys_to_virt(start)将物理地址转换成虚拟地址
			     prot, early_pgtable_alloc, flags);                            // __phys_to_virt(start)将物理地址转换成虚拟地址
}

extern bool rodata_full;
bool rodata_full __ro_after_init = IS_ENABLED(CONFIG_RODATA_FULL_DEFAULT_ENABLED);

static int __init parse_rodata(char *arg)
{
	int ret = strtobool(arg, &rodata_enabled);
	if (!ret) {
		rodata_full = false;
		return 0;
	}

	/* permit 'full' in addition to boolean options */
	if (strcmp(arg, "full"))
		return -EINVAL;

	rodata_enabled = true;
	rodata_full = true;
	return 0;
}
early_param("rodata", parse_rodata);

extern bool _debug_pagealloc_enabled_early;
DECLARE_STATIC_KEY_FALSE(_debug_pagealloc_enabled);

static inline bool debug_pagealloc_enabled(void)
{
	return IS_ENABLED(CONFIG_DEBUG_PAGEALLOC) &&
		_debug_pagealloc_enabled_early;
}

static inline resource_size_t resource_size(const struct resource *res)
{
	return res->end - res->start + 1;
}

/* Location of the reserved area for the crash kernel */
struct resource crashk_res = {
	.name  = "Crash kernel",
	.start = 0,
	.end   = 0,
	.flags = IORESOURCE_BUSY | IORESOURCE_SYSTEM_RAM,
	.desc  = IORES_DESC_CRASH_KERNEL
};

static void __init map_mem(pgd_t *pgdp)
{
	phys_addr_t kernel_start = __pa_symbol(_text);                             // 获取kernel image的起始地址kernel_start
	phys_addr_t kernel_end = __pa_symbol(__init_begin);                        // 获取kernel image的结束地址kernel_end
	struct memblock_region *reg;
	int flags = 0;

	if (rodata_full || debug_pagealloc_enabled())                              // 用于调试，暂不考虑
		flags = NO_BLOCK_MAPPINGS | NO_CONT_MAPPINGS;

	/*
	 * Take care not to create a writable alias for the
	 * read-only text and rodata sections of the kernel image.
	 * So temporarily mark them as NOMAP to skip mappings in
	 * the following for-loop
	 */
	memblock_mark_nomap(kernel_start, kernel_end - kernel_start);              // 将kernel image的地址区间标记为nomap，参考注释来理解
#ifdef CONFIG_KEXEC_CORE
	if (crashk_res.end)                                                        // 如果crashk_res.end不为0
		memblock_mark_nomap(crashk_res.start,                                  // 将crash kernel的区域标记为nomap
				    resource_size(&crashk_res));
#endif

	/* map all the memory banks */
	for_each_memblock(memory, reg) {                                           // 遍历memblock.memory下的所有region
		                                                                       // 注意：映射除nomap外所有memblock.memory中的内存（nomap的reserved内存不在memblock中管理）
		phys_addr_t start = reg->base;                                         // 取region的起始地址
		phys_addr_t end = start + reg->size;                                   // 取region的结束地址

		if (start >= end)                                                      // 若start >= end？
			break;                                                             // 退出遍历
		if (memblock_is_nomap(reg))                                            // 若region为nomap的
			continue;                                                          // 跳过这个region

		__map_memblock(pgdp, start, end, PAGE_KERNEL, flags);                  // 执行映射，注意：start和end都是物理地址
		                                                                       // __phys_to_virt(start)将物理地址转换成虚拟地址
		                                                                       // 因此可以看出先确定映射关系，再建立映射
	}

	/*
	 * Map the linear alias of the [_text, __init_begin) interval
	 * as non-executable now, and remove the write permission in
	 * mark_linear_text_alias_ro() below (which will be called after
	 * alternative patching has completed). This makes the contents
	 * of the region accessible to subsystems such as hibernate,
	 * but protects it from inadvertent modification or execution.
	 * Note that contiguous mappings cannot be remapped in this way,
	 * so we should avoid them here.
	 */
	__map_memblock(pgdp, kernel_start, kernel_end,                             // 使用NO_CONT_MAPPINGS的flags单独映射kernel image内存
		       PAGE_KERNEL, NO_CONT_MAPPINGS);
	memblock_clear_nomap(kernel_start, kernel_end - kernel_start);             // 与前面的memblock_mark_nomap相呼应

#ifdef CONFIG_KEXEC_CORE
	/*
	 * Use page-level mappings here so that we can shrink the region
	 * in page granularity and put back unused memory to buddy system
	 * through /sys/kernel/kexec_crash_size interface.
	 */
	if (crashk_res.end) {                                                      // crashk_res.end不为0
		__map_memblock(pgdp, crashk_res.start, crashk_res.end + 1,             // 使用NO_BLOCK_MAPPINGS | NO_CONT_MAPPINGS映射crash kernel
			       PAGE_KERNEL,
			       NO_BLOCK_MAPPINGS | NO_CONT_MAPPINGS);
		memblock_clear_nomap(crashk_res.start,                                 // 清除nomap标志
				     resource_size(&crashk_res));
	}
#endif
}

static void __init map_kernel_segment(pgd_t *pgdp, void *va_start, void *va_end,
				      pgprot_t prot, struct vm_struct *vma,
				      int flags, unsigned long vm_flags)
{
	phys_addr_t pa_start = __pa_symbol(va_start);
	unsigned long size = va_end - va_start;                                    // 计算size

	BUG_ON(!PAGE_ALIGNED(pa_start));                                           // 物理地址没有页对齐则报BUG，暗含虚拟地址也要页对齐（在__create_pgd_mapping中有判断）
	BUG_ON(!PAGE_ALIGNED(size));                                               // 如果size不是页对齐则报BUG

	__create_pgd_mapping(pgdp, pa_start, (unsigned long)va_start, size, prot,  // 创建映射
			     early_pgtable_alloc, flags);

	if (!(vm_flags & VM_NO_GUARD))                                             // add guard page
		size += PAGE_SIZE;                                                     // 原理是什么？

	vma->addr	= va_start;                                                    // 记录到vma
	vma->phys_addr	= pa_start;                                                // 记录到vma
	vma->size	= size;                                                        // 记录到vma
	vma->flags	= VM_MAP | vm_flags;                                           // 记录到vma
	vma->caller	= __builtin_return_address(0);                                 // 记录到vma

	vm_area_add_early(vma);                                                    // 添加vma
}

#define VM_NO_GUARD		0x00000040      /* don't add guard page */

/*
 * Create fine-grained mappings for the kernel.
 */
static void __init map_kernel(pgd_t *pgdp)
{
	static struct vm_struct vmlinux_text, vmlinux_rodata, vmlinux_inittext,
				vmlinux_initdata, vmlinux_data;

	/*
	 * External debuggers may need to write directly to the text
	 * mapping to install SW breakpoints. Allow this (only) when
	 * explicitly requested with rodata=off.
	 */
	pgprot_t text_prot = rodata_enabled ? PAGE_KERNEL_ROX : PAGE_KERNEL_EXEC;

	/*
	 * Only rodata will be remapped with different permissions later on,
	 * all other segments are allowed to use contiguous mappings.
	 */
	map_kernel_segment(pgdp, _text, _etext, text_prot, &vmlinux_text, 0,
			   VM_NO_GUARD);
	map_kernel_segment(pgdp, __start_rodata, __inittext_begin, PAGE_KERNEL,
			   &vmlinux_rodata, NO_CONT_MAPPINGS, VM_NO_GUARD);
	map_kernel_segment(pgdp, __inittext_begin, __inittext_end, text_prot,
			   &vmlinux_inittext, 0, VM_NO_GUARD);
	map_kernel_segment(pgdp, __initdata_begin, __initdata_end, PAGE_KERNEL,
			   &vmlinux_initdata, 0, VM_NO_GUARD);
	map_kernel_segment(pgdp, _data, _end, PAGE_KERNEL, &vmlinux_data, 0, 0);

	if (!READ_ONCE(pgd_val(*pgd_offset_raw(pgdp, FIXADDR_START)))) {
		/*
		 * The fixmap falls in a separate pgd to the kernel, and doesn't
		 * live in the carveout for the swapper_pg_dir. We can simply
		 * re-use the existing dir for the fixmap.
		 */
		set_pgd(pgd_offset_raw(pgdp, FIXADDR_START),
			READ_ONCE(*pgd_offset_k(FIXADDR_START)));
	} else if (CONFIG_PGTABLE_LEVELS > 3) {
		pgd_t *bm_pgdp;
		pud_t *bm_pudp;
		/*
		 * The fixmap shares its top level pgd entry with the kernel
		 * mapping. This can really only occur when we are running
		 * with 16k/4 levels, so we can simply reuse the pud level
		 * entry instead.
		 */
		BUG_ON(!IS_ENABLED(CONFIG_ARM64_16K_PAGES));
		bm_pgdp = pgd_offset_raw(pgdp, FIXADDR_START);
		bm_pudp = pud_set_fixmap_offset(bm_pgdp, FIXADDR_START);
		pud_populate(&init_mm, bm_pudp, lm_alias(bm_pmd));
		pud_clear_fixmap();
	} else {
		BUG();
	}

	kasan_copy_shadow(pgdp);
}

void __init paging_init(void)
{
	pgd_t *pgdp = pgd_set_fixmap(__pa_symbol(swapper_pg_dir));

	map_kernel(pgdp);
	map_mem(pgdp);

	pgd_clear_fixmap();

	cpu_replace_ttbr1(lm_alias(swapper_pg_dir));
	init_mm.pgd = swapper_pg_dir;

	memblock_free(__pa_symbol(init_pg_dir),
		      __pa_symbol(init_pg_end) - __pa_symbol(init_pg_dir));

	memblock_allow_resize();
}

void __init bootmem_init(void)
{
	unsigned long min, max;

	min = PFN_UP(memblock_start_of_DRAM());
	max = PFN_DOWN(memblock_end_of_DRAM());

	early_memtest(min << PAGE_SHIFT, max << PAGE_SHIFT);

	max_pfn = max_low_pfn = max;
	min_low_pfn = min;

	arm64_numa_init();                                                         // static inline void arm64_numa_init(void) { }
	/*
	 * Sparsemem tries to allocate bootmem in memory_present(), so must be
	 * done after the fixed reservations.
	 */
	memblocks_present();

	sparse_init();
	zone_sizes_init(min, max);

	memblock_dump_all();
}

bool numa_off;

static __init int numa_parse_early_param(char *opt)
{
	if (!opt)
		return -EINVAL;
	if (str_has_prefix(opt, "off"))
		numa_off = true;

	return 0;
}
early_param("numa", numa_parse_early_param);

/**
 * dummy_numa_init() - Fallback dummy NUMA init
 *
 * Used if there's no underlying NUMA architecture, NUMA initialization
 * fails, or NUMA is disabled on the command line.
 *
 * Must online at least one node (node 0) and add memory blocks that cover all
 * allowed memory. It is unlikely that this function fails.
 *
 * Return: 0 on success, -errno on failure.
 */
static int __init dummy_numa_init(void)
{
	int ret;
	struct memblock_region *mblk;

	if (numa_off)
		pr_info("NUMA disabled\n"); /* Forced off on command line. */
	pr_info("Faking a node at [mem %#018Lx-%#018Lx]\n",
		memblock_start_of_DRAM(), memblock_end_of_DRAM() - 1);

	for_each_memblock(memory, mblk) {
		ret = numa_add_memblk(0, mblk->base, mblk->base + mblk->size);
		if (!ret)
			continue;

		pr_err("NUMA init failed\n");
		return ret;
	}

	numa_off = true;
	return 0;
}

#ifdef CONFIG_OF_NUMA
extern int of_numa_init(void);
#else
static inline int of_numa_init(void)
{
	return -ENOSYS;
}
#endif

#ifdef CONFIG_NUMA

#define NR_NODE_MEMBLKS		(MAX_NUMNODES * 2)

int __node_distance(int from, int to);
#define node_distance(a, b) __node_distance(a, b)

extern nodemask_t numa_nodes_parsed __initdata;

extern bool numa_off;

/* Mappings between node number and cpus on that node. */
extern cpumask_var_t node_to_cpumask_map[MAX_NUMNODES];
void numa_clear_node(unsigned int cpu);

#ifdef CONFIG_DEBUG_PER_CPU_MAPS
const struct cpumask *cpumask_of_node(int node);
#else
/* Returns a pointer to the cpumask of CPUs on Node 'node'. */
static inline const struct cpumask *cpumask_of_node(int node)
{
	return node_to_cpumask_map[node];
}
#endif

void __init arm64_numa_init(void);
int __init numa_add_memblk(int nodeid, u64 start, u64 end);
void __init numa_set_distance(int from, int to, int distance);
void __init numa_free_distance(void);
void __init early_map_cpu_to_node(unsigned int cpu, int nid);
void numa_store_cpu_info(unsigned int cpu);
void numa_add_cpu(unsigned int cpu);
void numa_remove_cpu(unsigned int cpu);

#else	/* CONFIG_NUMA */

static inline void numa_store_cpu_info(unsigned int cpu) { }
static inline void numa_add_cpu(unsigned int cpu) { }
static inline void numa_remove_cpu(unsigned int cpu) { }
static inline void arm64_numa_init(void) { }
static inline void early_map_cpu_to_node(unsigned int cpu, int nid) { }

#endif	/* CONFIG_NUMA */

/**
 * arm64_numa_init() - Initialize NUMA
 *
 * Try each configured NUMA initialization method until one succeeds. The
 * last fallback is dummy single node config encomapssing whole memory.
 */
void __init arm64_numa_init(void)
{
	if (!numa_off) {
		if (!acpi_disabled && !numa_init(arm64_acpi_numa_init))
			return;
		if (acpi_disabled && !numa_init(of_numa_init))
			return;
	}

	numa_init(dummy_numa_init);
}

bool numa_off;

static __init int numa_parse_early_param(char *opt)
{
	if (!opt)
		return -EINVAL;
	if (str_has_prefix(opt, "off"))
		numa_off = true;

	return 0;
}
early_param("numa", numa_parse_early_param);

/**
 * numa_add_memblk() - Set node id to memblk
 * @nid: NUMA node ID of the new memblk
 * @start: Start address of the new memblk
 * @end:  End address of the new memblk
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
int __init numa_add_memblk(int nid, u64 start, u64 end)
{
	int ret;

	ret = memblock_set_node(start, (end - start), &memblock.memory, nid);
	if (ret < 0) {
		pr_err("memblock [0x%llx - 0x%llx] failed to add on node %d\n",
			start, (end - 1), nid);
		return ret;
	}

	node_set(nid, numa_nodes_parsed);
	return ret;
}

/**
 * dummy_numa_init() - Fallback dummy NUMA init
 *
 * Used if there's no underlying NUMA architecture, NUMA initialization
 * fails, or NUMA is disabled on the command line.
 *
 * Must online at least one node (node 0) and add memory blocks that cover all
 * allowed memory. It is unlikely that this function fails.
 *
 * Return: 0 on success, -errno on failure.
 */
static int __init dummy_numa_init(void)
{
	int ret;
	struct memblock_region *mblk;

	if (numa_off)
		pr_info("NUMA disabled\n"); /* Forced off on command line. */
	pr_info("Faking a node at [mem %#018Lx-%#018Lx]\n",
		memblock_start_of_DRAM(), memblock_end_of_DRAM() - 1);

	for_each_memblock(memory, mblk) {
		ret = numa_add_memblk(0, mblk->base, mblk->base + mblk->size);
		if (!ret)
			continue;

		pr_err("NUMA init failed\n");
		return ret;
	}

	numa_off = true;
	return 0;
}

enum zone_type {
#ifdef CONFIG_ZONE_DMA
	/*
	 * ZONE_DMA is used when there are devices that are not able
	 * to do DMA to all of addressable memory (ZONE_NORMAL). Then we
	 * carve out the portion of memory that is needed for these devices.
	 * The range is arch specific.
	 *
	 * Some examples
	 *
	 * Architecture		Limit
	 * ---------------------------
	 * parisc, ia64, sparc	<4G
	 * s390, powerpc	<2G
	 * arm			Various
	 * alpha		Unlimited or 0-16MB.
	 *
	 * i386, x86_64 and multiple other arches
	 * 			<16M.
	 */
	ZONE_DMA,
#endif
#ifdef CONFIG_ZONE_DMA32
	/*
	 * x86_64 needs two ZONE_DMAs because it supports devices that are
	 * only able to do DMA to the lower 16M but also 32 bit devices that
	 * can only do DMA areas below 4G.
	 */
	ZONE_DMA32,
#endif
	/*
	 * Normal addressable memory is in ZONE_NORMAL. DMA operations can be
	 * performed on pages in ZONE_NORMAL if the DMA devices support
	 * transfers to all addressable memory.
	 */
	ZONE_NORMAL,
#ifdef CONFIG_HIGHMEM
	/*
	 * A memory area that is only addressable by the kernel through
	 * mapping portions into its own address space. This is for example
	 * used by i386 to allow the kernel to address the memory beyond
	 * 900MB. The kernel will set up special mappings (page
	 * table entries on i386) for each page that the kernel needs to
	 * access.
	 */
	ZONE_HIGHMEM,
#endif
	ZONE_MOVABLE,
#ifdef CONFIG_ZONE_DEVICE
	ZONE_DEVICE,
#endif
	__MAX_NR_ZONES

};

int main(void)
{
	/* The enum constants to put into include/generated/bounds.h */
	DEFINE(NR_PAGEFLAGS, __NR_PAGEFLAGS);
	DEFINE(MAX_NR_ZONES, __MAX_NR_ZONES);
#ifdef CONFIG_SMP
	DEFINE(NR_CPUS_BITS, ilog2(CONFIG_NR_CPUS));
#endif
	DEFINE(SPINLOCK_SIZE, sizeof(spinlock_t));
	/* End of constants */

	return 0;
}

#ifdef CONFIG_NUMA

#include <asm/numa.h>

extern struct pglist_data *node_data[];
#define NODE_DATA(nid)		(node_data[(nid)])

#endif /* CONFIG_NUMA */

struct pglist_data *node_data[MAX_NUMNODES] __read_mostly;
EXPORT_SYMBOL(node_data);

#ifdef CONFIG_NUMA                                                             // 不考虑这种情况

static void __init zone_sizes_init(unsigned long min, unsigned long max)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES]  = {0};

#ifdef CONFIG_ZONE_DMA32
	max_zone_pfns[ZONE_DMA32] = PFN_DOWN(max_zone_dma_phys());
#endif
	max_zone_pfns[ZONE_NORMAL] = max;

	free_area_init_nodes(max_zone_pfns);
}

#else

static void __init zone_sizes_init(unsigned long min, unsigned long max)        // 
{
	struct memblock_region *reg;
	unsigned long zone_size[MAX_NR_ZONES], zhole_size[MAX_NR_ZONES];
	unsigned long max_dma = min;                                                // 

	memset(zone_size, 0, sizeof(zone_size));                                    // 初始化zone_size

	/* 4GB maximum for 32-bit only capable devices */
#ifdef CONFIG_ZONE_DMA32                                                        // 
	max_dma = PFN_DOWN(arm64_dma_phys_limit);
	zone_size[ZONE_DMA32] = max_dma - min;
#endif
	zone_size[ZONE_NORMAL] = max - max_dma;                                     // 计算ZONE_NORMAL的size

	memcpy(zhole_size, zone_size, sizeof(zhole_size));                          // 初始化zhole_size

	for_each_memblock(memory, reg) {
		unsigned long start = memblock_region_memory_base_pfn(reg);
		unsigned long end = memblock_region_memory_end_pfn(reg);

		if (start >= max)
			continue;

#ifdef CONFIG_ZONE_DMA32
		if (start < max_dma) {
			unsigned long dma_end = min(end, max_dma);
			zhole_size[ZONE_DMA32] -= dma_end - start;
		}
#endif
		if (end > max_dma) {
			unsigned long normal_end = min(end, max);
			unsigned long normal_start = max(start, max_dma);
			zhole_size[ZONE_NORMAL] -= normal_end - normal_start;
		}
	}

	free_area_init_node(0, zone_size, min, zhole_size);                         //
}

#endif /* CONFIG_NUMA */

void __init free_area_init_node(int nid, unsigned long *zones_size,
				   unsigned long node_start_pfn,
				   unsigned long *zholes_size)
{
	pg_data_t *pgdat = NODE_DATA(nid);
	unsigned long start_pfn = 0;
	unsigned long end_pfn = 0;

	/* pg_data_t should be reset to zero when it's allocated */
	WARN_ON(pgdat->nr_zones || pgdat->kswapd_classzone_idx);

	pgdat->node_id = nid;
	pgdat->node_start_pfn = node_start_pfn;
	pgdat->per_cpu_nodestats = NULL;
#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
	get_pfn_range_for_nid(nid, &start_pfn, &end_pfn);
	pr_info("Initmem setup node %d [mem %#018Lx-%#018Lx]\n", nid,
		(u64)start_pfn << PAGE_SHIFT,
		end_pfn ? ((u64)end_pfn << PAGE_SHIFT) - 1 : 0);
#else
	start_pfn = node_start_pfn;
#endif
	calculate_node_totalpages(pgdat, start_pfn, end_pfn,
				  zones_size, zholes_size);

	alloc_node_mem_map(pgdat);
	pgdat_set_deferred_range(pgdat);

	free_area_init_core(pgdat);
}

#if defined(CONFIG_SPARSEMEM)
void memblocks_present(void);
#else
static inline void memblocks_present(void) {}
#endif

/*
 * Mark all memblocks as present using memory_present(). This is a
 * convienence function that is useful for a number of arches
 * to mark all of the systems memory as present during initialization.
 */
void __init memblocks_present(void)
{
	struct memblock_region *reg;

	for_each_memblock(memory, reg) {
		memory_present(memblock_get_region_node(reg),
			       memblock_region_memory_base_pfn(reg),
			       memblock_region_memory_end_pfn(reg));
	}
}

#ifdef CONFIG_HAVE_MEMORY_PRESENT
void memory_present(int nid, unsigned long start, unsigned long end);
#else
static inline void memory_present(int nid, unsigned long start, unsigned long end) {}
#endif

/* Record a memory area against a node. */
void __init memory_present(int nid, unsigned long start, unsigned long end)
{
	unsigned long pfn;

#ifdef CONFIG_SPARSEMEM_EXTREME
	if (unlikely(!mem_section)) {
		unsigned long size, align;

		size = sizeof(struct mem_section*) * NR_SECTION_ROOTS;
		align = 1 << (INTERNODE_CACHE_SHIFT);
		mem_section = memblock_alloc(size, align);
		if (!mem_section)
			panic("%s: Failed to allocate %lu bytes align=0x%lx\n",
			      __func__, size, align);
	}
#endif

	start &= PAGE_SECTION_MASK;
	mminit_validate_memmodel_limits(&start, &end);
	for (pfn = start; pfn < end; pfn += PAGES_PER_SECTION) {
		unsigned long section = pfn_to_section_nr(pfn);
		struct mem_section *ms;

		sparse_index_init(section, nid);
		set_section_nid(section, nid);

		ms = __nr_to_section(section);
		if (!ms->section_mem_map) {
			ms->section_mem_map = sparse_encode_early_nid(nid) |
							SECTION_IS_ONLINE;
			section_mark_present(ms);
		}
	}
}

/**
 * struct memblock_region - represents a memory region
 * @base: physical address of the region
 * @size: size of the region
 * @flags: memory region attributes
 * @nid: NUMA node id
 */
struct memblock_region {
	phys_addr_t base;
	phys_addr_t size;
	enum memblock_flags flags;
#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
	int nid;
#endif
};

config CONSOLE_LOGLEVEL_DEFAULT
	int "Default console loglevel (1-15)"
	range 1 15
	default "7"
	help
	  Default loglevel to determine what will be printed on the console.

	  Setting a default here is equivalent to passing in loglevel=<x> in
	  the kernel bootargs. loglevel=<x> continues to override whatever
	  value is specified here as well.

	  Note: This does not affect the log level of un-prefixed printk()
	  usage in the kernel. That is controlled by the MESSAGE_LOGLEVEL_DEFAULT
	  option.

/*
 * Return the maximum physical address for ZONE_DMA32 (DMA_BIT_MASK(32)). It
 * currently assumes that for memory starting above 4G, 32-bit devices will
 * use a DMA offset.
 */
static phys_addr_t __init max_zone_dma_phys(void)
{
	phys_addr_t offset = memblock_start_of_DRAM() & GENMASK_ULL(63, 32);        // GENMASK_ULL(63, 32)：0xffffffff00000000，GENMASK_ULL: generate mask，参考GENMASK注释了解用法
	return min(offset + (1ULL << 32), memblock_end_of_DRAM());                  // offset + 4G, end
	                                                                            // offset = start & 0xffffffff00000000，就是对start进行4G向下对齐
	                                                                            // offset + (1ULL << 32) = offset + 4G，一般start不会大于4G，因此通常offset为0，结果为4G
	                                                                            // 因此ZONE_DMA32的size在0 - 4G之间，不会超过4G
}

#ifdef CONFIG_64BIT
#define BITS_PER_LONG 64
#else
#define BITS_PER_LONG 32
#endif /* CONFIG_64BIT */

#ifndef BITS_PER_LONG_LONG
#define BITS_PER_LONG_LONG 64
#endif

/*
 * Create a contiguous bitmask starting at bit position @l and ending at
 * position @h. For example
 * GENMASK_ULL(39, 21) gives us the 64bit vector 0x000000ffffe00000.
 */
#define GENMASK(h, l) \
	(((~UL(0)) - (UL(1) << (l)) + 1) & \
	 (~UL(0) >> (BITS_PER_LONG - 1 - (h))))

#define GENMASK_ULL(h, l) \
	(((~ULL(0)) - (ULL(1) << (l)) + 1) & \
	 (~ULL(0) >> (BITS_PER_LONG_LONG - 1 - (h))))

static void __init calculate_node_totalpages(struct pglist_data *pgdat,
						unsigned long node_start_pfn,
						unsigned long node_end_pfn,
						unsigned long *zones_size,
						unsigned long *zholes_size)
{
	unsigned long realtotalpages = 0, totalpages = 0;
	enum zone_type i;

	for (i = 0; i < MAX_NR_ZONES; i++) {
		struct zone *zone = pgdat->node_zones + i;
		unsigned long zone_start_pfn, zone_end_pfn;
		unsigned long size, real_size;

		size = zone_spanned_pages_in_node(pgdat->node_id, i,
						  node_start_pfn,
						  node_end_pfn,
						  &zone_start_pfn,
						  &zone_end_pfn,
						  zones_size);
		real_size = size - zone_absent_pages_in_node(pgdat->node_id, i,
						  node_start_pfn, node_end_pfn,
						  zholes_size);
		if (size)
			zone->zone_start_pfn = zone_start_pfn;
		else
			zone->zone_start_pfn = 0;
		zone->spanned_pages = size;
		zone->present_pages = real_size;

		totalpages += size;
		realtotalpages += real_size;
	}

	pgdat->node_spanned_pages = totalpages;
	pgdat->node_present_pages = realtotalpages;
	printk(KERN_DEBUG "On node %d totalpages: %lu\n", pgdat->node_id,
							realtotalpages);
}





