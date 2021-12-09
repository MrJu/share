
static inline mempool_t *mempool_create_page_pool(int min_nr, int order)
{
	return mempool_create(min_nr, mempool_alloc_pages, mempool_free_pages,
			      (void *)(long)order);
}

/**
 * mempool_create - create a memory pool
 * @min_nr:    the minimum number of elements guaranteed to be
 *             allocated for this pool.
 * @alloc_fn:  user-defined element-allocation function.
 * @free_fn:   user-defined element-freeing function.
 * @pool_data: optional private data available to the user-defined functions.
 *
 * this function creates and allocates a guaranteed size, preallocated
 * memory pool. The pool can be used from the mempool_alloc() and mempool_free()
 * functions. This function might sleep. Both the alloc_fn() and the free_fn()
 * functions might sleep - as long as the mempool_alloc() function is not called
 * from IRQ contexts.
 *
 * Return: pointer to the created memory pool object or %NULL on error.
 */
mempool_t *mempool_create(int min_nr, mempool_alloc_t *alloc_fn,
				mempool_free_t *free_fn, void *pool_data)
{
	return mempool_create_node(min_nr,alloc_fn,free_fn, pool_data,
				   GFP_KERNEL, NUMA_NO_NODE);
}
EXPORT_SYMBOL(mempool_create);

mempool_t *mempool_create_node(int min_nr, mempool_alloc_t *alloc_fn,
			       mempool_free_t *free_fn, void *pool_data,
			       gfp_t gfp_mask, int node_id)
{
	mempool_t *pool;

	pool = kzalloc_node(sizeof(*pool), gfp_mask, node_id);                               // 分配pool实例，注意gfp_mask
	if (!pool)                                                                           // 若pool为NULL
		return NULL;                                                                     // 返回NULL

	if (mempool_init_node(pool, min_nr, alloc_fn, free_fn, pool_data,
			      gfp_mask, node_id)) {                                                  // 初始化pool，
		kfree(pool);                                                                     // 释放pool
		return NULL;                                                                     // 返回NULL
	}

	return pool;                                                                         // 返回pool
}
EXPORT_SYMBOL(mempool_create_node);

int mempool_init_node(mempool_t *pool, int min_nr, mempool_alloc_t *alloc_fn,
		      mempool_free_t *free_fn, void *pool_data,
		      gfp_t gfp_mask, int node_id)
{
	spin_lock_init(&pool->lock);                                                         // 初始化pool->lock
	pool->min_nr	= min_nr;                                                            // 设置pool->min_nr为min_nr，即pool中element的数量
	pool->pool_data = pool_data;                                                         // 设置pool->pool_data为pool_data，即记录element的大小in pages
	pool->alloc	= alloc_fn;                                                              // 设置pool->alloc为alloc_fn
	pool->free	= free_fn;                                                               // 设置pool->free为free_fn
	init_waitqueue_head(&pool->wait);                                                    // 初始化pool->wait

	pool->elements = kmalloc_array_node(min_nr, sizeof(void *),
					    gfp_mask, node_id);                                              // 重点：分配pool->elements空间，注意这里分配的是element的地址
	if (!pool->elements)                                                                 // 若pool->elements为NULL
		return -ENOMEM;                                                                  // 返回-ENOMEM

	/*
	 * First pre-allocate the guaranteed number of buffers.
	 */
	while (pool->curr_nr < pool->min_nr) {                                               // 若pool->curr_nr < pool->min_nr为true
		void *element;

		element = pool->alloc(gfp_mask, pool->pool_data);                                // 分配一个element，实际是以gfp_mask从buddy system中分配(int)(long)pool->pool_data阶个pages
		if (unlikely(!element)) {                                                        // 若element为NULL
			mempool_exit(pool);                                                          // 释放已分配的pool中的element和pool->elements
			return -ENOMEM;                                                              // 返回-ENOMEM
		}
		add_element(pool, element);                                                      // pool->elements[pool->curr_nr++] = element
	}

	return 0;                                                                            // 返回0
}
EXPORT_SYMBOL(mempool_init_node);

/*
 * A simple mempool-backed page allocator that allocates pages
 * of the order specified by pool_data.
 */
void *mempool_alloc_pages(gfp_t gfp_mask, void *pool_data)
{
	int order = (int)(long)pool_data;                                                    // 获取order
	return alloc_pages(gfp_mask, order);                                                 // 分配order阶个pages
}
EXPORT_SYMBOL(mempool_alloc_pages);

/**
 * mempool_exit - exit a mempool initialized with mempool_init()
 * @pool:      pointer to the memory pool which was initialized with
 *             mempool_init().
 *
 * Free all reserved elements in @pool and @pool itself.  This function
 * only sleeps if the free_fn() function sleeps.
 *
 * May be called on a zeroed but uninitialized mempool (i.e. allocated with
 * kzalloc()).
 */
void mempool_exit(mempool_t *pool)
{
	while (pool->curr_nr) {                                                              // 若pool->curr_nr不为0
		void *element = remove_element(pool);                                            // 取出一个element，并自减pool->curr_nr
		pool->free(element, pool->pool_data);                                            // 释放element
	}
	kfree(pool->elements);                                                               // 释放pool->elements
	pool->elements = NULL;                                                               // 设置pool->elements为NULL
}
EXPORT_SYMBOL(mempool_exit);

static void *remove_element(mempool_t *pool)
{
	void *element = pool->elements[--pool->curr_nr];                                     // 取出一个element

	BUG_ON(pool->curr_nr < 0);                                                           // 自检
	kasan_unpoison_element(pool, element);                                               // debug code
	check_element(pool, element);                                                        // debug code
	return element;                                                                      // 返回element
}

static __always_inline void add_element(mempool_t *pool, void *element)
{
	BUG_ON(pool->curr_nr >= pool->min_nr);                                               // 自检
	poison_element(pool, element);                                                       // debug code
	kasan_poison_element(pool, element);                                                 // debug code
	pool->elements[pool->curr_nr++] = element;                                           // 设置pool->elements[pool->curr_nr++]为element
}

void mempool_free_pages(void *element, void *pool_data)
{
	int order = (int)(long)pool_data;                                                    // 获取order
	__free_pages(element, order);                                                        // 释放element
}
EXPORT_SYMBOL(mempool_free_pages);

/**
 * mempool_alloc - allocate an element from a specific memory pool
 * @pool:      pointer to the memory pool which was allocated via
 *             mempool_create().
 * @gfp_mask:  the usual allocation bitmask.
 *
 * this function only sleeps if the alloc_fn() function sleeps or
 * returns NULL. Note that due to preallocation, this function
 * *never* fails when called from process contexts. (it might
 * fail if called from an IRQ context.)
 * Note: using __GFP_ZERO is not supported.
 *
 * Return: pointer to the allocated element or %NULL on error.
 */
void *mempool_alloc(mempool_t *pool, gfp_t gfp_mask)
{
	void *element;
	unsigned long flags;
	wait_queue_entry_t wait;                                                             // 定义wait
	gfp_t gfp_temp;

	VM_WARN_ON_ONCE(gfp_mask & __GFP_ZERO);                                              // 自检警告
	might_sleep_if(gfp_mask & __GFP_DIRECT_RECLAIM);

	gfp_mask |= __GFP_NOMEMALLOC;	/* don't allocate emergency reserves */              // 重点
	gfp_mask |= __GFP_NORETRY;	/* don't loop in __alloc_pages */                        // 重点
	gfp_mask |= __GFP_NOWARN;	/* failures are OK */                                    // 重点

	gfp_temp = gfp_mask & ~(__GFP_DIRECT_RECLAIM|__GFP_IO);                              // 重点

repeat_alloc:

	element = pool->alloc(gfp_temp, pool->pool_data);                                    // 分配一个element，实际是以gfp_mask从buddy system中分配(int)(long)pool->pool_data阶个pages
	if (likely(element != NULL))                                                         // 若element不为NULL，表示从buddy system中分配element成功
		return element;                                                                  // 返回element

	spin_lock_irqsave(&pool->lock, flags);                                               // 获取锁pool->lock
	if (likely(pool->curr_nr)) {                                                         // 若pool->curr_nr不为0
		element = remove_element(pool);                                                  // 取出一个element
		spin_unlock_irqrestore(&pool->lock, flags);                                      // 释放锁pool->lock
		/* paired with rmb in mempool_free(), read comment there */
		smp_wmb();                                                                       // 内存同步
		/*
		 * Update the allocation stack trace as this is more useful
		 * for debugging.
		 */
		kmemleak_update_trace(element);                                                  // debug code
		return element;                                                                  // 返回element
	}

	/*
	 * We use gfp mask w/o direct reclaim or IO for the first round.  If
	 * alloc failed with that and @pool was empty, retry immediately.
	 */
	if (gfp_temp != gfp_mask) {                                                          // 若gfp_temp不为gfp_mask，说明gfp_mask包含__GFP_DIRECT_RECLAIM或__GFP_IO，相当于判断是否为第一次尝试分配
		spin_unlock_irqrestore(&pool->lock, flags);                                      // 释放锁pool->lock
		gfp_temp = gfp_mask;                                                             // 设置gfp_temp为gfp_mask，即第二次分配将带有__GFP_DIRECT_RECLAIM或__GFP_IO
		goto repeat_alloc;                                                               // 跳转至repeat_alloc
	}

	/* We must not sleep if !__GFP_DIRECT_RECLAIM */
	if (!(gfp_mask & __GFP_DIRECT_RECLAIM)) {                                            // 若gfp_mask不包含__GFP_DIRECT_RECLAIM，即若不允许直接内存回收，就不会在mempool分配内存时睡眠
		spin_unlock_irqrestore(&pool->lock, flags);                                      // 释放锁pool->lock
		return NULL;                                                                     // 返回NULL
	}

	/* Let's wait for someone else to return an element to @pool */
	init_wait(&wait);                                                                    // 初始化wait
	prepare_to_wait(&pool->wait, &wait, TASK_UNINTERRUPTIBLE);                           // 准备等待，注意进程状态TASK_UNINTERRUPTIBLE

	spin_unlock_irqrestore(&pool->lock, flags);                                          // 释放锁pool->lock

	/*
	 * FIXME: this should be io_schedule().  The timeout is there as a
	 * workaround for some DM problems in 2.6.18.
	 */
	io_schedule_timeout(5*HZ);                                                           // 让渡cpu

	finish_wait(&pool->wait, &wait);                                                     // 完成等待
	goto repeat_alloc;                                                                   // 跳转至repeat_alloc
}
EXPORT_SYMBOL(mempool_alloc);

/**
 * mempool_free - return an element to the pool.
 * @element:   pool element pointer.
 * @pool:      pointer to the memory pool which was allocated via
 *             mempool_create().
 *
 * this function only sleeps if the free_fn() function sleeps.
 */
void mempool_free(void *element, mempool_t *pool)
{
	unsigned long flags;

	if (unlikely(element == NULL))                                                       // 若element为NULL
		return;                                                                          // 返回

	/*
	 * Paired with the wmb in mempool_alloc().  The preceding read is
	 * for @element and the following @pool->curr_nr.  This ensures
	 * that the visible value of @pool->curr_nr is from after the
	 * allocation of @element.  This is necessary for fringe cases
	 * where @element was passed to this task without going through
	 * barriers.
	 *
	 * For example, assume @p is %NULL at the beginning and one task
	 * performs "p = mempool_alloc(...);" while another task is doing
	 * "while (!p) cpu_relax(); mempool_free(p, ...);".  This function
	 * may end up using curr_nr value which is from before allocation
	 * of @p without the following rmb.
	 */
	smp_rmb();                                                                           // 内存同步

	/*
	 * For correctness, we need a test which is guaranteed to trigger
	 * if curr_nr + #allocated == min_nr.  Testing curr_nr < min_nr
	 * without locking achieves that and refilling as soon as possible
	 * is desirable.
	 *
	 * Because curr_nr visible here is always a value after the
	 * allocation of @element, any task which decremented curr_nr below
	 * min_nr is guaranteed to see curr_nr < min_nr unless curr_nr gets
	 * incremented to min_nr afterwards.  If curr_nr gets incremented
	 * to min_nr after the allocation of @element, the elements
	 * allocated after that are subject to the same guarantee.
	 *
	 * Waiters happen iff curr_nr is 0 and the above guarantee also
	 * ensures that there will be frees which return elements to the
	 * pool waking up the waiters.
	 */
	if (unlikely(pool->curr_nr < pool->min_nr)) {                                        // 若pool->curr_nr < pool->min_nr为true，即表示可以将element添加回pool中
		spin_lock_irqsave(&pool->lock, flags);                                           // 释放锁pool->lock
		if (likely(pool->curr_nr < pool->min_nr)) {                                      // 若pool->curr_nr < pool->min_nr为true，即表示可以将element添加回pool中
			add_element(pool, element);                                                  // pool->elements[pool->curr_nr++] = element
			spin_unlock_irqrestore(&pool->lock, flags);                                  // 释放锁pool->lock
			wake_up(&pool->wait);                                                        // 唤醒在pool->wait等待的task
			return;                                                                      // 返回return
		}
		spin_unlock_irqrestore(&pool->lock, flags);                                      // 释放锁pool->lock
	}
	pool->free(element, pool->pool_data);                                                // 释放element
}
EXPORT_SYMBOL(mempool_free);

static inline mempool_t *
mempool_create_slab_pool(int min_nr, struct kmem_cache *kc)
{
	return mempool_create(min_nr, mempool_alloc_slab, mempool_free_slab,
			      (void *) kc);
}

/*
 * A commonly used alloc and free fn.
 */
void *mempool_alloc_slab(gfp_t gfp_mask, void *pool_data)
{
	struct kmem_cache *mem = pool_data;
	VM_BUG_ON(mem->ctor);
	return kmem_cache_alloc(mem, gfp_mask);
}
EXPORT_SYMBOL(mempool_alloc_slab);

void mempool_free_slab(void *element, void *pool_data)
{
	struct kmem_cache *mem = pool_data;
	kmem_cache_free(mem, element);
}
EXPORT_SYMBOL(mempool_free_slab);

/*
 * A commonly used alloc and free fn that kmalloc/kfrees the amount of memory
 * specified by pool_data
 */
void *mempool_kmalloc(gfp_t gfp_mask, void *pool_data)
{
	size_t size = (size_t)pool_data;
	return kmalloc(size, gfp_mask);
}
EXPORT_SYMBOL(mempool_kmalloc);

void mempool_kfree(void *element, void *pool_data)
{
	kfree(element);
}
EXPORT_SYMBOL(mempool_kfree);


/*
 * Copyright (C) 2019 Andrew <mrju.email@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/mempool.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#define STR(x) _STR(x)
#define _STR(x) #x

#define VERSION_PREFIX atomic
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define PATCH_VERSION 0
#define VERSION STR(VERSION_PREFIX-MAJOR_VERSION.MINOR_VERSION.PATCH_VERSION)
#define DEVICE_NAME "atomic"
#define NR_MIN_ELEMENTS 8
#define NR_ELEMENT_ORDER 0

static int atomic_probe(struct platform_device *pdev)
{
	int ret;
	mempool_t *pool;
	void *element;

	pool = mempool_create_page_pool(NR_MIN_ELEMENTS, NR_ELEMENT_ORDER);
	if (!pool) {
		ret = -ENOMEM;
		goto err0;
	}

	element = mempool_alloc(pool, GFP_KERNEL);
	if (!element) {
		ret = -ENOMEM;
		goto err1;
	}

	// memset(element, 0xff, (1 << NR_ELEMENT_ORDER) * PAGE_SIZE);

	mempool_free(element, pool);
	mempool_destroy(pool);

	return 0;
err1:
	mempool_destroy(pool);

err0:
	return ret;
}

static int atomic_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver atomic_drv = {
	.probe	= atomic_probe,
	.remove	= atomic_remove,
	.driver	= {
		.owner = THIS_MODULE,
		.name = DEVICE_NAME,
	}
};

static int __init atomic_init(void)
{
	int ret;
	struct platform_device *pdev;

	pdev = platform_device_register_simple(DEVICE_NAME, -1, NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	ret = platform_driver_register(&atomic_drv);
	if (ret) {
		platform_device_unregister(pdev);
		return ret;
	}

	return 0;
}

static void __exit atomic_exit(void)
{
	struct device *dev;

	dev = bus_find_device_by_name(atomic_drv.driver.bus, NULL, DEVICE_NAME);
	if (dev)
		platform_device_unregister(to_platform_device(dev));

	platform_driver_unregister(&atomic_drv);
}

module_init(atomic_init);
module_exit(atomic_exit);

MODULE_ALIAS("atomic-driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);
MODULE_DESCRIPTION("Linux is not Unix");
MODULE_AUTHOR("andrew, mrju.email@gmail.com");