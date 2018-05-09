/*
 * Copyright (C) 2017 
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief       COPSS-lite
 *
 * @author      Haitao Wang
 *
 * @}
 */

#include <stdio.h>

#include "tlsf-malloc.h"
#include "msg.h"
#include "shell.h"
//net/gnrc/netif.h
#include "net/gnrc/netif.h"
#include "thread.h"
#include "kernel_types.h"
#include "net/ipv6/addr.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/ipv6/netif.h"

#include "ccn-lite-riot.h"
#include "copss_riot_ccn_lite.h"
#include "copss_headers.h"
/* main thread's message queue */
#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

/* 10kB buffer for the heap should be enough for everyone */
//#define TLSF_BUFFER     (10240 / sizeof(uint32_t))
#define TLSF_BUFFER     (10240 / sizeof(uint32_t))
static uint32_t _tlsf_heap[TLSF_BUFFER];

extern int copss_client(int argc, char **argv);

extern int _test_pub_copss_multicast_pkt(int argc, char **argv);
extern int _test_sub_copss_control_pkt(int argc, char **argv);
extern int test_incr_mac_addr(int argc, char **argv);

extern int test_link(int argc, char **argv);
extern int test_fib(int argc, char **argv);
extern int copss_ccnl_interest(int argc, char **argv);
extern int set_RP(int argc, char **argv);
extern int _ccnl_interest(int argc, char **argv);
extern int copss_sub_or_unsub(int argc, char **argv);
extern int sim_pub_copss_multicast_pkt(int argc, char **argv);
extern int sim_sub(int argc, char **argv);
extern int sim_ccnl_cont(int argc, char **argv);
extern int sim_ccnl_int(int argc, char **argv) ;
extern int sim_set_ccnl_cache(int argc, char **argv);

int main(void) {
    tlsf_create_with_pool(_tlsf_heap, sizeof (_tlsf_heap));
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    
    puts("\nCOPSS v2.0");
    puts("Please type help to get available commands\n");
    uint16_t port = 9696;
    copss_start(port);
    copss.server.family = AF_INET6;
    kernel_pid_t ifs[GNRC_NETIF_NUMOF];
    /* get the first IPv6 interface and prints its address */
    size_t numof = gnrc_netif_get(ifs);
    if (numof > 0) {
        gnrc_ipv6_netif_t *entry = gnrc_ipv6_netif_get(ifs[0]);
        for (int i = 0; i < GNRC_IPV6_NETIF_ADDR_NUMOF; i++) {
            if ((ipv6_addr_is_link_local(&entry->addrs[i].addr)) && !(entry->addrs[i].flags & GNRC_IPV6_NETIF_ADDR_FLAGS_NON_UNICAST)) {
                memcpy(copss.server.addr.ipv6, &entry->addrs[i].addr.u8, sizeof (entry->addrs[i].addr.u8));
                copss.server.port = copss.port;
                /*char ipv6_addr[IPV6_ADDR_MAX_STR_LEN];
                ipv6_addr_to_str(ipv6_addr, &entry->addrs[i].addr, IPV6_ADDR_MAX_STR_LEN);
                printf("My address is %s\n", ipv6_addr);*/
            }
        }
    }

    static const shell_command_t commands[] = {

        { "link", "link to a remote note", test_link},
        { "copss_fib", "add an FIB entry", test_fib},
        { "copss_RP", "set a RP", set_RP},
        { "copss_pub", "pub", _ccnl_interest},
        { "copss", "sub or unsub", copss_sub_or_unsub},
        { "copss_client", "start copss client", copss_client},
        { "sub", "send copss control sub pkt(test only)", _test_sub_copss_control_pkt},
        //{ "unsub", "send copss control unsub pkt", _test_unsub_copss_control_pkt},
        { "pub", "send copss multicast pkt(test only)", _test_pub_copss_multicast_pkt},
        { "simpub", "simuation for auto-publishing", sim_pub_copss_multicast_pkt},
		{ "simsuball", "simuation sub all prefix", sim_sub},
		{ "sim_ccnl_cont", "simuation generate content in ccnl_cont.data", sim_ccnl_cont},
		{ "sim_ccnl_int", "simuation generate ccnl int in ccnl_int.data", sim_ccnl_int},
		{ "sim_set_ccnl_cache", "simuation set ccnl cache", sim_set_ccnl_cache},
        { "copss_ccnl_int", "send a ccnl interest by copss redefined ccnlite(ccnl_int) function", copss_ccnl_interest},
        { "testmac", "get a incr mac address as a id", test_incr_mac_addr},
        {NULL, NULL, NULL}
    };

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}

