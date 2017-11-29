/*

   +-------+-------+---------+----+------+-------+------------------+
   | dMAC  | sMAC  |8100 VLAN|Type| IPv4 | [UDP] |     Payload      |
   +-------+-------+---------+----+------+-------+------------------+

   +-------+-------+---------+----+------+----------+---------------+
   | dMAC  | sMAC  |8100 VLAN|Type| IPv4 |   [TCP]  |    Payload    |
   +-------+-------+---------+----+------+----------+---------------+

 */



#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdio.h>
#include <elf.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>             // _exit()
#include <time.h>               // time(),
#include <inttypes.h>           // uint16_t -> PRIu16,  //追加
#include <net/ethernet.h>       // L2 protocol          //追加


#define DEBUG       1
#define MAX_PACKET_SIZE 2048    // Sufficiently larger than the MTU
#define Period      1
//enum commMode {SendAndReceive = 0, ReceiveThenSend = 1};　// 使ってない? 列挙体p.191

/* ここを変える */
//#define VLAN    YES
#define IPv4    YES
#define UDP     YES
//#define TCP    YES






struct _EtherHeader {
    uint16_t destMAC1;
    uint32_t destMAC2;
    uint16_t srcMAC1;
    uint32_t srcMAC2;

#ifdef VLAN
    uint32_t VLANTag;
#endif

    uint16_t type;

#ifdef IPv4
    uint8_t  VerLen;
    uint8_t  tos;
    uint16_t totalLen;
    uint16_t Identify;
    uint16_t flag;
    uint8_t  TTL;
    uint8_t  protocol;
    uint16_t IpChecksum;
    uint32_t srcIP;
    uint32_t dstIP;
    //uint32_t option;
#endif

#ifdef  UDP
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t len;
    uint16_t UdpChecksum;
#endif

    /*
#ifdef  TCP
uint32_t TCPTag1;
uint32_t TCPTag2;
uint32_t TCPTag3;
uint32_t TCPTag4;
uint32_t TCPTag5;
    //uint32_t TCPTag6;      //option
#endif
     */
    int32_t  payload;

} __attribute__((packed));



typedef struct _EtherHeader EtherPacket;









/*****
 * [ ethernet frame type ]
 * /usr/include/net/ethernet.h
 *****/

/* ここを変える */
//#define ETH_P_Exp   0x8100        // type = IEEE 802.1Q VLAN tagging
#define ETH_P_Exp   0x0800          // type = IP
//#define ETH_P_Exp   0x86dd        // type = IPv6
//#define ETH_P_Exp   0x0806        // type = ARP  address resolution




#define InitialReplyDelay   40      // これ何???
#define MaxCommCount        9999    // これ何???
#define IFNAME  "ethX"              // or "gretapX"
//#define IFNAME  "p5p1"            // abileneのinterface名に変更した

extern void _exit(int32_t);     //プロトタイプ宣言．外部関数参照


/***
 * MAC addressを指定. MAC1とMAC2で前後を分離
 * 16進数「0x」付け忘れ注意
 * {src, dst, x, y}で借り置きした
 * 例）srcのMAC address -> MAC1[0]とMAC2[0]を直結したもの
 ***/
#define NTerminals  4           // 指定できるMAC address数
uint16_t MAC1[NTerminals] = {0x0060, 0x0060, 0x0200, 0x0200};
uint32_t MAC2[NTerminals] = {0xdd440bcb, 0xdd440c2f, 0x00000003, 0x00000004};





/*****
 * Open a socket for the network interface
 *****/
int32_t open_socket(int32_t index, int32_t *rifindex) {
    unsigned char buf[MAX_PACKET_SIZE];
    int32_t i;
    int32_t ifindex;
    struct ifreq ifr;
    struct sockaddr_ll sll;
    unsigned char ifname[IFNAMSIZ];
    strncpy(ifname, "p5p1", sizeof("p5p1"));
    //ifname[strlen(ifname) - 1] = '0' + index;

    //RAW socket生成
    int32_t fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd == -1) {
        printf("%s - ", ifname);
        perror("socket");
        _exit(1);
    };

    // get interface index
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) == -1) {
        printf("%s - ", ifname);
        perror("SIOCGIFINDEX");
        _exit(1);
    };
    ifindex = ifr.ifr_ifindex;
    *rifindex = ifindex;

    // set promiscuous mode
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ioctl(fd, SIOCGIFFLAGS, &ifr);
    ifr.ifr_flags |= IFF_PROMISC;
    ioctl(fd, SIOCSIFFLAGS, &ifr);

    memset(&sll, 0xff, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = ifindex;
    if (bind(fd, (struct sockaddr *)&sll, sizeof(sll)) == -1) {
        printf("%s - ", ifname);
        perror("bind");
        _exit(1);
    };

    /* flush all received packets. 
     *
     * raw-socket receives packets from all interfaces
     * when the socket is not bound to an interface
     */
    do {
        fd_set fds;
        struct timeval t;
        FD_ZERO(&fds);  
        FD_SET(fd, &fds);
        memset(&t, 0, sizeof(t));
        i = select(FD_SETSIZE, &fds, NULL, NULL, &t);
        if (i > 0) {
            recv(fd, buf, i, 0);
        };
        if (DEBUG) printf("interface %d flushed\n", ifindex);
    } while (i);

    if (DEBUG) printf("%s opened (fd=%d interface=%d)\n", ifname, fd, ifindex);

    return fd;
}


/**
 * Create an IPEC packet
 */

ssize_t createPacket(EtherPacket *packet, uint16_t destMAC1, uint32_t destMAC2,
        uint16_t srcMAC1, uint32_t srcMAC2, uint16_t type, uint32_t vlanTag,
        int32_t payload) {
    //ssize_t packetSize = sizeof(EtherPacket)-20;
    ssize_t packetSize = sizeof(EtherPacket);
    // ssize_t packetSize = payloadLength + sizeof(EtherPacket);
    memset(packet,0,sizeof(EtherPacket));

    /* [memo]
       uint8_t     不要
       uint16_t    htons();
       uint32_t    htonl();
     */
    packet->destMAC1 = htons(destMAC1);
    packet->destMAC2 = htonl(destMAC2);
    packet->srcMAC1 = htons(srcMAC1);
    packet->srcMAC2 = htonl(srcMAC2);

    memset(&payload,0,sizeof(payload));     //追記

    /*
#ifdef VLAN
packet->VLANTag = htonl(vlanTag);
#endif
     */

    packet->type = htons(type);


#ifdef IPv4
    packet->VerLen    = 0x45;
    packet->tos       = 0x00;
    packet->totalLen  = htons(0x002e);
    packet->Identify  = htons(0xddf2);
    packet->flag      = htons(0x4000);
    packet->TTL       = 0x40;
    packet->protocol  = 0x11;               //UDPなら11，TCPなら06
    packet->IpChecksum= htons(0xcf79);
    packet->srcIP     = htonl(0x0a3a3c45);
    packet->dstIP     = htonl(0x0a3a3c48);
#endif


#ifdef UDP
    packet->srcPort     = htons(0x0000);      //source port
    packet->dstPort     = htons(0x2710);      //destination port
    packet->len         = htons(0x001a);      //UDP len
    packet->UdpChecksum = htons(0x0000);    //UDP checksum
#endif

    /*
#ifdef TCP
packet->TCPTag1 = htonl(0x00002710);        //source port, destination port
packet->TCPTag2 = htonl(0x00000001);        //sequence number  開始はどこから？？とりあえず1にした
packet->TCPTag3 = htonl(0x00000002);        //acknowkedgement number　？？？とりあえず2にした
packet->TCPTag4 = htonl(0x8011002d);        //data offset, resrved, ctl flag, window size　コピペ
packet->TCPTag5 = htonl(0x00000000);        //checksum, urgent pointer  0埋め
    //packet->TCPTag6 = htonl(0x--------);      //option
#endif
     */

    packet->payload = htonl(payload);
    // strncpy(packet->payload, payload, packetSize);

    return packetSize;
}


int32_t lastPayload = -1;



/**
 * Print IPEC packet content
 */
void printPacket(EtherPacket *packet, ssize_t packetSize, char *message) {
#ifdef VLAN
    printf("%s #%d (VLAN %d) from %04x%04x to %04x%04x\n",
            message, ntohl(packet->payload), ntohl(packet->VLANTag) & 0xFFF,
#else
            printf("%s #%d from %04x%04x to %04x%04x\n",
                message, ntohl(packet->payload),
#endif
                ntohs(packet->srcMAC1), ntohl(packet->srcMAC2),
                ntohs(packet->destMAC1), ntohl(packet->destMAC2));
            lastPayload = ntohl(packet->payload);
            }


            /**
             * Send packets to terminals
             */
            void sendPackets(int32_t fd, int32_t ifindex, uint16_t SrcMAC1, uint32_t SrcMAC2,
                uint16_t DestMAC1, uint32_t DestMAC2, uint16_t type, uint32_t vlanTag,
                int32_t *count) {
            int32_t i;
            unsigned char packet[MAX_PACKET_SIZE];
            // unsigned char *payload = "Hello!";

            struct sockaddr_ll sll;
            memset(&sll, 0, sizeof(sll));
            sll.sll_family = AF_PACKET;
            sll.sll_protocol = htons(ETH_P_ALL);   // Ethernet type = Trans. Ether Bridging
            sll.sll_ifindex = ifindex;

            ssize_t packetSize = createPacket((EtherPacket*)packet, DestMAC1, DestMAC2,
                    SrcMAC1, SrcMAC2, type, vlanTag, (*count)++);

            ssize_t sizeout = sendto(fd, packet, packetSize, 0,
                    (struct sockaddr *)&sll, sizeof(sll));

            printPacket((EtherPacket*)packet, packetSize, "Sent:    ");
            if (sizeout < 0) {
                perror("sendto");
            } else {
                if (DEBUG) {
                    printf("%d bytes sent through interface (ifindex) %d\n",
                            (int32_t)sizeout, (int32_t)ifindex);
                }
            }
            }

void sendReceive(int32_t fd, int32_t ifindex, uint16_t SrcMAC1, uint32_t SrcMAC2,
        uint16_t DestMAC1, uint32_t DestMAC2, uint16_t type, uint16_t vlanID) {
    unsigned char buf[MAX_PACKET_SIZE];
    int32_t sendCount = 0;
    int32_t receiveCount = 0;
    time_t lastTime = time(NULL);
    int32_t replyDelay = 0;
    int32_t i;
    uint32_t vlanTag = 0x81000000 | vlanID;

    // Sending and receiving packets:
    for (; sendCount < MaxCommCount && receiveCount < MaxCommCount;) {
        if (DestMAC2 != 0 && replyDelay <= 0) {
            int32_t currTime = time(NULL);
            if (currTime - lastTime >= Period) {
                if (DEBUG) printf("currTime=%d lastTime=%d\n", currTime, (int32_t)lastTime);
                sendPackets(fd, ifindex, SrcMAC1, SrcMAC2, DestMAC1, DestMAC2, type, vlanTag,
                        &sendCount);
                lastTime = currTime;
            }
        }
        ssize_t sizein = recv(fd, buf, MAX_PACKET_SIZE, 0);
        if (sizein >= 0) {
            EtherPacket *packet = (EtherPacket*) buf;
            if (DestMAC2 == 0) {
                DestMAC1 = ntohs(packet->srcMAC1);
                DestMAC2 = ntohl(packet->srcMAC2);
                replyDelay = InitialReplyDelay;
            } else if (replyDelay > 0) {
                replyDelay--;
            }
            printPacket(packet, sizein, "Received:");
            receiveCount++;
        } else {
            usleep(10000); // sleep for 10 ms
        }
    }
}


/****************
 * Main program *
 ****************/
//int32_t main(int32_t argc, char **argv) {     //original
int32_t main(int32_t argc, char *argv[]) {
    int32_t ifindex;            //物理ifや論理ifに関連付けられる一意の識別番号 
    int32_t myTermNum = 0;      //MAC1{}の何要素目か
    int32_t destTermNum = 1;    //MAC2{}の何要素目か
    int32_t ifnum = 5;          // 物理port番号??　  IFNAMEと何が違う??
    uint16_t vlanID = 173;      //vlanIDを指定
    // int32_t i;               //使ってない?からコメントアウト


    // Get terminal and interface numbers（<-とは？） from the command line
    // 実行時に引数で渡す
    // int32_t count = 0; (original) createPacketと変数名重複するから変更した
    int32_t counter = 0;
    if (++counter < argc) {
        myTermNum = atoi(argv[counter]);      // My terminal number
    }
    if (myTermNum >= NTerminals || myTermNum < 0) {
        printf("My terminal number (%d) too large\n", myTermNum);
        myTermNum = 0;
    }

    if (++counter < argc) {
        destTermNum = atoi(argv[counter]);    // Destination terminal number
    }
    if (destTermNum >= NTerminals || destTermNum < 0) {
        printf("Destination terminal number (%d) too large\n", destTermNum);
        destTermNum = 1;
    }

    if (++counter < argc) {
        ifnum = atoi(argv[counter]);          // Interface number
    }

    if (++counter < argc) {
        vlanID = atoi(argv[counter]);         // VLAN ID
    }
    if (vlanID < 1 || 4095 < vlanID) {
        //printf("VLAN ID out of range (1..4095)\n", vlanID);       //before
        printf("%"PRIu16"\n", vlanID);     // 書き足した．header fileもincludeに足した
        vlanID = 1;
    }


    // Set locators and IDs using terminal number:
    uint16_t SrcMAC1  = MAC1[myTermNum];
    uint32_t SrcMAC2  = MAC2[myTermNum];
    uint16_t DestMAC1 = MAC1[destTermNum];
    uint32_t DestMAC2 = MAC2[destTermNum];

    //MAC addr->printfする時にキャストしている  uintからint↲
    // 変換指定子 %x ->小文字16進数表示．04-> 0フラグ↲
    printf("p%dp1 terminal#=%d VLAN:%d srcMAC:%04x%04x destMAC:%04x%04x\n",
            ifnum, myTermNum, vlanID,
            (int32_t)SrcMAC1, (int32_t)SrcMAC2, (int32_t)DestMAC1, (int32_t)DestMAC2);

    //socket生成
    int32_t fd = open_socket(ifnum, &ifindex);

    // Set non-blocking mode: receiveとsendができるように
    int32_t flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, O_NONBLOCK | flags);

    // ifindex??
    sendReceive(fd, ifindex, SrcMAC1, SrcMAC2, DestMAC1, DestMAC2, ETH_P_Exp, vlanID);
}
