static int task_func(void *data)

{

        struct task_info *task_info = data;

        int count = 0;



        // sched_set_fifo(task_info->task);



        while (!kthread_should_stop()) {

                // trace_instance_printk("foo", "%s: %d\n", __func__, __LINE__);

                if (count++ == 4) {

                        void *p = (void *)(current->cpus_ptr);

                        cpumask_set_cpu(3, &current->cpus_mask);

                        cpumask_set_cpu(3, p);

                        cpumask_copy(&current->cpus_mask, cpumask_of(3));

                        current->cpu = 3;

                }



                printk("%s: %d id:%d %d %d\n", __func__, __LINE__, task_info->id, current->cpu, smp_processor_id());

                msleep(INTERVAL_IN_MSECS);

        }



        return 0;

}
