/*
 *
 *Software-based Ethernet Common Header
 *
 * Coded by Yasusi Kanada
 * Ver 1.0  2012-5-20   Initial version
 *
 ***/



#include <linux/if_packet.h>
#include <linux/if_ether.h> 
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdio.h>
#include <elf.h>
#include <string.h>



// 0       6      12      12/16 14/18           18/22
// +-------+-------+---------+----+---------------+
// | DMAC  | SMAC  |8100 VLAN|Type|Payload (4Bfix)|
// +-------+-------+---------+----+---------------+
//                  <-------> when VLAN == Yes


struct _EtherHeader {
    uint16_t destMAC1;
    uint32_t destMAC2;
    uint16_t srcMAC1;
    uint32_t srcMAC2;
//#ifdef VLAN
    uint32_t VLANTag;
//#endif
    uint16_t type;
    int32_t  payload;
} __attribute__((packed));

typedef struct _EtherHeader EtherPacket;



