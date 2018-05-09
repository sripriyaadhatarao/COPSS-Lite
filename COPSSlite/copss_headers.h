/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   copss-headers.h
 * Author: Welle
 *
 * Created on November 19, 2016, 12:22 PM
 */

#ifndef COPSS_HEADERS_H
#define COPSS_HEADERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "copss_core.h"   
    //---------------------------------------------------------------------------------------------------------------------------------------
    /* copss-pkt-tlv.c */

    struct copss_pkt_s* copss_tlv_bytes2pkt(unsigned char *start, unsigned char **data, int *datalen);
    int copss_tlv_prependContentName(struct copss_contentName_s *contentName, int *offset, unsigned char *buf);
    int copss_tlv_prependControl(enum copss_control_type_e control_type, struct copss_contentName_s *contentName_add,
            struct copss_contentName_s *contentName_remove, int *version, int *ttl,
            int *offset, unsigned char *buf);
    int copss_tlv_prependMulticast(struct copss_contentName_s *contentName, unsigned char *payload, int paylen, int *offset, unsigned char *buf);

    //copss_core.c
    int print_copss_pkt(struct copss_pkt_s *pkt);
    void free_copss_contentName(struct copss_contentName_s *cn);
    void free_copss_packet(struct copss_pkt_s *pkt);
    void free_copss_contentName_table(struct copss_contentNames_table_s *cnt);

#ifdef __cplusplus
}
#endif

#endif /* COPSS_HEADERS_H */


