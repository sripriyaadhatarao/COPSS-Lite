#include <stdio.h>
#include <string.h>
#include "net/gnrc/netif.h"

#include "ccn-lite-riot.h"
#include "ccnl-ext.h"

#include "copss_util.h"
#include "copss_core.h"

void copss_incr_mac_addr(unsigned char *pAddr, unsigned int *i, unsigned int *p, unsigned int max_val) {
    unsigned int incr_index, parent_index;
    incr_index = *i;
    parent_index = *p;
    if (pAddr[incr_index] == max_val) {
        if (pAddr[parent_index] == max_val) {
            pAddr[parent_index] = 0;
            parent_index--;
        }
        pAddr[parent_index] += 1;
        pAddr[incr_index] = 0;
    } else {
        pAddr[incr_index]++;
    }

    *i = incr_index;
    *p = parent_index;
}

int get_incr_mac_addr(uint8_t *incr_mac_addr, int incr_mac_addr_len) {
    static unsigned int i = 0;
    unsigned int mac_incr_idx = MAC_ADDR_BYTES - 1, mac_parent_idx = MAC_ADDR_BYTES - 2;
    static unsigned char mac_addr[MAC_ADDR_BYTES];
    char addr_str[MAX_ADDR_PRINT_LEN];
    unsigned char init_mac_addr[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    if (!i) {
        memcpy(&mac_addr[0], &init_mac_addr, sizeof (unsigned char)*MAC_ADDR_BYTES);
        i = 1;
    }
    copss_incr_mac_addr(mac_addr, &mac_incr_idx, &mac_parent_idx, MAX_U8_VAL);
    sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    size_t addr_len = gnrc_netif_addr_from_str(incr_mac_addr, incr_mac_addr_len, addr_str);
    if (addr_len == 0) {
        printf("Error: %s is not a valid link layer address\n", addr_str);
        return -1;
    }

    return addr_len;
}

struct copss_contentNames_table_s* splitContentNames(struct copss_contentNames_table_s *CD2RP_table, struct copss_contentName_s *cnames) {

    struct copss_contentNames_table_s * res = NULL;
    struct copss_contentNames_table_s * res1 = NULL;

    for (struct copss_contentName_s* cn = cnames; cn; cn = cn->next) {
        for (struct copss_contentNames_table_s *cd2rp = CD2RP_table; cd2rp; cd2rp = cd2rp->next) {
            if (!cd2rp->content_name)
                continue;
            bool match = false, ematch = false;
            for (struct copss_contentName_s* cd2rp_cn = cd2rp->content_name; cd2rp_cn; cd2rp_cn = cd2rp_cn->next) {
                int rc = 0;
                rc = ccnl_prefix_cmp(cd2rp_cn->prefix, NULL, cn->prefix, CMP_LONGEST);
                // printf("\n splitCNs, rc=%d/%d\n", rc, cd2rp_cn->prefix->compcnt);
                if (rc < cd2rp_cn->prefix->compcnt)
                    continue;
                match = true;
                struct copss_contentName_s * m = (struct copss_contentName_s *) calloc(1, sizeof (struct copss_contentName_s));
                m->prefix = ccnl_prefix_dup(cn->prefix);

                for (struct copss_contentNames_table_s *res1_temp = res1; res1_temp; res1_temp = res1_temp->next) {
                    if (ccnl_prefix_cmp(cd2rp->prefix, NULL, res1->prefix, CMP_EXACT) == 0) {
                        DBL_LINKED_LIST_ADD(res1_temp->content_name, m);
                        ematch = true;
                        break;
                    }
                }
                if (ematch)
                    break;
                res1 = (struct copss_contentNames_table_s *) calloc(1, sizeof (struct copss_contentNames_table_s));
                res1->content_name = NULL;
                res1->prefix = NULL;
                DBL_LINKED_LIST_ADD(res1->content_name, m);
                break;
            }
            if (!match)
                continue;
            if (ematch)
                continue;
            if (!res1->prefix)
                res1->prefix = ccnl_prefix_dup(cd2rp->prefix);
            DBL_LINKED_LIST_ADD(res, res1);
            break;
        }
    }
    if (!res) {
//        printf("\nsplitContentNames. no matching\n");
        return NULL;
    }
    //printf("\nsplitContentNames.  matched\n");
    return res;
}
