
//
/* 関数とheader file の対応メモしておけ */
//

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main()
{
    int s, s1; 
    /* s；listenでconnection Max数の宣言まで．*/
    /*s1；実際の各connectionの3WHSから使用*/
    /* connection のmaxは5だから，for(;;) でs1が5重で回っている
       (karnelからみるとs1それぞれ別)と，次のconnectionは破棄？*/
    struct sockaddr_in myskt;
    struct sockaddr_in skt;
    socklen_t sktlen;
    char buf[100];
    //    char msg[6] = "hello\n";
    int count;




    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        /*socket( v4?v6?, UDP?TCP?, おまじない0 )*/
        /* socket生成に失敗なら負，成功なら正 */
        /* socketが正常に生成されると，socket番号が返る．そのためにsで置いている*/
    {
        perror("socket");
        exit(1);
    }



    myskt.sin_port = htons(49152);
    /* htons 確認せよ！*/
    myskt.sin_addr.s_addr = htonl(INADDR_ANY);
    /* htonl 確認せよ！*/
    /* INADDR_ANY ; 自アドレス，なんでも */
    /* sin_addrの中に複数ある要素のうちの，s_addr という要素*/

    
    if ((bind(s, (struct sockaddr *)&myskt, sizeof myskt)) < 0) 
    /* bind( 生成したsocket番号，キャストしたmysktのアドレス，mysktのサイズ) */
    /* socketとは違い返り値が意味を持たないので正負(成功or失敗)の判定のみでよい */
    /* bindとはport番号とaddrを関連付けする関数（myskt.sin_portとmyskt. sin_addr_s_addrを参照） */
    {
        perror("bind");
        exit(1);
    }




    if (listen(s, 5) < 0) {
        perror("listen");
        exit(1);
    }
    sktlen = sizeof skt;

    for (;;) {
        if ((s1 = accept(s, (struct sockaddr *)&skt, &sktlen)) < 0) {
            perror("accept");
            exit(1);
        }

        printf("client's IP address: %s\n", inet_ntoa(skt.sin_addr));
        printf("client's port number: %d\n", ntohs(skt.sin_port));

        for (;;) {
            if ((count = recv(s1, buf, sizeof(buf), 0)) < 0) {
                perror("recv");
                exit(1);
            }
            printf("received message: %s\n", buf);
            if (strcmp(buf, "FIN") == 0) {
                close(s1);
                break;
            }
            if ((count = send(s1, buf, sizeof(buf), 0)) < 0) {
                perror("send");
                exit(1);
            }
        }
    }
    close(s);
    return 0;
}
