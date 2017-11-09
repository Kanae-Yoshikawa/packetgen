/// TCP client ///

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

    int
main(int argc, char *argv[])
{
    struct sockaddr_in server;
    int sock;
    char buf[32];
    char *deststr;
    unsigned int **addrptr;

    if (argc != 2) {
        printf("Usage : %s dest\n", argv[0]);
        return 1;
    }
    deststr = argv[1];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(12345);

    server.sin_addr.s_addr = inet_addr(deststr);
    if (server.sin_addr.s_addr == 0xffffffff) {
        struct hostent *host;

        host = gethostbyname(deststr);
        if (host == NULL) {
            if (h_errno == HOST_NOT_FOUND) {
                /* h_errnoはexternで宣言されています */
                printf("host not found : %s\n", deststr);
            } else {
                /*
                   HOST_NOT_FOUNDだけ特別扱いする必要はないですが、
                   とりあえず例として分けてみました
                 */
                printf("%s : %s\n", hstrerror(h_errno), deststr);
            }
            return 1;
        }

        addrptr = (unsigned int **)host->h_addr_list;

        while (*addrptr != NULL) {
            server.sin_addr.s_addr = *(*addrptr);

            /* connect()が成功したらloopを抜けます */
            if (connect(sock,
                        (struct sockaddr *)&server,
                        sizeof(server)) == 0) {
                break;
            }

            addrptr++;
            /* connectが失敗したら次のアドレスで試します */
        }

        /* connectが全て失敗した場合 */
        if (*addrptr == NULL) {
            perror("connect");
            return 1;
        }
    } else {
        /* inet_addr()が成功したとき */

        /* connectが失敗したらエラーを表示して終了 */
        if (connect(sock,
                    (struct sockaddr *)&server, sizeof(server)) != 0) {
            perror("connect");
            return 1;
        }
    }

    memset(buf, 0, sizeof(buf));
    int n = read(sock, buf, sizeof(buf));
    if (n < 0) {
        perror("read");
        return 1;
    }

    printf("%d, %s\n", n, buf);

    close(sock);

    return 0;
}

