#include "unp.h"
#include <netinet/in_system.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define BUFSIZE 1500

/* global */
char    recvbuf[BUFSIZE]
char    sendbuf[BUFSIZE]

int     datalen;    //ICMPヘッダに続くデータのバイト数
int     nsent;      //sendto()ごとに1ずつ増加
pid_t   pid;        //このプロセスのpid
int     sockfd;
int     verbose;
char    *host;


/* 関数プロトタイプ */
void    proc_v4(char *, ssize_t, struct timeval *);
void    proc_v6(char *, ssize_t, struct timeval *);
void    send_v4(void);
void    send_v6(void);
void    readloop(void);
void    sig_alrm(int);
void    tv_sub(struct timeval *, struct timeval);


struct proto
{
    void    (*fproc)(char *, ssize_t, struct timeval *);
    void    (*fsend)(void);
    struct  sockaddr *sasend;    //送信用sockaddr{}, getaddrinfoより
    struct  sockaddr *sarecv;    //受信用sockaddr{}
    socklen_t   salen;           //sockaddr{}の大きさ
    int     icmpproto;           //ICMPのIPPROTO_xxx値

} *pr;  //globalポインタ pr

#ifdef IPV6

#include<netinet/ip6.h>
#include<netinet/icmp.h>

#endif
