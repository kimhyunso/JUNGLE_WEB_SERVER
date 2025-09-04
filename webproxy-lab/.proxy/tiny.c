/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/*
 * # tiny는 반복실행 서버, 명령줄에서 넘겨받은 포트로의 연결 요청을 듣는다.
 * 1. open_listenfd 함수를 호출해서 듣기 소켓을 오픈한 후
 * 2. tiny는 무한 서버 루프를 실행
 * 3. 반복적으로 연결 요청을 접수
 * 4. 트랜잭션을 수행
 * 5. 자신 쪽의 연결 끝을 닫는다.
 */
int main(int argc, char* argv[]) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);
        Close(connfd);
    }
}


/*
1. 정적 콘텐츠 (if (!strstr(uri, "cgi-bin")))
이 조건은 uri에 "cgi-bin"이 포함되어 있지 않으면 참(true)이 됩니다. 즉, 정적 파일(HTML, 이미지 등)을 요청했을 때 이 블록이 실행됩니다.
strcpy(cgiargs, "");: 정적 파일은 인수가 필요 없으므로 cgiargs를 빈 문자열로 초기화합니다.
strcpy(filename, ".");: filename을 현재 디렉토리(. )를 나타내는 문자열로 시작합니다.
strcat(filename, uri);: 여기서 uri의 경로를 . 뒤에 추가합니다. 예를 들어, uri가 "/home.html"이라면 filename은 "./home.html"이 됩니다.
if (uri[strlen(uri) - 1] == '/'): uri가 "/"로 끝나면, 즉 디렉토리만 지정된 경우, **기본 파일 이름인 home.html**을 추가합니다. 예를 들어, uri가 "/"이면 filename은 "./home.html"이 됩니다.
이후 stat()을 사용하여 해당 파일이 존재하는지 확인합니다. 파일이 없으면 .html 확장자를 붙여 다시 확인하는 로직이 있습니다.
2. 동적 콘텐츠 (else)
uri에 "cgi-bin"이 포함되어 있으면 이 블록이 실행됩니다. 이는 CGI 프로그램과 같은 동적 콘텐츠를 요청했을 때입니다.
ptr = index(uri, '?');: uri에서 ? 문자를 찾습니다. 쿼리 문자열의 시작을 찾는 것입니다.
if (ptr): ?가 존재하면,
strcpy(cgiargs, ptr + 1);: ? 바로 뒤에 있는 문자열, 즉 "num1=1000&num2=2000"을 cgiargs에 복사합니다.
*ptr = '\0';: 여기서 중요한 작업이 발생합니다. ? 문자를 문자열의 끝을 나타내는 널 문자(\0)로 바꿔서 uri를 " /cgi-bin/adder"까지만 남도록 만듭니다. 이렇게 하면 uri가 파일 경로로 사용될 수 있습니다.
else { strcpy(cgiargs, ""); }: ?가 없으면 cgiargs를 빈 문자열로 초기화합니다.
strcpy(filename, ".");: filename을 .으로 시작합니다.
strcat(filename, uri);: 변경된 uri를 filename에 추가하여 "./cgi-bin/adder"라는 최종 경로를 만듭니다.
*/



// 한 개의 HTTP 트랜잭션을 처리한다.
void doit(int fd) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    // request line and headers
    // 요청 라인을 읽고 분석한다.
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    // GET 메소드만 지원함, POST같은 요청을 하면 에러 메시지를 보낸 후 main루틴으로 돌아옴
    if (strcasecmp(method, "GET")) { 
        clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
        return;
    }
    // request header들을 읽기
    read_requesthdrs(&rio);

    // parse URI from GET request
    is_static = parse_uri(uri, filename, cgiargs); // 정적 또는 동적 컨텐츠를 위한 것인지 판별
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
        return;
    }
    
    if (is_static) { // 정적컨텐츠 제공
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 읽기 권한 및 보통파일인지 검증
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size); // 정적 컨텐츠 제공
    } else { // 동적컨텐츠일때
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // 쓰기 권한 및 보통파일인지 검증
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs); // 동적 컨텐츠 제공
    }
}

void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg) {
    char buf[MAXLINE], body[MAXLINE];

    // build the HTTP response body
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    // printf the response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

// 요청 해더 내의 어떤 정보도 사용하지 않음
void read_requesthdrs(rio_t* rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

/*
 * 정적 컨텐츠: 자신의 현재 디렉토리(.) ex) workingDirectory/webproxy-lab/tiny
 * 정적 컨텐츠의 기본파일명: home.html
 * 실행파일의 홈 디렉토리: /cgi-bin
 */
int parse_uri(char* uri, char* filename, char* cgiargs) {
    char* ptr;

    if (!strstr(uri, "cgi-bin")) { // 정적 컨텐츠
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if (uri[strlen(uri) - 1] == '/') {
            strcat(filename, "home.html");
        }

        struct stat sbuf;
        if (stat(filename, &sbuf) < 0) {
            // 파일이 없으면 .html 확장자를 붙여서 다시 확인
            char html_filename[MAXLINE];
            strcpy(html_filename, filename);
            strcat(html_filename, ".html");
            if (stat(html_filename, &sbuf) == 0) {
                // .html 파일이 존재하면 그 파일로 경로를 업데이트
                strcpy(filename, html_filename);
            }
        }

        return 1;
    } else { // 동적 컨텐츠
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        } else {
            strcpy(cgiargs, "");
        }
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

void serve_static(int fd, char* filename, int filesize) { // 정적 컨텐츠 제공 함수
    int srcfd;
    char* srcp, filetype[MAXLINE], buf[MAXLINE];

    // 클라이언트에게 response header 보내기
    get_filetype(filename, filetype);

    // sprintf(buf, "HTTP/1.0 200 OK\r\n");
    // Rio_writen(fd, buf, strlen(buf));
    // sprintf(buf, "Server: Tiny Web Server\r\n");
    // Rio_writen(fd, buf, strlen(buf));
    // sprintf(buf, "Connection: close\r\n");
    // Rio_writen(fd, buf, strlen(buf));
    // sprintf(buf, "Content-length: %d\r\n", filesize);
    // Rio_writen(fd, buf, strlen(buf));
    // sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    // Rio_writen(fd, buf, strlen(buf));

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    printf("Reponse headers:\n");
    printf("%s", buf);

    // response body 클라이언트에게 보내기
    srcfd = Open(filename, O_RDONLY, 0);

    char* file_buf = (char*)malloc(filesize);
    if (file_buf == NULL) {
        // 메모리 할당 실패 처리
        clienterror(fd, "malloc", "500", "Internal Server Error", "Failed to allocate memory for file content");
        Close(srcfd);
        return;
    }
    
    // 파일에서 클라이언트 소켓이 아닌, 새로 오픈한 파일 디스크립터에서 읽기
    Rio_readn(srcfd, file_buf, filesize);
    
    // 읽어온 내용을 소켓에 쓰기
    Rio_writen(fd, file_buf, filesize);

    Close(srcfd);
    free(file_buf);
}

void get_filetype(char* filename, char* filetype) {
    if (strstr(filename, ".html")) {
        strcpy(filetype, "text/html");
    } else if (strstr(filename, ".gif")) {
        strcpy(filetype, "image/gif");
    } else if (strstr(filename, ".png")) {
        strcpy(filetype, "image/png");
    } else if (strstr(filename, ",jpg")) {
        strcpy(filetype, "image/jpeg");
    } else {
        strcpy(filetype, "text/plain");
    }
}

void serve_dynamic(int fd, char* filename, char* cgiargs) {
    char buf[MAXLINE], *emptylist[] = { NULL };

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0) { // 자식 프로세스 생성
        setenv("QUERY_STRING", cgiargs, 1); //환경변수 설정 
        Dup2(fd, STDOUT_FILENO); // redirect 
        Execve(filename, emptylist, environ);
    }
    Wait(NULL); // 부모가 죽기전에 자식을 죽여
}

// int main(int argc, char **argv)
// {
//   int listenfd, connfd;
//   char hostname[MAXLINE], port[MAXLINE];
//   socklen_t clientlen;
//   struct sockaddr_storage clientaddr;

//   /* Check command line args */
//   if (argc != 2)
//   {
//     fprintf(stderr, "usage: %s <port>\n", argv[0]);
//     exit(1);
//   }

//   listenfd = Open_listenfd(argv[1]);
//   while (1)
//   {
//     clientlen = sizeof(clientaddr);
//     connfd = Accept(listenfd, (SA *)&clientaddr,
//                     &clientlen); // line:netp:tiny:accept
//     Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
//                 0);
//     printf("Accepted connection from (%s, %s)\n", hostname, port);
//     doit(connfd);  // line:netp:tiny:doit
//     Close(connfd); // line:netp:tiny:close
//   }
// }
