#include <stdio.h>
#include "csapp.h"
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_HEADERS 100

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

static void handle_client(int fd);
static void read_request_headers(rio_t* rio, char header[][MAXLINE], int* num_headers);
void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg);
static int parse_uri(const char* uri, char* host, char* port, char* path);
static int forward_request_to_origin(
  int clientfd,
  const char* host, const char* port, const char* path,
  char header[][MAXLINE], int num_headers);

// This function reaps zombie child processes
void sigchld_handler(int sig) {
    while (waitpid(-1, 0, WNOHANG) > 0) {
        // Repeatedly call waitpid to reap all available zombie children.
        // WNOHANG prevents blocking if there are no zombies.
    }
}

/*
* 명령행에서 포트를 받고 그 포트로 리스닝 소켓을 연다.
* 클라이언트 연결을 accept해서 한 연결씩 handle client로 처리한다.
* 처리가 끝난 소켓을 닫는다.
*/
int main(int argc, char** argv){
  
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // Install signal handlers for SIGPIPE and SIGCHLD
    Signal(SIGPIPE, SIG_IGN);
    Signal(SIGCHLD, sigchld_handler);

    listenfd = Open_listenfd(argv[1]);

    while(1){
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        
        // Fork a child process to handle the client request
        if (Fork() == 0) { // Child process
            Close(listenfd); // Child closes its copy of the listening socket
            handle_client(connfd);
            Close(connfd); // Child closes the connection socket
            exit(0); // Child process terminates
        }
        
        // Parent process
        Close(connfd); // Parent closes its copy of the connection socket
    }
}

/*
* method, uri, version: 요청라인의 3요소 저장용
* header[MAX_HEADERS][MAXLINE]: 클라이언트가 보낸 요청 헤더들을 한 줄씩 보관
* host, port, path: 원서버(오리진)에 접속할 때 필요할 주소 3종
*/
static void handle_client(int fd){

    int num_headers = 0;
    char method[MAXLINE], buf[MAXLINE], uri[MAXLINE], version[MAXLINE], header[MAX_HEADERS][MAXLINE];
    char host[MAXLINE], port[16], path[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, fd);

    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    if(strcasecmp(method, "GET")){
        clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
        return;
    }
    
    read_request_headers(&rio, header, &num_headers);

    int host_idx = -1;
    for(int i = 0; i < num_headers; i++){
        if(!strncasecmp(header[i], "Host:", 5)) {host_idx = i; break;}
    }
    
    if(uri[0] == '/') {
        if(host_idx < 0) {clienterror(fd, uri, "400", "Bad Request", "Host header missing"); return;}
        char hostline[MAXLINE];
        strncpy(hostline, header[host_idx] + 5, sizeof(hostline) - 1);
        hostline[sizeof(hostline) - 1] = '\0';

        char* h = hostline;
        while(*h == ' ' || *h == '\t') h++;

        char* colon = strchr(h, ':');
        if(colon){
        *colon = '\0';
        strncpy(host, h, MAXLINE-1);
        host[MAXLINE-1] = '\0';
        strncpy(port, colon+1, 15);
        port[15] = '\0';
        }
        else{
        strncpy(host, h, MAXLINE-1);
        host[MAXLINE - 1] = '\0';
        strcpy(port, "80");
        }

        strncpy(path, uri, MAXLINE - 1);
        path[MAXLINE - 1] = '\0';
    }
    else{
        if(parse_uri(uri, host, port, path) < 0){
        clienterror(fd, uri, "400", "Bad Request", "Proxy couldn't parse URI");
        return;
        }
    }

    if(forward_request_to_origin(fd, host, port, path, header, num_headers) < 0){
        clienterror(fd, host, "502", "Bad Gateway", "Proxy failed to connect to origin");
        return;
    }
}

static void read_request_headers(rio_t* rio, char header[][MAXLINE], int* num_headers){
    char buf[MAXLINE];

    while(Rio_readlineb(rio, buf, MAXLINE) > 0){
        if(!strcmp(buf, "\r\n")) break;
        if(*num_headers < MAX_HEADERS){
            strncpy(header[*num_headers], buf, MAXLINE - 1); 
            header[*num_headers][MAXLINE - 1] = '\0';
            (*num_headers)++;
            printf("%s", buf);
        }
    }
    return;
}

void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg){
    char buf[MAXLINE], body[MAXLINE];

    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

static int parse_uri(const char* uri, char* host, char* port, char* path){
    const char *p = uri;
    const char* host_begin, *host_end, *port_begin, *path_begin;

    const char* prefix = "http://";
    size_t plen = strlen(prefix);
    if(strncasecmp(p, prefix, plen) == 0){
        p += plen;
    }

    host_begin = p;

    while(*p && *p != ':' && *p != '/')
        p++;
    
    host_end = p;

    if(host_end == host_begin) return -1;

    size_t hlen = host_end - host_begin;
    strncpy(host, host_begin, hlen);
    host[hlen] = '\0';

    if(*p == ':'){
        p++;
        port_begin = p;
        while(*p && *p != '/')
        p++;
        if(p == port_begin) return -1;
        size_t tlen = p - port_begin;
        strncpy(port, port_begin, tlen);
        port[tlen] = '\0';
    }
    else{
        strcpy(port, "80");
    }

    if(*p == '/'){
        path_begin = p;
        strncpy(path, path_begin, MAXLINE - 1);
        path[MAXLINE - 1] = '\0';
    }
    else{
        strcpy(path, "/");
    }

    return 0;
}

static int forward_request_to_origin(int clientfd,
const char* host, const char* port, const char* path,
char header[][MAXLINE], int num_headers) {
    int serverfd = Open_clientfd(host, port);
    if(serverfd < 0) return -1;
    rio_t s_rio;
    Rio_readinitb(&s_rio, serverfd);

    char out[MAXBUF];
    int n = snprintf(out, sizeof(out), "GET %s HTTP/1.0\r\n", path);
    Rio_writen(serverfd, out, n);

    bool have_host = false;
    for(int i = 0; i < num_headers; i++){
      if(!strncasecmp(header[i], "Host:", 5)){
        have_host = true;
        break;
      }
    }
    if(!have_host){
      if(*port && strcmp(port, "80")){
        n = snprintf(out, sizeof(out), "Host: %s:%s\r\n", host, port);
      }
      else{
        n = snprintf(out, sizeof(out), "Host: %s\r\n", host);
      }
      Rio_writen(serverfd, out, n);
    }

    n = snprintf(out, sizeof(out), "%s", user_agent_hdr);
    Rio_writen(serverfd, out, n);
    n = snprintf(out, sizeof(out), "Connection: close\r\n");
    Rio_writen(serverfd, out, n);
    n = snprintf(out, sizeof(out), "Proxy-Connection: close\r\n");
    Rio_writen(serverfd, out, n);

    for(int i = 0; i < num_headers; i++){
      if (!strncasecmp(header[i], "Connection:", 11)) continue;
      if (!strncasecmp(header[i], "Proxy-Connection:", 17)) continue;
      if (!strncasecmp(header[i], "Keep-Alive:", 11)) continue;
      if (!strncasecmp(header[i], "Transfer-Encoding:", 18)) continue;
      if (!strncasecmp(header[i], "TE:", 3)) continue;
      if (!strncasecmp(header[i], "Trailer:", 8)) continue;
      if (!strncasecmp(header[i], "Upgrade:", 8)) continue;
      if (!strncasecmp(header[i], "User-Agent:", 11)) continue;
      Rio_writen(serverfd, header[i], strlen(header[i]));
    }
    Rio_writen(serverfd, "\r\n", 2);

    char buf[MAXBUF];
    ssize_t m;

    while((m = Rio_readnb(&s_rio, buf, sizeof(buf))) > 0){
      if(rio_writen(clientfd, buf, m) < 0){ 
        break;
      } 
    }
    
    Close(serverfd);
    return 0;
}