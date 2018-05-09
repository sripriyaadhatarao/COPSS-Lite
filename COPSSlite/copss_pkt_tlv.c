/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include "ccn-lite-riot.h"
#include "ccnl-pkt-ndntlv.h"
#include "copss_pkt_tlv.h"
#include "copss_core.h"
#include "ccnl-ext.h"


/**
 * @brief Debugging level
 */
extern int debug_level;
extern struct ccnl_prefix_s* ccnl_prefix_new(int suite, int cnt);
extern void free_copss_packet(struct copss_pkt_s *pkt);

struct copss_pkt_s*
copss_tlv_bytes2pkt(unsigned char *start, unsigned char **data, int *datalen) {
    struct copss_pkt_s *pkt;
    int oldpos_offset, len, i, j;
    unsigned int typ;

    DEBUGMSG(DEBUG, "copss_tlv_bytes2pkt len=%d\n", *datalen);
    pkt = (struct copss_pkt_s*) ccnl_calloc(1, sizeof (*pkt));
    if (!pkt)
        return NULL;
    oldpos_offset = *data - start;
    while (ccnl_ndntlv_dehead(data, datalen, (int*) &typ, &len) == 0) {
        unsigned char *cp = *data;
        int len2 = len;
        switch (typ) {
            case COPSS_TLV_ControlType:
                memcpy(&pkt->control_type, *data, len);
                break;

            case COPSS_TLV_ContentNameAdd:
                while (len2 > 0) {
                    struct ccnl_prefix_s *p = 0;
                    if (ccnl_ndntlv_dehead(&cp, &len2, (int*) &typ, &i))
                        goto Bail;
                    if (typ != NDN_TLV_Name)
                        goto Bail;

                    unsigned char *cp2 = cp;
                    int len3 = i;
                    if (p) {
                        DEBUGMSG(WARNING, " copss tlv: name already defined\n");
                        goto Bail;
                    }
                    p = ccnl_prefix_new(CCNL_SUITE_NDNTLV, CCNL_MAX_NAME_COMP);
                    if (!p)
                        goto Bail;
                    p->compcnt = 0;
                    struct copss_contentName_s *cn = (struct copss_contentName_s *) ccnl_calloc(1, sizeof (struct copss_contentName_s));
                    cn->prefix = p;
                    DBL_LINKED_LIST_ADD(pkt->content_name_add, cn);
                    p->nameptr = start + oldpos_offset;
                    while (len3 > 0) {
                        if (ccnl_ndntlv_dehead(&cp2, &len3, (int*) &typ, &j))
                            goto Bail;
                        if (typ == NDN_TLV_NameComponent &&
                                p->compcnt < CCNL_MAX_NAME_COMP) {
                            if (cp2[0] == NDN_Marker_SegmentNumber) {
                                p->chunknum = (int*) ccnl_malloc(sizeof (int));
                                // TODO: requires ccnl_ndntlv_includedNonNegInt which includes the length of the marker
                                // it is implemented for encode, the decode is not yet implemented
                                *p->chunknum = ccnl_ndntlv_nonNegInt(cp2 + 1, j - 1);
                            }
                            p->comp[p->compcnt] = cp2;
                            p->complen[p->compcnt] = j;
                            p->compcnt++;
                        } // else unknown type: skip
                        cp2 += j;
                        len3 -= j;
                    }
                    p->namelen = *data - p->nameptr;
                    cp += i;
                    len2 -= i;
                }
                break;
            case COPSS_TLV_ContentNameRemove:
                while (len2 > 0) {
                    struct ccnl_prefix_s *p = 0;
                    if (ccnl_ndntlv_dehead(&cp, &len2, (int*) &typ, &i))
                        goto Bail;
                    if (typ != NDN_TLV_Name)
                        goto Bail;
                    unsigned char *cp2 = cp;
                    int len3 = i;
                    if (p) {
                        DEBUGMSG(WARNING, " copss tlv: name already defined\n");
                        goto Bail;
                    }
                    p = ccnl_prefix_new(CCNL_SUITE_NDNTLV, CCNL_MAX_NAME_COMP);
                    if (!p)
                        goto Bail;
                    p->compcnt = 0;
                    struct copss_contentName_s *cn = (struct copss_contentName_s *) ccnl_calloc(1, sizeof (struct copss_contentName_s));
                    cn->prefix = p;
                    DBL_LINKED_LIST_ADD(pkt->content_name_remove, cn);
                    p->nameptr = start + oldpos_offset;
                    while (len3 > 0) {
                        if (ccnl_ndntlv_dehead(&cp2, &len3, (int*) &typ, &j))
                            goto Bail;
                        if (typ == NDN_TLV_NameComponent &&
                                p->compcnt < CCNL_MAX_NAME_COMP) {
                            if (cp2[0] == NDN_Marker_SegmentNumber) {
                                p->chunknum = (int*) ccnl_malloc(sizeof (int));
                                // TODO: requires ccnl_ndntlv_includedNonNegInt which includes the length of the marker
                                // it is implemented for encode, the decode is not yet implemented
                                *p->chunknum = ccnl_ndntlv_nonNegInt(cp2 + 1, j - 1);
                            }
                            p->comp[p->compcnt] = cp2;
                            p->complen[p->compcnt] = j;
                            p->compcnt++;
                        } // else unknown type: skip
                        cp2 += j;
                        len3 -= j;
                    }
                    p->namelen = *data - p->nameptr;
                    cp += i;
                    len2 -= i;
                }
                break;

            case COPSS_TLV_Version:
                memcpy(&pkt->version, *data, len);
                break;

            case COPSS_TLV_TTL:
                memcpy(&pkt->ttl, *data, len);
                break;

            case COPSS_TLV_ContentData:
                pkt->content = *data;
                pkt->contlen = len;
                break;

            default:
                break;
        }
        *data += len;
        *datalen -= len;
        oldpos_offset = *data - start;
    }
    if (*datalen > 0)
        goto Bail;

    pkt->buf = ccnl_buf_new(start, *data - start);
    if (!pkt->buf)
        goto Bail;
    // carefully rebase ptrs to new buf because of 64bit pointers:
    if (pkt->content)
        pkt->content = pkt->buf->data + (pkt->content - start);

    for (struct copss_contentName_s *contentName = pkt->content_name_add; contentName && contentName->prefix; contentName = contentName->next) {
        struct ccnl_prefix_s *p = contentName->prefix;
        for (i = 0; i < p->compcnt; i++)
            p->comp[i] = pkt->buf->data + (p->comp[i] - start);
        if (p->nameptr)
            p->nameptr = pkt->buf->data + (p->nameptr - start);
    }
    for (struct copss_contentName_s *contentName = pkt->content_name_remove; contentName && contentName->prefix; contentName = contentName->next) {
        struct ccnl_prefix_s *p = contentName->prefix;
        for (i = 0; i < p->compcnt; i++)
            p->comp[i] = pkt->buf->data + (p->comp[i] - start);
        if (p->nameptr)
            p->nameptr = pkt->buf->data + (p->nameptr - start);
    }
    return pkt;
Bail:
    free_copss_packet(pkt);
    return NULL;
}

// ----------------------------------------------------------------------
// COPSS packet composition

int
copss_tlv_prependContentName(struct copss_contentName_s **contentName,
        int *offset, unsigned char *buf) {
    struct copss_contentName_s *cn;
    for (cn = *contentName; cn; cn = cn->next) {
        if (ccnl_ndntlv_prependName(cn->prefix, offset, buf))
            return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------

int
copss_tlv_prependControl(enum copss_control_type_e control_type, struct copss_contentName_s *contentName_add,
        struct copss_contentName_s *contentName_remove, int *version, int *ttl,
        int *offset, unsigned char *buf) {
    int oldoffset = *offset;
    if (ttl && ccnl_ndntlv_prependBlob(COPSS_TLV_TTL, (unsigned char*) ttl, sizeof (*ttl),
            offset, buf) < 0)
        return -1;
    if (version && ccnl_ndntlv_prependBlob(COPSS_TLV_Version, (unsigned char*) version, sizeof (*version),
            offset, buf) < 0)
        return -1;
    int oldoffset_remove = *offset;
    if (copss_tlv_prependContentName(&contentName_remove, offset, buf) < 0)
        return -1;
    if (ccnl_ndntlv_prependTL(COPSS_TLV_ContentNameRemove, oldoffset_remove - *offset, offset, buf) < 0)
        return -1;
    int oldoffset_add = *offset;
    if (copss_tlv_prependContentName(&contentName_add, offset, buf) < 0)
        return -1;
    if (ccnl_ndntlv_prependTL(COPSS_TLV_ContentNameAdd, oldoffset_add - *offset, offset, buf) < 0)
        return -1;
    if (ccnl_ndntlv_prependNonNegInt(COPSS_TLV_ControlType, control_type, offset, buf) < 0)
        return -1;
    if (ccnl_ndntlv_prependTL(COPSS_TLV_Control, oldoffset - *offset,
            offset, buf) < 0)
        return -1;
    return oldoffset - *offset;
}

int
copss_tlv_prependMulticast(struct copss_contentName_s *contentName, unsigned char *payload, int paylen, int *offset, unsigned char *buf) {

    int oldoffset = *offset;

    // mandatory
    if (ccnl_ndntlv_prependBlob(COPSS_TLV_ContentData, payload, paylen,
            offset, buf) < 0)
        return -1;
    int oldoffset_contentName = *offset;
    if (copss_tlv_prependContentName(&contentName, offset, buf) < 0)
        return -1;
    if (ccnl_ndntlv_prependTL(COPSS_TLV_ContentNameAdd, oldoffset_contentName - *offset, offset, buf) < 0)
        return -1;
    if (ccnl_ndntlv_prependTL(COPSS_TLV_Multicast, oldoffset - *offset,
            offset, buf) < 0)
        return -1;

    return oldoffset - *offset;
} 
