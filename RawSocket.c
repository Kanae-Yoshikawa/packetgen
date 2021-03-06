
#include	<stdio.h>
#include	<string.h>
#include	<unistd.h>
#include	<sys/ioctl.h>
#include	<arpa/inet.h>
#include	<sys/socket.h>
#include	<linux/if.h>
#include	<net/ethernet.h>        /* L2 frame - protocol ID(type/長さ), struct ether_header */
#include	<netpacket/packet.h>    
#include	<netinet/if_ether.h>

#include	<net/if_arp.h>          /*（追加）arp packet生成に必要 */
#include	<netinet/ip.h>          /* (追加) ip packet生成に必要 */
#include	<netinet/ip_icmp.h>     /* (追加) icmp packet生成に必要 */


/*****
 * InitRawSocket()の引数
 * device ; NW interfce name
 * promiscFlag ; プロミスキャスモードにするかどうかのフラグ
 * ipOnly ; IP packetのみを対象とするかどうかのフラグ
 *****/
int InitRawSocket(char *device, int promiscFlag, int ipOnly)
{
    struct ifreq	ifreq;
    struct sockaddr_ll	sa;
    int	soc;

    /* ipOnly = 1の場合はIP packetのみを得る*/
    if(ipOnly){
        if((soc = socket(PF_PACKET,SOCK_RAW, htons(ETH_P_IP)))<0){
            perror("socket");
            return(-1);
        }
    }
    else{
        if((soc = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL)))<0){
            perror("socket");
            return(-1);
        }
    }

    /*****
     * memset() -> ifreqのサイズ分0埋め
     * ioctl()  -> NW interface name に対応したinterfaceのindexを取得
     *****/
    memset(&ifreq, 0, sizeof(struct ifreq));
    strncpy(ifreq.ifr_name, device, sizeof(ifreq.ifr_name)-1);
    if(ioctl(soc, SIOCGIFINDEX, &ifreq)<0){
        perror("ioctl");
        close(soc);
        return(-1);
    }

    /*****
     * interface indexとprotocol family, protocolをstruct sockaddr_llにセットし
     * bind()でディスクリプタsocに情報をセット
     *****/
    sa.sll_family = PF_PACKET;
    if(ipOnly){
        sa.sll_protocol = htons(ETH_P_IP);
    }
    else{
        sa.sll_protocol = htons(ETH_P_ALL);
    }
    sa.sll_ifindex = ifreq.ifr_ifindex;
    if(bind(soc, (struct sockaddr *)&sa, sizeof(sa))<0){
        perror("bind");
        close(soc);
        return(-1);
    }

    /* プロミスキャスモード；自分宛以外のpacketも受信*/
    if(promiscFlag){
        if(ioctl(soc, SIOCGIFFLAGS, &ifreq)<0){
            perror("ioctl");
            close(soc);
            return(-1);
        }
        ifreq.ifr_flags = ifreq.ifr_flags|IFF_PROMISC;
        if(ioctl(soc, SIOCSIFFLAGS, &ifreq)<0){
            perror("ioctl");
            close(soc);
            return(-1);
        }
    }

    return(soc);
}

char *my_ether_ntoa_r(u_char *hwaddr,char *buf,socklen_t size)
{
    snprintf(buf,size,"%02x:%02x:%02x:%02x:%02x:%02x",
            hwaddr[0],hwaddr[1],hwaddr[2],hwaddr[3],hwaddr[4],hwaddr[5]);

    return(buf);
}

int PrintEtherHeader(struct ether_header *eh,FILE *fp)
{
    char	buf[80];

    fprintf(fp,"ether_header----------------------------\n");
    fprintf(fp,"ether_dhost = %s\n",my_ether_ntoa_r(eh->ether_dhost,buf,sizeof(buf)));
    fprintf(fp,"ether_shost = %s\n",my_ether_ntoa_r(eh->ether_shost,buf,sizeof(buf)));
    fprintf(fp,"ether_type = %02X",ntohs(eh->ether_type));
    switch(ntohs(eh->ether_type)){
        case	ETH_P_IP:
            fprintf(fp,"(IP)\n");
            break;
        case	ETH_P_IPV6:
            fprintf(fp,"(IPv6)\n");
            break;
        case	ETH_P_ARP:
            fprintf(fp,"(ARP)\n");
            break;
        default:
            fprintf(fp,"(unknown)\n");
            break;
    }

    return(0);
}




/****************
 * main program
 ****************/

int main(int argc,char *argv[],char *envp[])
{
    int	soc,size;
    u_char	buf[2048];

    if(argc<=1){
        fprintf(stderr,"ltest device-name\n");
        return(1);
    }

    if((soc=InitRawSocket(argv[1],0,0))==-1){
        fprintf(stderr,"InitRawSocket:error:%s\n",argv[1]);
        return(-1);
    }
    /*****
     *****  
     while(1){
     if((size=read(soc,buf,sizeof(buf)))<=0){
     perror("read");
     }
     else{
     if(size>=sizeof(struct ether_header)){
     PrintEtherHeader((struct ether_header *)buf,stdout);
     }
     else{
     fprintf(stderr,"read size(%d) < %d\n",size,sizeof(struct ether_header));
     }
     }
     }
     *****/

    while(1){
        struct sockaddr_ll from;
        socklen_t   fromlen;
        memset(&from, 0, sizeof(from));

        fromlen = sizeof(from);
        if((size = recvfrom(soc, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen)) <= 0){
            perror("read");
        }
        else{
            printf("sll_family = %d\n", from.sll_family );
            printf("sll_protocol = %d\n", from.sll_protocol );
            printf("sll_ifindex = %d\n", from.sll_ifindex );
            printf("sll_hatype = %d\n", from.sll_pkttype );
            printf("sll_halen = %d", from.sll_halen );
            printf("sll_addr = %02x:%02x:%02x:%02x:%02x:%02x:\n", from.sll_addr[0], from.sll_addr[1], from.sll_addr[2], from.sll_addr[3], from.sll_addr[4], from.sll_addr[5], );
            AnalyzePacket(buf, size);
        }
    }

    close(soc);

    return(0);

}





//////////////////////////////////////
//////////    memo    ////////////////
//////////////////////////////////////
[payload]

    i++

[l4 header]
    UDP     s = socket(PF_INET, SOCK_DGRAM, 0)
    TCP     s = socket(PF_INET, SOCK_DGRAM, 0)
    
   myport = xxx;↲
   memset(&myskt, 0, sizeof myskt);↲
   myskt.sin_family = AF_INET;↲
   myskt.sin_port = htons(hoge);↲
   myskt.sin_addr.s_addr = htonl(x.x.x.x);



[l3 header]
    IP      s = socket(PF_INET, SOCK_RAW, )


[l2 header]
    Ethernet(IP packetだけ)    
            s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))
    Ethernet(IP packet以外も)
            s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))



