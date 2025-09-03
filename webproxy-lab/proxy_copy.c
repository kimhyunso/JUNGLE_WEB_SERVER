#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/* Function prototypes */
void proxy(int fd);
void build_http_request(char *http_request, char *hostname, char *path, rio_t *client_rio);
int parse_uri(char *uri, char *hostname, char *path, char *port);
void *thread(void *vargp); // 스레드 핸들러 함수
void sigpipe_handler(int sig); // SIGPIPE 핸들러

/// @brief main 함수 서버 소켕 열고 클라이언트 연결을 받아 proxy 함수로 처리
/// @param argc 인자 개수 
/// @param argv 인자 배열
/// @return 성공 시 0, 실패 시 종료
int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  // 인자 개수 확안
  if (argc != 2)
  {
    fprintf(stderr, "usege: %s <port>\n", argv[0]);
    exit(1);
  }

  // 지정 포트 번호로 리슨 소켓 생성 및 초기화
  listenfd = Open_listenfd(argv[1]);
  
  while (1)
  {
    clientlen = sizeof(clientaddr); // 클라이언트 주소 초기화
    connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen); // 클라이언트 연결 대기 및 수락, 연결 소켓 생성
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 연결된 클라이언트의 호스트명과 호스트번호 얻기
    printf("Acceped connection form (%s, %s)\n", hostname, port);
    proxy(connfd);
    Close(connfd);
  }
}

/// @brief 클라이언트 연결을 처리하는 스레드 함수
/// @param vargp 파일 디스크립터 주소 
/// @return NULL
void *thread(void *vargp)
{
  int connfd = *((int *)vargp); // 동적으로 할당된 connfd 값 가져오기
  Pthread_detach(pthread_self()); // 스레드가 종료 시 자동으로 자원 해제
  free(vargp); // 동적 할당된 connfd 포인터 메모리 해제
  proxy(connfd); // 클라이언트 요청 처리
  Close(connfd); // 클라이언트와 연결된 소켓 닫기
  return NULL;
}

/// @brief SIGPIPE 시그널을 무시하는 핸들러
void sigpipe_handler(int sig) {
    return;
}

/// @brief proxy 함수 클라이언트로 요청을 받아 서버에 전달, 서버의 응답을 다시 클라이언트에게 전달
/// @param fd 클라이언트와 연결된 파일 디스크럽터
void proxy(int fd)
{
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 요청과 관련된 정보 저장 버퍼
  char hostname[MAXLINE], path[MAXLINE], port[MAXLINE] = "80"; // 기본 포트 80
  char http_request[MAXLINE]; // 서버에 보낼 HTTP 요청 메시지 저장할 버퍼
  int serverfd; // 서버와 연결할 소켓 디스크립터
  rio_t client_rio, server_rio; // 클라이언트, 서버 RIO

  Rio_readinitb(&client_rio, fd); //클라이언트와의 연결을 RIO 버퍼로 초기화
  Rio_readlineb(&client_rio, buf, MAXLINE); //클라리언트로부터 요청 라인을 읽어옴
  printf("Request header: \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); // 요청 라인 파싱

  if (strcasecmp(method, "GET")) // GET이 아니라면
  {
    sprintf(buf, "Proxy does not implement this method: %s\r\n", method);
    Rio_writen(fd, buf, strlen(buf)); // 실패 시 에러
    return;
  }

  if (parse_uri(uri, hostname, path, port) < 0) // URI를 파싱
  {
    sprintf(buf, "Proxy could not parse URI: %s\r\n", uri);
    Rio_writen(fd, buf, strlen(buf)); // 실패 시 에러
    return;
  }

  // 서버에 보낼 HTTP 요청 메시지 생성
  build_http_request(http_request, hostname, path, &client_rio);

  if ((serverfd = Open_clientfd(hostname, port)) < 0) // 서버에 연결 시도
  {
    sprintf(buf, "Connection failed to %s:%s\r\n", hostname, port);
    Rio_writen(fd, buf, strlen(buf)); // 실패 시 에러
    return;
  }

  Rio_readinitb(&server_rio, serverfd); // 서버와 연결을 위한 RIO 초기화
  Rio_writen(serverfd, http_request, strlen(http_request)); // 생성한 요청 메시지를 서버에 전송

  size_t n;

  // 서버로부터 한 줄씩 응답을 읽어 buf에 저장 클라이언트로 전달
  while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) > 0) 
  {
    Rio_writen(fd, buf, n); // 응답의 각 줄을 클라이언트로 전달
  }

  Close(serverfd);
}

/// @brief 클라이언트 요청을 읽어 서버로 보낼 HTTP 요청 메시지 생성
/// @param http_request 새로 만들 HTTP 요청 메시지 저장할 버퍼
/// @param hostname 요청할 서버의 호스트 이름
/// @param path 요청할 리소스 경로
/// @param client_rio 클라이언트 연결의 RIO 
void build_http_request(char *http_request, char *hostname, char *path, rio_t *client_rio)
{
  char buf[MAXLINE]; // 클라이언트 요청 헤더 한 줄을 읽어 저장할 버퍼

  sprintf(http_request, "GET %s HTTP/1.0\r\n", path);                     // 요청 라인 생성 -> 명시적으로 1.1에서 1.0으로 변환 
  sprintf(http_request + strlen(http_request), "Host: %s\r\n", hostname); // 필수 Host 헤더 추가
  sprintf(http_request + strlen(http_request), "%s", user_agent_hdr);     // user_agent_hdr 헤더 추가
  sprintf(http_request + strlen(http_request), "Connection: close\r\n");  // Connection
  sprintf(http_request + strlen(http_request), "Proxy-Connection: close\r\n"); // Proxy-connection 

  // 클라이언트가 보낸 나머지 헤더들을 한 줄씩 읽기
  while (Rio_readlineb(client_rio, buf, MAXLINE) > 0)
  {
    // 빈 줄이면 반복 종료
    if (strcmp(buf, "\r\n") == 0)
      break;
    
    // 이미 넣은 헤더들은 건너뜀 -> 클라이언트가 보낸 나머지 헤더는 그대로 전달
        if (!strncasecmp(buf, "Host:", 5) ||
            !strncasecmp(buf, "User-Agent:", 11) ||
            !strncasecmp(buf, "Connection:", 11) ||
            !strncasecmp(buf, "Proxy-Connection:", 17))
            continue;
        
        // 나머지 헤더는 그대로 요청 메세지에 추가
        strcat(http_request, buf);
  }
  
  // 끝을 알리기 위해 빈 줄 추가
  strcat(http_request, "\r\n");
}

/// @brief URI 문자열 파싱하여 정보를 분리하는 함수
/// @param uri 파싱할 uri 문자열
/// @param hostname 파싱된 호스트명 저장 버퍼
/// @param path 파싱된 경로 저장 버퍼
/// @param port 파싱된 포트 저장 버퍼
/// @return 성공 시 0, 실패 시 -1
int parse_uri(char *uri, char *hostname, char *path, char *port)
{
  char *hostbegin, *hostend, *pathbegin, *portbegin;

  // URI가 http://로 시작하는지 검사
  if (strncasecmp(uri, "http://", 7) != 0)
    return -1;
  
  hostbegin = uri + 7;                  // http:// 다음부터 hostname 부분 시작  uri + 7 = http:// 글자 건너뛰기
  pathbegin = strchr(hostbegin, '/');   // hostname 뒤에 경로가 시작하는 / 문자 찾기

  if (pathbegin != NULL) // / 가 있다면
  {
    strcpy(path, pathbegin); // path에 / 부터 끝까지 복사
    *pathbegin = '\0';       // hostname 문자열 끝에 \0 추가해서 자르기
  }
  else // / 가 없다면
  {
    strcpy(path, "/"); // 기본 경로는 /
  }

  // hostname에서 : 문자를 찾아 포트번호 여부 확인
  portbegin = strchr(hostbegin, ':');

  if (portbegin != NULL) // : 있다면
  {
    *portbegin = '\0';           // hostname 끝 자르기
    strcpy(hostname, hostbegin); // hostname 복사
    strcpy(port, portbegin + 1); // : 뒤부터 포트 복사
  }
  else // : 없다면
  {
    strcpy(hostname, hostbegin); // hostname 전체 복사
    strcpy(port, "80");          // 기본 포트는 80으로 설정
  }

  return 0; // 성공 시 0
}