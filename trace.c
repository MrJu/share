int trace_instance_printk(const char *instance, const char *fmt, ...)
{
        struct trace_array *tr;
        va_list ap;
        int ret;

        tr = trace_array_get_by_name(instance);
        if (!tr)
                return -ENOENT;

        if (!(tr->trace_flags & TRACE_ITER_PRINTK))
                return 0;

        va_start(ap, fmt);
        ret = trace_array_vprintk(tr, 0, fmt, ap);
        va_end(ap);

        trace_array_put(tr);

        return ret;
}
EXPORT_SYMBOL_GPL(trace_instance_printk);
