#include <infiniband/verbs.h>
//#include <infiniband/verbs_exp.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <assert.h>

// options
// #define SEND_SIGNALING
// #define SEND_INLINING

#define PORT_NUM 1
#define ENTRY_SIZE 9000 /* maximum size of each packet buffer */
#define SQ_NUM_DESC 512 /* maximum number of sends waiting for completion */
#define RQ_NUM_DESC 512 /* The maximum receive ring length without processing */

#define MAC_SANCTUARY {0x24, 0x8A, 0x07, 0x91, 0x69, 0xA0}
#define MAC_MOSCOW {0x24, 0x8A, 0x07, 0x91, 0x68, 0xE8}

/* template of packet to send - in this case icmp */

// #define DST_MAC 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 // 6 bytes
// #define SRC_MAC 0xe4, 0x1d, 0x2d, 0xf3, 0xdd, 0xcc // 6 bytes
#define ETH_TYPE 0x08, 0x00 // 2 bytes
#define IP_HDRS 0x45, 0x00, 0x00, 0x54, 0x00, 0x00, 0x40, 0x00, 0x40, 0x01, 0xaf, 0xb6 // 12 bytes
#define SRC_IP 0x0d, 0x07, 0x38, 0x66 // 4 bytes
#define DST_IP 0x0d, 0x07, 0x38, 0x7f // 4 bytes
#define IP_OPT 0x08, 0x00, 0x59, 0xd0, 0x88 // 5 bytes
#define ICMP_HDR 0x2c, 0x00, 0x09, 0x52, 0xae, 0x96, 0x57, 0x00, 0x00 // 9 bytes

#define TCP_SPORT 0x00, 0x03
#define TCP_DPORT 0x03, 0x00
#define TCP_SEQ 0x00, 0x00, 0x01, 0x00
#define TCP_ACK 0x00, 0x01, 0x00, 0x00
#define TCP_HDR 0x00, 0x02, 0x02, 0x00, 0xAA, 0xAA, 0x00, 0x00

char packet[] = {
    0x24, 0x8A, 0x07, 0x91, 0x69, 0xA0, // dst: MAC_SANCTUARY
    0x24, 0x8A, 0x07, 0x91, 0x68, 0xE8, // src: MAC_MOSCOW
    ETH_TYPE, IP_HDRS, SRC_IP, DST_IP, IP_OPT, ICMP_HDR,
    0x00, 0x00, 0x62, 0x21, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
    0x36, 0x37}; // 6 + 6 + 2 + 12 + 4 + 4 + 5 + 9 + 50 = 98 bytes

// char packet[1514] = {
//     0x24, 0x8A, 0x07, 0x91, 0x69, 0xA0, // dst: MAC_SANCTUARY
//     0x24, 0x8A, 0x07, 0x91, 0x68, 0xE8, // src: MAC_MOSCOW
//     ETH_TYPE,
//     IP_HDRS, SRC_IP, DST_IP,
//     TCP_SPORT, TCP_DPORT, TCP_SEQ, TCP_ACK, TCP_HDR,
//     0x01, 0x02, 0x03, 0x04, };

// char packet[1514] = {
//     0x24, 0x8A, 0x07, 0x91, 0x69, 0xA0, // dst: MAC_SANCTUARY
//     0x24, 0x8A, 0x07, 0x91, 0x68, 0xE8, // src: MAC_MOSCOW
//     ETH_TYPE,
//     IP_HDRS, SRC_IP, DST_IP,
//     TCP_SPORT, TCP_DPORT, TCP_SEQ, TCP_ACK, TCP_HDR,
//     0x00, 0x00, 0x62, 0x21, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
//     0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
//     0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
//     0x36, 0x37, };



int time_diff_nsec(struct timespec t1, struct timespec t2) {
    return (t2.tv_nsec - t1.tv_nsec) + 1000000000 * (t2.tv_sec - t1.tv_sec);
}


int main()
{
#ifndef SEND_SIGNALING
    printf("Not get signaled for send\n\n");
#endif

#ifndef SEND_INLINING
    printf("Not inlining for send\n\n");
#endif

    struct ibv_device **dev_list;
    struct ibv_device *ib_dev;
    struct ibv_context *context;
    struct ibv_pd *pd;
    int ret;

    /*1. Get the list of offload capable devices */
    dev_list = ibv_get_device_list(NULL);
    if (!dev_list)
    {
        perror("Failed to get devices list");
        exit(1);
    }
    
    /* In this example, we will use the first adapter (device) we find on the list (dev_list[0]) . You may change the code in case you have a setup with more than one adapter installed. */
    ib_dev = dev_list[0];
    if (!ib_dev)
    {
        fprintf(stderr, "IB device not found\n");
        exit(1);
    }

    /* 2. Get the device context */
    /* Get context to device. The context is a descriptor and needed for resource tracking and operations */
    context = ibv_open_device(ib_dev);
    if (!context)
    {
        fprintf(stderr, "Couldn't get context for %s\n",
                ibv_get_device_name(ib_dev));
        exit(1);
    }

    /* 3. Allocate Protection Domain */
    /* Allocate a protection domain to group memory regions (MR) and rings */
    pd = ibv_alloc_pd(context);
    if (!pd)
    {
        fprintf(stderr, "Couldn't allocate PD\n");
        exit(1);
    }

    /* 4. Create Complition Queue (CQ) */
    struct ibv_cq *send_cq;
    send_cq = ibv_create_cq(context, SQ_NUM_DESC, NULL, NULL, 0);
    if (!send_cq)
    {
        fprintf(stderr, "Couldn't create CQ %d\n", errno);
        exit(1);
    }

    struct ibv_cq *recv_cq;
    recv_cq = ibv_create_cq(context, RQ_NUM_DESC, NULL, NULL, 0);
    if (!recv_cq)
    {
        fprintf(stderr, "Couldn't create CQ %d\n", errno);
        exit(1);
    }


    /* 5. Initialize QP */
    struct ibv_qp *qp;
    struct ibv_qp_init_attr qp_init_attr = {
        .qp_context = NULL,

        /* report send completion to cq */
        .send_cq = send_cq,
        .recv_cq = recv_cq,
        // .srq = NULL,
        .cap = {
            /* number of allowed outstanding sends without waiting for a completion */
            .max_send_wr = SQ_NUM_DESC,

            /* maximum number of pointers in each descriptor */
            .max_send_sge = 1,

            /* if inline maximum of payload data in the descriptors themselves */
            .max_inline_data = 512,

            .max_recv_wr = RQ_NUM_DESC,
            .max_recv_sge = 1,
        },

        .qp_type = IBV_QPT_RAW_PACKET,
        .sq_sig_all = 0,
    };

    /* 6. Create Queue Pair (QP) - Send Ring */
    qp = ibv_create_qp(pd, &qp_init_attr);
    if (!qp)
    {
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

    if (ret < 0)
    {
        fprintf(stderr, "failed modify qp to init\n");
        exit(1);
    }
    memset(&qp_attr, 0, sizeof(qp_attr));

    /* 8. Move the ring to ready to send in two steps (a,b) */

    /* a. Move ring state to ready to receive, this is needed to be able to move ring to ready to send even if receive queue is not enabled */
    qp_flags = IBV_QP_STATE;
    qp_attr.qp_state = IBV_QPS_RTR;
    ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
    if (ret < 0)
    {
        fprintf(stderr, "failed modify qp to receive\n");
        exit(1);
    }

    /* b. Move the ring to ready to send */
    qp_flags = IBV_QP_STATE;
    qp_attr.qp_state = IBV_QPS_RTS;
    ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
    if (ret < 0)
    {
        fprintf(stderr, "failed modify qp to send\n");
        exit(1);
    }

    /* 9. Allocate Memory */
    int send_buf_size = ENTRY_SIZE * SQ_NUM_DESC; /* maximum size of data to be access directly by hw */
    void *send_buf;
    send_buf = malloc(send_buf_size);
    if (!send_buf)
    {
        fprintf(stderr, "Coudln't allocate memory\n");
        exit(1);
    }
    memset(send_buf, 0, send_buf_size);

    int recv_buf_size = ENTRY_SIZE * RQ_NUM_DESC; /* maximum size of data to be access directly by hw */
    void *recv_buf;
    recv_buf = malloc(recv_buf_size);
    if (!recv_buf)
    {
        fprintf(stderr, "Coudln't allocate memory\n");
        exit(1);
    }
    memset(recv_buf, 0, recv_buf_size);

    /* 10. Register the user memory so it can be accessed by the HW directly */
    struct ibv_mr *send_mr;
    send_mr = ibv_reg_mr(pd, send_buf, send_buf_size, IBV_ACCESS_LOCAL_WRITE);
    if (!send_mr)
    {
        fprintf(stderr, "Couldn't register mr\n");
        exit(1);
    }

    struct ibv_mr *recv_mr;
    recv_mr = ibv_reg_mr(pd, recv_buf, recv_buf_size, IBV_ACCESS_LOCAL_WRITE);
    if (!recv_mr)
    {
        fprintf(stderr, "Couldn't register mr\n");
        exit(1);
    }

    int n;
    struct ibv_sge send_sg_entry, recv_sg_entry;
    struct ibv_send_wr send_wr, *send_bad_wr;
    struct ibv_recv_wr recv_wr, *recv_bad_wr;
    int msgs_completed;
    struct ibv_wc send_wc, wc;

    memset(&send_sg_entry, 0, sizeof(send_sg_entry));
    memset(&recv_sg_entry, 0, sizeof(recv_sg_entry));
    memset(&send_wr, 0, sizeof(send_wr));
    memset(&recv_wr, 0, sizeof(recv_wr));
    memset(&send_wc, 0, sizeof(send_wc));
    memset(&wc, 0, sizeof(wc));
    
    for (n=0; n < RQ_NUM_DESC; n++) {
        recv_sg_entry.addr = (uint64_t) recv_buf + ENTRY_SIZE*n;
        recv_sg_entry.length = ENTRY_SIZE;
        recv_sg_entry.lkey = recv_mr->lkey;

        recv_wr.wr_id = n;
        recv_wr.num_sge = 1;
        recv_wr.sg_list = &recv_sg_entry;
        recv_wr.next = NULL;
        ibv_post_recv(qp, &recv_wr, &recv_bad_wr);
    }


    // Create steeting rule
    struct raw_eth_flow_attr {
        struct ibv_flow_attr attr;
        struct ibv_flow_spec_eth spec_eth;
    } __attribute__((packed)) flow_attr = {
        .attr = {
            .comp_mask = 0,
            .type = IBV_FLOW_ATTR_NORMAL,
            .size = sizeof(flow_attr),
            .priority = 0,
            .num_of_specs = 1,
            .port = PORT_NUM,
            .flags = 0,
        },
        .spec_eth = {
            .type = IBV_FLOW_SPEC_ETH,
            .size = sizeof(struct ibv_flow_spec_eth),
            .val = {
                .dst_mac = MAC_MOSCOW,
                .src_mac = MAC_SANCTUARY,
                .ether_type = 0,
                .vlan_tag = 0,
            },
            .mask = {
                .dst_mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
                .src_mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
                .ether_type = 0,
                .vlan_tag = 0,   
            },
        }
    };

    struct ibv_flow_attr attr2 = {
        .comp_mask = 0,
        .type = IBV_FLOW_ATTR_SNIFFER,
        .size = sizeof(struct ibv_flow_attr),
        .priority = 0,
        .num_of_specs = 0,
        .port = PORT_NUM,
        .flags = 0
    };

    // Register steering rule
    struct ibv_flow *eth_flow;

    eth_flow = ibv_create_flow(qp, &flow_attr.attr);
    // eth_flow = ibv_create_flow(qp, &attr2);
    if (!eth_flow) {
        perror("Couldn't attach steering flow");
        exit(-1);
    }


    /* scatter/gather entry describes location and size of data to send*/


    // prepare packets for send
    for (int i = 0; i < SQ_NUM_DESC; i++) {
        memcpy((void *) ((char *) send_buf + i*ENTRY_SIZE), packet, sizeof(packet));
    }
    
    // set values that will not change during loop
    recv_sg_entry.length = ENTRY_SIZE;
    recv_sg_entry.lkey = recv_mr->lkey;

    recv_wr.num_sge = 1;
    recv_wr.sg_list = &recv_sg_entry;
    recv_wr.next = NULL;

    send_sg_entry.lkey = send_mr->lkey;

    send_wr.num_sge = 1;
    send_wr.sg_list = &send_sg_entry;
    send_wr.next = NULL;
    send_wr.opcode = IBV_WR_SEND;
// #ifdef SEND_INLINING
//     send_wr.send_flags = IBV_SEND_INLINE;
// #endif

// #ifdef SEND_SIGNALING
//     send_wr.send_flags |= IBV_SEND_SIGNALED;
// #endif

    // send_wc.wr_id = 123;

    n = 0;
    struct timespec t1, t2;
    while (1)
    {  
        msgs_completed = ibv_poll_cq(recv_cq, 1, &wc);
        if (msgs_completed > 0) {
            // Receive message from client
            // printf("Received message %ld: %d bytes\n", wc.wr_id, wc.byte_len);
            recv_sg_entry.addr = (uint64_t) recv_buf + wc.wr_id*ENTRY_SIZE;
            recv_wr.wr_id = wc.wr_id;
            ibv_post_recv(qp, &recv_wr, &recv_bad_wr);

            // Send reply to client
            send_sg_entry.addr = (uint64_t) send_buf + (n % SQ_NUM_DESC)*ENTRY_SIZE;
            send_sg_entry.length = sizeof(packet);
            send_wr.send_flags = 0;

#ifdef SEND_INLINING
            send_wr.send_flags |= IBV_SEND_INLINE;
#endif

#ifdef SEND_SIGNALING
            send_wr.wr_id = n;
            send_wr.send_flags |= IBV_SEND_SIGNALED;
#else
            if ((n % (SQ_NUM_DESC / 2)) == 0) {
            // if ((n % SQ_NUM_DESC) == 0) {
                send_wr.wr_id = n;
                send_wr.send_flags |= IBV_SEND_SIGNALED;
            }
#endif

            ret = ibv_post_send(qp, &send_wr, &send_bad_wr);
            if (ret < 0) {
                perror("Failed in post send");
                exit(-1);
            }

#ifdef SEND_SIGNALING
            do
                msgs_completed = ibv_poll_cq(send_cq, 1, &send_wc);
            while (msgs_completed <= 0);
            if (msgs_completed < 0) {
                perror("ibv_poll_cq()");
                exit(-1);
            }
            printf("Replied message %ld: %d bytes\n", send_wc.wr_id, (int) sizeof(packet));
#else
            if  ((n % (SQ_NUM_DESC / 2)) == 0) {
            // if  ((n % SQ_NUM_DESC) == 0) {
                do
                    msgs_completed = ibv_poll_cq(send_cq, 1, &wc);
                while (msgs_completed <= 0);
                if (msgs_completed < 0) {
                    perror("ibv_poll_cq()");
                    exit(-1);
                }
                // assert(send_wc.status == IBV_WC_SUCCESS);
                // printf("Replied message %ld: %d bytes (n=%d)\n", wc.wr_id, (int) sizeof(packet), n);
            }
#endif
            n++;
        }
        else if (msgs_completed < 0) {
            perror("ibv_poll_cq()");
            exit(-1);
        }

    }

    printf("We are done\n");

    return 0;
}
