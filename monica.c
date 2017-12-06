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
#include <net/if.h>             // struct ifreq
#include <sys/ioctl.h>          // ioctl()
#include <sys/socket.h>
#include <sys/time.h>
#include <stdio.h>
#include <elf.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>          // uint{8,16,32}_t
#include <sys/socket.h>         // sa_family_t, socklen_t
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>         // htonl(),htons(),ntohs(),ntohl(),  in_addr_t, in_port_t
#include <arpa/inet.h>
#include <unistd.h>             // _exit()
#include <time.h>               // time(),
#include <inttypes.h>           // uint16_t -> PRIu16,  //追加
#include <net/ethernet.h>       // L2 protocol          //追加
#include <unistd.h>             // getopt()

#define DEBUG       1
#define MAX_PACKET_SIZE 2048    // Sufficiently larger than the MTU(1518byte)
#define Period      1

/* ここを変える */
#define VLAN    YES
#define IPv4    YES

#define UDPflag 1
#define TCPflag 2

struct _UDP {
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

    /* UDP header */
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t len;
    uint16_t UdpChecksum;

    /* payload */
    unsigned char *buf;

} __attribute__((packed));
typedef struct _UDP UDP;



struct _TCP {
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

    /*  TCP header */
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t seqNumber;
    uint16_t ackNumber;
    uint16_t offsetReservCtl;
    uint16_t windowSize;
    uint16_t TcpChecksum;
    uint16_t urgentPointer;
    //uint?_t   option;

    /* payload */
    unsigned char *buf;

} __attribute__((packed));
typedef struct _TCP TCP;


//struct  _Packet{
//    struct _L2L3L4Header Header;        // ここで用いる構造体は既に定義済みのもの．それをrename する
//    unsigned char *buf;
//};
//typedef struct _Packet Packet;



// ここを変える
/* (ethernet frame type)    /usr/include/net/ethernet.h  */
#define ETH_P_Exp   0x0800          // Ethernet type = IP
//#define ETH_P_Exp   0x86dd        // Ethernet type = IPv6
//#define ETH_P_Exp   0x0806        // Ethernet type = ARP  address resolution

#define InitialReplyDelay   40      // これ何???
#define MaxCommCount        9999    // send 回数
#define IFNAME  "ethX"              // or "gretapX"
//#define IFNAME  "p5p1"            // abileneのinterface名に変更した
//extern void _exit(int32_t);       //プロトタイプ宣言．外部関数参照



/**************************************************************************
 * MAC addressを指定. MAC1とMAC2で前後を分離. 16進数「0x」付け忘れ注意    *
 * {src, dst, x, y}で借り置きした.  例）srcのMAC address -> MAC1[0]MAC2[0]*
 **************************************************************************/
#define NTerminals  4               // 指定できるMAC address数
uint16_t MAC1[NTerminals] = {0x0060, 0x0060, 0x0200, 0x0200};
uint32_t MAC2[NTerminals] = {0xdd440bcb, 0xdd440c2f, 0x00000003, 0x00000004};



/*******************************************
 * Open a socket for the network interface *
 *******************************************/
int32_t open_socket(int32_t index, int32_t *rifindex)
{
    unsigned char buf[MAX_PACKET_SIZE];
    int32_t i;
    int32_t ifindex;
    struct ifreq ifr;
    struct sockaddr_ll sll;
    char ifname[IFNAMSIZ];
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

    /* flush all received packets.  raw-socket receives packets from
       all interfaces when the socket is not bound to an interface */
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



/*********************
 * Create UDP header *
 *********************/
//ssize_t createPacket(Header *packet, uint16_t destMAC1, uint32_t destMAC2,
//          uint16_t srcMAC1, uint32_t srcMAC2, uint32_t vlanTag, uint16_t type, int32_t payload)
ssize_t createUdpHeader(unsigned char *buf, ssize_t bufsize, uint16_t destMAC1, uint32_t destMAC2,
        uint16_t srcMAC1, uint32_t srcMAC2, uint32_t vlanTag, uint16_t type, int32_t pValue)
{
    //Packet *packet = (Packet *)buf;     //(Packet *)にキャスト
    UDP *packet = (UDP *)buf;             //(UDP *)にキャスト

    ssize_t headerSize = 0;
    int32_t calcuLen = 0;

    //headerSize = sizeof(Packet) - sizeof(unsigned char *); //errorかも?? -> error 回避sampleみろ
    //headerSize = sizeof(UDP) - sizeof(unsigned char *); //errorかも?? -> error 回避sampleみろ
    if(headerSize > bufsize)
        return (-1);    

    /* [memo]   uint8_t     不要                        *
     *          uint16_t    htons();                    *
     *          uint32_t    htonl();                    *
     * 構造体変数[．]メンバ,    　　pointer[->]メンバ   */

    //packet->Header.destMAC1 = htons(destMAC1);
    //	    headerSize += sizeof(packet->Header.destMAC1);  //error 回避sample
    packet->destMAC1 = htons(destMAC1);
    headerSize += sizeof(packet->destMAC1);  //error 回避sample
    packet->destMAC2 = htonl(destMAC2);
    headerSize += sizeof(packet->destMAC2);
    packet->srcMAC1 = htons(srcMAC1);
    headerSize += sizeof(packet->srcMAC1);
    packet->srcMAC2 = htonl(srcMAC2);
    headerSize += sizeof(packet->srcMAC2);

#ifdef VLAN
    packet->VLANTag = htonl(vlanTag);
    headerSize += sizeof(packet->VLANTag);
#endif

    packet->type = htons(type);
    headerSize += sizeof(packet->type);

#ifdef IPv4
    packet->VerLen    = 0x45;
    headerSize += sizeof(packet->VerLen);
    packet->tos       = 0x00;
    headerSize += sizeof(packet->tos);

    /* totalLen計算 ; pValue + UDP header(8byte) + IP header(option 無->20byte) */
    calcuLen = pValue + 8 + 20;
    packet->totalLen  = htons(calcuLen);   //error;IPv4 total lentgh exceeds packet length 32byte
    headerSize += sizeof(packet->totalLen);

    packet->Identify  = htons(0xddf2);
    headerSize += sizeof(packet->Identify);
    packet->flag      = htons(0x4000);
    headerSize += sizeof(packet->flag);
    packet->TTL       = 0x40;
    headerSize += sizeof(packet->TTL);
    packet->protocol  = 0x11;                 //UDPなら11，TCPなら06
    headerSize += sizeof(packet->protocol);
    packet->IpChecksum= htons(0xcf79);
    headerSize += sizeof(packet->IpChecksum);
    packet->srcIP     = htonl(0x0a3a3c45);
    headerSize += sizeof(packet->srcIP);
    packet->dstIP     = htonl(0x0a3a3c48);
    headerSize += sizeof(packet->dstIP);
#endif

    /*** UDP header ***/
    packet->srcPort     = htons(0x0000);      //source port
    headerSize += sizeof(packet->srcPort);
    packet->dstPort     = htons(0x2710);      //destination port
    headerSize += sizeof(packet->dstPort);

    /* len計算 ; pValue + UDP header(8byte) */
    calcuLen = pValue + 8;
    packet->len         = htons(calcuLen);      //UDP len　//pValue + UDP header
    headerSize += sizeof(packet->len);

    packet->UdpChecksum = htons(0x0000);      //UDP checksum
    headerSize += sizeof(packet->UdpChecksum);
    
    printf("headerSize at createUdpHeader %d \n", headerSize);
    return headerSize;
}
//int32_t lastPayload = -1;


/*********************
 * Create TCP header *
 *********************/
//ssize_t createPacket(Header *packet, uint16_t destMAC1, uint32_t destMAC2,
//          uint16_t srcMAC1, uint32_t srcMAC2, uint32_t vlanTag, uint16_t type, int32_t payload)
ssize_t createTcpHeader(unsigned char *buf, ssize_t bufsize, uint16_t destMAC1, uint32_t destMAC2,
        uint16_t srcMAC1, uint32_t srcMAC2, uint32_t vlanTag, uint16_t type, int32_t pValue)
{
    //Packet *packet = (Packet *)buf;   //(Packet *)にキャスト
    TCP *packet = (TCP *)buf;           //(TCP *)にキャスト

    ssize_t headerSize = 0;
    int32_t calcuLen = 0;

    headerSize = sizeof(TCP) - sizeof(unsigned char *); //errorかも?? -> error 回避sampleみろ
    if(headerSize > bufsize)
        return (-1); 

    /* [memo]   uint8_t     不要                        *
     *          uint16_t    htons();                    *
     *          uint32_t    htonl();                    *
     * 構造体変数[．]メンバ,    　　pointer[->]メンバ   */
    //packet->Header.destMAC1 = htons(destMAC1);
    //	    headerSize += sizeof(packet->Header.destMAC1);  //error 回避sample
    packet->destMAC1 = htons(destMAC1);
    packet->destMAC2 = htonl(destMAC2);
    packet->srcMAC1 = htons(srcMAC1);
    packet->srcMAC2 = htonl(srcMAC2);

#ifdef VLAN
    packet->VLANTag = htonl(vlanTag);
#endif

    packet->type = htons(type);

#ifdef IPv4
    packet->VerLen    = 0x45;
    packet->tos       = 0x00;

    /* totalLen計算 */
    //pValue + TCP header(optrion無->20byte) + IP header(option 無->20byte)
    calcuLen = pValue + 20 + 20;
    packet->totalLen  = htons(calcuLen);   //error;IPv4 total lentgh exceeds packet length 32byte

    packet->Identify  = htons(0xddf2);
    packet->flag      = htons(0x4000);
    packet->TTL       = 0x40;
    packet->protocol  = 0x06;            //UDPなら11，TCPなら06
    packet->IpChecksum= htons(0xcf79);
    packet->srcIP     = htonl(0x0a3a3c45);
    packet->dstIP     = htonl(0x0a3a3c48);
#endif

    /*** TCP header ***/
    packet->srcPort        = htons(0x2710);           //source port
    packet->dstPort        = htons(0x2710);           //destination port
    packet->seqNumber      = htonl(0x00000001);       //sequence number 
    packet->ackNumber      = htonl(0x00000002);       //相手から受信したシーケンス番号+ data size
    packet->offsetReservCtl= htons(0xa002);           //data offset, reserved, ctl flag
    //packet->offsetReservCtl= htons(0x8011);         //コピペのもの．なぜheader size = 8 ?
    /* data offset(4)  TCPヘッダの長さ
     * resrved(6)      全bit 0. 将来の拡張のため
     * ctl flag(6)     URG/ACK/PSH/RST/SYN(connection要求)/FIN 
     * 1010000000000010*/
    packet->windowSize     = htons(0x002d);           //受信側が一度に受信可能なdata量を送信側に通知
    packet->TcpChecksum    = htons(0x0000);           //checksum
    packet->urgentPointer  = htons(0x0000);           //ctl flagのURGの値が「1」である場合にのみ使用
    //packet-> ? = hton?(?);     //option

    return headerSize;
}


/******************
 * Create payload *
 ******************/
ssize_t createPayload(unsigned char * buf, ssize_t payloadBuf, int32_t pValue, int32_t count)
{
    //Packet *packet = (Packet *)buf;
    ssize_t payloadSize = 0;

    if(pValue > payloadBuf){
        printf("pValue %d, payloadBuf %d \n", pValue, payloadBuf);
        return (-1);
    }
    printf("payloadSize at createPacket 1 %d\n", payloadSize);
    payloadSize = pValue;
    printf("payloadSize at createPacket 2 %d\n", payloadSize);

    return payloadSize;  
}	


/************************
 * Print packet content *
 ************************/
/*
//void printPacket(EtherPacket *packet, ssize_t packetSize, char *message)
void printPacket(Header *packet, ssize_t packetSize, char *message)

{
#ifdef VLAN     // show vlan ID
printf("%s #%s (VLAN %d) from %04x%04x to %04x%04x\n",
message, packet->payload, ntohl(packet->VLANTag) & 0xFFF
,ntohs(packet->srcMAC1), ntohl(packet->srcMAC2),
ntohs(packet->destMAC1), ntohl(packet->destMAC2));
#else
printf("%s #%s from %04x%04x to %04x%04x\n",
message, packet->payload,
ntohs(packet->srcMAC1), ntohl(packet->srcMAC2),
ntohs(packet->destMAC1), ntohl(packet->destMAC2));
#endif
}
 */


/*****************************
 * Send packets to terminals *
 *****************************/
int sendPackets(int32_t fd, int32_t ifindex, uint16_t SrcMAC1, uint32_t SrcMAC2,
        uint16_t DestMAC1, uint32_t DestMAC2, uint32_t vlanTag, uint16_t type,
        int32_t L4flag, int32_t pValue, int32_t *count)
{
    //int32_t i;
    unsigned char packet[MAX_PACKET_SIZE];	
    memset(packet,0, MAX_PACKET_SIZE);
    unsigned char *address = NULL;   
    printf("after memset\n");

    ssize_t packetSize = 0;
    ssize_t headerSize = 0;
    ssize_t payloadBuf = 0;
    //int32_t L4flag = 0;

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);   // Ethernet type = Trans. Ether Bridging
    sll.sll_ifindex = ifindex;

    printf("pValue at sendPackets%d \n", pValue);
    if(L4flag == UDPflag){
        // [memo] sizeof(packet) = sizeof(packet[0])
        headerSize = createUdpHeader(packet, sizeof(packet), DestMAC1, DestMAC2,
                SrcMAC1, SrcMAC2, vlanTag, type, pValue);
        if(headerSize == -1){
            printf("error: createUdpHeader at hederSize\n");
            return (-1);
        }
        payloadBuf = sizeof(packet) - headerSize;
        address = packet + headerSize;
    }else if(L4flag == TCPflag){
        // [memo] sizeof(packet) = sizeof(packet[0])
        headerSize = createTcpHeader(packet, sizeof(packet), DestMAC1, DestMAC2,
                SrcMAC1, SrcMAC2, vlanTag, type, pValue);
        if(headerSize == -1){
            printf("error: createTCPHeader at hederSize\n");
            return (-1);
        }
        payloadBuf = sizeof(packet) - headerSize;
        address = packet + headerSize;
    }else{
        printf("error: please pless [u] or [t] as options\n");
        return (-1);
    };

    ssize_t payloadSize = createPayload(address, payloadBuf, pValue,(*count)++);
    printf("payloadSize at sendPackets; %d\n", payloadSize);

    if(payloadSize == -1){
        printf("payloadSize\n");
        return (-1);
    }
    packetSize = headerSize + payloadSize;
    printf("packetSize before sendto %d\n", packetSize);

    ssize_t sizeout = sendto(fd, packet, packetSize, 0,(struct sockaddr *)&sll, sizeof(sll));

    //printPacket((EtherPacket*)packet, packetSize, "Sent:    ");
    //ややこしいからコメントアウト　あとでもどす
    /*printPacket((Header*)packet, packetSize, "Sent:    "); */
    if (sizeout < 0) {
        perror("sendto");
    } else {
        if (DEBUG) {
            printf("%d bytes sent through interface (ifindex) %d\n",
                    (int32_t)sizeout, (int32_t)ifindex);
        }
    }
    return (0);
}



/*********************************
 * The terms of sending  packets *
 *********************************/
int sendTerms(int32_t fd, int32_t ifindex, uint16_t SrcMAC1, uint32_t SrcMAC2,
        uint16_t DestMAC1, uint32_t DestMAC2, uint16_t vlanID, uint16_t type,
        int32_t L4flag, int32_t pValue) 
{
    int32_t sendCount = 0;
    time_t lastTime = time(NULL);
    int32_t replyDelay = 0;
    uint32_t vlanTag = 0x81000000 | vlanID;
    int ret = 0;                                //sendPacket()の返り値処理

    // Sending packets:
    for (; sendCount < MaxCommCount ;) {
        if (DestMAC2 != 0 && replyDelay <= 0) {
            int32_t currTime = time(NULL);
            if (currTime - lastTime >= Period) {
                if (DEBUG) printf("currTime=%d lastTime=%d\n", currTime, (int32_t)lastTime);
                ret =  sendPackets(fd, ifindex, SrcMAC1, SrcMAC2, DestMAC1, DestMAC2,
                        vlanTag, type, L4flag, pValue, &sendCount);
                if(ret < 0){
                    printf("sendPacket error\n");
                    return (-1);
                }
                lastTime = currTime;
            }
        }
    }
    return (0);
} // vimのインデント修正でずれる．なぜ？



/****************
 * Main program *
 ****************/
int32_t main(int32_t argc, char **argv)     // **argv = *argv[]
{

    int32_t ifindex;            //物理ifや論理ifに関連付けられる一意の識別番号 
    int32_t myTermNum = 0;      //MAC1{}の何要素目か
    int32_t destTermNum = 1;    //MAC2{}の何要素目か
    int32_t ifnum = 5;          // 物理port番号??　  IFNAMEと何が違う??
    uint16_t vlanID = 173;      //vlanIDを指定
    int ret = 0;                //sendTerms()の返り値処理

    /* add getopt() */
    // とりあえず，payloadだけやってみる
    int opt;
    int payload = 0;            //これだけなぜか変数未使用のwarningでる(?)
    int L4flag = 0;
    //int udp = 0, tcp = 0;
    int pValue = 0;
    while((opt = getopt(argc, argv, "utp:")) != -1){
        switch (opt) {
            case 'p':
                pValue = atoi(optarg);      //文字列を int 型に変換
                printf("pValue %d\n", pValue);
                payload = 1;
                break;
            case 'u':
                L4flag = UDPflag;
                break;
            case 't':
                //tcp = 1;
                L4flag = TCPflag;
                break;
            default:    // '?'
                fprintf(stderr, "Usage: %s [-p payload value]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    // ここの処理よくわからん
    //if (optind >= argc) {
    if (optind > argc) {
        printf("optind;%d, argc%d\n", optind,argc);            
        fprintf(stderr, "Expected argument after options\n");
        exit(EXIT_FAILURE);
    }

    unsigned char *buf = NULL;
    buf = malloc(sizeof(unsigned char)*pValue);
    if(buf == NULL){
        printf("malloc error");
    }

    // Set locators and IDs using terminal number:
    uint16_t SrcMAC1  = MAC1[myTermNum];
    uint32_t SrcMAC2  = MAC2[myTermNum];
    uint16_t DestMAC1 = MAC1[destTermNum];
    uint32_t DestMAC2 = MAC2[destTermNum];

    // 注）MAC addr->printfでのキャスト （uintからint）↲
    // 変換指定子 %x ->小文字16進数表示．04-> 0フラグ↲
    printf("p%dp1 terminal#=%d VLAN:%d srcMAC:%04x%04x destMAC:%04x%04x\n",
            ifnum, myTermNum, vlanID,
            (int32_t)SrcMAC1, (int32_t)SrcMAC2, (int32_t)DestMAC1, (int32_t)DestMAC2);

    //socket生成
    int32_t fd = open_socket(ifnum, &ifindex);

    // Set non-blocking mode: receiveとsendができるように
    //int32_t flags = fcntl(fd, F_GETFL, 0);
    //fcntl(fd, F_SETFL, O_NONBLOCK | flags);

    // ifindex ??
    ret = sendTerms(fd, ifindex, SrcMAC1, SrcMAC2, DestMAC1, DestMAC2, vlanID, ETH_P_Exp, L4flag, pValue);
    if(ret < 0){
        printf("sendTerms error\n");
        return (-1);
    }

    return (0);
}
