#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/blkdev.h>
#include <linux/blk_types.h>
#include <linux/module.h>
#include <linux/bio.h>
#include <linux/gfp.h>
#include <linux/blk-mq.h>
#include "blk-convert-biotordma.h"
#include "blk.h"
#include "blk-mq.h"
#include "blk-mq-tag.h"
#include "blk-mq-sched.h"
void print_rdmareq(struct rdmareq req)
{
    // printk(KERN_INFO "pid: %d,%s\n", current->pid, current->comm);
    printk(KERN_INFO "rw_flag: %d\n", req.rw_flag);
    printk(KERN_INFO "sector: %llu\n", req.sector);
    printk(KERN_INFO "totaldatalen: %u\n", req.totaldata_len);
    // printk(KERN_INFO "payload_bytes: %u\n", req.payload_bytes);
}

void getRequest(struct request *bi)
{
    print_request(bi);
    // my_request_handler(bi);
    /* struct rdmareq rrq;
    convert_bio_to_rdma(bi, &rrq); // 传递指向 rrq 的指针 */
}

void convert_bio_to_rdma(struct request *bi, struct rdmareq *rrq)
{
    struct request *rq = bi;

    struct bio_vec bvec;
    struct req_iterator iter;
    /*
       REQ_WRITE 标志位用于表示写操作
       REQ_READ 标志位用于表示读操作
    */
    if (rq_data_dir(rq) == READ)
    {
        // 读取操作
        rrq->rw_flag = 0;
    }
    else if (rq_data_dir(rq) == WRITE)
    {
        // 写入操作
        rrq->rw_flag = 1;
    }
    else
    {
        // 其他操作
        rrq->rw_flag = -1;
    }
    // disk
    /*
    device_number = disk_devt(disk);
    // 使用 MAJOR() 和 MINOR() 宏分别获取主设备号和次设备号
    major_number = MAJOR(device_number);
    minor_number = MINOR(device_number);
    */
    // 获取设备号
    // rrq->bdev = rq->part;

    // 扇区
    rrq->sector = blk_rq_pos(rq);

    // length
    rrq->totaldata_len = blk_rq_bytes(rq);

    // payload bytes
    // rrq->payload_bytes = blk_rq_payload_bytes(rq);

    // 数据长度和物理地址
    // printk(KERN_INFO "pid: %d,%s\n", current->pid, current->comm);

    print_rdmareq(*rrq);
    rq_for_each_bvec(bvec, bi, iter)
    {
        struct page *page = bvec.bv_page;
        unsigned int offset = bvec.bv_offset;
        unsigned int len = bvec.bv_len;
        phys_addr_t phys_addr;

        // 计算物理地址
        phys_addr = page_to_phys(page) + offset;

        // 在这里处理当前 bvec 的物理地址
        // 例如，打印物理地址或执行其他操作
        printk(KERN_INFO "Physical Address: %llx,Partlen: %u\n",
               (unsigned long long)phys_addr, len);
    }
}

void print_request(struct request *req)
{
    printk(KERN_INFO "q: %p\n", req->q);
    printk(KERN_INFO "mq_ctx: %p\n", req->mq_ctx);
    printk(KERN_INFO "mq_hctx: %p\n", req->mq_hctx);
    printk(KERN_INFO "cmd_flags: %u\n", req->cmd_flags);
    printk(KERN_INFO "rq_flags: %u\n", req->rq_flags);
    printk(KERN_INFO "tag: %d\n", req->tag);
    printk(KERN_INFO "internal_tag: %d\n", req->internal_tag);
    printk(KERN_INFO "__data_len: %u\n", req->__data_len);
    printk(KERN_INFO "__sector: %llu\n", (unsigned long long)req->__sector);
    printk(KERN_INFO "bio: %p\n", req->bio);
    printk(KERN_INFO "biotail: %p\n", req->biotail);
    printk(KERN_INFO "queuelist: %p\n", &req->queuelist);
    printk(KERN_INFO "hash or ipi_list: %p\n", &req->hash);                             // Use proper condition to differentiate
    printk(KERN_INFO "rb_node or special_vec or completion_data: %p\n", &req->rb_node); // Use proper condition to differentiate
    printk(KERN_INFO "icq: %p\n", req->elv.icq);
    printk(KERN_INFO "priv[0]: %p\n", req->elv.priv[0]);
    printk(KERN_INFO "priv[1]: %p\n", req->elv.priv[1]);
    printk(KERN_INFO "rq_disk: %p\n", req->rq_disk);
    printk(KERN_INFO "part: %p\n", req->part);
    printk(KERN_INFO "start_time_ns: %llu\n", (unsigned long long)req->start_time_ns);
    printk(KERN_INFO "io_start_time_ns: %llu\n", (unsigned long long)req->io_start_time_ns);
    printk(KERN_INFO "write_hint: %hu\n", req->write_hint);
    printk(KERN_INFO "ioprio: %hu\n", req->ioprio);
    printk(KERN_INFO "extra_len: %u\n", req->extra_len);
    printk(KERN_INFO "state: %u\n", req->state);
    printk(KERN_INFO "timeout: %u\n", req->timeout);
    printk(KERN_INFO "deadline: %lu\n", req->deadline);
    // printk(KERN_INFO "csd or fifo_time: %llu\n", (unsigned long long)req->csd); // Use proper condition to differentiate
    printk(KERN_INFO "end_io: %p\n", req->end_io);
    printk(KERN_INFO "end_io_data: %p\n", req->end_io_data);
}

void my_request_handler(struct request *req)
{
    // 修改 request 的字段

    /* req->rq_flags = 0x23082;
    req->state = MQ_RQ_COMPLETE; // 设置 request 的状态为完成
    req->part = req->biotail->bi_bdev;
    req->__data_len = 0;
    req->tag = 18;
    req->io_start_time_ns = 26169068149;
    req->stats_sectors = 1024;
    req->timeout = 45000;
    req->deadline = 4294943802; */

    // 调用 blk_mq_end_request() 函数返回
    // blk_mq_end_request(req, 0);
    blk_mq_complete_request(req);
}
