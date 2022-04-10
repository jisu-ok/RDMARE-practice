#include <infiniband/verbs.h>

// #include <infiniband/verbs_exp.h>

#include <stdio.h>

#include <unistd.h>

#include <string.h>

 

 

#define PORT_NUM 1

#define ENTRY_SIZE 9000 /* maximum size of each send buffer */

#define SQ_NUM_DESC 512 /* maximum number of sends waiting for completion */

 

/* template of packet to send - in this case icmp */

#define DST_MAC 0x00, 0x01, 0x02, 0x03, 0x04, 0x05

#define SRC_MAC 0xe4, 0x1d, 0x2d, 0xf3, 0xdd, 0xcc

#define ETH_TYPE 0x08, 0x00

#define IP_HDRS 0x45, 0x00, 0x00, 0x54, 0x00, 0x00, 0x40, 0x00, 0x40, 0x01, 0xaf, 0xb6

#define SRC_IP 0x0d, 0x07, 0x38, 0x66

#define DST_IP 0x0d, 0x07, 0x38, 0x7f

#define IP_OPT 0x08, 0x00, 0x59, 0xd0, 0x88

#define ICMP_HDR 0x2c, 0x00, 0x09, 0x52, 0xae, 0x96, 0x57, 0x00, 0x00

char packet[] = {

DST_MAC , SRC_MAC, ETH_TYPE, IP_HDRS, SRC_IP, DST_IP, IP_OPT, ICMP_HDR,

0x00, 0x00, 0x62, 0x21, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,

0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,

0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,

0x36, 0x37};

 

int main() {

struct ibv_device **dev_list;

struct ibv_device *ib_dev;

struct ibv_context *context;

struct ibv_pd *pd;

int ret;

 

/*1. Get the list of offload capable devices */

dev_list = ibv_get_device_list(NULL);

if (!dev_list) {

perror("Failed to get devices list");

exit(1);

}

 

/* In this example, we will use the first adapter (device) we find on the list (dev_list[0]) . You may change the code in case you have a setup with more than one adapter installed. */

ib_dev = dev_list[0];

if (!ib_dev) {

fprintf(stderr, "IB device not found\n");

exit(1);

}

 

/* 2. Get the device context */

/* Get context to device. The context is a descriptor and needed for resource tracking and operations */

context = ibv_open_device(ib_dev);

if (!context) {

fprintf(stderr, "Couldn't get context for %s\n",

ibv_get_device_name(ib_dev));

exit(1);

}

 

/* 3. Allocate Protection Domain */

/* Allocate a protection domain to group memory regions (MR) and rings */

pd = ibv_alloc_pd(context);

if (!pd) {

fprintf(stderr, "Couldn't allocate PD\n");

exit(1);

}

 

/* 4. Create Complition Queue (CQ) */

struct ibv_cq *cq;

cq = ibv_create_cq(context, SQ_NUM_DESC, NULL, NULL, 0);

if (!cq) {

fprintf(stderr, "Couldn't create CQ %d\n", errno);

exit (1);

}

 

/* 5. Initialize QP */

struct ibv_qp *qp;

struct ibv_qp_init_attr qp_init_attr = {

.qp_context = NULL,

/* report send completion to cq */

.send_cq = cq,

.recv_cq = cq,

 

.cap = {

/* number of allowed outstanding sends without waiting for a completion */

.max_send_wr = SQ_NUM_DESC,

/* maximum number of pointers in each descriptor */

.max_send_sge = 1,

/* if inline maximum of payload data in the descriptors themselves */

.max_inline_data = 512,

.max_recv_wr = 0

},

.qp_type = IBV_QPT_RAW_PACKET,

};

 

/* 6. Create Queue Pair (QP) - Send Ring */

qp = ibv_create_qp(pd, &qp_init_attr);

if (!qp) {

fprintf(stderr, "Couldn't create RSS QP\n");

exit(1);

}

 

/* 7. Initialize the QP (receive ring) and assign a port */

struct ibv_qp_attr qp_attr;

int qp_flags;

memset(&qp_attr, 0, sizeof(qp_attr));

 

qp_flags = IBV_QP_STATE | IBV_QP_PORT;

qp_attr.qp_state = IBV_QPS_INIT;

qp_attr.port_num = 1;

ret = ibv_modify_qp(qp, &qp_attr, qp_flags);

if (ret < 0) {

fprintf(stderr, "failed modify qp to init\n");

exit(1);

}

memset(&qp_attr, 0, sizeof(qp_attr));

 

/* 8. Move the ring to ready to send in two steps (a,b) */

/* a. Move ring state to ready to receive, this is needed to be able to move ring to ready to send even if receive queue is not enabled */

qp_flags = IBV_QP_STATE;

qp_attr.qp_state = IBV_QPS_RTR;

ret = ibv_modify_qp(qp, &qp_attr, qp_flags);

if (ret < 0) {

fprintf(stderr, "failed modify qp to receive\n");

exit(1);

}

 

/* b. Move the ring to ready to send */

qp_flags = IBV_QP_STATE;

qp_attr.qp_state = IBV_QPS_RTS;

ret = ibv_modify_qp(qp, &qp_attr, qp_flags);

if (ret < 0) {

fprintf(stderr, "failed modify qp to receive\n");

exit(1);

}

 

/* 9. Allocate Memory */

int buf_size = ENTRY_SIZE*SQ_NUM_DESC; /* maximum size of data to be access directly by hw */

void *buf;

buf = malloc(buf_size);

if (!buf) {

fprintf(stderr, "Coudln't allocate memory\n");

exit(1);

}

 

/* 10. Register the user memory so it can be accessed by the HW directly */

struct ibv_mr *mr;

mr = ibv_reg_mr(pd, buf, buf_size, IBV_ACCESS_LOCAL_WRITE);

if (!mr) {

fprintf(stderr, "Couldn't register mr\n");

exit(1);

}

memcpy(buf, packet, sizeof(packet));

 

int n;

struct ibv_sge sg_entry;

struct ibv_send_wr wr, *bad_wr;

int msgs_completed;

struct ibv_wc wc;

 

 

/* scatter/gather entry describes location and size of data to send*/

sg_entry.addr = (uint64_t)buf;

sg_entry.length = sizeof(packet);

sg_entry.lkey = mr->lkey;

 

memset(&wr, 0, sizeof(wr));

 

 

/*

* descriptor for send transaction - details:

* - how many pointer to data to use

* - if this is a single descriptor or a list (next == NULL single)

* - if we want inline and/or completion

*/

wr.num_sge = 1;

wr.sg_list = &sg_entry;

wr.next = NULL;

wr.opcode = IBV_WR_SEND;

 

/* 10. Send Operation */

while(1) {

/*

* inline means data will be copied to space pre-allocated in descriptor

* as long as it is small enough. otherwise pointer reference will be used.

* see max_inline_data = 512 above.

*/

wr.send_flags = IBV_SEND_INLINE;

 

/*

* we ask for a completion every half queue. only interested in completions to monitor progress.

*/

if ( (n % (SQ_NUM_DESC/2)) == 0) {

wr.wr_id = n;

wr.send_flags |= IBV_SEND_SIGNALED;

}

 

/* push descriptor to hardware */

ret = ibv_post_send(qp, &wr, &bad_wr);

if (ret < 0) {

fprintf(stderr, "failed in post send\n");

exit(1);

}

n++;

 

/* poll for completion after half ring is posted */

if ( (n % (SQ_NUM_DESC/2)) == 0 && n > 0) {

msgs_completed = ibv_poll_cq(cq, 1, &wc);

if (msgs_completed > 0) {

printf("completed message %ld\n", wc.wr_id);

} else if (msgs_completed < 0) {

printf("Polling error\n");

exit(1);

}

}

}

 

printf("We are done\n");

 

return 0;

}
