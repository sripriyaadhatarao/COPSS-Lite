#include "random.h"
#include "sched.h"
#include "net/gnrc/netif.h"
#include "ccn-lite-riot.h"
#include "ccnl-pkt-ndntlv.h"
#include "copss_riot_ccn_lite.h"



/**
 * Maximum number of Interest retransmissions
 */
#define CCNL_INTEREST_RETRIES   (3)

#define MAX_ADDR_LEN            (8U)

static unsigned char _int_buf[BUF_SIZE];
static unsigned char _cont_buf[BUF_SIZE];

static struct ccnl_face_s *_intern_face_get(char *addr_str) {
    /* initialize address with 0xFF for broadcast */
    uint8_t relay_addr[MAX_ADDR_LEN];
    memset(relay_addr, UINT8_MAX, MAX_ADDR_LEN);
    size_t addr_len = gnrc_netif_addr_from_str(relay_addr, sizeof (relay_addr), addr_str);

    if (addr_len == 0) {
        printf("Error: %s is not a valid link layer address\n", addr_str);
        return NULL;
    }

    sockunion sun;
    sun.sa.sa_family = AF_PACKET;
    memcpy(&(sun.linklayer.sll_addr), relay_addr, addr_len);
    sun.linklayer.sll_halen = addr_len;
    sun.linklayer.sll_protocol = htons(ETHERTYPE_NDN);

    /* TODO: set correct interface instead of always 0 */
    struct ccnl_face_s *fibface = ccnl_get_face_or_create(&ccnl_relay, 0, &sun.sa, sizeof (sun.linklayer));

    return fibface;
}

static int _intern_fib_add(char *pfx, char *addr_str) {
    int suite = CCNL_SUITE_NDNTLV;
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(pfx, suite, NULL, 0);
    if (!prefix) {
        puts("Error: prefix could not be created!");
        return -1;
    }

    struct ccnl_face_s *fibface = _intern_face_get(addr_str);
    if (fibface == NULL) {
        return -1;
    }
    fibface->flags |= CCNL_FACE_FLAGS_STATIC;

    if (ccnl_fib_add_entry(&ccnl_relay, prefix, fibface) != 0) {
        printf("Error adding (%s : %s) to the FIB\n", pfx, addr_str);
        return -1;
    }

    return 0;
}

static void _interest_usage(char *arg) {
    printf("usage: %s <URI> [relay]\n%% %s /riot/peter/schmerzl                     (classic lookup)\n", arg, arg);
}

int copss_ccnl_interest(int argc, char **argv) {
    if (argc < 2) {
        _interest_usage(argv[0]);
        return -1;
    }

    if (argc > 2) {
        if (_intern_fib_add(argv[1], argv[2]) < 0) {
            _interest_usage(argv[0]);
            return -1;
        }
    }

    memset(_int_buf, '\0', BUF_SIZE);
    memset(_cont_buf, '\0', BUF_SIZE);
    for (int cnt = 0; cnt < CCNL_INTEREST_RETRIES; cnt++) {
        gnrc_netreg_entry_t _ne =
                GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL,
                sched_active_pid);
        /* register for content chunks */
        gnrc_netreg_register(GNRC_NETTYPE_CCN_CHUNK, &_ne);

        struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(argv[1], CCNL_SUITE_NDNTLV, NULL, 0);

        if (!prefix) {
            // DEBUGMSG(ERROR, "prefix could not be created!\n");
            return -1;
        }
        
        struct ccnl_face_s * loopback_face = ccnl_get_face_or_create(&ccnl_relay, -1, NULL, 0);
        copss_send_interest(&ccnl_relay, prefix, loopback_face, _int_buf, BUF_SIZE);
        //  ccnl_send_interest(prefix, _int_buf, BUF_SIZE);
        if (ccnl_wait_for_chunk(_cont_buf, BUF_SIZE, 0) > 0) {
            gnrc_netreg_unregister(GNRC_NETTYPE_CCN_CHUNK, &_ne);
            printf("Content received: %s\n", _cont_buf);
            return 0;
        }
        ccnl_free(prefix);
        gnrc_netreg_unregister(GNRC_NETTYPE_CCN_CHUNK, &_ne);
    }
    printf("Timeout! No content received in response to the Interest for %s.\n", argv[1]);

    return -1;
}

