# share
http://www.ilinuxkernel.com/files/Linux.Kernel.Cache.pdf
http://ilinuxkernel.com/?p=1700
commit 5ea360cdebf69a3f60fb4856adcf4fd59b620a06
Author: andrew <mrju.email@gmail.com>
Date:   Sat Dec 12 01:43:03 2020 +0800

    add dentry_from_inode
    
    Signed-off-by: andrew <mrju.email@gmail.com>

diff --git a/mm/filemap.c b/mm/filemap.c
index 85b7d08..6464dbe 100644
--- a/mm/filemap.c
+++ b/mm/filemap.c
@@ -2471,6 +2471,25 @@ static struct file *do_async_mmap_readahead(struct vm_fault *vmf,
        return fpin;
 }
 
+static int dentry_from_inode(struct inode *inode,
+               struct dentry **dentry, int size)
+{
+       struct hlist_node *pos;
+       int count = 0;
+
+       if (!inode)
+               return -EINVAL;
+
+       hlist_for_each(pos, &inode->i_dentry) {
+               if (count == size)
+                       break;
+               dentry[count++] = hlist_entry(pos,
+                               struct dentry, d_u.d_alias);
+       }
+
+       return count;
+}
+
 /**
  * filemap_fault - read in file data for page fault handling
  * @vmf:       struct vm_fault containing details of the fault
@@ -2506,11 +2525,24 @@ vm_fault_t filemap_fault(struct vm_fault *vmf)
        pgoff_t max_off;
        struct page *page;
        vm_fault_t ret = 0;
+       struct dentry **dentry;
+       int count, i;
 
        max_off = DIV_ROUND_UP(i_size_read(inode), PAGE_SIZE);
        if (unlikely(offset >= max_off))
                return VM_FAULT_SIGBUS;
 
+       if (!strcmp("foo.txt", file->f_path.dentry->d_name.name)) {
+               dentry = (struct dentry **)__get_free_page(GFP_KERNEL);
+               count = dentry_from_inode(inode,
+                               dentry, PAGE_SIZE / sizeof(void *));
+               for (i = 0; i < count; i++)
+                       printk("foo: %s %d %s\n", __func__,
+                                       __LINE__, dentry[i]->d_name.name);
+
+               free_page((unsigned long)dentry);
+       }
+
        /*
         * Do we have something in the page cache already?
         */
