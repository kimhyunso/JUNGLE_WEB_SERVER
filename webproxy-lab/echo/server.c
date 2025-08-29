#include "csapp.h"
// #define MAXLINE 10000

static void echo(int connfd);
typedef struct sockaddr SA;

int main(int arg, char* args[]) {
    // getaddinfo
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (arg != 2) {
        printf(stderr, "usage: %s", args[0]);
        return -1;
    }
    // create socket

    // bind

    // listen
    listenfd = Open_listenfd(args[1]);
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        // accept
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        Getnameinfo((SA*)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        echo(connfd);
        Close(connfd);
    }

    return 0;
}

static void echo(int connfd) {
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    // read, write
    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("server received %d bytes \n", (int)n);
        Rio_writen(connfd, buf, n);
    }
}