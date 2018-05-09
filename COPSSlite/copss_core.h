/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   copss-core.h
 * Author: Welle
 *
 * Created on November 17, 2016, 5:46 PM
 */

#ifndef COPSS_CORE_H
#define COPSS_CORE_H

#ifdef __cplusplus
extern "C" {
#endif
#include "ccnl-core.h"
#include "net/sock.h"
#include "net/sock/udp.h"

    enum copss_control_type_e {
        None,
        ST_Change,
        FIB_Change
    };

    struct copss_s {
        int id;
        uint16_t port;
        sock_udp_ep_t server;
        struct copss_face_s *faces;
        struct copss_RP_s *RPs;
        struct copss_contentNames_table_s *CD2RP_table;
        struct ccnl_content_s *contents; //, *contentsend;
        struct ccnl_buf_s *nonces;
        char is_has_RP_flag;
        struct sockaddr_ll copss_ccnl_riot_RP2f_conn;
        int _copss_ccnl_riot_ifndx;
        char halt_flag;
    };

    struct copss_contentNames_table_s {
        struct copss_contentNames_table_s *next, *prev;
        struct ccnl_prefix_s * prefix;
        struct copss_contentName_s * content_name;
    };

    struct copss_RP_s {
        struct copss_RP_s *next, *prev;
        struct ccnl_prefix_s* rp_name;
    };

    struct copss_face_s {
        struct copss_face_s *next, *prev;
        struct sockaddr_ll copss_ccnl_riot_f_conn;
        struct copss_contentName_s *sub_conentName;
        sock_udp_ep_t peer;
        char is_router_flag;
        int flags; 
    };

    //  a list of prefixs(a list of ContentNames in CCNx))

    struct copss_contentName_s {
        struct copss_contentName_s *next, *prev;
        struct ccnl_prefix_s *prefix;
    };

    struct copss_pkt_s {
        struct ccnl_buf_s *buf; // the packet's bytes
        enum copss_control_type_e control_type;
        struct copss_contentName_s *content_name_add, *content_name_remove; // prefix/name
        unsigned char *content; // pointer into the data buffer
        int contlen;
        unsigned int type; // suite-specific value (outermost type)
        unsigned int flags;
        int version;
        int ttl;
    };

    struct copss_control_s {
        enum copss_control_type_e control_type;
        struct copss_pkt_s *pkt;
        struct copss_contentName_s *content_name_add, *content_name_remove;
    };

    struct copss_multicast_s {
        struct copss_contentName_s *content_name;
        struct copss_pkt_s *pkt;
    };

#ifdef __cplusplus
}
#endif

#endif /* COPSS_CORE_H */


