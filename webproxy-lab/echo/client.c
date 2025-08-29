#include "csapp.h"
// #define MAXLINE 10000

int main(int arg, char* args[]) {
    // getaddrinfo
    int clientfd;
    char* host, *port, buf[MAXLINE];
    rio_t rio;

    if (arg != 3) {
        printf(stderr, "usage: %s", args[0]);
        return -1;
    }

    host = args[1];
    port = args[2];
    // connect
    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd); 

    // write
    // read
    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf));
        Rio_readlineb(&rio, buf, MAXLINE);
        Fputs(buf, stdout);
    }
    Close(clientfd);
    return 0;
}
