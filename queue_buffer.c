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
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>

#define STR(x) _STR(x)
#define _STR(x) #x

#define VERSION_PREFIX queue_buffer
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define PATCH_VERSION 0
#define VERSION STR(VERSION_PREFIX-MAJOR_VERSION.MINOR_VERSION.PATCH_VERSION)

#define QB_NR_ENTRIES (1024)

struct queue_entry {
	int index;
	char *addr;
	struct list_head list;
};

struct queue_buffer {
	unsigned long int nr_entries;
	size_t limit;
	struct queue_entry *entries;
	unsigned long *map;
	char *area;
	struct list_head head;
};

static DEFINE_SPINLOCK(slock);
static struct queue_buffer *queue = NULL;


static void set_queue_buffer(struct queue_buffer *qbuf)
{
	queue = qbuf;
}

static struct queue_buffer *get_queue_buffer(void)
{
	return queue;
}

int alloc_queue_buffer(unsigned long nr_entries, unsigned long limit)
{
	struct queue_buffer *qbuf;
	unsigned long int flags;
	void *entries, *area = NULL;
	size_t size;
	int ret = 0;

	if (WARN(nr_entries > 1024 * 64 || limit > PAGE_SIZE,
			"%s: nr_entries:%lu limit:%lu\n",
			__func__, nr_entries, limit))
		return -EINVAL;

	size = sizeof(struct queue_entry) * nr_entries;
	entries = vmalloc(size);
	if (!entries) {
		ret = -ENOMEM;
		goto err0;
	}

	if (limit) {
		size = limit * nr_entries;
		area = vmalloc(size);
		if (!area) {
			ret = -ENOMEM;
			goto err1;
		}
	}

	spin_lock_irqsave(&slock, flags);
	qbuf = kzalloc(sizeof(*qbuf), GFP_NOWAIT);
	if (!qbuf) {
		ret = -ENOMEM;
		goto err2;
	}

	size = BITS_TO_LONGS(nr_entries) * sizeof(long);
	qbuf->map = kzalloc(size, GFP_NOWAIT);
	if (!qbuf->map) {
		ret = -ENOMEM;
		goto err3;
	}

	qbuf->nr_entries = nr_entries;
	qbuf->limit = limit;
	INIT_LIST_HEAD(&qbuf->head);
	qbuf->entries = entries;
	qbuf->area = area;
	set_queue_buffer(qbuf);
	spin_unlock_irqrestore(&slock, flags);

	return 0;

err3:
	kfree(qbuf);

err2:
	spin_unlock_irqrestore(&slock, flags);
	vfree(area);

err1:
	vfree(entries);

err0:
	return ret;
}

void free_queue_buffer(void)
{
	struct queue_buffer *qbuf = get_queue_buffer();
	unsigned long int flags;

	spin_lock_irqsave(&slock, flags);
	if (!qbuf)
		return;

	kfree(qbuf->map);
	vfree(qbuf->entries);
	if (qbuf->area)
		vfree(qbuf->area);

	kfree(qbuf);
	spin_unlock_irqrestore(&slock, flags);
}

size_t print_queue(const char *fmt, ...)
{
	struct queue_buffer *qbuf = get_queue_buffer();
	struct queue_entry *entry;
	va_list args;
	char *addr = NULL;
	size_t size;
	unsigned long int index, flags;

	va_start(args, fmt);
	size = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	if (WARN(size > 1024 * 64, "%s: size:%lu\n", __func__, size))
		return -EINVAL;

	if (!qbuf->limit) {
		addr = kzalloc(size + 1, GFP_NOWAIT);
		if (!addr)
			return -ENOMEM;
	} else
		if (WARN(size > qbuf->limit - 1, "%s: size:%lu limit:%lu\n",
					__func__, size, qbuf->limit))
			return -EINVAL;

	spin_lock_irqsave(&slock, flags);
	index = find_first_zero_bit(qbuf->map, qbuf->nr_entries);
	if (index ==  qbuf->nr_entries) {
		list_move(qbuf->head.prev, &qbuf->head);
		entry = list_entry(qbuf->head.next, struct queue_entry, list);
		if (!qbuf->limit)
			kfree(entry->addr);
	} else {
		entry = &qbuf->entries[index];
		entry->index = index;
		INIT_LIST_HEAD(&entry->list);
		list_add(&entry->list, &qbuf->head);
		set_bit(entry->index, qbuf->map);
	}

	if (!addr)
		addr = (char *)(qbuf->area + entry->index * qbuf->limit);

	entry->addr = addr;
	size = vsnprintf(entry->addr, size + 1, fmt, args);
	spin_unlock_irqrestore(&slock, flags);

	return size;
}
EXPORT_SYMBOL_GPL(print_queue);

unsigned int flush_queue(void)
{
	struct queue_buffer *qbuf = get_queue_buffer();
	struct queue_entry *entry, *next;
	unsigned long int flags;
	unsigned int nr_entries = 0;

	spin_lock_irqsave(&slock, flags);
	list_for_each_entry_safe_reverse(entry, next,
			&qbuf->head, list) {
		list_del_init(&entry->list);
		printk("[%5d] %s", entry->index, entry->addr);
		if (!qbuf->limit)
			kfree(entry->addr);
		clear_bit(entry->index, qbuf->map);
		nr_entries++;
	}
	spin_unlock_irqrestore(&slock, flags);

	return nr_entries;
}
EXPORT_SYMBOL_GPL(flush_queue);

static int __init atomic_init(void)
{
	return alloc_queue_buffer(QB_NR_ENTRIES, 2048);
}

static void __exit atomic_exit(void)
{
	free_queue_buffer();
}

module_init(atomic_init);
module_exit(atomic_exit);

MODULE_ALIAS("atomic-driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);
MODULE_DESCRIPTION("Linux is not Unix");
MODULE_AUTHOR("andrew, mrju.email@gmail.com");
