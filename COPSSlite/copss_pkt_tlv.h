/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   copss-pkt-tlv.h
 * Author: Welle
 *
 * Created on November 17, 2016, 3:04 PM
 */

#ifndef COPSS_PKT_TLV_H
#define COPSS_PKT_TLV_H

#ifdef __cplusplus
extern "C" {
#endif

    // Packet types:
#define COPSS_TLV_Control                       0x20
#define COPSS_TLV_Multicast                     0x21

    //Common fields:
#define COPSS_TLV_ContentNameAdd                0x22

    //Control packet:
#define COPSS_TLV_ControlType                   0x23
#define COPSS_TLV_ContentNameRemove             0x24
#define COPSS_TLV_Version                       0x25
#define COPSS_TLV_TTL                           0x26

    //Multicast packet
#define COPSS_TLV_ContentData                   0x27

#ifdef __cplusplus
}
#endif

#endif /* COPSS_PKT_TLV_H */
