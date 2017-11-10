/// UDP client ///

/*error処理いれなきゃ*/


#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>     /* struct sockaddr_in */

    int
main()
{
    int sock;
    struct sockaddr_in addr;

    char buf[2048];


    /* ソケットの生成*/
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;          /*アドレスfamily */
    addr.sin_port = htons(12345);       /*port 番号 */
    addr.sin_addr.s_addr = INADDR_ANY;  /*IP address , INADDR_ANY->自ホストのアドレス*/


    /*bind -> 自ホストのソケットにIP addrとport番号を割り当てる*/
    /*成功；0，失敗；1 */
    bind(sock, (struct sockaddr *)&addr, sizeof(addr));

    memset(buf, 0, sizeof(buf));

    recv(sock, buf, sizeof(buf), 0);

    printf("%s\n", buf);

    /* ソケットクローズ */
    close(sock);

    return 0;
}



