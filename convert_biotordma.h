#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/blkdev.h>
struct phyaddr
{
    phys_addr_t phys_addr;
    unsigned int datalen;
};

struct rdmareq
{
    /* 物理地址数组，存储（地址，长度）二元组 */
    struct phyaddr *phyvec;
    /*
    rw_flag读写标志。0：读；1：写；-1：其他操作
     */
    int rw_flag;
    /* 扇区 */
    sector_t sector;
    unsigned int totaldata_len;
    // unsigned int payload_bytes;
};

void convert_bio_to_rdma(struct request *bi, struct rdmareq *rrq);
void print_rdmareq(struct rdmareq req);

void getRequest(struct request *bi);
void print_request(struct request *req);
void my_request_handler(struct request *req);