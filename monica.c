
#include <stdio.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <net/if.h>             // struct ifreq
#include <sys/ioctl.h>          // ioctl()
#include <sys/socket.h>
#include <sys/time.h>
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
#define MAX_PACKET_SIZE 1518    // MTU = 1518byte
#define Period      1
#define IPv4        YES

#define ETH_P_Exp   0x0800          // Ethernet frame type = IP  (/usr/include/net/ethernet.)

#define InitialReplyDelay   40      // これ何???
#define MaxCommCount        30    // send+1 回数   for(; sendCount < MaxCommCount ;)
#define IFNAME              "ethX"  // or "gretapX"
//#define IFNAME            "p5p1"  // abileneのinterface名に変更した

#define UDPflag     1               // 値に特に意味は無い
#define TCPflag     2               // 値に特に意味は無い
#define ETHflag     3               // 値に特に意味は無い
#define ETH_VLANflag 4              // 値に特に意味は無い



struct _ETH {
    uint16_t destMAC1;
    uint32_t destMAC2;
    uint16_t srcMAC1;
    uint32_t srcMAC2;
    uint16_t type;
} __attribute__((packed));
typedef struct _ETH ETH;        // typedef: 既に定義済みの構造体をrename


struct _ETH_VLAN {
    uint16_t destMAC1;
    uint32_t destMAC2;
    uint16_t srcMAC1;
    uint32_t srcMAC2;
    uint32_t VLANTag;           // add vlan tag between L2 header
    uint16_t type;
} __attribute__((packed));
typedef struct _ETH_VLAN ETH_VLAN;


struct _UDP {
    /***  IPv4 header  ***/
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

    /*** UDP header ***/
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t len;
    uint16_t UdpChecksum;

    /*** payload ***/
    unsigned char *buf;

} __attribute__((packed));
typedef struct _UDP UDP;


struct _TCP {
    /***  IPv4 header  ***/
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

    /***  TCP header ***/
    uint16_t srcPort;
    uint16_t dstPort;
    uint32_t seqNumber;
    uint32_t ackNumber;
    uint16_t offsetReservCtl;
    uint16_t windowSize;
    uint16_t TcpChecksum;
    uint16_t urgentPointer;
    //uint?_t   option;

    /*** payload ***/
    unsigned char *buf;

} __attribute__((packed));
typedef struct _TCP TCP;



/**************************************************************************
 * MAC addressを指定. MAC1とMAC2で前後を分離. 16進数「0x」付け忘れ注意    *
      補足）2進数は末尾に「b」
 * {src, dst, x, y}で借り置きした.  例）srcのMAC address -> MAC1[0]MAC2[0]*
 **************************************************************************/
#define NTerminals  5       // 指定できるMAC address数
//MAC[]={abilene5-p5p1, abilene6-p5p1, abilne7-p1p1, abilene8-p1p1, abilene8-p5p1 }
uint16_t MAC1[NTerminals] = {0x0060, 0x0060, 0x0060, 0x0060, 0x0060};
uint32_t MAC2[NTerminals] = {0xdd440bcb, 0xdd440c21, 0xdd440c3a, 0xdd440c2e, 0xdd440c2f};


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

    /* RAW socket生成 */
    int32_t fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd == -1) {
        printf("%s - ", ifname);
        perror("socket");
        _exit(1);
    };

    /* get interface index */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) == -1) {
        printf("%s - ", ifname);
        perror("SIOCGIFINDEX");
        _exit(1);
    };
    ifindex = ifr.ifr_ifindex;
    *rifindex = ifindex;

    /* set promiscuous mode (自分宛のパケット以外も処理)*/
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ioctl(fd, SIOCGIFFLAGS, &ifr);
    ifr.ifr_flags |= IFF_PROMISC;
    ioctl(fd, SIOCSIFFLAGS, &ifr);

    /* bind */
    memset(&sll, 0xff, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = ifindex;
    if (bind(fd, (struct sockaddr *)&sll, sizeof(sll)) == -1) {
        printf("%s - ", ifname);
        perror("bind");
        _exit(1);
    };

    // sendだけに変更したからここいらないかも？
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


/**************************
 * Create Ethernet header *
 **************************/
// 呼び出し；L2headerSize = createEthHeader(packet, sizeof(packet), DestMAC1, DestMAC2, SrcMAC1, SrcMAC2, type);
ssize_t createEthHeader(unsigned char *ETHbuf, ssize_t ETHbufsize, uint16_t destMAC1, uint32_t destMAC2, 
        uint16_t srcMAC1, uint32_t srcMAC2, uint16_t type)
{
    ETH *packet = (ETH *)ETHbuf;             //(ETH_VLAN *)にキャスト
    ssize_t L2headerSize = 0;

    L2headerSize = sizeof(ETH);
    if(L2headerSize > ETHbufsize){
        return(-1);
    }
    packet->destMAC1 = htons(destMAC1);
    packet->destMAC2 = htonl(destMAC2);
    packet->srcMAC1 = htons(srcMAC1);
    packet->srcMAC2 = htonl(srcMAC2);
    packet->type = htons(type);

    return L2headerSize;
}


/*********************************
 * Create Ethernet + Vlan header *
 *********************************/
// 呼び出し；L2headerSize = createEthHeader(packet, sizeof(packet), DestMAC1, DestMAC2, SrcMAC1, SrcMAC2, vlanTag, type);
ssize_t createEthVlanHeader(unsigned char *ETH_VLANbuf, ssize_t ETH_VLANbufsize, uint16_t destMAC1, uint32_t destMAC2, 
        uint16_t srcMAC1, uint32_t srcMAC2, uint32_t vlanTag, uint16_t type)
{
    ETH_VLAN *packet = (ETH_VLAN *)ETH_VLANbuf;     //(ETH_VLAN *)にキャスト
    ssize_t L2headerSize = 0;

    L2headerSize = sizeof(ETH_VLAN);
    if(L2headerSize > ETH_VLANbufsize){
        return(-1);
    }
    packet->destMAC1 = htons(destMAC1);
    packet->destMAC2 = htonl(destMAC2);
    packet->srcMAC1 = htons(srcMAC1);
    packet->srcMAC2 = htonl(srcMAC2);
    packet->VLANTag = htonl(vlanTag);               // vlan tag
    packet->type = htons(type);

    return L2headerSize;
}


/*********************
 * Create UDP header *
 *********************/
// 呼び出し；L3_L4headerSize = createUdpHeader(address, sizeof(payloadBuf), sValue, dValue, pValue);
ssize_t createUdpHeader(unsigned char *UDPbuf, ssize_t UDPbufsize, int32_t sValue, int32_t dValue, int32_t pValue)
{
    UDP *packet = (UDP *)UDPbuf;                    //(UDP *)にキャスト

    ssize_t L3_L4headerSize = 0;
    int32_t calcuLen = 0;

    L3_L4headerSize = sizeof(UDP) - sizeof(unsigned char *);
    /* headerSize = sizeof(UDP) - sizeof(unsigned char *); errorするかも??
     * もしerrorする場合は　以下のerror回避sampleを参考にすること
     * headerSize += sizeof(packet->Header.destMAC1);
     * でも回避sampleの方式にするとbufsizeのif文が不成立になるから要編集 */

    //printf("L3_L4headerSize %ld\n", L3_L4headerSize);
    //printf("UDPbufsize%zd\n", UDPbufsize);

    if(L3_L4headerSize > UDPbufsize){
        return (-1);
    }

    /*** IPv4 header ***/
#ifdef IPv4
    packet->VerLen    = 0x45;
    packet->tos       = 0x00;

    /* totalLen計算 */
    /* pValue + UDP header(8byte) + IP header(option 無->20byte) */
    calcuLen = pValue + 8 + 20;
    packet->totalLen  = htons(calcuLen);    //error;IPv4 total lentgh exceeds packet length 32byte

    packet->Identify  = htons(0xddf2);
    packet->flag      = htons(0x4000);
    packet->TTL       = 0x40;
    packet->protocol  = 0x11;               //UDPなら11，TCPなら06
    packet->IpChecksum= htons(0xcf79);
    packet->srcIP     = htonl(0x0a3a3c45);
    packet->dstIP     = htonl(0x0a3a3c48);
#endif

    /*** UDP header ***/
    packet->srcPort     = htons(sValue);    //source port
    packet->dstPort     = htons(dValue);    //destination port

    /* UDP len計算 */
    /* pValue + UDP header(8byte) */
    calcuLen = pValue + 8;
    packet->len         = htons(calcuLen);  //UDP len = pValue + UDP header

    packet->UdpChecksum = htons(0x0000);    //UDP checksum あとで直す！！！！！

    //printf("L3_L4headerSize at createUdpHeader %d \n", L3_L4headerSize);
    return L3_L4headerSize;
}


/*********************
 * Create TCP header *
 *********************/
// 呼び出し；L3_L4headerSize = createUdpHeader(address, sizeof(payloadBuf), sValue, dValue, pValue);
ssize_t createTcpHeader(unsigned char *TCPbuf, ssize_t TCPbufsize, int32_t sValue, int32_t dValue, int32_t pValue)
{
    TCP *packet = (TCP *)TCPbuf;           //(TCP *)にキャスト

    ssize_t L3_L4headerSize = 0;
    int32_t calcuLen = 0;
    int32_t tcpSeqNum = 0;
    //int32_t tcpAckNum = 0;
    int16_t tcpORCcal = 0;

    L3_L4headerSize = sizeof(TCP) - sizeof(unsigned char *);
    if(L3_L4headerSize > TCPbufsize){
        return (-1);
    }


    /*** IPv4 header ***/
#ifdef IPv4
    packet->VerLen    = 0x45;
    packet->tos       = 0x00;

    /* totalLen計算 */
    //pValue + TCP header(optrion無->20byte) + IP header(option 無->20byte)
    calcuLen = pValue + 20 + 20;
    packet->totalLen  = htons(calcuLen);    //error;IPv4 total lentgh exceeds packet length 32byte

    packet->Identify  = htons(0xddf2);
    packet->flag      = htons(0x4000);
    packet->TTL       = 0x40;
    packet->protocol  = 0x06;               //UDPなら11，TCPなら06
    packet->IpChecksum= htons(0xcf79);
    packet->srcIP     = htonl(0x0a3a3c45);
    packet->dstIP     = htonl(0x0a3a3c48);
#endif

    /*** TCP header ***/
    packet->srcPort        = htons(sValue);     //source port
    packet->dstPort        = htons(dValue);     //destination port

    /* sequence number計算 */
    //seq #＝seq #の初期値＋相手に送ったTCPデータのbyte数
    //初期値はランダム設定-> 10000とする
    tcpSeqNum = 10000 + pValue;
    packet->seqNumber      = htonl(tcpSeqNum);  //sequence number

    /* ackNumberの計算 */
    //相手から受信したシーケンス番号+ data size
    //ACKフラグがOFFだから適当でよい？？
    //tcpAckNum = tcpSeqNum - 1;                  //式要確認
    packet->ackNumber      = htonl(0x0000);

    /* offsetReservCtlの計算 */
    //packet->offsetReservCtl= htons(0x8011);         //コピペのもの．なぜheader size = 8 ?
    
    /*  data offset(4)  TCPヘッダの長さ※ 4byte単位 -> 20/4byte(option無) = 5 = 0101
        resrved(6)      全bit 0 (将来の拡張のため) = 000000
        ctl flag(6)     URG(緊急)[0]/ACK[1]/PSH[1]/RST(中断)[0]/SYN(接続要求)[0]/FIN = 011000
                    SYN[1]にすると3way hand shake開始しちゃうから0にした
                    ACK->3WHSの最初を除き他の全てのTCPパケットはACKのフラグがON
        0101000000011000 = 0x5018
    //packet->offsetReservCtl= htons(0x5018);       //data offset, reserved, ctl flag


/* bit shiftしなさい kaneko-san
    offsetReservCtl = 20;
    sample ; offsetReservCtl = offsetReservCtl << 12;
    で，flagたてたものを足す*/
tcpORCcal = 5;                  //data offset
    tcpORCcal = tcpORCcal << 12;
tcpORCcal = tcpORCcal + 0;      //reserved
    tcpORCcal = tcpORCcal << 6;
tcpORCcal = tcpORCcal + 011000; //ctl flag

    packet->offsetReservCtl= htons(tcpORCcal);


    packet->windowSize     = htons(0x002d);         //受信側が一度に受信可能なdata量を送信側に通知
    packet->TcpChecksum    = htons(0x0000);         //checksum
    packet->urgentPointer  = htons(0x0000);         //ctl flagのURG(緊急)の値が「1」である場合にのみ使用
    //packet-> ? = hton?(?);     //option

    return L3_L4headerSize;
}


/******************
 * Create payload *
 ******************/
ssize_t createPayload(unsigned char * buf, ssize_t payloadBuf, int32_t pValue, int32_t count)
{
    ssize_t payloadSize = 0;

    if(pValue > payloadBuf){
        printf("error  pValue is over than payloadBuf\npValue %d, payloadBuf %zd \n", pValue, payloadBuf);
        return (-1);
    }
    payloadSize = pValue;

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
        uint16_t DestMAC1, uint32_t DestMAC2, int32_t L2flag, uint32_t vlanTag, uint16_t type,
        int32_t L4flag, int32_t sValue, int32_t dValue, int32_t pValue, int32_t *count)
{
    unsigned char packet[MAX_PACKET_SIZE];  //送信可能最大frameサイズ
    memset(packet,0, MAX_PACKET_SIZE);

    ssize_t packetSize = 0;                 //実際に送るframeのサイズ
    ssize_t L2headerSize = 0;               //Ether headerとVlan tag
    ssize_t L3_L4headerSize = 0;            //IP header と UDP/TCP header
    ssize_t payloadBuf = 0;                 //packet[]からheader部分を引いたもの
    unsigned char *address = NULL;          //packet[0]から各headerを詰めた時のpayload先頭アドレス

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);    // Ethernet type = Trans. Ether Bridging
    sll.sll_ifindex = ifindex;


    /*** L2 header ***/
    if(L2flag == ETHflag){
        /* +-------+-------+----+---------------------------------------------+
           | dMAC  | sMAC  |Type|                   Payload                   |
           +-------+-------+----+---------------------------------------------+ */

        // [memo] sizeof(packet) = sizeof(packet[0])
        L2headerSize = createEthHeader(packet, sizeof(packet), DestMAC1, DestMAC2, SrcMAC1, SrcMAC2, type);
        if(L2headerSize == -1){
            printf("error: createEthHeader at L2hederSize\n");
            return (-1);
        }
        payloadBuf = sizeof(packet) - L2headerSize;         //最大freameからL2headerSizeを引いた値
        address = packet + L2headerSize;

    }else if(L2flag == ETH_VLANflag){
        /* +-------+-------+----------+----+----------------------------------+
           | dMAC  | sMAC  |8100 VLAN |Type|        Payload                   |
           +-------+-------+----------+----+------------------------------- --+ */
        L2headerSize = createEthVlanHeader(packet, sizeof(packet), DestMAC1, DestMAC2, SrcMAC1, SrcMAC2, vlanTag, type);
        if(L2headerSize == -1){
            printf("error: createEthHeader at L2hederSize\n");
            return (-1);
        }
        payloadBuf = sizeof(packet) - L2headerSize;         //最大freameからL2headerSizeを引いた値
        //printf("payloadBuf%zd\n", payloadBuf);
        //printf("L2headerSize%zd\n", L2headerSize);
        address = packet + L2headerSize;
    }else{
        printf("error: please enter [e] or [v] as options\n");
        return (-1);
    };

    /*** L3,L4 header ***/
    if(L4flag == UDPflag){
        /* +-------+-------+-----------+----+------+-------+------------------+
           | dMAC  | sMAC  |(8100 VLAN)|Type| IPv4 | [UDP] |     Payload      |
           +-------+-------+-----------+----+------+-------+------------------+*/

        L3_L4headerSize = createUdpHeader(address, (int)payloadBuf, sValue, dValue, pValue);
        if(L3_L4headerSize == -1){
            printf("error: createUdpHeader at L3_L4hederSize\n");
            return (-1);
        }
        //上のL2headerSize で既に payloadBuf = sizeof(packet) - L2headerSize; してるから
        payloadBuf = payloadBuf - L3_L4headerSize;  
        //上のL2headerSize で既に address = packet + L2headerSize; してるから
        address = address + L3_L4headerSize;

    }else if(L4flag == TCPflag){
        /* +-------+-------+-----------+----+------+----------+---------------+
           | dMAC  | sMAC  |(8100 VLAN)|Type| IPv4 |   [TCP]  |    Payload    |
           +-------+-------+-----------+----+------+----------+---------------+ */

        L3_L4headerSize = createTcpHeader(address, (int)payloadBuf, sValue, dValue, pValue);
        //L3_L4headerSize = createTcpHeader(address, sizeof(payloadBuf), sValue, dValue, pValue);
        if(L3_L4headerSize == -1){
            printf("error: createTCPHeader at L3_L4hederSize\n");
            return (-1);
        }
        //上のL2headerSize で既に payloadBuf = sizeof(packet) - L2headerSize; してるから
        payloadBuf = payloadBuf - L3_L4headerSize;  
        //上のL2headerSize で既に address = packet + L2headerSize; してるから
        address = address + L3_L4headerSize;

    }else{
        printf("error: please enter [u] or [t] as options\n");
        return (-1);
    };

    /*** laypoad ***/
    ssize_t payloadSize = createPayload(address, payloadBuf, pValue,(*count)++);
    //printf("payloadSize at sendPackets(); %d\n", payloadSize);
    if(payloadSize == -1){
        printf("error: payloadSize is over than payloadBuf\n");
        return (-1);
    }

    packetSize = L2headerSize + L3_L4headerSize + payloadSize;
    ssize_t sizeout = sendto(fd, packet, packetSize, 0,(struct sockaddr *)&sll, sizeof(sll));

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
        uint16_t DestMAC1, uint32_t DestMAC2, int32_t L2flag, int32_t vValue,
        uint16_t type,int32_t L4flag, int32_t sValue, int32_t dValue, int32_t pValue) 
{
    int32_t sendCount = 0;
    time_t lastTime = time(NULL);
    int32_t replyDelay = 0;
    uint32_t vlanTag = 0x81000000 | vValue;
    int ret = 0;                                //sendPacket()の返り値処理

    for (; sendCount < MaxCommCount ;) {
        if (DestMAC2 != 0 && replyDelay <= 0) {
            int32_t currTime = time(NULL);
            if (currTime - lastTime >= Period) {
                if (DEBUG) printf("currTime=%d lastTime=%d\n", currTime, (int32_t)lastTime);
                ret =  sendPackets(fd, ifindex, SrcMAC1, SrcMAC2, DestMAC1, DestMAC2,
                        L2flag, vlanTag, type, L4flag, sValue, dValue, pValue, &sendCount);
                if(ret < 0){
                    printf("sendPacket error\n");
                    return (-1);
                }
                lastTime = currTime;
            }
        }
    }
    return (0);
}


/****************
 * Main program *
 ****************/
int32_t main(int32_t argc, char **argv)     // **argv = *argv[]
{
    int32_t ifindex;            //物理ifや論理ifに関連付けられる一意の識別番号 
    int32_t myTermNum = 0;      //MAC[]の何要素目か
    int32_t destTermNum = 4;    //MAC[]の何要素目か
    int32_t ifnum = 5;          // 物理port番号??　  IFNAMEと何が違う??
    int ret = 0;                //sendTerms()の返り値処理
    /* add getopt() で使うもの */
    int opt;
    int L4flag = 0;
    int L2flag = 0;
    int vValue = 0;             //vlanIDを指定
    int srcPortNum = 0;         //なぜか変数未使用のwarningでる(?)
    int sValue = 0;             //送信元port number
    int destPortNum = 0;        //なぜか変数未使用のwarningでる(?)
    int dValue = 0;             //宛先　port number
    int payload = 0;            //なぜか変数未使用のwarningでる(?)
    int pValue = 0;

    /* 実行例；./fileName -v 173 -u -s 49152 -d 49153 -p 100 
     * -> vlanID 173 で送信元port 49152から宛先port 49153 へpayload 100byteのUDP packetを送信*/
    while((opt = getopt(argc, argv, "ev:uts:d:p:")) != -1){
        switch (opt) {
            case 'e':
                L2flag = ETHflag;
                break;
            case 'v':
                L2flag = ETH_VLANflag;
                vValue = atoi(optarg);      //文字列を int 型に変換
                printf("vValue %d\n", vValue);
                break;
            case 'u':
                L4flag = UDPflag;
                break;
            case 't':
                L4flag = TCPflag;
                break;
            case 's':
                sValue = atoi(optarg);
                printf("sValue %d\n", sValue);
                srcPortNum = 1;
                break;
            case 'd':
                dValue = atoi(optarg);
                printf("dValue %d\n", dValue);
                destPortNum = 1;
                break;
            case 'p':
                pValue = atoi(optarg);
                printf("pValue %d\n", pValue);
                payload = 1;
                break;
            default:    // '?'
                fprintf(stderr, "Usage: %s [-p payload value]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
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
            ifnum, myTermNum, vValue,
            (int32_t)SrcMAC1, (int32_t)SrcMAC2, (int32_t)DestMAC1, (int32_t)DestMAC2);
    //socket生成
    int32_t fd = open_socket(ifnum, &ifindex);

    // Set non-blocking mode: receiveとsendができるように
    //int32_t flags = fcntl(fd, F_GETFL, 0);
    //fcntl(fd, F_SETFL, O_NONBLOCK | flags);

    // ifindex ??
    ret = sendTerms(fd, ifindex, SrcMAC1, SrcMAC2, DestMAC1, DestMAC2,
            L2flag, vValue, ETH_P_Exp, L4flag, sValue, dValue, pValue);
    if(ret < 0){
        printf("sendTerms error\n");
        return (-1);
    }
    return (0);
}
