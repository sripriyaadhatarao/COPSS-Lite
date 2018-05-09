/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   ccn_lite_riot_copss.h
 * Author: Welle
 *
 * Created on November 22, 2016, 6:00 PM
 */

#ifndef CCN_LITE_RIOT_COPSS_H
#define CCN_LITE_RIOT_COPSS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel_types.h"
#include "copss_core.h"    
    //#define BUF_SIZE (100)
#define BUF_SIZE CCNL_MAX_PACKET_SIZE 
    /**
     * Struct holding copss information
     */
    extern struct copss_s copss;

    kernel_pid_t copss_start(uint16_t port);
    int get_incr_mac_addr(uint8_t *incr_mac_addr, int incr_mac_addr_len);
    struct copss_contentNames_table_s* splitContentNames(struct copss_contentNames_table_s *CD2RP_table, struct copss_contentName_s *cnames);

    int copss_link(struct copss_s *copss, sock_udp_ep_t *remote, char is_router_flag);
    int copss_add_fib(struct copss_s *copss, struct ccnl_prefix_s *prefix, sock_udp_ep_t *remote);
    /* add a pid as a if to CCN-lite's interfaces, set the nettype, and register a receiver */
    int copss_ccnl_open_if(kernel_pid_t if_pid, struct copss_s *copss);
    struct ccnl_face_s * copss_find_ccnlf_by_cf(struct ccnl_relay_s* ccnl, struct copss_face_s* cf);
    int copss_send_interest(struct ccnl_relay_s* relay, struct ccnl_prefix_s *prefix, struct ccnl_face_s *from, unsigned char *buf, size_t buf_len);
    int copss_set_RP(struct copss_s *copss, char* RP_name);
#ifdef __cplusplus
}
#endif

#endif /* CCN_LITE_RIOT_COPSS_H */


