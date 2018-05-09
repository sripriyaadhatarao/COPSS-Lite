/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#include <stdio.h>
#include "ccn-lite-riot.h"
#include "ccnl-pkt-ndntlv.h"
#include "ccnl-ext.h"
#include "net/gnrc/netif.h"

#include "net/ipv6/addr.h"
#include "net/packet.h"


#include "copss_pkt_tlv.h"
#include "copss_headers.h"
#include "copss_riot_ccn_lite.h"
#include "copss_util.h"

static const char *_default_content = "Start the RIOT!";
static unsigned char _out[CCNL_MAX_PACKET_SIZE];

/* usage for open command
static void _test_send_usage(void)
{
puts("sub");
}
    
int _test_sub_copss_control_pkt(int argc, char **argv)
{	printf("size: %d \n",argc);
	



    return 0;

}
 */

static void _test_fib_usage(char *argv) {
    printf("usage: %s <URI> <ipv6-addr> <port>\n"
            "%% %s /peter/schmerzl fe80::207e:b9ff:fe04:9ff0 9696\n",
            argv, argv);
}

int test_fib(int argc, char **argv) {

    if (argc != 4) {
        _test_fib_usage(argv[0]);
        return -1;
    }

    ipv6_addr_t remote_ipv6;
    if (ipv6_addr_from_str(&remote_ipv6, argv[2]) == NULL) {
        puts("Error: unable to parse destination address");
        _test_fib_usage(argv[0]);
        return -1;
    }

    sock_udp_ep_t remote = {.family = AF_INET6};
    remote.port = atoi(argv[3]);
    memcpy(remote.addr.ipv6, remote_ipv6.u8, sizeof (remote_ipv6.u8));


    int suite = CCNL_SUITE_NDNTLV;
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(argv[1], suite, NULL, NULL);
    if (!prefix) {
        puts("Error: prefix could not be created!");
        _test_fib_usage(argv[0]);
        return -1;
    }
    printf("prefix: %s \n", ccnl_prefix_to_path(prefix));

    copss_add_fib(&copss, prefix, &remote);
    return 0;
}

static void _link_usage(char *argv) {
    printf("usage: %s <ipv6-addr> <port>\n"
            "%% %s fe80::207e:b9ff:fe04:9ff0 9696\n",
            argv, argv);
}

int test_link(int argc, char **argv) {

    if (argc != 3) {
        _link_usage(argv[0]);
        return -1;
    }

    ipv6_addr_t remote_ipv6;
    if (ipv6_addr_from_str(&remote_ipv6, argv[1]) == NULL) {
        puts("Error: unable to parse destination address");
        _link_usage(argv[0]);
        return -1;
    }

    sock_udp_ep_t remote = {.family = AF_INET6};
    remote.port = atoi(argv[2]);
    memcpy(remote.addr.ipv6, remote_ipv6.u8, sizeof (remote_ipv6.u8));

    int flag = copss_link(&copss, &remote,'1');

    if (flag == -1) {
        puts("face already exists");
        return -1;
    }
    if (flag == -2) {
        puts("no memory for face");
        return -1;
    }

    return 0;
}

int _test_sub_copss_control_pkt(int argc, char **argv) {

    int arg_len;
    int offs = CCNL_MAX_PACKET_SIZE;

    int suite = CCNL_SUITE_NDNTLV;

    enum copss_control_type_e type;
    int version = 0;
    int ttl = -1;
    type = 0;

    char uri1[] = "hello/test";
    struct ccnl_prefix_s *prefix1 = ccnl_URItoPrefix(&uri1[0], suite, NULL, NULL);
    if (!prefix1) {
        puts("Error: prefix could not be created!");
        return -1;
    }
    printf("prefix1: %s \n", ccnl_prefix_to_path(prefix1));
    char uri2[] = "i/am/ok";
    struct ccnl_prefix_s *prefix2 = ccnl_URItoPrefix(&uri2[0], suite, NULL, NULL);
    printf("prefix2: %s \n", ccnl_prefix_to_path(prefix2));
    char uri3[] = "hh/amh/oh/k";
    struct ccnl_prefix_s *prefix3 = ccnl_URItoPrefix(&uri3[0], suite, NULL, NULL);
    printf("prefix3: %s \n", ccnl_prefix_to_path(prefix3));
    struct copss_contentName_s *cn = (struct copss_contentName_s *) ccnl_calloc(1, sizeof (struct copss_contentName_s));
    cn->prefix = prefix1;
    struct copss_contentName_s *cn2 = (struct copss_contentName_s *) ccnl_calloc(1, sizeof (struct copss_contentName_s));
    cn2->prefix = prefix2;
    struct copss_contentName_s *cn3 = (struct copss_contentName_s *) ccnl_calloc(1, sizeof (struct copss_contentName_s));
    cn3->prefix = prefix3;

    DBL_LINKED_LIST_ADD(cn, cn2);
    DBL_LINKED_LIST_ADD(cn, cn3);

    arg_len = copss_tlv_prependControl(type, cn, cn, &version, &ttl, &offs, _out);
    unsigned char *olddata;
    unsigned char *data = olddata = _out + offs;

    int len;
    unsigned typ;
    if (ccnl_ndntlv_dehead(&data, &arg_len, (int*) &typ, &len) ||
            typ != COPSS_TLV_Control) {
        return -1;
    }
    printf("type: %x \n", typ);
    struct copss_pkt_s *pkt = copss_tlv_bytes2pkt(olddata, &data, &arg_len);

    //print pkt
    printf("argc %d  argv %s \n addprefix1: %s \n addprefix2: %s \n addprefix3: %s \n contentData: %s \n "
            "\n rmtype: %d \n rmprefix1: %s \n rmprefix2: %s \n rmversion:%d \n rmttl:%d \n",
            argc, argv[0], ccnl_prefix_to_path(pkt->content_name_add->prefix), ccnl_prefix_to_path(pkt->content_name_add->next->prefix),
            ccnl_prefix_to_path(pkt->content_name_add->next->next->prefix), pkt->content,
            pkt->control_type, ccnl_prefix_to_path(pkt->content_name_remove->prefix), ccnl_prefix_to_path(pkt->content_name_remove->next->prefix),
            pkt->version, pkt->ttl);

    return 0;
}

int _test_pub_copss_multicast_pkt(int argc, char **argv) {

    char *body = (char*) _default_content;
    int arg_len = strlen(_default_content) + 1;
    int offs = CCNL_MAX_PACKET_SIZE;

    int suite = CCNL_SUITE_NDNTLV;

    char uri1[] = "hello/test";
    struct ccnl_prefix_s *prefix1 = ccnl_URItoPrefix(&uri1[0], suite, NULL, NULL);
    if (!prefix1) {
        puts("Error: prefix could not be created!");
        return -1;
    }
    printf("prefix1: %s \n", ccnl_prefix_to_path(prefix1));
    char uri2[] = "i/am/ok";
    struct ccnl_prefix_s *prefix2 = ccnl_URItoPrefix(&uri2[0], suite, NULL, NULL);
    printf("prefix2: %s \n", ccnl_prefix_to_path(prefix2));
    struct copss_contentName_s *cn = (struct copss_contentName_s *) ccnl_calloc(1, sizeof (struct copss_contentName_s));
    cn->prefix = prefix1;
    struct copss_contentName_s *cn2 = (struct copss_contentName_s *) ccnl_calloc(1, sizeof (struct copss_contentName_s));
    cn2->prefix = prefix2;

    DBL_LINKED_LIST_ADD(cn, cn2);
    arg_len = copss_tlv_prependMulticast(cn, (unsigned char*) body, arg_len, &offs, _out);
    unsigned char *olddata;
    unsigned char *data = olddata = _out + offs;

    int len;
    unsigned typ;
    if (ccnl_ndntlv_dehead(&data, &arg_len, (int*) &typ, &len) ||
            typ != COPSS_TLV_Multicast) {
        return -1;
    }
    printf("type: %x \n", typ);
    struct copss_pkt_s *pkt = copss_tlv_bytes2pkt(olddata, &data, &arg_len);

    //print pkt
    printf("argc %d  argv %s \n prefix1: %s \n prefix2: %s \n contentData: %s \n",
            argc, argv[0], ccnl_prefix_to_path(pkt->content_name_add->prefix), ccnl_prefix_to_path(pkt->content_name_add->next->prefix), pkt->content);

    return 0;
}

static void _set_RP_usage(char *argv) {
    printf("usage: %s <RP_name(without \"/\" inside >\n"
            "%% %s RP\n",
            argv, argv);
}

int set_RP(int argc, char **argv) {

    if (argc != 2) {
        _set_RP_usage(argv[0]);
        return -1;
    }
    if (strstr(argv[1], "/")) {
        _set_RP_usage(argv[0]);
        return -1;
    }
    int i = copss_set_RP(&copss, argv[1]);
    if (i != 0)
        puts("Error: RP could not be created!");
    if (i == -1)
        puts("this RP already exists!");
    return 0;
}

int test_incr_mac_addr(int argc, char **argv) {

    /* initialize address with 0xFF for broadcast */
    uint8_t incr_mac_addr[MAX_ADDR_LEN];
    memset(incr_mac_addr, UINT8_MAX, MAX_ADDR_LEN);
    char str_addr[MAX_ADDR_PRINT_LEN];
    for (int i = 0; i < 1; i++) {
        size_t addr_len = get_incr_mac_addr(incr_mac_addr, sizeof (incr_mac_addr));
        gnrc_netif_addr_to_str(str_addr, sizeof (str_addr), incr_mac_addr, addr_len);
        printf("argc %d  argv %s [%s]\n", argc, argv[0], str_addr);
    }
    return 0;
}


