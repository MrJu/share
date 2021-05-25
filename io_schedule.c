https://blog.csdn.net/u012414189/article/details/88358648
https://blog.csdn.net/weixin_30855761/article/details/95924136
https://www.codenong.com/cs106302820/
https://blog.csdn.net/hu1610552336/article/details/111464548
http://www.361way.com/kernel-hung-task-analysis/4326.html // 一次内核hung task分析
https://zhuanlan.zhihu.com/p/345592034                    // 由 OOM 引发的 ext4 文件系统卡死
https://www.sohu.com/a/384319777_261288                   // Linux IO Block layer 解析 
https://www.sohu.com/a/395091887_467784                   // Multi-queue 架构分析 
https://www.byteisland.com/linux-%E9%80%9A%E7%94%A8%E5%9D%97%E5%B1%82-bio-%E8%AF%A6%E8%A7%A3/


[   42.960719] CPU: 1 PID: 68 Comm: sh Not tainted 5.4.0+ #174
[   42.961063] Hardware name: linux,dummy-virt (DT)
[   42.961394] Call trace:
[   42.961546]  dump_backtrace+0x0/0x108
[   42.961718]  show_stack+0x14/0x20
[   42.961883]  dump_stack+0xb0/0xd8
[   42.962039]  virtio_queue_rq+0x38/0x3e4
[   42.962236]  blk_mq_dispatch_rq_list+0x70/0x588
[   42.962594]  blk_mq_do_dispatch_sched+0x48/0xd0
[   42.962820]  blk_mq_sched_dispatch_requests+0x128/0x1a0
[   42.963262]  __blk_mq_run_hw_queue+0x8c/0x108
[   42.963749]  __blk_mq_delay_run_hw_queue+0x1d0/0x1d8
[   42.964128]  blk_mq_run_hw_queue+0x94/0x110
[   42.964411]  blk_mq_sched_insert_requests+0x84/0x150
[   42.964672]  blk_mq_flush_plug_list+0x13c/0x168
[   42.964900]  blk_flush_plug_list+0xbc/0xd0
[   42.965125]  blk_finish_plug+0x30/0x40
[   42.965485]  ext4_writepages+0x390/0xa28
[   42.966004]  do_writepages+0x34/0xd8
[   42.966325]  __filemap_fdatawrite_range+0x74/0x98
[   42.966536]  filemap_flush+0x18/0x20
[   42.966698]  ext4_alloc_da_blocks+0x20/0x28
[   42.966878]  ext4_release_file+0x70/0xc8
[   42.967057]  __fput+0x48/0x128
[   42.967240]  ____fput+0xc/0x18
[   42.967385]  task_work_run+0xc4/0xf8
[   42.967559]  do_notify_resume+0x1f0/0x278
[   42.967763]  work_pending+0x8/0x10

[   45.168559] CPU: 2 PID: 55 Comm: jbd2/vda-8 Not tainted 5.4.0+ #174
[   45.169056] Hardware name: linux,dummy-virt (DT)
[   45.169507] Call trace:
[   45.169803]  dump_backtrace+0x0/0x108
[   45.170312]  show_stack+0x14/0x20
[   45.170678]  dump_stack+0xb0/0xd8
[   45.170929]  virtio_queue_rq+0x38/0x3e4
[   45.171288]  blk_mq_dispatch_rq_list+0x70/0x588
[   45.172085]  blk_mq_do_dispatch_sched+0x48/0xd0
[   45.172470]  blk_mq_sched_dispatch_requests+0x128/0x1a0
[   45.173071]  __blk_mq_run_hw_queue+0x8c/0x108
[   45.173541]  __blk_mq_delay_run_hw_queue+0x1d0/0x1d8
[   45.173941]  blk_mq_run_hw_queue+0x94/0x110
[   45.174324]  blk_mq_sched_insert_requests+0x84/0x150
[   45.174671]  blk_mq_flush_plug_list+0x13c/0x168
[   45.175176]  blk_flush_plug_list+0xbc/0xd0
[   45.175751]  blk_finish_plug+0x30/0x40
[   45.176166]  jbd2_journal_commit_transaction+0x9bc/0x1788
[   45.176619]  kjournald2+0xd4/0x248
[   45.176927]  kthread+0x114/0x118
[   45.177197]  ret_from_fork+0x10/0x18

[   45.180325] CPU: 2 PID: 58 Comm: kworker/2:1H Not tainted 5.4.0+ #174
[   45.181138] Hardware name: linux,dummy-virt (DT)
[   45.181782] Workqueue: kblockd blk_mq_requeue_work
[   45.182346] Call trace:
[   45.182715]  dump_backtrace+0x0/0x108
[   45.183195]  show_stack+0x14/0x20
[   45.183763]  dump_stack+0xb0/0xd8
[   45.184758]  virtio_queue_rq+0x38/0x3e4
[   45.185530]  blk_mq_dispatch_rq_list+0x70/0x588
[   45.185976]  blk_mq_sched_dispatch_requests+0x114/0x1a0
[   45.186387]  __blk_mq_run_hw_queue+0x8c/0x108
[   45.187117]  __blk_mq_delay_run_hw_queue+0x1d0/0x1d8
[   45.188171]  blk_mq_run_hw_queue+0x94/0x110
[   45.188653]  blk_mq_run_hw_queues+0x40/0x68
[   45.189118]  blk_mq_requeue_work+0x168/0x180
[   45.189444]  process_one_work+0x1d0/0x330
[   45.190042]  worker_thread+0x4c/0x458
[   45.190404]  kthread+0x114/0x118
[   45.190823]  ret_from_fork+0x10/0x18


[  245.832445] CPU: 3 PID: 69 Comm: sh Not tainted 5.4.0+ #177
[  245.833355] Hardware name: linux,dummy-virt (DT)
[  245.833838] Call trace:
[  245.834085]  dump_backtrace+0x0/0x108
[  245.834590]  show_stack+0x14/0x20
[  245.834923]  dump_stack+0xb0/0xd8
[  245.835237]  attach_page_buffers+0x18/0x64
[  245.836656]  create_empty_buffers+0x124/0x170
[  245.837494]  create_page_buffers+0x68/0x70
[  245.837842]  __block_write_begin_int+0x74/0x6e8
[  245.838320]  __block_write_begin+0x10/0x18
[  245.838901]  ext4_da_write_begin+0x104/0x418
[  245.839389]  generic_perform_write+0xc0/0x178
[  245.840279]  __generic_file_write_iter+0x12c/0x198
[  245.840687]  ext4_file_write_iter+0xbc/0x350
[  245.841206]  new_sync_write+0xe8/0x150
[  245.841580]  __vfs_write+0x2c/0x40
[  245.841920]  vfs_write+0xa0/0x118
[  245.842260]  ksys_write+0x4c/0xc8
[  245.842528]  __arm64_sys_write+0x18/0x20
[  245.842856]  el0_svc_handler+0x7c/0xd0
[  245.843182]  el0_svc+0x8/0xc


[  251.040513] CPU: 1 PID: 55 Comm: jbd2/vda-8 Not tainted 5.4.0+ #177
[  251.043585] Hardware name: linux,dummy-virt (DT)
[  251.045706] Call trace:
[  251.046825]  dump_backtrace+0x0/0x108
[  251.047911]  show_stack+0x14/0x20
[  251.048522]  dump_stack+0xb0/0xd8
[  251.049107]  attach_page_buffers+0x18/0x64
[  251.050122]  __getblk_gfp+0x198/0x2a8
[  251.051664]  jbd2_journal_get_descriptor_buffer+0x40/0xf8
[  251.053085]  jbd2_journal_commit_transaction+0xda0/0x1788
[  251.053509]  kjournald2+0xd4/0x248
[  251.053875]  kthread+0x114/0x118
[  251.054246]  ret_from_fork+0x10/0x18

[   31.866916] CPU: 3 PID: 68 Comm: sh Not tainted 5.4.0+ #178
[   31.867377] Hardware name: linux,dummy-virt (DT)
[   31.868159] Call trace:
[   31.869463]  dump_backtrace+0x0/0x108
[   31.869640]  show_stack+0x14/0x20
[   31.869761]  dump_stack+0xb0/0xd8
[   31.869880]  map_bh.isra.104+0x24/0x68
[   31.870001]  ext4_da_get_block_prep+0x188/0x3e0
[   31.870137]  __block_write_begin_int+0x10c/0x6e8
[   31.870454]  __block_write_begin+0x10/0x18
[   31.870626]  ext4_da_write_begin+0x104/0x418
[   31.870860]  generic_perform_write+0xc0/0x178
[   31.871056]  __generic_file_write_iter+0x12c/0x198
[   31.871417]  ext4_file_write_iter+0xbc/0x350
[   31.871693]  new_sync_write+0xe8/0x150
[   31.871905]  __vfs_write+0x2c/0x40
[   31.872578]  vfs_write+0xa0/0x118
[   31.872880]  ksys_write+0x4c/0xc8
[   31.873214]  __arm64_sys_write+0x18/0x20
[   31.873426]  el0_svc_handler+0x7c/0xd0
[   31.873633]  el0_svc+0x8/0xc


机制

plug/unplug
Linux块设备层早期就引入了plug/unplug机制，为每个task分配一个plug list，
当task提交IO时不立马处理，而是积攒一定量再统一加入下一级派发队列，减少对派发队列的竞争，
从而提高性能，block multi-queue也继承了这一机制，与单队列的实现基本一致，网络上已有大量分析，这里就不再赘述。

IO 合并
request由bio转化而来，最初一个request内只包含一个bio，若有新提交的bio与已经在排队等待处理的request物理上连续，
就直接将该bio合入request。推荐这篇文章https://blog.csdn.net/juS3Ve/article/details/80576965，
已经分析的十分透彻了。block multi-queue 新增的部分是，当不使用调度器时request会在软件队列的rq_list上等待派发，
提交IO时也会尝试与rq_list中的request进行合并。
对于block mulit-queue架构，可能发生IO合并的队列有三个：plug list ，调度器内部队列(使用调度器)，软件队列的rq_list(不使用调度器)。

调度器
block multi-queue最开始是不支持调度器的，调度器只在单队列中生效，随着单队列代码逐步移出内核，
而调度器对于hdd这种慢速设备又很有必要，调度器框架也加入了block multi-queue。
从代码也能看出multi-queue框架中是否使用调度器，处理流程差异很大。
Linux5.x后支持mq-deadline，bfq，kyber三种调度器，其中只有kyber有针对multi-queue框架中的多软硬队列做了处理，
也被称为是真正意义上的多队列调度器。

并发管理
block multi-queue通过控制request的获取，来限制一个硬件队列在block层最大并发request数量。由之前的数据结构分析可知，request是预先分配好的保存在struct blk_mq_tags结构中。运行时根据是否使用调度器，决定从硬件队列的tags或是sched_tags中获取request，所以最大并发request数也要分是否使用调度器来讨论。获取request的函数为blk_mq_get_request()。

使用调度器流程：

找到当前软件队列映射的硬件队列
调用调度器limit_depth方法(若有实现)，设置shallow_depth
sched_tags有空闲的request且分配的request总数
若分配不成功，调用blk_mq_run_hw_queue()启动request派发，再重试获取request，期间可能进入iowait
不使用调度器流程：

找到当前软件队列映射的硬件队列
若硬件blk_mq_tag_set支持共享，增加tags的active_queues
若硬件blk_mq_tag_set支持共享，且当前硬件队列已分配的request超过均分上限，分配失败
tags有空闲的request，分配成功
若分配不成功，调用blk_mq_run_hw_queue()启动request派发，再重试获取request，期间可能进入iowait
总结一下，每个硬件队列的最大并发request数是：

使用调度器：request_queue的nr_request，与调度器limit_depth方法返回值的较小值。

不使用调度器且不支持共享tag_set：硬件队列queue_depth。

不使用调度器支持共享tag_set：queue_depth个request，各active_queue均分。






Multi queue多队列核心IO传输函数是blk_mq_make_request
Multi queue多队列架构引入了struct blk_mq_tag_set、struct blk_mq_tag、struct blk_mq_hw_ctx、struct blk_mq_ctx等数据结构
1 struct blk_mq_ctx代表每个CPU独有的软件队列；
2 struct blk_mq_hw_ctx代表硬件队列，块设备至少有一个；
3 struct blk_mq_tag每个硬件队列结构struct blk_mq_hw_ctx对应一个；
4 struct blk_mq_tag主要是管理struct request(下文简称req)的分配。struct request大家应该都比较熟悉了，单队列时代就存在，IO传输的最后都要把bio转换成request；
5 struct blk_mq_tag_set包含了块设备的硬件配置信息，比如支持的硬件队列数nr_hw_queues、队列深度queue_depth等，在块设备驱动初始化时多处使用blk_mq_tag_set初始化其他成员；

每个CPU对应唯一的软件队列blk_mq_ctx，blk_mq_ctx对应唯一的硬件队列blk_mq_hw_ctx，blk_mq_hw_ctx对应唯一的blk_mq_tag，三者的关系在后续代码分析中多次出现。


/ # echo 2 > /mnt/hello.txt
[  117.886499] sblkdev: request start from sector 24  pos = 12288  dev_size = 536870912
[  117.887161] CPU: 3 PID: 212 Comm: kworker/3:1H Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  117.888006] Hardware name: linux,dummy-virt (DT)
[  117.888634] Workqueue: kblockd blk_mq_run_work_fn
[  117.889387] Call trace:
[  117.889756]  dump_backtrace+0x0/0x140
[  117.890131]  show_stack+0x14/0x20
[  117.890661]  dump_stack+0xbc/0x100
[  117.891015]  queue_rq+0x5c/0x234 [gendisk]
[  117.891566]  blk_mq_dispatch_rq_list+0xa8/0x580
[  117.892294]  blk_mq_do_dispatch_sched+0x68/0x108
[  117.892774]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  117.893423]  __blk_mq_run_hw_queue+0xac/0x120
[  117.894220]  blk_mq_run_work_fn+0x1c/0x28
[  117.894706]  process_one_work+0x1e0/0x358
[  117.895088]  worker_thread+0x40/0x488
[  117.895438]  kthread+0x118/0x120
[  117.895934]  ret_from_fork+0x10/0x18
/ # 
/ # sync
[  126.137309] sblkdev: request start from sector 16  pos = 8192  dev_size = 536870912
[  126.144651] CPU: 0 PID: 201 Comm: kworker/0:1H Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.145113] Hardware name: linux,dummy-virt (DT)
[  126.145386] Workqueue: kblockd blk_mq_run_work_fn
[  126.145715] Call trace:
[  126.153113]  dump_backtrace+0x0/0x140
[  126.153739]  show_stack+0x14/0x20
[  126.154215]  dump_stack+0xbc/0x100
[  126.154685]  queue_rq+0x5c/0x234 [gendisk]
[  126.155244]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.156319]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.156811]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.157311]  __blk_mq_run_hw_queue+0xac/0x120
[  126.157852]  blk_mq_run_work_fn+0x1c/0x28
[  126.158297]  process_one_work+0x1e0/0x358
[  126.158766]  worker_thread+0x40/0x488
[  126.159366]  kthread+0x118/0x120
[  126.159944]  ret_from_fork+0x10/0x18
[  126.161219] sblkdev: request start from sector 0  pos = 0  dev_size = 536870912
[  126.161742] CPU: 0 PID: 201 Comm: kworker/0:1H Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.162606] Hardware name: linux,dummy-virt (DT)
[  126.163079] Workqueue: kblockd blk_mq_run_work_fn
[  126.163345] Call trace:
[  126.163679]  dump_backtrace+0x0/0x140
[  126.163863]  show_stack+0x14/0x20
[  126.164069]  dump_stack+0xbc/0x100
[  126.164362]  queue_rq+0x5c/0x234 [gendisk]
[  126.165313]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.165751]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.166019]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.166312]  __blk_mq_run_hw_queue+0xac/0x120
[  126.166565]  blk_mq_run_work_fn+0x1c/0x28
[  126.166887]  process_one_work+0x1e0/0x358
[  126.167139]  worker_thread+0x40/0x488
[  126.167419]  kthread+0x118/0x120
[  126.167645]  ret_from_fork+0x10/0x18
[  126.168685] sblkdev: request start from sector 24  pos = 12288  dev_size = 536870912
[  126.169072] CPU: 0 PID: 201 Comm: kworker/0:1H Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.169562] Hardware name: linux,dummy-virt (DT)
[  126.169850] Workqueue: kblockd blk_mq_run_work_fn
[  126.170163] Call trace:
[  126.170332]  dump_backtrace+0x0/0x140
[  126.170526]  show_stack+0x14/0x20
[  126.170707]  dump_stack+0xbc/0x100
[  126.170903]  queue_rq+0x5c/0x234 [gendisk]
[  126.171088]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.171297]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.171490]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.171706]  __blk_mq_run_hw_queue+0xac/0x120
[  126.171924]  blk_mq_run_work_fn+0x1c/0x28
[  126.172122]  process_one_work+0x1e0/0x358
[  126.172316]  worker_thread+0x40/0x488
[  126.172403] sblkdev: request start from sector 4128  pos = 2113536  dev_size = 536870912
[  126.172641]  kthread+0x118/0x120
[  126.172666]  ret_from_fork+0x10/0x18
[  126.172853] sblkdev: request start from sector 4168  pos = 2134016  dev_size = 536870912
[  126.172899] CPU: 0 PID: 201 Comm: kworker/0:1H Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.172926] Hardware name: linux,dummy-virt (DT)
[  126.174700] Workqueue: kblockd blk_mq_run_work_fn
[  126.174947] Call trace:
[  126.175090]  dump_backtrace+0x0/0x140
[  126.175443]  show_stack+0x14/0x20
[  126.175730]  dump_stack+0xbc/0x100
[  126.175996]  queue_rq+0x5c/0x234 [gendisk]
[  126.176476]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.176770]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.177061]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.177421]  __blk_mq_run_hw_queue+0xac/0x120
[  126.177751]  blk_mq_run_work_fn+0x1c/0x28
[  126.177964]  process_one_work+0x1e0/0x358
[  126.178201]  worker_thread+0x40/0x488
[  126.178363]  kthread+0x118/0x120
[  126.178510]  ret_from_fork+0x10/0x18
[  126.178810] CPU: 1 PID: 190 Comm: kworker/u16:5 Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.179230] Hardware name: linux,dummy-virt (DT)
[  126.179463] Workqueue: writeback wb_workfn (flush-253:0)
[  126.179740] Call trace:
[  126.179886]  dump_backtrace+0x0/0x140
[  126.180085]  show_stack+0x14/0x20
[  126.180241]  dump_stack+0xbc/0x100
[  126.180418]  queue_rq+0x5c/0x234 [gendisk]
[  126.180666]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.180863]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.181053]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.181259]  __blk_mq_run_hw_queue+0xac/0x120
[  126.181439]  __blk_mq_delay_run_hw_queue+0x1f4/0x220
[  126.181688]  blk_mq_run_hw_queue+0xa0/0xf8
[  126.181863]  blk_mq_sched_insert_requests+0xe4/0x248
[  126.182059]  blk_mq_flush_plug_list+0x14c/0x190
[  126.182244]  blk_flush_plug_list+0xd4/0x100
[  126.182417]  blk_finish_plug+0x30/0x21c
[  126.198886]  wb_writeback+0x14c/0x1e8
[  126.199934]  wb_workfn+0x184/0x310
[  126.201133]  process_one_work+0x1e0/0x358
[  126.201745]  worker_thread+0x40/0x488
[  126.202163]  kthread+0x118/0x120
[  126.202631]  ret_from_fork+0x10/0x18
[  126.238845] sblkdev: request start from sector 8  pos = 4096  dev_size = 536870912
[  126.241012] CPU: 2 PID: 215 Comm: sync Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.242697] Hardware name: linux,dummy-virt (DT)
[  126.243064] Call trace:
[  126.243290]  dump_backtrace+0x0/0x140
[  126.243557]  show_stack+0x14/0x20
[  126.244386]  dump_stack+0xbc/0x100
[  126.245224]  queue_rq+0x5c/0x234 [gendisk]
[  126.246055]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.246950]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.248411]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.248840]  __blk_mq_run_hw_queue+0xac/0x120
[  126.249953]  __blk_mq_delay_run_hw_queue+0x1f4/0x220
[  126.251175]  blk_mq_run_hw_queue+0xa0/0xf8
[  126.251674]  blk_mq_sched_insert_requests+0xe4/0x248
[  126.252963]  blk_mq_flush_plug_list+0x14c/0x190
[  126.253871]  blk_flush_plug_list+0xd4/0x100
[  126.254497]  blk_finish_plug+0x30/0x21c
[  126.254877]  generic_writepages+0x68/0x90
[  126.255472]  blkdev_writepages+0xc/0x18
[  126.256143]  do_writepages+0x54/0x100
[  126.257314]  __filemap_fdatawrite_range+0xdc/0x118
[  126.259288]  filemap_fdatawrite+0x18/0x20
[  126.259937]  fdatawrite_one_bdev+0x14/0x20
[  126.261070]  iterate_bdevs+0x98/0x230
[  126.262749]  ksys_sync+0x70/0xb8
[  126.263709]  __arm64_sys_sync+0xc/0x18
[  126.265700]  el0_svc_common.constprop.2+0x88/0x150
[  126.266623]  el0_svc_handler+0x20/0x80
[  126.277226]  el0_svc+0x8/0xc
[  126.280129] sblkdev: request start from sector 32  pos = 16384  dev_size = 536870912
[  126.281419] CPU: 2 PID: 215 Comm: sync Tainted: G           O      5.4.0-g84ca32c-dirty #187
[  126.282253] Hardware name: linux,dummy-virt (DT)
[  126.282659] Call trace:
[  126.283544]  dump_backtrace+0x0/0x140
[  126.284075]  show_stack+0x14/0x20
[  126.284725]  dump_stack+0xbc/0x100
[  126.285980]  queue_rq+0x5c/0x234 [gendisk]
[  126.287178]  blk_mq_dispatch_rq_list+0xa8/0x580
[  126.288268]  blk_mq_do_dispatch_sched+0x68/0x108
[  126.288669]  blk_mq_sched_dispatch_requests+0x130/0x1d8
[  126.289035]  __blk_mq_run_hw_queue+0xac/0x120
[  126.289353]  __blk_mq_delay_run_hw_queue+0x1f4/0x220
[  126.289825]  blk_mq_run_hw_queue+0xa0/0xf8
[  126.290195]  blk_mq_sched_insert_requests+0xe4/0x248
[  126.290638]  blk_mq_flush_plug_list+0x14c/0x190
[  126.291093]  blk_flush_plug_list+0xd4/0x100
[  126.291422]  blk_finish_plug+0x30/0x21c
[  126.291780]  generic_writepages+0x68/0x90
[  126.292178]  blkdev_writepages+0xc/0x18
[  126.292528]  do_writepages+0x54/0x100
[  126.292863]  __filemap_fdatawrite_range+0xdc/0x118
[  126.293277]  filemap_fdatawrite+0x18/0x20
[  126.293724]  fdatawrite_one_bdev+0x14/0x20
[  126.294078]  iterate_bdevs+0x98/0x230
[  126.294427]  ksys_sync+0x70/0xb8
[  126.294770]  __arm64_sys_sync+0xc/0x18
[  126.295119]  el0_svc_common.constprop.2+0x88/0x150
[  126.295581]  el0_svc_handler+0x20/0x80
[  126.295921]  el0_svc+0x8/0xc















































==================================================================================================================================================




static blk_qc_t blk_mq_make_request(struct request_queue *q, struct bio *bio)
{
	const int is_sync = op_is_sync(bio->bi_opf);                                              // 从bio->bi_opf判断是否为sync
	const int is_flush_fua = op_is_flush(bio->bi_opf);                                        // 从bio->bi_opf判断是否为flush
	struct blk_mq_alloc_data data = { .flags = 0};                                            // 定义临时变量data
	struct request *rq;                                                                       // 定义临时变量rq
	struct blk_plug *plug;                                                                    // 定义临时变量plug
	struct request *same_queue_rq = NULL;                                                     // 定义临时变量same_queue_rq
	unsigned int nr_segs;
	blk_qc_t cookie;

	blk_queue_bounce(q, &bio);                                                                // 
	__blk_queue_split(q, &bio, &nr_segs);                                                     // 

	if (!bio_integrity_prep(bio))                                                             // 
		return BLK_QC_T_NONE;

	if (!is_flush_fua && !blk_queue_nomerges(q) &&                                            // 
	    blk_attempt_plug_merge(q, bio, nr_segs, &same_queue_rq))
		return BLK_QC_T_NONE;

	if (blk_mq_sched_bio_merge(q, bio, nr_segs))
		return BLK_QC_T_NONE;

	rq_qos_throttle(q, bio);                                                                  // 

	data.cmd_flags = bio->bi_opf;
	rq = blk_mq_get_request(q, bio, &data);                                                   // 
	if (unlikely(!rq)) {
		rq_qos_cleanup(q, bio);
		if (bio->bi_opf & REQ_NOWAIT)
			bio_wouldblock_error(bio);
		return BLK_QC_T_NONE;
	}

	trace_block_getrq(q, bio, bio->bi_opf);

	rq_qos_track(q, rq, bio);

	cookie = request_to_qc_t(data.hctx, rq);

	blk_mq_bio_to_request(rq, bio, nr_segs);

	plug = blk_mq_plug(q, bio);
	if (unlikely(is_flush_fua)) {
		/* bypass scheduler for flush rq */
		blk_insert_flush(rq);
		blk_mq_run_hw_queue(data.hctx, true);
	} else if (plug && (q->nr_hw_queues == 1 || q->mq_ops->commit_rqs ||
				!blk_queue_nonrot(q))) {
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
			last = list_entry_rq(plug->mq_list.prev);

		if (request_count >= BLK_MAX_REQUEST_COUNT || (last &&
		    blk_rq_bytes(last) >= BLK_PLUG_FLUSH_SIZE)) {
			blk_flush_plug_list(plug, false);
			trace_block_plug(q);
		}

		blk_add_rq_to_plug(plug, rq);
	} else if (q->elevator) {
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
		blk_mq_try_issue_directly(data.hctx, rq, &cookie);
	} else {
		blk_mq_sched_insert_request(rq, false, true, true);
	}

	return cookie;
}

struct request_queue {
	struct request		*last_merge;
	struct elevator_queue	*elevator;

	struct blk_queue_stats	*stats;
	struct rq_qos		*rq_qos;

	make_request_fn		*make_request_fn;
	dma_drain_needed_fn	*dma_drain_needed;

	const struct blk_mq_ops	*mq_ops;

	/* sw queues */
	struct blk_mq_ctx __percpu	*queue_ctx;
	unsigned int		nr_queues;

	unsigned int		queue_depth;

	/* hw dispatch queues */
	struct blk_mq_hw_ctx	**queue_hw_ctx;
	unsigned int		nr_hw_queues;

	struct backing_dev_info	*backing_dev_info;

	/*
	 * The queue owner gets to use this for whatever they like.
	 * ll_rw_blk doesn't touch it.
	 */
	void			*queuedata;

	/*
	 * various queue flags, see QUEUE_* below
	 */
	unsigned long		queue_flags;
	/*
	 * Number of contexts that have called blk_set_pm_only(). If this
	 * counter is above zero then only RQF_PM and RQF_PREEMPT requests are
	 * processed.
	 */
	atomic_t		pm_only;

	/*
	 * ida allocated id for this queue.  Used to index queues from
	 * ioctx.
	 */
	int			id;

	/*
	 * queue needs bounce pages for pages above this limit
	 */
	gfp_t			bounce_gfp;

	spinlock_t		queue_lock;

	/*
	 * queue kobject
	 */
	struct kobject kobj;

	/*
	 * mq queue kobject
	 */
	struct kobject *mq_kobj;

#ifdef  CONFIG_BLK_DEV_INTEGRITY
	struct blk_integrity integrity;
#endif	/* CONFIG_BLK_DEV_INTEGRITY */

#ifdef CONFIG_PM
	struct device		*dev;
	int			rpm_status;
	unsigned int		nr_pending;
#endif

	/*
	 * queue settings
	 */
	unsigned long		nr_requests;	/* Max # of requests */

	unsigned int		dma_drain_size;
	void			*dma_drain_buffer;
	unsigned int		dma_pad_mask;
	unsigned int		dma_alignment;

	unsigned int		rq_timeout;
	int			poll_nsec;

	struct blk_stat_callback	*poll_cb;
	struct blk_rq_stat	poll_stat[BLK_MQ_POLL_STATS_BKTS];

	struct timer_list	timeout;
	struct work_struct	timeout_work;

	struct list_head	icq_list;
#ifdef CONFIG_BLK_CGROUP
	DECLARE_BITMAP		(blkcg_pols, BLKCG_MAX_POLS);
	struct blkcg_gq		*root_blkg;
	struct list_head	blkg_list;
#endif

	struct queue_limits	limits;

	unsigned int		required_elevator_features;

#ifdef CONFIG_BLK_DEV_ZONED
	/*
	 * Zoned block device information for request dispatch control.
	 * nr_zones is the total number of zones of the device. This is always
	 * 0 for regular block devices. seq_zones_bitmap is a bitmap of nr_zones
	 * bits which indicates if a zone is conventional (bit clear) or
	 * sequential (bit set). seq_zones_wlock is a bitmap of nr_zones
	 * bits which indicates if a zone is write locked, that is, if a write
	 * request targeting the zone was dispatched. All three fields are
	 * initialized by the low level device driver (e.g. scsi/sd.c).
	 * Stacking drivers (device mappers) may or may not initialize
	 * these fields.
	 *
	 * Reads of this information must be protected with blk_queue_enter() /
	 * blk_queue_exit(). Modifying this information is only allowed while
	 * no requests are being processed. See also blk_mq_freeze_queue() and
	 * blk_mq_unfreeze_queue().
	 */
	unsigned int		nr_zones;
	unsigned long		*seq_zones_bitmap;
	unsigned long		*seq_zones_wlock;
#endif /* CONFIG_BLK_DEV_ZONED */

	/*
	 * sg stuff
	 */
	unsigned int		sg_timeout;
	unsigned int		sg_reserved_size;
	int			node;
#ifdef CONFIG_BLK_DEV_IO_TRACE
	struct blk_trace	*blk_trace;
	struct mutex		blk_trace_mutex;
#endif
	/*
	 * for flush operations
	 */
	struct blk_flush_queue	*fq;

	struct list_head	requeue_list;
	spinlock_t		requeue_lock;
	struct delayed_work	requeue_work;

	struct mutex		sysfs_lock;
	struct mutex		sysfs_dir_lock;

	/*
	 * for reusing dead hctx instance in case of updating
	 * nr_hw_queues
	 */
	struct list_head	unused_hctx_list;
	spinlock_t		unused_hctx_lock;

	int			mq_freeze_depth;

#if defined(CONFIG_BLK_DEV_BSG)
	struct bsg_class_device bsg_dev;
#endif

#ifdef CONFIG_BLK_DEV_THROTTLING
	/* Throttle data */
	struct throtl_data *td;
#endif
	struct rcu_head		rcu_head;
	wait_queue_head_t	mq_freeze_wq;
	/*
	 * Protect concurrent access to q_usage_counter by
	 * percpu_ref_kill() and percpu_ref_reinit().
	 */
	struct mutex		mq_freeze_lock;
	struct percpu_ref	q_usage_counter;

	struct blk_mq_tag_set	*tag_set;
	struct list_head	tag_set_list;
	struct bio_set		bio_split;

#ifdef CONFIG_BLK_DEBUG_FS
	struct dentry		*debugfs_dir;
	struct dentry		*sched_debugfs_dir;
	struct dentry		*rqos_debugfs_dir;
#endif

	bool			mq_sysfs_init_done;

	size_t			cmd_size;

	struct work_struct	release_work;

#define BLK_MAX_WRITE_HINTS	5
	u64			write_hints[BLK_MAX_WRITE_HINTS];
};

struct blk_mq_ctxs {
	struct kobject kobj;
	struct blk_mq_ctx __percpu	*queue_ctx;
};

/*
 * Try to put the fields that are referenced together in the same cacheline.
 *
 * If you modify this structure, make sure to update blk_rq_init() and
 * especially blk_mq_rq_ctx_init() to take care of the added fields.
 */
struct request {
	struct request_queue *q;
	struct blk_mq_ctx *mq_ctx;
	struct blk_mq_hw_ctx *mq_hctx;

	unsigned int cmd_flags;		/* op and common flags */
	req_flags_t rq_flags;

	int tag;
	int internal_tag;

	/* the following two fields are internal, NEVER access directly */
	unsigned int __data_len;	/* total data len */
	sector_t __sector;		/* sector cursor */

	struct bio *bio;
	struct bio *biotail;

	struct list_head queuelist;

	/*
	 * The hash is used inside the scheduler, and killed once the
	 * request reaches the dispatch list. The ipi_list is only used
	 * to queue the request for softirq completion, which is long
	 * after the request has been unhashed (and even removed from
	 * the dispatch list).
	 */
	union {
		struct hlist_node hash;	/* merge hash */
		struct list_head ipi_list;                                      // 在blk_done_softirq中被用到
	};

	/*
	 * The rb_node is only used inside the io scheduler, requests
	 * are pruned when moved to the dispatch queue. So let the
	 * completion_data share space with the rb_node.
	 */
	union {
		struct rb_node rb_node;	/* sort/lookup */
		struct bio_vec special_vec;
		void *completion_data;
		int error_count; /* for legacy drivers, don't use */
	};

	/*
	 * Three pointers are available for the IO schedulers, if they need
	 * more they have to dynamically allocate it.  Flush requests are
	 * never put on the IO scheduler. So let the flush fields share
	 * space with the elevator data.
	 */
	union {
		struct {
			struct io_cq		*icq;
			void			*priv[2];
		} elv;

		struct {
			unsigned int		seq;
			struct list_head	list;
			rq_end_io_fn		*saved_end_io;
		} flush;
	};

	struct gendisk *rq_disk;
	struct hd_struct *part;
#ifdef CONFIG_BLK_RQ_ALLOC_TIME
	/* Time that the first bio started allocating this request. */
	u64 alloc_time_ns;
#endif
	/* Time that this request was allocated for this IO. */
	u64 start_time_ns;
	/* Time that I/O was submitted to the device. */
	u64 io_start_time_ns;

#ifdef CONFIG_BLK_WBT
	unsigned short wbt_flags;
#endif
	/*
	 * rq sectors used for blk stats. It has the same value
	 * with blk_rq_sectors(rq), except that it never be zeroed
	 * by completion.
	 */
	unsigned short stats_sectors;

	/*
	 * Number of scatter-gather DMA addr+len pairs after
	 * physical address coalescing is performed.
	 */
	unsigned short nr_phys_segments;

#if defined(CONFIG_BLK_DEV_INTEGRITY)
	unsigned short nr_integrity_segments;
#endif

	unsigned short write_hint;
	unsigned short ioprio;

	unsigned int extra_len;	/* length of alignment and padding */

	enum mq_rq_state state;
	refcount_t ref;

	unsigned int timeout;
	unsigned long deadline;

	union {
		struct __call_single_data csd;                                           // 用于__blk_mq_complete_request和raise_blk_irq中
		u64 fifo_time;
	};

	/*
	 * completion callback.
	 */
	rq_end_io_fn *end_io;
	void *end_io_data;
};

/**
 * struct blk_mq_ctx - State for a software queue facing the submitting CPUs
 */
struct blk_mq_ctx {
	struct {
		spinlock_t		lock;
		struct list_head	rq_lists[HCTX_MAX_TYPES];
	} ____cacheline_aligned_in_smp;

	unsigned int		cpu;
	unsigned short		index_hw[HCTX_MAX_TYPES];
	struct blk_mq_hw_ctx 	*hctxs[HCTX_MAX_TYPES];

	/* incremented at dispatch time */
	unsigned long		rq_dispatched[2];
	unsigned long		rq_merged;

	/* incremented at completion time */
	unsigned long		____cacheline_aligned_in_smp rq_completed[2];

	struct request_queue	*queue;
	struct blk_mq_ctxs      *ctxs;
	struct kobject		kobj;
} ____cacheline_aligned_in_smp;

static inline struct blk_mq_ctx *__blk_mq_get_ctx(struct request_queue *q,
					   unsigned int cpu)
{
	return per_cpu_ptr(q->queue_ctx, cpu);
}

/*
 * This assumes per-cpu software queueing queues. They could be per-node
 * as well, for instance. For now this is hardcoded as-is. Note that we don't
 * care about preemption, since we know the ctx's are persistent. This does
 * mean that we can't rely on ctx always matching the currently running CPU.
 */
static inline struct blk_mq_ctx *blk_mq_get_ctx(struct request_queue *q)
{
	return __blk_mq_get_ctx(q, raw_smp_processor_id());
}

static inline struct blk_mq_tags *blk_mq_tags_from_data(struct blk_mq_alloc_data *data)
{
	if (data->flags & BLK_MQ_REQ_INTERNAL)
		return data->hctx->sched_tags;

	return data->hctx->tags;
}

/*
 * Tag address space map.
 */
struct blk_mq_tags {
	unsigned int nr_tags;
	unsigned int nr_reserved_tags;

	atomic_t active_queues;

	struct sbitmap_queue bitmap_tags;
	struct sbitmap_queue breserved_tags;

	struct request **rqs;
	struct request **static_rqs;
	struct list_head page_list;
};

unsigned int blk_mq_get_tag(struct blk_mq_alloc_data *data)
{
	struct blk_mq_tags *tags = blk_mq_tags_from_data(data);
	struct sbitmap_queue *bt;
	struct sbq_wait_state *ws;
	DEFINE_SBQ_WAIT(wait);
	unsigned int tag_offset;
	int tag;

	if (data->flags & BLK_MQ_REQ_RESERVED) {
		if (unlikely(!tags->nr_reserved_tags)) {
			WARN_ON_ONCE(1);
			return BLK_MQ_TAG_FAIL;
		}
		bt = &tags->breserved_tags;
		tag_offset = 0;
	} else {
		bt = &tags->bitmap_tags;
		tag_offset = tags->nr_reserved_tags;
	}

	tag = __blk_mq_get_tag(data, bt);
	if (tag != -1)
		goto found_tag;

	if (data->flags & BLK_MQ_REQ_NOWAIT)
		return BLK_MQ_TAG_FAIL;

	ws = bt_wait_ptr(bt, data->hctx);
	do {
		struct sbitmap_queue *bt_prev;

		/*
		 * We're out of tags on this hardware queue, kick any
		 * pending IO submits before going to sleep waiting for
		 * some to complete.
		 */
		blk_mq_run_hw_queue(data->hctx, false);

		/*
		 * Retry tag allocation after running the hardware queue,
		 * as running the queue may also have found completions.
		 */
		tag = __blk_mq_get_tag(data, bt);
		if (tag != -1)
			break;

		sbitmap_prepare_to_wait(bt, ws, &wait, TASK_UNINTERRUPTIBLE);

		tag = __blk_mq_get_tag(data, bt);
		if (tag != -1)
			break;

		bt_prev = bt;
		io_schedule();

		sbitmap_finish_wait(bt, ws, &wait);

		data->ctx = blk_mq_get_ctx(data->q);
		data->hctx = blk_mq_map_queue(data->q, data->cmd_flags,
						data->ctx);
		tags = blk_mq_tags_from_data(data);
		if (data->flags & BLK_MQ_REQ_RESERVED)
			bt = &tags->breserved_tags;
		else
			bt = &tags->bitmap_tags;

		/*
		 * If destination hw queue is changed, fake wake up on
		 * previous queue for compensating the wake up miss, so
		 * other allocations on previous queue won't be starved.
		 */
		if (bt != bt_prev)
			sbitmap_queue_wake_up(bt_prev);

		ws = bt_wait_ptr(bt, data->hctx);
	} while (1);

	sbitmap_finish_wait(bt, ws, &wait);

found_tag:
	return tag + tag_offset;
}

static struct request *blk_mq_get_request(struct request_queue *q,
					  struct bio *bio,
					  struct blk_mq_alloc_data *data)
{
	struct elevator_queue *e = q->elevator;                                                   // 取出io调度器
	struct request *rq;                                                                       // 定义临时变量rq
	unsigned int tag;                                                                         // 
	bool clear_ctx_on_error = false;                                                          // 
	u64 alloc_time_ns = 0;                                                                    // 

	blk_queue_enter_live(q);                                                                  // 

	/* alloc_time includes depth and tag waits */
	if (blk_queue_rq_alloc_time(q))
		alloc_time_ns = ktime_get_ns();

	data->q = q;                                                                               // 
	if (likely(!data->ctx)) {
		data->ctx = blk_mq_get_ctx(q);
		clear_ctx_on_error = true;
	}
	if (likely(!data->hctx))
		data->hctx = blk_mq_map_queue(q, data->cmd_flags,
						data->ctx);
	if (data->cmd_flags & REQ_NOWAIT)
		data->flags |= BLK_MQ_REQ_NOWAIT;

	if (e) {
		data->flags |= BLK_MQ_REQ_INTERNAL;

		/*
		 * Flush requests are special and go directly to the
		 * dispatch list. Don't include reserved tags in the
		 * limiting, as it isn't useful.
		 */
		if (!op_is_flush(data->cmd_flags) &&
		    e->type->ops.limit_depth &&
		    !(data->flags & BLK_MQ_REQ_RESERVED))
			e->type->ops.limit_depth(data->cmd_flags, data);
	} else {
		blk_mq_tag_busy(data->hctx);
	}

	tag = blk_mq_get_tag(data);
	if (tag == BLK_MQ_TAG_FAIL) {
		if (clear_ctx_on_error)
			data->ctx = NULL;
		blk_queue_exit(q);
		return NULL;
	}

	rq = blk_mq_rq_ctx_init(data, tag, data->cmd_flags, alloc_time_ns);
	if (!op_is_flush(data->cmd_flags)) {
		rq->elv.icq = NULL;
		if (e && e->type->ops.prepare_request) {
			if (e->type->icq_cache)
				blk_mq_sched_assign_ioc(rq);

			e->type->ops.prepare_request(rq, bio);
			rq->rq_flags |= RQF_ELVPRIV;
		}
	}
	data->hctx->queued++;
	return rq;
}


unsigned int blk_mq_get_tag(struct blk_mq_alloc_data *data)
{
	struct blk_mq_tags *tags = blk_mq_tags_from_data(data);
	struct sbitmap_queue *bt;
	struct sbq_wait_state *ws;
	DEFINE_SBQ_WAIT(wait);
	unsigned int tag_offset;
	int tag;

	if (data->flags & BLK_MQ_REQ_RESERVED) {
		if (unlikely(!tags->nr_reserved_tags)) {
			WARN_ON_ONCE(1);
			return BLK_MQ_TAG_FAIL;
		}
		bt = &tags->breserved_tags;
		tag_offset = 0;
	} else {
		bt = &tags->bitmap_tags;
		tag_offset = tags->nr_reserved_tags;
	}

	tag = __blk_mq_get_tag(data, bt);
	if (tag != -1)
		goto found_tag;

	if (data->flags & BLK_MQ_REQ_NOWAIT)
		return BLK_MQ_TAG_FAIL;

	ws = bt_wait_ptr(bt, data->hctx);
	do {
		struct sbitmap_queue *bt_prev;

		/*
		 * We're out of tags on this hardware queue, kick any
		 * pending IO submits before going to sleep waiting for
		 * some to complete.
		 */
		blk_mq_run_hw_queue(data->hctx, false);

		/*
		 * Retry tag allocation after running the hardware queue,
		 * as running the queue may also have found completions.
		 */
		tag = __blk_mq_get_tag(data, bt);
		if (tag != -1)
			break;

		sbitmap_prepare_to_wait(bt, ws, &wait, TASK_UNINTERRUPTIBLE);

		tag = __blk_mq_get_tag(data, bt);
		if (tag != -1)
			break;

		bt_prev = bt;
		io_schedule();

		sbitmap_finish_wait(bt, ws, &wait);

		data->ctx = blk_mq_get_ctx(data->q);
		data->hctx = blk_mq_map_queue(data->q, data->cmd_flags,
						data->ctx);
		tags = blk_mq_tags_from_data(data);
		if (data->flags & BLK_MQ_REQ_RESERVED)
			bt = &tags->breserved_tags;
		else
			bt = &tags->bitmap_tags;

		/*
		 * If destination hw queue is changed, fake wake up on
		 * previous queue for compensating the wake up miss, so
		 * other allocations on previous queue won't be starved.
		 */
		if (bt != bt_prev)
			sbitmap_queue_wake_up(bt_prev);

		ws = bt_wait_ptr(bt, data->hctx);
	} while (1);

	sbitmap_finish_wait(bt, ws, &wait);

found_tag:
	return tag + tag_offset;
}

static int __blk_mq_get_tag(struct blk_mq_alloc_data *data,
			    struct sbitmap_queue *bt)
{
	if (!(data->flags & BLK_MQ_REQ_INTERNAL) &&
	    !hctx_may_queue(data->hctx, bt))
		return -1;
	if (data->shallow_depth)
		return __sbitmap_queue_get_shallow(bt, data->shallow_depth);
	else
		return __sbitmap_queue_get(bt);
}

/*
 * Driver command data is immediately after the request. So subtract request
 * size to get back to the original request, add request size to get the PDU.
 */
static inline struct request *blk_mq_rq_from_pdu(void *pdu)
{
	return pdu - sizeof(struct request);
}
static inline void *blk_mq_rq_to_pdu(struct request *rq)
{
	return rq + 1;
}

static blk_status_t virtio_queue_rq(struct blk_mq_hw_ctx *hctx,
			   const struct blk_mq_queue_data *bd)
{
	struct virtio_blk *vblk = hctx->queue->queuedata;                            // 取出vblk
	struct request *req = bd->rq;                                                // 取出request
	struct virtblk_req *vbr = blk_mq_rq_to_pdu(req);                             // rq + 1，vblk->tag_set.cmd_size = sizeof(struct virtblk_req) + sizeof(struct scatterlist) * sg_elems;
	unsigned long flags;
	unsigned int num;
	int qid = hctx->queue_num;
	int err;
	bool notify = false;
	bool unmap = false;
	u32 type;

	BUG_ON(req->nr_phys_segments + 2 > vblk->sg_elems);

	switch (req_op(req)) {
	case REQ_OP_READ:
	case REQ_OP_WRITE:
		type = 0;
		break;
	case REQ_OP_FLUSH:
		type = VIRTIO_BLK_T_FLUSH;
		break;
	case REQ_OP_DISCARD:
		type = VIRTIO_BLK_T_DISCARD;
		break;
	case REQ_OP_WRITE_ZEROES:
		type = VIRTIO_BLK_T_WRITE_ZEROES;
		unmap = !(req->cmd_flags & REQ_NOUNMAP);
		break;
	case REQ_OP_SCSI_IN:
	case REQ_OP_SCSI_OUT:
		type = VIRTIO_BLK_T_SCSI_CMD;
		break;
	case REQ_OP_DRV_IN:
		type = VIRTIO_BLK_T_GET_ID;
		break;
	default:
		WARN_ON_ONCE(1);
		return BLK_STS_IOERR;
	}

	vbr->out_hdr.type = cpu_to_virtio32(vblk->vdev, type);
	vbr->out_hdr.sector = type ?
		0 : cpu_to_virtio64(vblk->vdev, blk_rq_pos(req));
	vbr->out_hdr.ioprio = cpu_to_virtio32(vblk->vdev, req_get_ioprio(req));

	blk_mq_start_request(req);                                                   // 这个接口功能为记录信息，并非真正开始数据传输

	if (type == VIRTIO_BLK_T_DISCARD || type == VIRTIO_BLK_T_WRITE_ZEROES) {
		err = virtblk_setup_discard_write_zeroes(req, unmap);
		if (err)
			return BLK_STS_RESOURCE;
	}

	num = blk_rq_map_sg(hctx->queue, req, vbr->sg);
	if (num) {
		if (rq_data_dir(req) == WRITE)
			vbr->out_hdr.type |= cpu_to_virtio32(vblk->vdev, VIRTIO_BLK_T_OUT);
		else
			vbr->out_hdr.type |= cpu_to_virtio32(vblk->vdev, VIRTIO_BLK_T_IN);
	}

	spin_lock_irqsave(&vblk->vqs[qid].lock, flags);
	if (blk_rq_is_scsi(req))
		err = virtblk_add_req_scsi(vblk->vqs[qid].vq, vbr, vbr->sg, num);
	else
		err = virtblk_add_req(vblk->vqs[qid].vq, vbr, vbr->sg, num);
	if (err) {
		virtqueue_kick(vblk->vqs[qid].vq);
		blk_mq_stop_hw_queue(hctx);
		spin_unlock_irqrestore(&vblk->vqs[qid].lock, flags);
		/* Out of mem doesn't actually happen, since we fall back
		 * to direct descriptors */
		if (err == -ENOMEM || err == -ENOSPC)
			return BLK_STS_DEV_RESOURCE;
		return BLK_STS_IOERR;
	}

	if (bd->last && virtqueue_kick_prepare(vblk->vqs[qid].vq))
		notify = true;
	spin_unlock_irqrestore(&vblk->vqs[qid].lock, flags);

	if (notify)
		virtqueue_notify(vblk->vqs[qid].vq);
	return BLK_STS_OK;
}

static void trigger_softirq(void *data)
{
	struct request *rq = data;
	unsigned long flags;
	struct list_head *list;

	local_irq_save(flags);
	list = this_cpu_ptr(&blk_cpu_done);
	list_add_tail(&rq->ipi_list, list);

	if (list->next == &rq->ipi_list)
		raise_softirq_irqoff(BLOCK_SOFTIRQ);

	local_irq_restore(flags);
}

/**
 * smp_call_function_single_async(): Run an asynchronous function on a
 * 			         specific CPU.
 * @cpu: The CPU to run on.
 * @csd: Pre-allocated and setup data structure
 *
 * Like smp_call_function_single(), but the call is asynchonous and
 * can thus be done from contexts with disabled interrupts.
 *
 * The caller passes his own pre-allocated data structure
 * (ie: embedded in an object) and is responsible for synchronizing it
 * such that the IPIs performed on the @csd are strictly serialized.
 *
 * NOTE: Be careful, there is unfortunately no current debugging facility to
 * validate the correctness of this serialization.
 */
int smp_call_function_single_async(int cpu, call_single_data_t *csd)                          // 参考注释理解该接口的功能
{
	int err = 0;

	preempt_disable();

	/* We could deadlock if we have to wait here with interrupts disabled! */
	if (WARN_ON_ONCE(csd->flags & CSD_FLAG_LOCK))
		csd_lock_wait(csd);

	csd->flags = CSD_FLAG_LOCK;
	smp_wmb();

	err = generic_exec_single(cpu, csd, csd->func, csd->info);
	preempt_enable();

	return err;
}
EXPORT_SYMBOL_GPL(smp_call_function_single_async);

/*
 * Setup and invoke a run of 'trigger_softirq' on the given cpu.
 */
static int raise_blk_irq(int cpu, struct request *rq)                                  // 参考注释理解该接口的功能
{
	if (cpu_online(cpu)) {
		call_single_data_t *data = &rq->csd;

		data->func = trigger_softirq;
		data->info = rq;
		data->flags = 0;

		smp_call_function_single_async(cpu, data);
		return 0;
	}

	return 1;
}

static DEFINE_PER_CPU(struct list_head, blk_cpu_done);

void __raise_softirq_irqoff(unsigned int nr)
{
	trace_softirq_raise(nr);
	or_softirq_pending(1UL << nr);
}

/*
 * This function must run with irqs disabled!
 */
inline void raise_softirq_irqoff(unsigned int nr)
{
	__raise_softirq_irqoff(nr);

	/*
	 * If we're in an interrupt or softirq, we're done
	 * (this also catches softirq-disabled code). We will
	 * actually run the softirq once we return from
	 * the irq or softirq.
	 *
	 * Otherwise we wake up ksoftirqd to make sure we
	 * schedule the softirq soon.
	 */
	if (!in_interrupt())                                                   // 参考注释来理解
		wakeup_softirqd();
}

void __blk_complete_request(struct request *req)
{
	struct request_queue *q = req->q;
	int cpu, ccpu = req->mq_ctx->cpu;                                      // 
	unsigned long flags;
	bool shared = false;

	BUG_ON(!q->mq_ops->complete);                                          // 调用到这里q->mq_ops->complete不能为NULL

	local_irq_save(flags);
	cpu = smp_processor_id();

	/*
	 * Select completion CPU
	 */
	if (test_bit(QUEUE_FLAG_SAME_COMP, &q->queue_flags) && ccpu != -1) {
		if (!test_bit(QUEUE_FLAG_SAME_FORCE, &q->queue_flags))
			shared = cpus_share_cache(cpu, ccpu);
	} else
		ccpu = cpu;

	/*
	 * If current CPU and requested CPU share a cache, run the softirq on
	 * the current CPU. One might concern this is just like
	 * QUEUE_FLAG_SAME_FORCE, but actually not. blk_complete_request() is
	 * running in interrupt handler, and currently I/O controller doesn't
	 * support multiple interrupts, so current CPU is unique actually. This
	 * avoids IPI sending from current CPU to the first CPU of a group.
	 */
	if (ccpu == cpu || shared) {
		struct list_head *list;
do_local:
		list = this_cpu_ptr(&blk_cpu_done);                                  // 这个链表是per CPU的
		list_add_tail(&req->ipi_list, list);                                 // 将要完成的request添加到list链表中

		/*
		 * if the list only contains our just added request,
		 * signal a raise of the softirq. If there are already
		 * entries there, someone already raised the irq but it
		 * hasn't run yet.
		 */
		if (list->next == &req->ipi_list)                                    // 参考注释理解
			raise_softirq_irqoff(BLOCK_SOFTIRQ);
	} else if (raise_blk_irq(ccpu, req))
		goto do_local;

	local_irq_restore(flags);
}

static void __blk_mq_complete_request(struct request *rq)
{
	struct blk_mq_ctx *ctx = rq->mq_ctx;
	struct request_queue *q = rq->q;
	bool shared = false;
	int cpu;

	WRITE_ONCE(rq->state, MQ_RQ_COMPLETE);
	/*
	 * Most of single queue controllers, there is only one irq vector
	 * for handling IO completion, and the only irq's affinity is set
	 * as all possible CPUs. On most of ARCHs, this affinity means the
	 * irq is handled on one specific CPU.
	 *
	 * So complete IO reqeust in softirq context in case of single queue
	 * for not degrading IO performance by irqsoff latency.
	 */
	if (q->nr_hw_queues == 1) {
		__blk_complete_request(rq);
		return;
	}

	/*
	 * For a polled request, always complete locallly, it's pointless
	 * to redirect the completion.
	 */
	if ((rq->cmd_flags & REQ_HIPRI) ||
	    !test_bit(QUEUE_FLAG_SAME_COMP, &q->queue_flags)) {
		q->mq_ops->complete(rq);
		return;
	}

	cpu = get_cpu();
	if (!test_bit(QUEUE_FLAG_SAME_FORCE, &q->queue_flags))
		shared = cpus_share_cache(cpu, ctx->cpu);

	if (cpu != ctx->cpu && !shared && cpu_online(ctx->cpu)) {
		rq->csd.func = __blk_mq_complete_request_remote;
		rq->csd.info = rq;
		rq->csd.flags = 0;
		smp_call_function_single_async(ctx->cpu, &rq->csd);
	} else {
		q->mq_ops->complete(rq);
	}
	put_cpu();
}

/**
 * blk_mq_complete_request - end I/O on a request
 * @rq:		the request being processed
 *
 * Description:
 *	Ends all I/O on a request. It does not handle partial completions.
 *	The actual completion happens out-of-order, through a IPI handler.
 **/
bool blk_mq_complete_request(struct request *rq)                                   // 该接口在中断中被调用
{
	if (unlikely(blk_should_fake_timeout(rq->q)))                                  // 默认不定义CONFIG_FAIL_IO_TIMEOUT，该接口返回0
		return false;
	__blk_mq_complete_request(rq);
	return true;
}
EXPORT_SYMBOL(blk_mq_complete_request);

static void virtblk_done(struct virtqueue *vq)
{
	struct virtio_blk *vblk = vq->vdev->priv;
	bool req_done = false;
	int qid = vq->index;
	struct virtblk_req *vbr;
	unsigned long flags;
	unsigned int len;

	spin_lock_irqsave(&vblk->vqs[qid].lock, flags);
	do {
		virtqueue_disable_cb(vq);
		while ((vbr = virtqueue_get_buf(vblk->vqs[qid].vq, &len)) != NULL) {
			struct request *req = blk_mq_rq_from_pdu(vbr);

			blk_mq_complete_request(req);
			req_done = true;
		}
		if (unlikely(virtqueue_is_broken(vq)))
			break;
	} while (!virtqueue_enable_cb(vq));

	/* In case queue is stopped waiting for more buffers. */
	if (req_done)
		blk_mq_start_stopped_hw_queues(vblk->disk->queue, true);
	spin_unlock_irqrestore(&vblk->vqs[qid].lock, flags);
}

irqreturn_t vring_interrupt(int irq, void *_vq)
{
	struct vring_virtqueue *vq = to_vvq(_vq);

	if (!more_used(vq)) {
		pr_debug("virtqueue interrupt with no work for %p\n", vq);
		return IRQ_NONE;
	}

	if (unlikely(vq->broken))
		return IRQ_HANDLED;

	pr_debug("virtqueue callback for %p (%p)\n", vq, vq->vq.callback);
	if (vq->vq.callback)
		vq->vq.callback(&vq->vq);

	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(vring_interrupt);

/* Notify all virtqueues on an interrupt. */
static irqreturn_t vm_interrupt(int irq, void *opaque)
{
	struct virtio_mmio_device *vm_dev = opaque;
	struct virtio_mmio_vq_info *info;
	unsigned long status;
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;

	/* Read and acknowledge interrupts */
	status = readl(vm_dev->base + VIRTIO_MMIO_INTERRUPT_STATUS);
	writel(status, vm_dev->base + VIRTIO_MMIO_INTERRUPT_ACK);

	if (unlikely(status & VIRTIO_MMIO_INT_CONFIG)) {
		virtio_config_changed(&vm_dev->vdev);
		ret = IRQ_HANDLED;
	}

	if (likely(status & VIRTIO_MMIO_INT_VRING)) {
		spin_lock_irqsave(&vm_dev->lock, flags);
		list_for_each_entry(info, &vm_dev->virtqueues, node)
			ret |= vring_interrupt(irq, info->vq);
		spin_unlock_irqrestore(&vm_dev->lock, flags);
	}

	return ret;
}

irqreturn_t __handle_irq_event_percpu(struct irq_desc *desc, unsigned int *flags)
{
	irqreturn_t retval = IRQ_NONE;
	unsigned int irq = desc->irq_data.irq;
	struct irqaction *action;

	record_irq_time(desc);

	for_each_action_of_desc(desc, action) {
		irqreturn_t res;

		trace_irq_handler_entry(irq, action);
		res = action->handler(irq, action->dev_id);
		trace_irq_handler_exit(irq, action, res);

		if (WARN_ONCE(!irqs_disabled(),"irq %u handler %pS enabled interrupts\n",
			      irq, action->handler))
			local_irq_disable();

		switch (res) {
		case IRQ_WAKE_THREAD:
			/*
			 * Catch drivers which return WAKE_THREAD but
			 * did not set up a thread function
			 */
			if (unlikely(!action->thread_fn)) {
				warn_no_thread(irq, action);
				break;
			}

			__irq_wake_thread(desc, action);

			/* Fall through - to add to randomness */
		case IRQ_HANDLED:
			*flags |= action->flags;
			break;

		default:
			break;
		}

		retval |= res;
	}

	return retval;
}

/*
 * Softirq action handler - move entries to local list and loop over them
 * while passing them to the queue registered handler.
 */
static __latent_entropy void blk_done_softirq(struct softirq_action *h)
{
	struct list_head *cpu_list, local_list;

	local_irq_disable();
	cpu_list = this_cpu_ptr(&blk_cpu_done);
	list_replace_init(cpu_list, &local_list);
	local_irq_enable();

	while (!list_empty(&local_list)) {
		struct request *rq;

		rq = list_entry(local_list.next, struct request, ipi_list);
		list_del_init(&rq->ipi_list);
		rq->q->mq_ops->complete(rq);
	}
}

void open_softirq(int nr, void (*action)(struct softirq_action *))
{
	softirq_vec[nr].action = action;
}

static __init int blk_softirq_init(void)
{
	int i;

	for_each_possible_cpu(i)
		INIT_LIST_HEAD(&per_cpu(blk_cpu_done, i));

	open_softirq(BLOCK_SOFTIRQ, blk_done_softirq);
	cpuhp_setup_state_nocalls(CPUHP_BLOCK_SOFTIRQ_DEAD,
				  "block/softirq:dead", NULL,
				  blk_softirq_cpu_dead);
	return 0;
}
subsys_initcall(blk_softirq_init);

void blk_mq_end_request(struct request *rq, blk_status_t error)
{
	if (blk_update_request(rq, error, blk_rq_bytes(rq)))
		BUG();
	__blk_mq_end_request(rq, error);
}
EXPORT_SYMBOL(blk_mq_end_request);

static inline void virtblk_request_done(struct request *req)
{
	struct virtblk_req *vbr = blk_mq_rq_to_pdu(req);

	if (req->rq_flags & RQF_SPECIAL_PAYLOAD) {
		kfree(page_address(req->special_vec.bv_page) +
		      req->special_vec.bv_offset);
	}

	switch (req_op(req)) {
	case REQ_OP_SCSI_IN:
	case REQ_OP_SCSI_OUT:
		virtblk_scsi_request_done(req);
		break;
	}

	blk_mq_end_request(req, virtblk_result(vbr));
}


static const struct blk_mq_ops virtio_mq_ops = {
        .queue_rq       = virtio_queue_rq,
        .commit_rqs     = virtio_commit_rqs,
        .complete       = virtblk_request_done,                                  // 该接口在blk_done_softirq中被调用
        .init_request   = virtblk_init_request,
#ifdef CONFIG_VIRTIO_BLK_SCSI
        .initialize_rq_fn = virtblk_initialize_rq,
#endif
        .map_queues     = virtblk_map_queues,
};

struct request_queue *blk_mq_init_queue(struct blk_mq_tag_set *set)
{
	struct request_queue *uninit_q, *q;

	uninit_q = blk_alloc_queue_node(GFP_KERNEL, set->numa_node);
	if (!uninit_q)
		return ERR_PTR(-ENOMEM);

	/*
	 * Initialize the queue without an elevator. device_add_disk() will do
	 * the initialization.
	 */
	q = blk_mq_init_allocated_queue(set, uninit_q, false);
	if (IS_ERR(q))
		blk_cleanup_queue(uninit_q);

	return q;
}
EXPORT_SYMBOL(blk_mq_init_queue);

enum {
	BLK_MQ_F_SHOULD_MERGE	= 1 << 0,
	BLK_MQ_F_TAG_SHARED	= 1 << 1,
	BLK_MQ_F_BLOCKING	= 1 << 5,
	BLK_MQ_F_NO_SCHED	= 1 << 6,
	BLK_MQ_F_ALLOC_POLICY_START_BIT = 8,
	BLK_MQ_F_ALLOC_POLICY_BITS = 1,

	BLK_MQ_S_STOPPED	= 0,
	BLK_MQ_S_TAG_ACTIVE	= 1,
	BLK_MQ_S_SCHED_RESTART	= 2,

	BLK_MQ_MAX_DEPTH	= 10240,

	BLK_MQ_CPU_WORK_BATCH	= 8,
};

/*
 * Alloc a tag set to be associated with one or more request queues.
 * May fail with EINVAL for various error conditions. May adjust the
 * requested depth down, if it's too large. In that case, the set
 * value will be stored in set->queue_depth.
 */
int blk_mq_alloc_tag_set(struct blk_mq_tag_set *set)
{
	int i, ret;

	BUILD_BUG_ON(BLK_MQ_MAX_DEPTH > 1 << BLK_MQ_UNIQUE_TAG_BITS);                // 自检

	if (!set->nr_hw_queues)                                                      // 若set->nr_hw_queues为0
		return -EINVAL;                                                          // 则返回-EINVAL
	if (!set->queue_depth)                                                       // 若set->queue_depth为0
		return -EINVAL;                                                          // 则返回-EINVAL
	if (set->queue_depth < set->reserved_tags + BLK_MQ_TAG_MIN)                  // 若set->queue_depth < set->reserved_tags + BLK_MQ_TAG_MIN
		return -EINVAL;                                                          // 则返回-EINVAL

	if (!set->ops->queue_rq)                                                     // 若set->ops->queue_rq为NULL
		return -EINVAL;                                                          // 则返回-EINVAL

	if (!set->ops->get_budget ^ !set->ops->put_budget)                           // 若set->ops->get_budget和set->ops->put_budget没有同时定义或不定义
		return -EINVAL;                                                          // 则返回-EINVAL

	if (set->queue_depth > BLK_MQ_MAX_DEPTH) {                                   // set->queue_depth > BLK_MQ_MAX_DEPTH
		pr_info("blk-mq: reduced tag depth to %u\n",                             // 打印提示
			BLK_MQ_MAX_DEPTH);
		set->queue_depth = BLK_MQ_MAX_DEPTH;                                     // 限定set->queue_depth为BLK_MQ_MAX_DEPTH
	}

	if (!set->nr_maps)                                                           // 若set->nr_maps为0
		set->nr_maps = 1;                                                        // set->nr_maps设定为1
	else if (set->nr_maps > HCTX_MAX_TYPES)                                      // 若set->nr_maps > HCTX_MAX_TYPES
		return -EINVAL;                                                          // 返回-EINVAL

	/*
	 * If a crashdump is active, then we are potentially in a very
	 * memory constrained environment. Limit us to 1 queue and
	 * 64 tags to prevent using too much memory.
	 */
	if (is_kdump_kernel()) {                                                     // 若当前内核时kdump kernel
		set->nr_hw_queues = 1;                                                   // 限定set->nr_hw_queues为1
		set->nr_maps = 1;                                                        // 限定set->nr_maps为1
		set->queue_depth = min(64U, set->queue_depth);                           // 限定set->queue_depth不超过64
	}
	/*
	 * There is no use for more h/w queues than cpus if we just have
	 * a single map
	 */
	if (set->nr_maps == 1 && set->nr_hw_queues > nr_cpu_ids)                     // 参考注释，这涉及到映射算法，可参考blk_mq_map_queues
		set->nr_hw_queues = nr_cpu_ids;                                          // 限定set->nr_hw_queue为nr_cpu_ids

	set->tags = kcalloc_node(nr_hw_queues(set), sizeof(struct blk_mq_tags *),    // 为每一个hw queue分配tags指针空间，返回的是数组
				 GFP_KERNEL, set->numa_node);
	if (!set->tags)                                                              // 如果分配失败
		return -ENOMEM;                                                          // 返回-ENOMEM

	ret = -ENOMEM;                                                               // 默认返回-ENOMEM
	for (i = 0; i < set->nr_maps; i++) {                                         // 0 - set->nr_maps
		set->map[i].mq_map = kcalloc_node(nr_cpu_ids,                            // 为set->map[i].mq_map是一个数组，这里为其分配空间
						  sizeof(set->map[i].mq_map[0]),
						  GFP_KERNEL, set->numa_node);
		if (!set->map[i].mq_map)                                                 // 若分配失败
			goto out_free_mq_map;                                                // 跳转到out_free_mq_map
		set->map[i].nr_queues = is_kdump_kernel() ? 1 : set->nr_hw_queues;       // 设定set->map[i].nr_queues
	}

	ret = blk_mq_update_queue_map(set);                                          // 在该接口中建立ctx -> hctx的映射
	if (ret)                                                                     // 若失败
		goto out_free_mq_map;                                                    // 跳转到out_free_mq_map

	ret = blk_mq_alloc_rq_maps(set);                                             // 为每一个hw queue分配tags和初始话其中成员，会分配request内存，分配失败则减小set->queue_depth再重试
	if (ret)                                                                     // 若失败
		goto out_free_mq_map;                                                    // 跳转到out_free_mq_map

	mutex_init(&set->tag_list_lock);                                             // 初始化set->tag_list_lock
	INIT_LIST_HEAD(&set->tag_list);                                              // 初始化set->tag_list

	return 0;                                                                    // 成功返回0

out_free_mq_map:                                                                 // 以下是错误处理
	for (i = 0; i < set->nr_maps; i++) {
		kfree(set->map[i].mq_map);
		set->map[i].mq_map = NULL;
	}
	kfree(set->tags);
	set->tags = NULL;
	return ret;
}
EXPORT_SYMBOL(blk_mq_alloc_tag_set);

static int blk_mq_update_queue_map(struct blk_mq_tag_set *set)
{
	if (set->ops->map_queues && !is_kdump_kernel()) {                            // 若set->ops->map_queues定义，并且当前不为kdump kernel
		int i;

		/*
		 * transport .map_queues is usually done in the following
		 * way:
		 *
		 * for (queue = 0; queue < set->nr_hw_queues; queue++) {
		 * 	mask = get_cpu_mask(queue)
		 * 	for_each_cpu(cpu, mask)
		 * 		set->map[x].mq_map[cpu] = queue;
		 * }
		 *
		 * When we need to remap, the table has to be cleared for
		 * killing stale mapping since one CPU may not be mapped
		 * to any hw queue.
		 */
		for (i = 0; i < set->nr_maps; i++)                                      // 0 - set->nr_maps
			blk_mq_clear_mq_map(&set->map[i]);                                  // 清除映射

		return set->ops->map_queues(set);                                       // 使用set->ops->map_queues建立ctx -> hctx映射
	} else {
		BUG_ON(set->nr_maps > 1);                                               // 若set->ops->map_queues未定义，set->nr_maps不应大于1
		return blk_mq_map_queues(&set->map[HCTX_TYPE_DEFAULT]);                 // 建立ctx -> hctx映射
	}
}

static int virtblk_map_queues(struct blk_mq_tag_set *set)
{
	struct virtio_blk *vblk = set->driver_data;

	return blk_mq_virtio_map_queues(&set->map[HCTX_TYPE_DEFAULT],
					vblk->vdev, 0);
}

/**
 * blk_mq_virtio_map_queues - provide a default queue mapping for virtio device
 * @qmap:	CPU to hardware queue map.
 * @vdev:	virtio device to provide a mapping for.
 * @first_vec:	first interrupt vectors to use for queues (usually 0)
 *
 * This function assumes the virtio device @vdev has at least as many available
 * interrupt vetors as @set has queues.  It will then queuery the vector
 * corresponding to each queue for it's affinity mask and built queue mapping
 * that maps a queue to the CPUs that have irq affinity for the corresponding
 * vector.
 */
int blk_mq_virtio_map_queues(struct blk_mq_queue_map *qmap,
		struct virtio_device *vdev, int first_vec)
{
	const struct cpumask *mask;
	unsigned int queue, cpu;

	if (!vdev->config->get_vq_affinity)                                         // 代码中未定义vdev->config->get_vq_affinity
		goto fallback;                                                          // 跳转至fallback

	for (queue = 0; queue < qmap->nr_queues; queue++) {
		mask = vdev->config->get_vq_affinity(vdev, first_vec + queue);
		if (!mask)
			goto fallback;

		for_each_cpu(cpu, mask)
			qmap->mq_map[cpu] = qmap->queue_offset + queue;
	}

	return 0;
fallback:
	return blk_mq_map_queues(qmap);                                             // virtio-blk使用该接口映射
}
EXPORT_SYMBOL_GPL(blk_mq_virtio_map_queues);

static int queue_index(struct blk_mq_queue_map *qmap,
		       unsigned int nr_queues, const int q)
{
	return qmap->queue_offset + (q % nr_queues);                               // qmap->queue_offset表示计算映射的起点，小与qmap->queue_offset的hctx将不会参与映射
}

int blk_mq_map_queues(struct blk_mq_queue_map *qmap)
{
	unsigned int *map = qmap->mq_map;
	unsigned int nr_queues = qmap->nr_queues;                                   // set->map[i].nr_queues = is_kdump_kernel() ? 1 : set->nr_hw_queues
	unsigned int cpu, first_sibling, q = 0;

	for_each_possible_cpu(cpu)                                                  // possible cpu表示设备树中定义的cpu
		map[cpu] = -1;                                                          // 先清除映射

	/*
	 * Spread queues among present CPUs first for minimizing
	 * count of dead queues which are mapped by all un-present CPUs
	 */
	for_each_present_cpu(cpu) {                                                 // present cpu表示boot cpu启动的cpu
		if (q >= nr_queues)                                                     // 先一一映射ctx -> hctx
			break;                                                              // 完成一一映射退出遍历
		map[cpu] = queue_index(qmap, nr_queues, q++);                           // qmap->queue_offset + (q % nr_queues)
	}

	for_each_possible_cpu(cpu) {                                                // 遍历possible cpu
		if (map[cpu] != -1)                                                     // map[cpu] != -1表示一一映射时已经映射过
			continue;                                                           // 跳过本次映射
		/*
		 * First do sequential mapping between CPUs and queues.
		 * In case we still have CPUs to map, and we have some number of
		 * threads per cores then map sibling threads to the same queue
		 * for performance optimizations.
		 */
		if (q < nr_queues) {                                                    // 在q < nr_queues情况下，对possible cpu继续进行一一映射
			map[cpu] = queue_index(qmap, nr_queues, q++);                       // qmap->queue_offset + (q % nr_queues)
		} else {                                                                // 若q >= nr_queues，将与第一个兄弟cpu使用相同的映射
			first_sibling = get_first_sibling(cpu);                             // 获取第一个兄弟cpu
			if (first_sibling == cpu)                                           // 在q >= nr_queues情况下，如果兄弟cpu就是本相同，说明本cpu还没有映射
				map[cpu] = queue_index(qmap, nr_queues, q++);                   // qmap->queue_offset + (q % nr_queues)
			else
				map[cpu] = map[first_sibling];                                  // 与兄弟cpu使用相同的映射
		}
	}

	return 0;                                                                   // 返回0
}
EXPORT_SYMBOL_GPL(blk_mq_map_queues);

/*
 * Allocate the request maps associated with this tag_set. Note that this
 * may reduce the depth asked for, if memory is tight. set->queue_depth
 * will be updated to reflect the allocated depth.
 */
static int blk_mq_alloc_rq_maps(struct blk_mq_tag_set *set)                      // 为每一个hw queue分配tags和初始话其中成员，会分配request内存，分配失败则减小set->queue_depth再重试
{
	unsigned int depth;
	int err;

	depth = set->queue_depth;                                                    // 1024 for virtblk
	do {
		err = __blk_mq_alloc_rq_maps(set);                                       // 为每一个hw queue分配tags和初始话其中成员，会分配request内存，如果成功则返回0
		if (!err)                                                                // 如果分配成功
			break;                                                               // 退出循环

		set->queue_depth >>= 1;                                                  // 如果分配失败则减小set->queue_depth，再重试
		if (set->queue_depth < set->reserved_tags + BLK_MQ_TAG_MIN) {            // set->queue_depth不应小于set->reserved_tags + BLK_MQ_TAG_MIN，BLK_MQ_TAG_MIN为1
			err = -ENOMEM;                                                       // 设置err为-ENOMEM
			break;                                                               // 退出循环
		}
	} while (set->queue_depth);                                                  // 循环条件为set->queue_depth

	if (!set->queue_depth || err) {                                              // 当!set->queue_depth或err为true表示分配失败
		pr_err("blk-mq: failed to allocate request map\n");                      // 打印错误提示信息
		return -ENOMEM;                                                          // 返回-ENOMEM
	}

	if (depth != set->queue_depth)                                               // 在分配时set->queue_depth有变化
		pr_info("blk-mq: reduced tag depth (%u -> %u)\n",                        // 打印提示信息
						depth, set->queue_depth);

	return 0;                                                                    // 返回0表示分配成功
}

static int __blk_mq_alloc_rq_maps(struct blk_mq_tag_set *set)                   // 为每一个hw queue分配tags和初始话其中成员，会分配request内存
{
	int i;

	for (i = 0; i < set->nr_hw_queues; i++)                                      // 为每一个hw queue分配
		if (!__blk_mq_alloc_rq_map(set, i))                                      // 分配tags和初始话其中成员，会分配request内存
			goto out_unwind;                                                     // 若分配失败则跳转到out_unwind

	return 0;                                                                    // 分配成功，返回0

out_unwind:
	while (--i >= 0)                                                             // 已分配成功的rq map将被释放
		blk_mq_free_rq_map(set->tags[i]);                                        // 释放已分配的rq map

	return -ENOMEM;                                                              // 分配失败则返回-ENOMEM
}

static bool __blk_mq_alloc_rq_map(struct blk_mq_tag_set *set, int hctx_idx)      // 分配tags和初始话其中成员，会分配request内存
{
	int ret = 0;

	set->tags[hctx_idx] = blk_mq_alloc_rq_map(set, hctx_idx,                     // 分配tags及其成员tags->bitmap_tags，tags->breserved_tags，tags->rqs和tags->static_rqs，这个接口中还没有实际分配request
					set->queue_depth, set->reserved_tags);                       // set->queue_depth -> nr_tags
	if (!set->tags[hctx_idx])                                                    // 如果分配失败
		return false;                                                            // 返回false

	ret = blk_mq_alloc_rqs(set, set->tags[hctx_idx], hctx_idx,                   // 为tags->static_rqs[]分配request内存
				set->queue_depth);
	if (!ret)                                                                    // 分配成功
		return true;                                                             // 返回true

	blk_mq_free_rq_map(set->tags[hctx_idx]);                                     // 释放已分配的tags
	set->tags[hctx_idx] = NULL;                                                  // 重设set->tags[hctx_idx]为NULL
	return false;                                                                // 返回false
}

int blk_mq_alloc_rqs(struct blk_mq_tag_set *set, struct blk_mq_tags *tags,       // 为tags->static_rqs[]分配request内存
		     unsigned int hctx_idx, unsigned int depth)
{
	unsigned int i, j, entries_per_page, max_order = 4;                           // 为了避免分配大块连续内存，设定循环中每次分配的内存不应超过order
	size_t rq_size, left;
	int node;

	node = blk_mq_hw_queue_to_node(&set->map[HCTX_TYPE_DEFAULT], hctx_idx);       // 确定hctx_idx的hw queue对应的最小possible cpu id所在内存的node id
	if (node == NUMA_NO_NODE)                                                     // 如果node id为NUMA_NO_NODE
		node = set->numa_node;                                                    // 使用set->numa_node的设定

	INIT_LIST_HEAD(&tags->page_list);                                             // 初始化tags->page_list

	/*
	 * rq_size is the size of the request plus driver payload, rounded
	 * to the cacheline size
	 */
	rq_size = round_up(sizeof(struct request) + set->cmd_size,                    // 计算为一个request分配的内存大小，参考注释
				cache_line_size());
	left = rq_size * depth;                                                       // 根据队列深度和rq大小确定要分配的内存大小，保存在left

	for (i = 0; i < depth; ) {                                                    // 循环0 - depth，之所以在循环中处理分配，是避免分配大块连续内存
		int this_order = max_order;                                               // max_order = 4
		struct page *page;
		int to_do;
		void *p;

		while (this_order && left < order_to_size(this_order - 1))                // 确定buddy分配的阶数
			this_order--;

		do {
			page = alloc_pages_node(node,
				GFP_NOIO | __GFP_NOWARN | __GFP_NORETRY | __GFP_ZERO,
				this_order);                                                      // 如果分配不到page，则减小this_order再重试
			if (page)                                                             // 如果分配成功
				break;                                                            // 结束循环
			if (!this_order--)                                                    // 分配失败的话，若this_order为0，判断后this_order还要自减
				break;                                                            // 退出循环
			if (order_to_size(this_order) < rq_size)                              // 如果this_order对应的size小于一个rq_size
				break;                                                            // 退出循环
		} while (1);

		if (!page)                                                                // 如果分配失败
			goto fail;                                                            // 跳转到fail

		page->private = this_order;                                               // 将this_order保存在page->private中 
		list_add_tail(&page->lru, &tags->page_list);                              // 将page添加到tags->page_list

		p = page_address(page);                                                   // 将page转换成地址
		/*
		 * Allow kmemleak to scan these pages as they contain pointers
		 * to additional allocations like via ops->init_request().
		 */
		kmemleak_alloc(p, order_to_size(this_order), 1, GFP_NOIO);                // 参考注释
		entries_per_page = order_to_size(this_order) / rq_size;                   // 计算这次分配了多少个request
		to_do = min(entries_per_page, depth - i);                                 // 计算有效的本次要处理的request数量
		left -= to_do * rq_size;                                                  // 计算剩余要分配的size
		for (j = 0; j < to_do; j++) {                                             // 处理本次已分配的内存
			struct request *rq = p;                                               // 计算本次分配的第一次个request的地址

			tags->static_rqs[i] = rq;                                             // 使用rq初始化tags->static_rqs[i]
			if (blk_mq_init_request(set, rq, hctx_idx, node)) {                   // 如果set->ops->init_request存在则调用来初始化request，并设置rq->state为MQ_RQ_IDLE
				tags->static_rqs[i] = NULL;                                       // 失败则设定tags->static_rqs[i]为NULL
				goto fail;                                                        // 失败则跳转到fail
			}

			p += rq_size;                                                         // 更新p到下一个request地址
			i++;                                                                  // i自加，用于最外层循环
		}
	}
	return 0;                                                                     // 成功则返回0

fail:
	blk_mq_free_rqs(set, tags, hctx_idx);                                         // 失败则回退当前接口已完成的工作
	return -ENOMEM;                                                               // 失败则返回-ENOMEM
}

static int blk_mq_init_request(struct blk_mq_tag_set *set, struct request *rq,
			       unsigned int hctx_idx, int node)
{
	int ret;

	if (set->ops->init_request) {
		ret = set->ops->init_request(set, rq, hctx_idx, node);
		if (ret)
			return ret;
	}

	WRITE_ONCE(rq->state, MQ_RQ_IDLE);
	return 0;
}

static const struct blk_mq_ops virtio_mq_ops = {
	.queue_rq	= virtio_queue_rq,
	.commit_rqs	= virtio_commit_rqs,
	.complete	= virtblk_request_done,
	.init_request	= virtblk_init_request,
#ifdef CONFIG_VIRTIO_BLK_SCSI
	.initialize_rq_fn = virtblk_initialize_rq,
#endif
	.map_queues	= virtblk_map_queues,
};

static int virtblk_init_request(struct blk_mq_tag_set *set, struct request *rq,
		unsigned int hctx_idx, unsigned int numa_node)
{
	struct virtio_blk *vblk = set->driver_data;
	struct virtblk_req *vbr = blk_mq_rq_to_pdu(rq);

#ifdef CONFIG_VIRTIO_BLK_SCSI
	vbr->sreq.sense = vbr->sense;
#endif
	sg_init_table(vbr->sg, vblk->sg_elems);
	return 0;
}

/**
 * blk_mq_hw_queue_to_node - Look up the memory node for a hardware queue index
 * @qmap: CPU to hardware queue map.
 * @index: hardware queue index.
 *
 * We have no quick way of doing reverse lookups. This is only used at
 * queue init time, so runtime isn't important.
 */
int blk_mq_hw_queue_to_node(struct blk_mq_queue_map *qmap, unsigned int index)
{
	int i;

	for_each_possible_cpu(i) {                                                 // possible cpu表示设备树中定义的cpu
		if (index == qmap->mq_map[i])                                          // hw queue id对应的最小的possible cpu id
			return local_memory_node(cpu_to_node(i));                          // 实际在UMA架构下返回0
	}

	return NUMA_NO_NODE;                                                       // 返回NUMA_NO_NODE表示任意node都可以
}

/*
 * Tag address space map.
 */
struct blk_mq_tags {
	unsigned int nr_tags;
	unsigned int nr_reserved_tags;

	atomic_t active_queues;

	struct sbitmap_queue bitmap_tags;
	struct sbitmap_queue breserved_tags;

	struct request **rqs;                                                     // request指针数组
	struct request **static_rqs;                                              // request指针数组
	struct list_head page_list;
};

struct blk_mq_tags *blk_mq_alloc_rq_map(struct blk_mq_tag_set *set,            // 分配tags及其成员tags->bitmap_tags，tags->breserved_tags，tags->rqs和tags->static_rqs
					unsigned int hctx_idx,                                     // 这个接口中还没有实际分配request
					unsigned int nr_tags,
					unsigned int reserved_tags)
{
	struct blk_mq_tags *tags;
	int node;

	node = blk_mq_hw_queue_to_node(&set->map[HCTX_TYPE_DEFAULT], hctx_idx);      // 确定hctx_idx的hw queue对应的最小possible cpu id所在内存的node id
	if (node == NUMA_NO_NODE)                                                    // 如果node id为NUMA_NO_NODE
		node = set->numa_node;                                                   // 使用set->numa_node的设定

	tags = blk_mq_init_tags(nr_tags, reserved_tags, node,                        // nr_tags -> set->queue_depth，分配tags及其成员tags->bitmap_tags和tags->breserved_tags
				BLK_MQ_FLAG_TO_ALLOC_POLICY(set->flags));
	if (!tags)                                                                   // 分配失败
		return NULL;                                                             // 返回NULL

	tags->rqs = kcalloc_node(nr_tags, sizeof(struct request *),                  // nr_tags -> set->queue_depth，分配tags->rqs，这里分配的时request指针数组
				 GFP_NOIO | __GFP_NOWARN | __GFP_NORETRY,
				 node);
	if (!tags->rqs) {                                                            // 分配失败
		blk_mq_free_tags(tags);                                                  // 释放已分配的tags
		return NULL;                                                             // 返回NULL
	}

	tags->static_rqs = kcalloc_node(nr_tags, sizeof(struct request *),           // nr_tags -> set->queue_depth，分配tags->static_rqs，这里分配的时request指针数组
					GFP_NOIO | __GFP_NOWARN | __GFP_NORETRY,
					node);
	if (!tags->static_rqs) {                                                     // 分配失败
		kfree(tags->rqs);                                                        // 释放已分配的tags->rqs
		blk_mq_free_tags(tags);                                                  // 释放已分配的tags
		return NULL;                                                             // 返回NULL
	}

	return tags;                                                                 // 返回分配的tags
}

enum {
	BLK_MQ_TAG_FAIL		= -1U,                                                  // 0xffffffffffffffff
	BLK_MQ_TAG_MIN		= 1,
	BLK_MQ_TAG_MAX		= BLK_MQ_TAG_FAIL - 1,                                  // 0xfffffffffffffffe
};

struct blk_mq_tags *blk_mq_init_tags(unsigned int total_tags,                   // 分配tags及其成员tags->bitmap_tags和tags->breserved_tags
				     unsigned int reserved_tags,
				     int node, int alloc_policy)
{
	struct blk_mq_tags *tags;                                                    // tag的集合，request将从中申请

	if (total_tags > BLK_MQ_TAG_MAX) {                                           // total_tags -> set->queue_depth
		pr_err("blk-mq: tag depth too large\n");                                 // 打印错误提示
		return NULL;                                                             // 返回NULL
	}

	tags = kzalloc_node(sizeof(*tags), GFP_KERNEL, node);                        // 分配tags内存空间
	if (!tags)                                                                   // 分配失败
		return NULL;                                                             // 返回NULL

	tags->nr_tags = total_tags;                                                  // total_tags -> set->queue_depth
	tags->nr_reserved_tags = reserved_tags;                                      // reserved_tags

	return blk_mq_init_bitmap_tags(tags, node, alloc_policy);                    // 分配tags->bitmap_tags和tags->breserved_tags
}

static struct blk_mq_tags *blk_mq_init_bitmap_tags(struct blk_mq_tags *tags,
						   int node, int alloc_policy)
{
	unsigned int depth = tags->nr_tags - tags->nr_reserved_tags;
	bool round_robin = alloc_policy == BLK_TAG_ALLOC_RR;

	if (bt_alloc(&tags->bitmap_tags, depth, round_robin, node))
		goto free_tags;
	if (bt_alloc(&tags->breserved_tags, tags->nr_reserved_tags, round_robin,
		     node))
		goto free_bitmap_tags;

	return tags;
free_bitmap_tags:
	sbitmap_queue_free(&tags->bitmap_tags);
free_tags:
	kfree(tags);
	return NULL;
}

static int virtblk_probe(struct virtio_device *vdev)
{
	struct virtio_blk *vblk;
	struct request_queue *q;
	int err, index;

	u32 v, blk_size, max_size, sg_elems, opt_io_size;
	u16 min_io_size;
	u8 physical_block_exp, alignment_offset;

	if (!vdev->config->get) {
		dev_err(&vdev->dev, "%s failure: config access disabled\n",
			__func__);
		return -EINVAL;
	}

	err = ida_simple_get(&vd_index_ida, 0, minor_to_index(1 << MINORBITS),
			     GFP_KERNEL);
	if (err < 0)
		goto out;
	index = err;

	/* We need to know how many segments before we allocate. */
	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_SEG_MAX,
				   struct virtio_blk_config, seg_max,
				   &sg_elems);

	/* We need at least one SG element, whatever they say. */
	if (err || !sg_elems)
		sg_elems = 1;

	/* We need an extra sg elements at head and tail. */
	sg_elems += 2;
	vdev->priv = vblk = kmalloc(sizeof(*vblk), GFP_KERNEL);
	if (!vblk) {
		err = -ENOMEM;
		goto out_free_index;
	}

	vblk->vdev = vdev;
	vblk->sg_elems = sg_elems;

	INIT_WORK(&vblk->config_work, virtblk_config_changed_work);

	err = init_vq(vblk);
	if (err)
		goto out_free_vblk;

	/* FIXME: How many partitions?  How long is a piece of string? */
	vblk->disk = alloc_disk(1 << PART_BITS);
	if (!vblk->disk) {
		err = -ENOMEM;
		goto out_free_vq;
	}

	/* Default queue sizing is to fill the ring. */
	if (!virtblk_queue_depth) {
		virtblk_queue_depth = vblk->vqs[0].vq->num_free;
		/* ... but without indirect descs, we use 2 descs per req */
		if (!virtio_has_feature(vdev, VIRTIO_RING_F_INDIRECT_DESC))
			virtblk_queue_depth /= 2;
	}

	memset(&vblk->tag_set, 0, sizeof(vblk->tag_set));
	vblk->tag_set.ops = &virtio_mq_ops;
	vblk->tag_set.queue_depth = virtblk_queue_depth;
	vblk->tag_set.numa_node = NUMA_NO_NODE;
	vblk->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	vblk->tag_set.cmd_size =
		sizeof(struct virtblk_req) +
		sizeof(struct scatterlist) * sg_elems;
	vblk->tag_set.driver_data = vblk;
	vblk->tag_set.nr_hw_queues = vblk->num_vqs;

	err = blk_mq_alloc_tag_set(&vblk->tag_set);                                           // 分配并初始化tag set下的关键成员，细节参考分析
	if (err)                                                                              // 若失败
		goto out_put_disk;                                                                // 则跳转至out_put_disk

	q = blk_mq_init_queue(&vblk->tag_set);
	if (IS_ERR(q)) {
		err = -ENOMEM;
		goto out_free_tags;
	}
	vblk->disk->queue = q;

	q->queuedata = vblk;

	virtblk_name_format("vd", index, vblk->disk->disk_name, DISK_NAME_LEN);

	vblk->disk->major = major;
	vblk->disk->first_minor = index_to_minor(index);
	vblk->disk->private_data = vblk;
	vblk->disk->fops = &virtblk_fops;
	vblk->disk->flags |= GENHD_FL_EXT_DEVT;
	vblk->index = index;

	/* configure queue flush support */
	virtblk_update_cache_mode(vdev);

	/* If disk is read-only in the host, the guest should obey */
	if (virtio_has_feature(vdev, VIRTIO_BLK_F_RO))
		set_disk_ro(vblk->disk, 1);

	/* We can handle whatever the host told us to handle. */
	blk_queue_max_segments(q, vblk->sg_elems-2);

	/* No real sector limit. */
	blk_queue_max_hw_sectors(q, -1U);

	max_size = virtio_max_dma_size(vdev);

	/* Host can optionally specify maximum segment size and number of
	 * segments. */
	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_SIZE_MAX,
				   struct virtio_blk_config, size_max, &v);
	if (!err)
		max_size = min(max_size, v);

	blk_queue_max_segment_size(q, max_size);

	/* Host can optionally specify the block size of the device */
	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_BLK_SIZE,
				   struct virtio_blk_config, blk_size,
				   &blk_size);
	if (!err)
		blk_queue_logical_block_size(q, blk_size);
	else
		blk_size = queue_logical_block_size(q);

	/* Use topology information if available */
	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_TOPOLOGY,
				   struct virtio_blk_config, physical_block_exp,
				   &physical_block_exp);
	if (!err && physical_block_exp)
		blk_queue_physical_block_size(q,
				blk_size * (1 << physical_block_exp));

	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_TOPOLOGY,
				   struct virtio_blk_config, alignment_offset,
				   &alignment_offset);
	if (!err && alignment_offset)
		blk_queue_alignment_offset(q, blk_size * alignment_offset);

	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_TOPOLOGY,
				   struct virtio_blk_config, min_io_size,
				   &min_io_size);
	if (!err && min_io_size)
		blk_queue_io_min(q, blk_size * min_io_size);

	err = virtio_cread_feature(vdev, VIRTIO_BLK_F_TOPOLOGY,
				   struct virtio_blk_config, opt_io_size,
				   &opt_io_size);
	if (!err && opt_io_size)
		blk_queue_io_opt(q, blk_size * opt_io_size);

	if (virtio_has_feature(vdev, VIRTIO_BLK_F_DISCARD)) {
		q->limits.discard_granularity = blk_size;

		virtio_cread(vdev, struct virtio_blk_config,
			     discard_sector_alignment, &v);
		q->limits.discard_alignment = v ? v << SECTOR_SHIFT : 0;

		virtio_cread(vdev, struct virtio_blk_config,
			     max_discard_sectors, &v);
		blk_queue_max_discard_sectors(q, v ? v : UINT_MAX);

		virtio_cread(vdev, struct virtio_blk_config, max_discard_seg,
			     &v);
		blk_queue_max_discard_segments(q,
					       min_not_zero(v,
							    MAX_DISCARD_SEGMENTS));

		blk_queue_flag_set(QUEUE_FLAG_DISCARD, q);
	}

	if (virtio_has_feature(vdev, VIRTIO_BLK_F_WRITE_ZEROES)) {
		virtio_cread(vdev, struct virtio_blk_config,
			     max_write_zeroes_sectors, &v);
		blk_queue_max_write_zeroes_sectors(q, v ? v : UINT_MAX);
	}

	virtblk_update_capacity(vblk, false);
	virtio_device_ready(vdev);

	device_add_disk(&vdev->dev, vblk->disk, virtblk_attr_groups);
	return 0;

out_free_tags:
	blk_mq_free_tag_set(&vblk->tag_set);
out_put_disk:
	put_disk(vblk->disk);
out_free_vq:
	vdev->config->del_vqs(vdev);
out_free_vblk:
	kfree(vblk);
out_free_index:
	ida_simple_remove(&vd_index_ida, index);
out:
	return err;
}



struct request_queue *blk_mq_init_queue(struct blk_mq_tag_set *set)
{
	struct request_queue *uninit_q, *q;

	uninit_q = blk_alloc_queue_node(GFP_KERNEL, set->numa_node);
	if (!uninit_q)
		return ERR_PTR(-ENOMEM);

	/*
	 * Initialize the queue without an elevator. device_add_disk() will do
	 * the initialization.
	 */
	q = blk_mq_init_allocated_queue(set, uninit_q, false);
	if (IS_ERR(q))
		blk_cleanup_queue(uninit_q);

	return q;
}
EXPORT_SYMBOL(blk_mq_init_queue);

/**
 * blk_alloc_queue_node - allocate a request queue
 * @gfp_mask: memory allocation flags
 * @node_id: NUMA node to allocate memory from
 */
struct request_queue *blk_alloc_queue_node(gfp_t gfp_mask, int node_id)
{
	struct request_queue *q;
	int ret;

	q = kmem_cache_alloc_node(blk_requestq_cachep,                                             // 从缓存中申请一个request queue
				gfp_mask | __GFP_ZERO, node_id);
	if (!q)                                                                                    // 若分配失败
		return NULL;                                                                           // 返回NULL

	q->last_merge = NULL;                                                                      // 初始化q->last_merge未NULL

	q->id = ida_simple_get(&blk_queue_ida, 0, 0, gfp_mask);
	if (q->id < 0)
		goto fail_q;

	ret = bioset_init(&q->bio_split, BIO_POOL_SIZE, 0, BIOSET_NEED_BVECS);
	if (ret)
		goto fail_id;

	q->backing_dev_info = bdi_alloc_node(gfp_mask, node_id);
	if (!q->backing_dev_info)
		goto fail_split;

	q->stats = blk_alloc_queue_stats();
	if (!q->stats)
		goto fail_stats;

	q->backing_dev_info->ra_pages = VM_READAHEAD_PAGES;
	q->backing_dev_info->capabilities = BDI_CAP_CGROUP_WRITEBACK;
	q->backing_dev_info->name = "block";
	q->node = node_id;

	timer_setup(&q->backing_dev_info->laptop_mode_wb_timer,
		    laptop_mode_timer_fn, 0);
	timer_setup(&q->timeout, blk_rq_timed_out_timer, 0);
	INIT_WORK(&q->timeout_work, blk_timeout_work);
	INIT_LIST_HEAD(&q->icq_list);
#ifdef CONFIG_BLK_CGROUP
	INIT_LIST_HEAD(&q->blkg_list);
#endif

	kobject_init(&q->kobj, &blk_queue_ktype);

#ifdef CONFIG_BLK_DEV_IO_TRACE
	mutex_init(&q->blk_trace_mutex);
#endif
	mutex_init(&q->sysfs_lock);
	mutex_init(&q->sysfs_dir_lock);
	spin_lock_init(&q->queue_lock);

	init_waitqueue_head(&q->mq_freeze_wq);
	mutex_init(&q->mq_freeze_lock);

	/*
	 * Init percpu_ref in atomic mode so that it's faster to shutdown.
	 * See blk_register_queue() for details.
	 */
	if (percpu_ref_init(&q->q_usage_counter,
				blk_queue_usage_counter_release,
				PERCPU_REF_INIT_ATOMIC, GFP_KERNEL))
		goto fail_bdi;

	if (blkcg_init_queue(q))
		goto fail_ref;

	return q;

fail_ref:
	percpu_ref_exit(&q->q_usage_counter);
fail_bdi:
	blk_free_queue_stats(q->stats);
fail_stats:
	bdi_put(q->backing_dev_info);
fail_split:
	bioset_exit(&q->bio_split);
fail_id:
	ida_simple_remove(&blk_queue_ida, q->id);
fail_q:
	kmem_cache_free(blk_requestq_cachep, q);
	return NULL;
}
EXPORT_SYMBOL(blk_alloc_queue_node);

bool blk_get_queue(struct request_queue *q)
{
	if (likely(!blk_queue_dying(q))) {
		__blk_get_queue(q);
		return true;
	}

	return false;
}
EXPORT_SYMBOL(blk_get_queue);

struct request_queue *blk_mq_init_allocated_queue(struct blk_mq_tag_set *set,
						  struct request_queue *q,
						  bool elevator_init)
{
	/* mark the queue as mq asap */
	q->mq_ops = set->ops;

	q->poll_cb = blk_stat_alloc_callback(blk_mq_poll_stats_fn,
					     blk_mq_poll_stats_bkt,
					     BLK_MQ_POLL_STATS_BKTS, q);
	if (!q->poll_cb)
		goto err_exit;

	if (blk_mq_alloc_ctxs(q))
		goto err_poll;

	/* init q->mq_kobj and sw queues' kobjects */
	blk_mq_sysfs_init(q);

	q->nr_queues = nr_hw_queues(set);
	q->queue_hw_ctx = kcalloc_node(q->nr_queues, sizeof(*(q->queue_hw_ctx)),
						GFP_KERNEL, set->numa_node);
	if (!q->queue_hw_ctx)
		goto err_sys_init;

	INIT_LIST_HEAD(&q->unused_hctx_list);
	spin_lock_init(&q->unused_hctx_lock);

	blk_mq_realloc_hw_ctxs(set, q);
	if (!q->nr_hw_queues)
		goto err_hctxs;

	INIT_WORK(&q->timeout_work, blk_mq_timeout_work);
	blk_queue_rq_timeout(q, set->timeout ? set->timeout : 30 * HZ);

	q->tag_set = set;

	q->queue_flags |= QUEUE_FLAG_MQ_DEFAULT;
	if (set->nr_maps > HCTX_TYPE_POLL &&
	    set->map[HCTX_TYPE_POLL].nr_queues)
		blk_queue_flag_set(QUEUE_FLAG_POLL, q);

	q->sg_reserved_size = INT_MAX;

	INIT_DELAYED_WORK(&q->requeue_work, blk_mq_requeue_work);
	INIT_LIST_HEAD(&q->requeue_list);
	spin_lock_init(&q->requeue_lock);

	blk_queue_make_request(q, blk_mq_make_request);

	/*
	 * Do this after blk_queue_make_request() overrides it...
	 */
	q->nr_requests = set->queue_depth;

	/*
	 * Default to classic polling
	 */
	q->poll_nsec = BLK_MQ_POLL_CLASSIC;

	blk_mq_init_cpu_queues(q, set->nr_hw_queues);
	blk_mq_add_queue_tag_set(set, q);
	blk_mq_map_swqueue(q);

	if (elevator_init)
		elevator_init_mq(q);

	return q;

err_hctxs:
	kfree(q->queue_hw_ctx);
	q->nr_hw_queues = 0;
err_sys_init:
	blk_mq_sysfs_deinit(q);
err_poll:
	blk_stat_free_callback(q->poll_cb);
	q->poll_cb = NULL;
err_exit:
	q->mq_ops = NULL;
	return ERR_PTR(-ENOMEM);
}
EXPORT_SYMBOL(blk_mq_init_allocated_queue);

struct blk_mq_tag_set {
	/*
	 * map[] holds ctx -> hctx mappings, one map exists for each type
	 * that the driver wishes to support. There are no restrictions
	 * on maps being of the same size, and it's perfectly legal to
	 * share maps between types.
	 */
	struct blk_mq_queue_map	map[HCTX_MAX_TYPES];
	unsigned int		nr_maps;	/* nr entries in map[] */
	const struct blk_mq_ops	*ops;
	unsigned int		nr_hw_queues;	/* nr hw queues across maps */
	unsigned int		queue_depth;	/* max hw supported */
	unsigned int		reserved_tags;
	unsigned int		cmd_size;	/* per-request extra data */
	int			numa_node;
	unsigned int		timeout;
	unsigned int		flags;		/* BLK_MQ_F_* */
	void			*driver_data;

	struct blk_mq_tags	**tags;

	struct mutex		tag_list_lock;
	struct list_head	tag_list;
};

static void dump_tag_set(struct blk_mq_tag_set *set) {
	printk("set->map = 0x%px\n", set->map);
	printk("set->nr_maps = %d\n", set->nr_maps);
	printk("set->ops = 0x%px\n", set->ops);
	printk("set->nr_hw_queues = %d\n", set->nr_hw_queues);
	printk("set->queue_depth = %d\n", set->queue_depth);
	printk("set->reserved_tags = %d\n", set->reserved_tags);
	printk("set->cmd_size = %d\n", set->cmd_size);
	printk("set->numa_node = %d\n", set->numa_node);
	printk("set->timeout = %d\n", set->timeout);
	printk("set->flags = 0x%px\n", set->flags);
	printk("set->driver_data = 0x%px\n", set->driver_data);
	printk("set->tags = 0x%px\n", set->tags);
	printk("set->tag_list_lock = 0x%px\n", &set->tag_list_lock);
	printk("set->tag_list = 0x%px\n", &set->tag_list);
}

[    8.079185] set->map = 0xffff0000772f7c10
[    8.083030] set->nr_maps = 1
[    8.084829] set->ops = 0xffff800010df9e60
[    8.087510] set->nr_hw_queues = 1
[    8.089446] set->queue_depth = 1024
[    8.090725] set->reserved_tags = 0
[    8.090990] set->cmd_size = 4120
[    8.091205] set->numa_node = -1
[    8.091670] set->timeout = 0
[    8.091970] set->flags = 1
[    8.093906] set->driver_data = 0xffff0000772f7c00
[    8.094791] set->tags = 0xffff000076af2280
[    8.096857] set->tag_list_lock = 0xffff0000772f7c80
[    8.098532] set->tag_list = 0xffff0000772f7ca0

/data # sync
[   36.652679] andrew foo: blk_flush_queue_rq 136 request = 0xffff000076628000 now = 35697767536
[   36.663366] CPU: 1 PID: 217 Comm: sync Not tainted 5.4.0-g84ca32c-dirty #207
[   36.668846] Hardware name: linux,dummy-virt (DT)
[   36.669858] Call trace:
[   36.670278]  dump_backtrace+0x0/0x140
[   36.671045]  show_stack+0x14/0x20
[   36.672614]  dump_stack+0xbc/0x100
[   36.675389]  blk_flush_queue_rq+0x3c/0x58
[   36.693689]  blk_flush_complete_seq+0x1c4/0x298
[   36.695792]  blk_insert_flush+0xc8/0x150
[   36.696543]  blk_mq_make_request+0x378/0x3d0
[   36.697320]  generic_make_request+0xac/0x348
[   36.698218]  submit_bio+0x40/0x1e8
[   36.698952]  submit_bio_wait+0x58/0x90
[   36.699826]  blkdev_issue_flush+0x90/0xc8
[   36.700611]  ext4_sync_fs+0xec/0x180
[   36.701234]  sync_fs_one_sb+0x24/0x30
[   36.701775]  iterate_supers+0xa8/0xf8
[   36.702387]  ksys_sync+0x60/0xb8
[   36.702925]  __arm64_sys_sync+0xc/0x18
[   36.703622]  el0_svc_common.constprop.2+0x88/0x150
[   36.704351]  el0_svc_handler+0x20/0x80
[   36.705215]  el0_svc+0x8/0xc
[   36.706337] andrew foo: virtio_queue_rq 289
[   36.709975] andrew foo: blk_mq_start_request 712 request = 0xffff000076628000, now = 35755072368
[   36.714643] andrew foo: virtblk_request_done 214
[   36.729449] andrew foo: blk_mq_end_request 572 request = 0xffff0000763a0000, now = 35774540976
[   36.730206] CPU: 0 PID: 0 Comm: swapper/0 Not tainted 5.4.0-g84ca32c-dirty #207
[   36.730824] Hardware name: linux,dummy-virt (DT)
[   36.750177] Call trace:
[   36.750527]  dump_backtrace+0x0/0x140
[   36.751014]  show_stack+0x14/0x20
[   36.751376]  dump_stack+0xbc/0x100
[   36.751807]  blk_mq_end_request+0xec/0x158
[   36.752348]  blk_flush_complete_seq+0xc8/0x298
[   36.752897]  flush_end_io+0x134/0x228
[   36.756943]  blk_mq_end_request+0xc0/0x158
[   36.762657]  virtblk_request_done+0x74/0x84
[   36.766390]  blk_done_softirq+0xa8/0xd8
[   36.787449]  __do_softirq+0x124/0x240
[   36.788716]  irq_exit+0xb8/0xc8
[   36.790134]  __handle_domain_irq+0x64/0xb8
[   36.790448]  gic_handle_irq+0x50/0xa0
[   36.790712]  el1_irq+0xb8/0x180
[   36.790981]  cpuidle_enter_state+0x84/0x360
[   36.791323]  cpuidle_enter+0x34/0x48
[   36.791587]  call_cpuidle+0x1c/0x40
[   36.791839]  do_idle+0x254/0x290
[   36.792110]  cpu_startup_entry+0x24/0x40
[   36.792388]  rest_init+0xd4/0xe0
[   36.792654]  arch_call_rest_init+0xc/0x14
[   36.793047]  start_kernel+0x420/0x44c
[   36.793490] andrew foo: blk_mq_end_request 572 request = 0xffff000076628000, now = 35838595776
[   36.794887] CPU: 0 PID: 0 Comm: swapper/0 Not tainted 5.4.0-g84ca32c-dirty #207
[   36.796092] Hardware name: linux,dummy-virt (DT)
[   36.796858] Call trace:
[   36.797340]  dump_backtrace+0x0/0x140
[   36.797892]  show_stack+0x14/0x20
[   36.798531]  dump_stack+0xbc/0x100
[   36.812189]  blk_mq_end_request+0xec/0x158
[   36.812530]  virtblk_request_done+0x74/0x84
[   36.812853]  blk_done_softirq+0xa8/0xd8
[   36.813894]  __do_softirq+0x124/0x240
[   36.815175]  irq_exit+0xb8/0xc8
[   36.816080]  __handle_domain_irq+0x64/0xb8
[   36.816422]  gic_handle_irq+0x50/0xa0
[   36.816905]  el1_irq+0xb8/0x180
[   36.817125]  cpuidle_enter_state+0x84/0x360
[   36.817292]  cpuidle_enter+0x34/0x48
[   36.817440]  call_cpuidle+0x1c/0x40
[   36.818181]  do_idle+0x254/0x290
[   36.818368]  cpu_startup_entry+0x24/0x40
[   36.818686]  rest_init+0xd4/0xe0
[   36.819394]  arch_call_rest_init+0xc/0x14
[   36.819987]  start_kernel+0x420/0x44c

[   40.612574] CPU: 0 PID: 207 Comm: kworker/0:1H Not tainted 5.4.0-g84ca32c-dirty #208
[   40.612935] Hardware name: linux,dummy-virt (DT)
[   40.613871] Workqueue:  0x0 (kblockd)
[   40.614191] Call trace:
[   40.614369]  dump_backtrace+0x0/0x140
[   40.614585]  show_stack+0x14/0x20
[   40.614829]  dump_stack+0xbc/0x100
[   40.615022]  virtblk_done+0x40/0xf4
[   40.615266]  vring_interrupt+0x68/0x98
[   40.615538]  vm_interrupt+0x8c/0xd8
[   40.615710]  __handle_irq_event_percpu+0x6c/0x170
[   40.615978]  handle_irq_event_percpu+0x34/0x88
[   40.616185]  handle_irq_event+0x44/0xc8
[   40.616422]  handle_fasteoi_irq+0xb4/0x160
[   40.616759]  generic_handle_irq+0x24/0x38
[   40.623394]  __handle_domain_irq+0x60/0xb8
[   40.623682]  gic_handle_irq+0x50/0xa0
[   40.624881]  el1_irq+0xb8/0x180
[   40.625047]  _raw_spin_unlock_irq+0x18/0x50
[   40.625246]  worker_thread+0xa8/0x488
[   40.625438]  kthread+0x118/0x120
[   40.625614]  ret_from_fork+0x10/0x18































































