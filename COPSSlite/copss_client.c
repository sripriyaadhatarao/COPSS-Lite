#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "net/af.h"
#include "net/ipv6/addr.h"
#include "thread.h"
#include "net/sock/udp.h"
#include "ccn-lite-riot.h"
#include "ccnl-pkt-ndntlv.h"
#include "ccnl-ext.h"

#include "copss_pkt_tlv.h"
#include "copss_headers.h"
#include "copss_riot_ccn_lite.h"

#define SERVER_MSG_QUEUE_SIZE   (8)
//#define BUFFER_SIZE      (64)
#define BUFFER_SIZE      (CCNL_MAX_PACKET_SIZE/2)
#define MAX_ADDR_LEN            (8U)
#define MAX_ADDR_PRINT_LEN      (MAX_ADDR_LEN*3)

static bool copss_client_running = false;
static unsigned char server_buffer[CCNL_MAX_PACKET_SIZE];
static char server_stack[THREAD_STACKSIZE_DEFAULT];
//static msg_t server_msg_queue[SERVER_MSG_QUEUE_SIZE];
static sock_udp_ep_t local = SOCK_IPV6_EP_ANY;
static sock_udp_t sock;
static char buf[BUFFER_SIZE];
static unsigned char _out[CCNL_MAX_PACKET_SIZE];
static int suite = CCNL_SUITE_NDNTLV;

void *_client_server(void *args) {
    local.port = (uint16_t) atoi(args);
    if (local.port == copss.port) {
        puts("Error please try again with another port");
        return NULL;
    }
    if (sock_udp_create(&sock, &local, NULL, 0) < 0) {
        puts("Error creating UDP sock");
        return NULL;
    }

    local.addr = copss.server.addr;
    //link copss with this client server
    int flag = copss_link(&copss, &local, '0');

    if (flag == -1) {
        puts("face already exists");
        return NULL;
    }
    if (flag == -2) {
        puts("no memory for face");
        return NULL;
    }

    copss_client_running = true;
    printf("Success: started COPSS Client server on port %" PRIu8 "\n", local.port);

    while (1) {
        sock_udp_ep_t remote;
        ssize_t res;
        if ((res = sock_udp_recv(&sock, server_buffer, sizeof (server_buffer), SOCK_NO_TIMEOUT, &remote)) >= 0) {

            int len, datalen = res;
            unsigned int typ;
            unsigned char *olddata;
            unsigned char *data = olddata = server_buffer;
            struct copss_pkt_s *copkt;
            if (ccnl_ndntlv_dehead(&data, &datalen, (int*) &typ, &len) == 0) {
                //                printf("dehead, type: %x \n", typ);
                switch (typ) {
                    case COPSS_TLV_Multicast:
                        //                        puts("multicast pkt");
                        copkt = copss_tlv_bytes2pkt(olddata, &data, &datalen);
                        //print pkt
                        printf("\n******content Received: %.*s\n\n", copkt->contlen, copkt->content);
                        free_copss_packet(copkt);
                        break;
                    default:
                        //                        puts("default");
                        break;
                }
            }
        }
    }
    return NULL;
}

static void _copss_pub(char *argv) {
    printf("usage: %s <URI,URI,......> <content>\n"
            "%% %s /sports/football Hello\n"
            "%% %s /sports/football,/news/bbc Hello COPSS\n",
            argv, argv, argv);
}

int copss_pub(int argc, char **argv) {
    if (!copss_client_running) {
        puts("Warning:please start copss client first");
        return -1;
    }
    if (argc < 3) {
        _copss_pub(argv[0]);
        return -1;
    }

    //get CN
    struct copss_contentName_s *cn = NULL;
    char *delim = ",";
    char *p = strtok(argv[1], delim);
    while (p) {
        struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(p, suite, NULL, NULL);
        struct copss_contentName_s *cn_temp = (struct copss_contentName_s *) ccnl_calloc(1, sizeof (struct copss_contentName_s));
        if (!cn_temp) {
            printf("no enough memory for content Name");
            free_prefix(prefix);
            prefix = NULL;
            break;
        }
        cn_temp->prefix = prefix;
        DBL_LINKED_LIST_ADD(cn, cn_temp);
        p = strtok(NULL, delim);
    }

    //get content
    char *content;
    int arg_len;
    memset(buf, ' ', BUFFER_SIZE);
    char *buf_ptr = buf;
    for (int i = 2; (i < argc) && (buf_ptr < (buf + BUFFER_SIZE)); i++) {
        arg_len = strlen(argv[i]);
        if ((buf_ptr + arg_len) > (buf + BUFFER_SIZE)) {
            arg_len = (buf + BUFFER_SIZE) - buf_ptr;
        }
        strncpy(buf_ptr, argv[i], arg_len);
        buf_ptr += arg_len + 1;
    }
    *buf_ptr = '\0';
    content = buf;

    printf("\n******content: [%s] is going to be sent\n\n", content);
    arg_len = strlen(content);

    int offs = CCNL_MAX_PACKET_SIZE;
    arg_len = copss_tlv_prependMulticast(cn, (unsigned char*) content, arg_len, &offs, _out);
    free_copss_contentName(cn);
    unsigned char *olddata;
    unsigned char *data = olddata = _out + offs;

    int len, oldlen = arg_len;
    unsigned typ;
    if (ccnl_ndntlv_dehead(&data, &arg_len, (int*) &typ, &len) ||
            typ != COPSS_TLV_Multicast) {
        return -1;
    }
    //    ipv6_addr_t remote_ipv6;
    //    memcpy(remote_ipv6.u8, &copss.server.addr.ipv6, sizeof (copss.server.addr.ipv6));
    //    char str_addr[MAX_ADDR_PRINT_LEN * 3];
    //    ipv6_addr_to_str(str_addr, &remote_ipv6, sizeof (str_addr));
    //    printf("try to send: send %u byte to %s:%d\n", (unsigned) oldlen, str_addr, copss.server.port);
    int res;
    if ((res = sock_udp_send(&sock, olddata, oldlen, &copss.server)) < 0) {
        printf("could not send res=%d\n", res);
    } else {
        printf("\n******copss client pub content done\n\n");
    }
    return 0;
}

static void _copss_sub_or_unsub(char *argv) {
    printf("usage: %s sub/unsub <URI,URI,......>\n"
            "%% %s sub /sports/football\n"
            "%% %s sub /sports/football,/news/bbc\n"
            "%% %s unsub /sports/football\n",
            argv, argv, argv, argv);
}

int copss_sub_or_unsub(int argc, char **argv) {
    if (!copss_client_running) {
        puts("error: please start copss client first");
        return -1;
    }
    if (argc != 3) {
        _copss_sub_or_unsub(argv[0]);
        return -1;
    }

    struct copss_contentName_s *cn = NULL;

    if ((strncmp(argv[1], "sub", 3) == 0) || ((strncmp(argv[1], "unsub", 3) == 0))) {
        //get CN
        char *delim = ",";
        char *p = strtok(argv[2], delim);
        while (p) {
            struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(p, suite, NULL, NULL);
            struct copss_contentName_s *cn_temp = (struct copss_contentName_s *) ccnl_calloc(1, sizeof (struct copss_contentName_s));
            if (!cn_temp) {
                printf("no enough memory for content Name");
                free_prefix(prefix);
                prefix = NULL;
                break;
            }
            cn_temp->prefix = prefix;
            DBL_LINKED_LIST_ADD(cn, cn_temp);
            p = strtok(NULL, delim);
        }
        int arg_len;
        int offs = CCNL_MAX_PACKET_SIZE;
        enum copss_control_type_e type;
        int version = 0;
        int ttl = -1;
        type = ST_Change;
        if ((strncmp(argv[1], "sub", 3) == 0)) {
            arg_len = copss_tlv_prependControl(type, cn, NULL, &version, &ttl, &offs, _out);
        } else {
            arg_len = copss_tlv_prependControl(type, NULL, cn, &version, &ttl, &offs, _out);

        }
        free_copss_contentName(cn);
        unsigned char *olddata;
        unsigned char *data = olddata = _out + offs;

        int len, oldlen = arg_len;
        unsigned typ;
        if (ccnl_ndntlv_dehead(&data, &arg_len, (int*) &typ, &len) ||
                typ != COPSS_TLV_Control) {
            return -1;
        }

        //        ipv6_addr_t remote_ipv6;
        //        memcpy(remote_ipv6.u8, &copss.server.addr.ipv6, sizeof (copss.server.addr.ipv6));
        //        char str_addr[MAX_ADDR_PRINT_LEN * 3];
        //        ipv6_addr_to_str(str_addr, &remote_ipv6, sizeof (str_addr));
        //        printf("try to send: send %u byte to %s:%d\n", (unsigned) oldlen, str_addr, copss.server.port);

        int res;
        if ((res = sock_udp_send(&sock, olddata, oldlen, &copss.server)) < 0) {
            printf("could not send res=%d\n", res);
        } else {
            printf("\n******copss client send Control done\n\n");
        }
        return 0;
    } else {
        _copss_sub_or_unsub(argv[0]);
        return -1;
    }
}

int copss_client(int argc, char **argv) {
    if (argc != 1 && argc != 2) {
        puts("Usage: copss_client [port]");
        return -1;
    }
    if (copss_client_running) {
        puts("copss client already running");
        return -1;
    }
    if (argc == 1)
        argv[1] = "88888"; //random port


    if ((copss_client_running == false) &&
            thread_create(server_stack, sizeof (server_stack), THREAD_PRIORITY_MAIN - 1,
            THREAD_CREATE_STACKTEST, _client_server, argv[1], "COPSS Client")
            <= KERNEL_PID_UNDEF) {
        return -1;
    }

    return 0;
}

