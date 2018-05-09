/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#include <stdio.h>

#include "ccn-lite-riot.h"
#include "ccnl-pkt-ndntlv.h"
#include "ccnl-ext.h"
#include "copss_util.h"
#include "copss_headers.h"
#include "net/packet.h"
#include "net/ipv6/addr.h"
#include "copss_riot_ccn_lite.h"
static char* str;
//static gnrc_netreg_entry_t copss_ccnl_if;

//print pkt

int print_copss_pkt(struct copss_pkt_s *pkt) {

    puts("\n Print_copss_pkt");
    if (!pkt)
        return -1;
    switch (pkt->control_type) {
        case ST_Change:
            printf("Control type:[%s]\t", "ST_change");
            break;
        case FIB_Change:
            printf("Control type: [%s]\t", "FIB_change");
            break;
        default:
            break;
    }

    for (struct copss_contentName_s *contentName = pkt->content_name_add; contentName && contentName->prefix; contentName = contentName->next) {
        struct ccnl_prefix_s *p = contentName->prefix;
        str = ccnl_prefix_to_path(p);
        printf("Add_prefix: [%s] \t", str);
        free(str);
        str = NULL;
    }
    for (struct copss_contentName_s *contentName = pkt->content_name_remove; contentName && contentName->prefix; contentName = contentName->next) {
        struct ccnl_prefix_s *p = contentName->prefix;
        str = ccnl_prefix_to_path(p);
        printf("Remove_prefix: [%s] \t", str);
        free(str);
        str = NULL;
    }
    if (pkt->control_type == None)
        printf("content: [%.*s]\n", pkt->contlen, pkt->content);
    else
        printf("version:[%d] \t TTL:[%d] \t", pkt->version, pkt->ttl);

    return 0;
}

void
free_copss_contentName(struct copss_contentName_s *cn) {

    if (cn) {
        struct copss_contentName_s *cn_temp = cn;
        while (cn_temp) {
            if (cn_temp->prefix) {
                //puts("try to free_prefix(cn_temp->prefix); ");
                free_prefix(cn_temp->prefix);
                cn_temp->prefix = NULL;
                //puts("free_prefix(cn_temp->prefix); done");
            }
            struct copss_contentName_s *cn_temp1 = cn_temp;
            cn_temp = cn_temp->next;
            ccnl_free(cn_temp1);
            cn_temp1 = NULL;
            //puts("ccnl_free(cn_temp1);; done");
        }
    }
}

void
free_copss_contentName_table(struct copss_contentNames_table_s *cnt) {

    if (cnt) {
        struct copss_contentNames_table_s *cnt_temp = cnt;
        while (cnt_temp) {

            if (cnt_temp->prefix) {
                free_prefix(cnt_temp->prefix);
                cnt_temp->prefix = NULL;
            }
            if (cnt_temp->content_name) {
                free_copss_contentName(cnt_temp->content_name);
                cnt_temp->content_name = NULL;
            }
            struct copss_contentNames_table_s *cnt_temp1 = cnt_temp;
            cnt_temp = cnt_temp->next;
            ccnl_free(cnt_temp1);
            cnt_temp1 = NULL;
        }
    }
}

void
free_copss_packet(struct copss_pkt_s * pkt) {
    if (pkt) {
        if (pkt->content_name_add) {
            // puts("free_copss_packet:try free pkt->content_name_add");
            free_copss_contentName(pkt->content_name_add);
        }
        if (pkt->content_name_remove) {
            // puts("free_copss_packet:try free pkt->content_name_remove");
            free_copss_contentName(pkt->content_name_remove);
        }
        //puts("free_copss_packet:free pkt->content_name_add content_name_remove done");
        ccnl_free(pkt->buf);
        pkt->buf = NULL;
        //ccnl_free(pkt->content);
        ccnl_free(pkt);
        pkt = NULL;
    }
}

int copss_link(struct copss_s *copss, sock_udp_ep_t *remote, char is_router_flag) {
    // add a face in COPSS
    struct copss_face_s *cf;
    for (cf = copss->faces; cf; cf = cf->next) {
        if (cf->peer.port == remote->port) {
            if (memcmp(&(cf->peer.addr.ipv6), &(remote->addr.ipv6), sizeof (remote->addr.ipv6)) == 0) {
                puts("face already exist");
                return -1;
            }
        }
    }

    cf = (struct copss_face_s *) ccnl_calloc(1, sizeof (struct copss_face_s));
    if (!cf) {
        puts("Error: no memory for face");
        return -2;
    }
    cf->peer = *remote;

    cf->copss_ccnl_riot_f_conn.sll_family = AF_PACKET;

    uint8_t incr_mac_addr[MAX_ADDR_LEN];
    memset(incr_mac_addr, UINT8_MAX, MAX_ADDR_LEN);
    //    char str_addr[MAX_ADDR_PRINT_LEN];
    size_t addr_len;
    if ((addr_len = get_incr_mac_addr(incr_mac_addr, sizeof (incr_mac_addr))) <= 0) {
        puts("Error: get mac addr error");
        return -1;
    }
    //    gnrc_netif_addr_to_str(str_addr, sizeof (str_addr), incr_mac_addr, addr_len);
    //    printf("[%s]\n", str_addr);
    cf->copss_ccnl_riot_f_conn.sll_halen = addr_len;
    memcpy(&(cf->copss_ccnl_riot_f_conn.sll_addr), incr_mac_addr, addr_len);
    cf->is_router_flag = is_router_flag;
    // cf->copss_ccnl_riot_f_id.sll_protocol = htons(ETHERTYPE_NDN);
    DBL_LINKED_LIST_ADD(copss->faces, cf);

    sockunion sun;
    sun.linklayer = cf->copss_ccnl_riot_f_conn;

    //add ccnl-riot face
    struct ccnl_face_s * ccnl_riot_face = ccnl_get_face_or_create(&ccnl_relay, copss->_copss_ccnl_riot_ifndx, &sun.sa, sizeof (sun.linklayer));
    ccnl_riot_face->flags |= CCNL_FACE_FLAGS_STATIC;
    return 0;
}

int copss_set_RP(struct copss_s *copss, char* RP_name) {

    if (strstr(RP_name, "/")) {
        return -5;
    }
    int suite = CCNL_SUITE_NDNTLV;
    struct ccnl_prefix_s *RP_prefix = ccnl_URItoPrefix(RP_name, suite, NULL, NULL);
    if (!RP_prefix) {
        puts("RP_prefix could not be created!");
        return -2;
    }
    /*
   str = ccnl_prefix_to_path(RP_prefix);
   
       printf("prefix: %s \n", str);
       free(str);
       str = NULL;
     */
    struct copss_RP_s* RP;
    if (copss->is_has_RP_flag == 1) {
        //check if already exists
        for (RP = copss->RPs; RP; RP = RP->next) {
            if (ccnl_prefix_cmp(RP->rp_name, NULL, RP_prefix, CMP_EXACT) == 0) {
                puts("Rendezvous point already exists");
                return -1;
            }
        }
    }

    //set RP flag and add into copss
    copss->is_has_RP_flag = '1';
    RP = (struct copss_RP_s *) ccnl_calloc(1, sizeof (struct copss_RP_s));
    if (!RP) {
        puts("no memory for RP");
        return -2;
    }
    RP->rp_name = RP_prefix;
    DBL_LINKED_LIST_ADD(copss->RPs, RP);

    //set RP conn in copss
    copss->copss_ccnl_riot_RP2f_conn.sll_family = AF_PACKET;
    uint8_t incr_mac_addr[MAX_ADDR_LEN];
    memset(incr_mac_addr, UINT8_MAX, MAX_ADDR_LEN);
    char str_addr[MAX_ADDR_PRINT_LEN];
    size_t addr_len;
    if ((addr_len = get_incr_mac_addr(incr_mac_addr, sizeof (incr_mac_addr))) <= 0) {
        puts("Error: RP get mac addr error");
        return -3;
    }
    //    gnrc_netif_addr_to_str(str_addr, sizeof (str_addr), incr_mac_addr, addr_len);
    //    printf("[%s]\n", str_addr);
    copss->copss_ccnl_riot_RP2f_conn.sll_halen = addr_len;
    memcpy(&(copss->copss_ccnl_riot_RP2f_conn.sll_addr), incr_mac_addr, addr_len);

    //register this RP in ccnl
    sockunion sun;
    sun.linklayer = copss->copss_ccnl_riot_RP2f_conn;
    //add or get a ccnl-riot face
    struct ccnl_face_s *fibface = ccnl_get_face_or_create(&ccnl_relay, copss->_copss_ccnl_riot_ifndx, &sun.sa, sizeof (sun.linklayer));

    fibface->flags |= CCNL_FACE_FLAGS_STATIC;

    if (ccnl_fib_add_entry(&ccnl_relay, RP_prefix, fibface) != 0) {
        str = ccnl_prefix_to_path(RP_prefix);
        printf("Error adding (%s : %s) to the FIB\n", str, fibface->peer.linklayer.sll_addr);
        free(str);
        str = NULL;
        return -4;
    }
    gnrc_netif_addr_to_str(str_addr, sizeof (str_addr), fibface->peer.linklayer.sll_addr, fibface->peer.linklayer.sll_halen);
    str = ccnl_prefix_to_path(RP_prefix);
    printf("RP: add (%s : %s) to the ccnl FIB\n", str, str_addr);
    free(str);
    str = NULL;
    return 0;
}

int copss_add_fib(struct copss_s *copss, struct ccnl_prefix_s *prefix, sock_udp_ep_t * remote) {
    //find suitable face in copss faces

    ipv6_addr_t remote_ipv6;
    memcpy(remote_ipv6.u8, remote->addr.ipv6, sizeof (remote->addr.ipv6));
    char str_addr[MAX_ADDR_PRINT_LEN * 3];

    struct copss_face_s *cf;
    for (cf = copss->faces; cf; cf = cf->next) {

        if (cf->peer.port == remote->port) {
            if (memcmp(&(cf->peer.addr.ipv6), &(remote->addr.ipv6), sizeof (remote->addr.ipv6)) == 0) {
                memcpy(remote_ipv6.u8, cf->peer.addr.ipv6, sizeof (cf->peer.addr.ipv6));
                ipv6_addr_to_str(str_addr, &remote_ipv6, sizeof (str_addr));
                printf("Find matched cf, addr: %s port: %d\n", str_addr, cf->peer.port);
                break;
            }
        }
    }

    if (!cf) {
        puts("Error: no suitable copss face found");
        // no suitable copss face found
        return -1;
    }

    struct ccnl_face_s *fibface;
    for (fibface = ccnl_relay.faces; fibface; fibface = fibface->next) {
        if (memcmp(fibface->peer.linklayer.sll_addr, cf->copss_ccnl_riot_f_conn.sll_addr, cf->copss_ccnl_riot_f_conn.sll_halen) == 0) {
            break;
        }
    }
    if (!fibface) {
        puts("no suitable ccn face found");
        // no suitable ccn face found
        return -1;
    }

    fibface->flags |= CCNL_FACE_FLAGS_STATIC;

    if (ccnl_fib_add_entry(&ccnl_relay, prefix, fibface) != 0) {
        str = ccnl_prefix_to_path(prefix);
        printf("Error adding (%s : %s) to the FIB\n", str, fibface->peer.linklayer.sll_addr);
        free(str);
        str = NULL;
        return -1;
    }

    gnrc_netif_addr_to_str(str_addr, sizeof (str_addr), fibface->peer.linklayer.sll_addr, fibface->peer.linklayer.sll_halen);
    str = ccnl_prefix_to_path(prefix);
    printf("add (%s : %s) to the ccnl FIB\n", str, str_addr);
    free(str);
    str = NULL;
    return 0;
}

/* add a pid as a if to CCN-lite's interfaces, set the nettype, and register a receiver */
int
copss_ccnl_open_if(kernel_pid_t if_pid, struct copss_s * copss) {

    assert(pid_is_valid(if_pid));

    if (ccnl_relay.ifcount >= CCNL_MAX_INTERFACES) {
        printf("cannot open more than %u interfaces for CCN-Lite\n", (unsigned) CCNL_MAX_INTERFACES);
        return -1;
    }

    /* get current interface from CCN-Lite's relay */
    struct ccnl_if_s *i;
    i = &ccnl_relay.ifs[ccnl_relay.ifcount];
    i->mtu = NDN_DEFAULT_MTU;
    i->fwdalli = 1;
    i->if_pid = if_pid;
    i->addr.sa.sa_family = AF_PACKET;
    copss->_copss_ccnl_riot_ifndx = ccnl_relay.ifcount;

    //    printf("ifcount: %d\n", copss->_copss_ccnl_riot_ifndx);
    /* advance interface counter in relay */
    ccnl_relay.ifcount++;
    /*
        copss_ccnl_if.demux_ctx = GNRC_NETREG_DEMUX_CTX_ALL;
        copss_ccnl_if.target.pid = if_pid;

        gnrc_netreg_register(GNRC_NETTYPE_CCN, &copss_ccnl_if);
     */
    return 0;
}

struct ccnl_face_s * copss_find_ccnlf_by_cf(struct ccnl_relay_s* ccnl, struct copss_face_s * cf) {

    if (!cf)
        return NULL;
    //find corresponding ccnl face 
    struct ccnl_face_s *ccnl_f;
    for (ccnl_f = ccnl->faces; ccnl_f; ccnl_f = ccnl_f->next) {
        if (ccnl_f->peer.linklayer.sll_halen == cf->copss_ccnl_riot_f_conn.sll_halen) {
            if (memcmp(ccnl_f->peer.linklayer.sll_addr, cf->copss_ccnl_riot_f_conn.sll_addr, cf->copss_ccnl_riot_f_conn.sll_halen) == 0) {
                /*
                                char str_addr[MAX_ADDR_PRINT_LEN];
                                gnrc_netif_addr_to_str(str_addr, sizeof (str_addr), ccnl_f->peer.linklayer.sll_addr, ccnl_f->peer.linklayer.sll_halen);
                                printf("copss_find_ccnlf_by_cf  : find a ccnl_f matching [%s]\n", str_addr);
                 */
                break;
            }
        }
    }
    if (!ccnl_f) {
        printf("copss_find_ccnlf_by_cf: No corresponding ccnl face found!\n");
        return NULL;
    }
    return ccnl_f;
}
