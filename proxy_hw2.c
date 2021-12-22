#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key= "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";
void *thread(void *vargp); // 여기서부터는 쓰레드가 추가 -> 쓰레드가 생성될 때 수행하게 될 쓰레드 함수를 선언

void doit(int connfd);
void parse_uri(char *uri,char *hostname,char *path,int *port);
void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio);
int connect_endServer(char *hostname,int port,char *http_header);

// main: 클라이언트를 연결할 때마다 그 연결을 수행하는 쓰레드를 생성
int main(int argc,char **argv)
{
    int listenfd,connfd;
    socklen_t  clientlen;
    char hostname[MAXLINE], port[MAXLINE];
    pthread_t tid; // thread id: 새로 만들어진 쓰레드의 식별자
    struct sockaddr_storage clientaddr;/*generic sockaddr struct which is 28 Bytes.The same use as sockaddr*/

    if(argc != 2){
        fprintf(stderr,"usage :%s <port> \n",argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);

    // 연결할 때마다 쓰레드를 생성 -> 아래 Pthread create에서. 
    while(1){
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd,(SA *)&clientaddr,&clientlen);

        /*print accepted message*/
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s %s).\n", hostname, port);

        /*sequential handle the client transaction*/
        // doit(connfd); concurrent에서는 doit이 아래 Pthread_create 안에서 실행. Close()역시 마찬가지.
        Pthread_create(&tid, NULL, thread, (void *)connfd);
        // sequential handle the client transaction

    }
    return 0;
}

/*
쓰레드 함수 추가: 새롭게 생성된 쓰레드 안에서 클라이언트와의 통신을 수행
*/
void *thread(void *vargp) {
    int connfd = (int)vargp;

    /* 자기 자신을 분리. 쓰레드를 떼어내면 쓰레드가 독립*/
    Pthread_detach(pthread_self()); 
    doit(connfd);
    Close(connfd);
}

/* handle the client HTTP transaction */
/* 
doit(): 클라이언트의 요청 라인을 파싱해 엔드 서버의 hostname, path, port를 가져오고
엔드 서버에 보낼 요청 라인과 헤더를 만들 변수를 생성.
*/
void doit(int connfd)
{
    int end_serverfd; // the end server file descriptor

    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char endserver_http_header [MAXLINE];
    // store the request line arguments
    char hostname[MAXLINE], path[MAXLINE];
    int port;

    rio_t rio, server_rio; // rio: client's rio & server_rio: endserver's rio

    // 클라이언트가 보낸 요청 헤더에서 method, uri, version을 가져옴
    // GET http://localhost:8000/home.html HTTP/1.1

    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version); // read the client request line

    if (strcasecmp(method, "GET")) {
        printf("Proxy does not implement the method");
        return;
    }
    // parser the uri to get hostname, file path, port.
    // 프록시 서버가 엔드 서버로 보낼 정보를 파싱.
    //hostname: localhost, path: /home.html, port: 8000
    parse_uri(uri, hostname, path, &port);

    // 프록시 서버가 엔드 서버로 보낼 요청 헤더를 만든다.
    // build the http header which will send to the end server
    build_http_header(endserver_http_header, hostname, path, port, &rio);

    // connect to the end server 프록시 서버와 엔드 서버를 연결
    end_serverfd = connect_endServer(hostname, port, endserver_http_header);
    if(end_serverfd<0) {
        printf("connection failed\n");
        return;
    }

    // 엔드 서버에 HTTP 요청 헤더를 보냄
    Rio_readinitb(&server_rio, end_serverfd);
    // write the http header to endserver
    Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));

    // 엔드 서버로부터 응답 메세지를 받아 클라이언트에 보내줌
    // receive message from end server and send to the client
    size_t n;
    while ((n= Rio_readlineb(&server_rio, buf, MAXLINE))!=0)
    {
        printf("proxy received %d bytes, then send\n", n);
        Rio_writen(connfd, buf, n); // connfd: 클라와 프록시 연결 소켓.
    }
    Close(end_serverfd);
}

void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio)
{
    char buf[MAXLINE],request_hdr[MAXLINE],other_hdr[MAXLINE],host_hdr[MAXLINE];
    /*request line: 응답 라인 만들기*/
    sprintf(request_hdr, requestlint_hdr_format, path);

    /*get other request header for client rio and change it */
    while(Rio_readlineb(client_rio,buf,MAXLINE)>0)
    {
        if(strcmp(buf,endof_hdr)==0) break;/*EOF*/
        
        // 호스트 헤더 찾기
        if(!strncasecmp(buf,host_key,strlen(host_key)))/*Host:*/
        {
            strcpy(host_hdr,buf);
            continue;
        }
        // 나머지 헤더 찾기
        if(strncasecmp(buf,connection_key,strlen(connection_key))
                &&strncasecmp(buf,proxy_connection_key,strlen(proxy_connection_key))
                &&strncasecmp(buf,user_agent_key,strlen(user_agent_key)))
        {
            strcat(other_hdr,buf);
        }
    }
    if(strlen(host_hdr)==0)
    {
        sprintf(host_hdr,host_hdr_format,hostname);
    }
    // 프록시 서버가 엔드 서버로 보낼 요청 헤더 작성
    sprintf(http_header,"%s%s%s%s%s%s%s",
            request_hdr,
            host_hdr,
            conn_hdr,
            prox_hdr,
            user_agent_hdr,
            other_hdr,
            endof_hdr);

    return ;
}
/*Connect to the end server*/
inline int connect_endServer(char *hostname,int port,char *http_header){
    char portStr[100];
    sprintf(portStr,"%d",port);
    return Open_clientfd(hostname,portStr);
}

/*parse the uri to get hostname,file path ,port*/
void parse_uri(char *uri,char *hostname,char *path,int *port)
{
    *port = 80;
    char* pos = strstr(uri,"//");

    pos = pos!=NULL? pos+2:uri;

    char*pos2 = strstr(pos,":");
    if(pos2!=NULL)
    {
        *pos2 = '\0';
        sscanf(pos,"%s",hostname);
        sscanf(pos2+1,"%d%s",port,path);
    }
    else
    {
        pos2 = strstr(pos,"/");
        if(pos2!=NULL)
        {
            *pos2 = '\0';
            sscanf(pos,"%s",hostname);
            *pos2 = '/';
            sscanf(pos2,"%s",path);
        }
        else
        {
            sscanf(pos,"%s",hostname);
        }
    }
    return;
}