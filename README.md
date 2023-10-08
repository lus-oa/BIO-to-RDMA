# BIO-to-RDMA
**如何从Linux内核中的BLOCK Layer中截取出当前IO操作的bio,并将bio中的信息解析转换成RDMA请求需要的参数。**

**How to extract the bio of the current I/O operation from the BLOCK Layer in the Linux kernel and parse the information in the bio into the parameters required for RDMA requests.**

## BIO简介
**文件系统层(File System Layer)** 和 **块层(Block Layer)** 之间传递的主要数据是块设备的I/O请求。这些I/O请求包含了对块设备的读取和写入操作。
![image](https://github.com/fusemen/BIO-to-RDMA/assets/122666739/6c166fca-3c50-4fd8-812b-a3f93702cffb)

#### 数据在文件系统层(file system layer)和块层(block layer)之间的传递过程如下：
-	`文件系统层生成I/O请求`：当应用程序需要读取或写入文件时，文件系统层会将相应的I/O请求生成并传递给块层。
-	`块层处理I/O请求`：块层接收到来自文件系统层的I/O请求后，负责将其转换为与底层块设备交互的操作。
- `块设备驱动程序处理请求`：块层将生成的块设备I/O请求传递给相应的块设备驱动程序。
-	`块设备访问`：块设备驱动程序通过硬件接口与实际的块设备进行通信，执行读取或写入操作。
-	`数据传输`：块设备驱动程序通过硬件接口将数据从磁盘读取到内存缓冲区，或将内存中的数据写入到磁盘。
-	`完成请求`：块设备驱动程序在数据传输完成后，将相应的完成状态和结果信息返回给块层。
-	`块层处理结果`：块层接收到来自块设备驱动程序的完成状态和结果信息后，将其传递给文件系统层。
-	`文件系统层继续处理`：文件系统层根据块层返回的结果继续进行文件操作或处理其他请求。

#### 文件系统层和块层之间传输的数据格式主要涉及两个方面：`I/O请求描述和数据缓冲区`。
1. I/O请求描述数据格式：
   - I/O请求描述包含了对块设备的读取或写入操作的相关参数，如*文件位置、大小、读写标志*等。
   - 通常使用数据结构来表示，在块层中常见的数据结构是`struct request`或`struct bio`。

2. 数据缓冲区数据格式：
   - 数据缓冲区用于存储从块设备读取的数据或将数据写入到块设备的内存区域。
   - 数据缓冲区的格式取决于应用程序或文件系统层的需要，通常是以字节流的形式表示。
   - 数据缓冲区可以使用普通的内存缓冲区，也可以使用散列表（scatter-gather）形式的数据结构，用于描述非连续的数据块。

#### bio
```cpp
struct bio
{
    sector_t bi_sector; /*磁盘起始扇区号 */
    struct block_device *bi_bdev; /* bio操作的块设备 */
    unsigned long bi_flags; /* 状态标志位 */
    unsigned long bi_rw; /*读写 */
    unsigned short bi_vcnt; /*  bio_vec's 个数*/
    unsigned short bi_idx; /* 当前bvl_vec 数组的index*/
    unsigned int bi_size; /* 整个bio的大小:所有bi_io_vec->len之和 */
    bio_end_io_t *bi_end_io; /* bio完成时调用*/
    void *bi_private; /*bio私有数据 */
    unsigned int bi_max_vecs; /* bio携带的最大bio_vec数量(实际使用的bio_vec由bi_vcnt表示) */
    atomic_t bi_cnt; /* bio引用计数*/
    struct bio_vec *bi_io_vec; /* bio_vec数组 */
    struct bio_set *bi_pool; /*bio_set维护了若干不同大小的bio slab */
    struct bio_vec bi_inline_vecs[0]; /* bio 内嵌的bio_vec*/
};
```

#### request
```cpp
struct request
{
    struct request_queue *q;         // 请求队列指针
    struct blk_mq_ctx *mq_ctx;       // 请求所属的多队列上下文
    struct blk_mq_hw_ctx *mq_hctx;   // 请求所属的硬件队列上下文
    unsigned int cmd_flags;          // 命令标志位，用于存储操作和通用标志
    req_flags_t rq_flags;            // 请求标志位
    int tag;                         // 请求的标签
    int internal_tag;                // 内部标签，不要直接访问
    unsigned int __data_len;         // 数据总长度
    sector_t __sector;               // 扇区游标
    struct bio *bio;                 // 请求对应的 bio
    struct bio *biotail;             // bio 链表的尾部
    struct list_head queuelist;      // 队列链表
    struct gendisk *rq_disk;         // 关联的 gendisk 结构体指针
    struct block_device *part;       // 关联的分区块设备指针
    u64 start_time_ns;               // 分配此请求的时间戳（纳秒）
    u64 io_start_time_ns;            // I/O 提交给设备的时间戳（纳秒）
    unsigned short stats_sectors;    // 统计扇区数
    unsigned short nr_phys_segments; // 物理段数
    unsigned short write_hint;       // 写入提示
    unsigned short ioprio;           // I/O 优先级
    enum mq_rq_state state;          // 请求状态
    refcount_t ref;                  // 引用计数
    unsigned int timeout;            // 超时时间
    unsigned long deadline;          // 截止时间
    rq_end_io_fn *end_io;            // 完成回调函数
    void *end_io_data;               // 完成回调函数的私有数据
};
```

### BLOCK框架
![image](https://github.com/fusemen/BIO-to-RDMA/assets/122666739/58bf8b19-e05c-4380-9726-85e3212a6a75)

Block 层连接着文件系统层和设备驱动层，从 submit_bio() 开始，bio 就进入了 block 层，这些 bio 被 Block 层抽象成 request 管理，在适当的时候这些 request 离开 Block 层进入设备驱动层。cmd是设备驱动处理的IO请求，设备驱动程序根据器件协议，将request转换成cmd，然后发送给器件处理。IO 请求完成后，Block 层的软中断负责处理 IO 完成后的工作。

![image](https://github.com/fusemen/BIO-to-RDMA/assets/122666739/e1c9bdb0-54a5-475c-ac2b-088fc9f52b36)

### BLOCK层主要负责：
#### 管理IO请求

IO 请求暂存、合并，以及决定以何种顺序处理IO请求。这里面涉及到 single-queue、multi-queue 框架以及具体的 IO 调度器。

![image](https://github.com/fusemen/BIO-to-RDMA/assets/122666739/ef3c311f-5b97-459d-8c14-aeb0c59fe3ee)

#### bio与request的关系

![image](https://github.com/fusemen/BIO-to-RDMA/assets/122666739/8b3dea74-2a8a-4303-8a5a-43a6c3d42f50)
![image](https://github.com/fusemen/BIO-to-RDMA/assets/122666739/a64b667a-ba22-4208-9279-a693e314933e)
![image](https://github.com/fusemen/BIO-to-RDMA/assets/122666739/9017e88f-d324-47af-b84b-30630b413519)

### request执行流程

#### 开始执行
调用blk_mq_start_request()函数执行request,若要截取完整的为执行的request及bio则需要在此处截取。

![image](https://github.com/fusemen/BIO-to-RDMA/assets/122666739/86ac06d0-cf2d-4090-a7b5-6a118a578d80)


```cpp
void blk_mq_start_request(struct request *rq)
{
getRequest(rq)

    struct request_queue *q = rq->q; // 获取请求所属的请求队列

    trace_block_rq_issue(rq); // 发出跟踪记录，表示请求已经发出

    if (test_bit(QUEUE_FLAG_STATS, &q->queue_flags))
{
    rq->io_start_time_ns = ktime_get_ns();  // 获取当前时间并保存为请求的开始时间
        rq->stats_sectors = blk_rq_sectors(rq); // 获取请求的扇区数并保存到统计数据中
        rq->rq_flags |= RQF_STATS;              // 设置请求标志，表示请求具有统计数据
        rq_qos_issue(q, rq);                    // 执行请求质量控制相关的操作
    }

    WARN_ON_ONCE(blk_mq_rq_state(rq) != MQ_RQ_IDLE); // 检查请求的状态是否为空闲状态，如果不是则发出警告

    blk_add_timer(rq);                      // 向请求队列添加计时器，以便在超时时处理请求
    WRITE_ONCE(rq->state, MQ_RQ_IN_FLIGHT); // 设置请求状态为飞行状态，表示请求已经开始执行

#ifdef CONFIG_BLK_DEV_INTEGRITY
    if (blk_integrity_rq(rq) && req_op(rq) == REQ_OP_WRITE)
        q->integrity.profile->prepare_fn(rq); // 如果请求需要进行数据完整性检查，并且操作为写操作，则执行相应的数据准备操作
#endif
}
```

#### IO完成后设备驱动向块层传递完成情况：

1. blk_mq_complete_request(struct request *rq)函数：
   - 功能：用于标记请求的完成状态，并进行相关的处理。
   - 调用时机：通常在设备驱动程序中，当设备完成请求处理后，通过调用blk_mq_complete_request()函数来标记请求的完成状态。
   - 参数：struct request *rq，表示完成的请求对象。
   - 处理逻辑：该函数将请求的状态设置为完成状态，并执行后续的处理，如调用完成回调函数、更新统计信息等。它不负责将请求从设备队列中移除。

2. blk_mq_end_request(struct request *rq, blk_status_t error)函数：
   - 功能：用于通知块层请求的完成情况，并将请求从设备队列中移除。
   - 调用时机：在设备驱动程序中，当设备完成请求处理后，通过调用blk_mq_end_request()函数来通知块层请求的完成情况。
   - 参数：struct request *rq，表示完成的请求对象；blk_status_t error，表示请求的完成状态。
   - 处理逻辑：该函数根据`error`参数更新请求的状态，并将请求从设备队列中移除。它还可能执行后续的处理，如调用完成回调函数、更新统计信息等。

总结起来，`blk_mq_complete_request()`函数用于标记请求的完成状态，并进行相关的处理，而`blk_mq_end_request()`函数则用于通知块层请求的完成情况，并将请求从设备队列中移除。

#### 抓取开始执行和完成执行的request进行对比

![image](https://github.com/fusemen/BIO-to-RDMA/assets/122666739/741412a7-d9ce-4824-954e-29267cc373e8)






