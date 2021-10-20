#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

void *func(void *vargp)
{
        sleep(60);

        return 0;
}

int main(int argc, char **argv)
{
        pthread_t tid;

        pthread_create(&tid, NULL, func, NULL);
        pthread_join(tid, NULL);

        return 0;
}


https://programmer.ink/think/kernel-kernel-per-cpu-variable-module-writing.html

static irqreturn_t foo_irq_handler(int irq, void *data)
{
        struct virt_foo *foo = data;
        u32 status;

        status = readl_relaxed(foo->base + REG_INT_STATUS);

        pr_info("%s(): %d status:0x%x "
                        "preempt_count:0x%x "
                        "in_task:%d "
                        "in_interrupt:%ld "
                        "in_irq:%ld "
                        "in_softirq:%ld "
                        "in_serving_softirq:%ld "
                        "irq_count:%ld "
                        "hardirq_count:%ld "
                        "softirq_count:%ld "
                        "irqs_disabled:0x%x "
                        "preempt:%ld "
                        "irq:%ld "
                        "softirq:%ld\n",
                        __func__, __LINE__,
                        status,
                        preempt_count(),
                        in_task(),
                        in_interrupt(),
                        in_irq(),
                        in_softirq(),
                        in_serving_softirq(),
                        irq_count(),
                        hardirq_count(),
                        softirq_count(),
                        irqs_disabled(),
                        (preempt_count() & PREEMPT_MASK) >> PREEMPT_SHIFT,
                        (preempt_count() & HARDIRQ_MASK) >> HARDIRQ_SHIFT,
                        (preempt_count() & SOFTIRQ_MASK) >> SOFTIRQ_SHIFT);

        return IRQ_HANDLED;
}

static int save_stack(char *buf, size_t size)
{
        int count = 0;

#if defined(CONFIG_STACKTRACE)
        unsigned long entries[MAX_STACK_TRACE_DEPTH];

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
        unsigned int nr_entries;

        nr_entries = stack_trace_save(entries,
                        ARRAY_SIZE(entries), 0);
        count += stack_trace_snprint(buf + count,
                        size - count, entries, nr_entries, 0);
#else
        struct stack_trace trace = {
                .entries = entries,
                .max_entries = ARRAY_SIZE(entries),
        };

        save_stack_trace(&trace);
        if (trace.nr_entries != 0 &&
                        trace.entries[trace.nr_entries-1] == ULONG_MAX)
                trace.nr_entries--;
        count += snprint_stack_trace(buf + count, size, &trace, 0);
        /* to remove invalid frame 0xffffffffffffffff */
#endif
        if (count > size)
                count = size;
#endif /* CONFIG_STACKTRACE */

        return count;
}


