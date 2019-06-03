/*
 * proxy.c - ICS Web proxy
 *
 *
 */

#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>
#include <string.h>
/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);
void doit(int fd,struct sockaddr_in sockaddr);
void* thread(void* vargp);


// Struct used to pass arguments to each thread
typedef struct{
    struct sockaddr_in sockaddr;
    int connfd;
} sock_arg;

sem_t mutex;
/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    int listenfd,connfd;
    char hostname[MAXLINE],port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    
    Signal(SIGPIPE,SIG_IGN);
    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    Sem_init(&mutex,0,1);

    listenfd = Open_listenfd(argv[1]);
    while(1){
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd,(SA *)&clientaddr,&clientlen);
        /* 127.0.0.1 instead of localhost */
        int flag = NI_NUMERICHOST;
        Getnameinfo((SA *) &clientaddr,clientlen,hostname,MAXLINE,port,MAXLINE,flag);
        
        struct sockaddr_in sockaddr;
        struct in_addr addr;
        inet_pton(AF_INET,hostname,(void *)&addr.s_addr);
        sockaddr.sin_addr = addr;
        sockaddr.sin_family = AF_INET;

        sock_arg* arg = (sock_arg*) malloc(sizeof(sock_arg));
        arg->connfd = connfd;
        arg->sockaddr = sockaddr;

        Pthread_create(&tid,NULL,thread,arg);
        /*
        doit(connfd,sockaddr);
        Close(connfd);
        */
    }
    exit(0);
}

void *thread(void *arg){
    int connfd = ((sock_arg *)arg)->connfd;
    struct sockaddr_in sockaddr = ((sock_arg *)arg)->sockaddr;
    Pthread_detach(pthread_self());
    free(arg);
    doit(connfd,sockaddr);
    Close(connfd);
    return NULL;
}

/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, char *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
        hostname[0] = '\0';
        return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    if (hostend == NULL)
        return -1;
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    if (*hostend == ':') {
        char *p = hostend + 1;
        while (isdigit(*p))
            *port++ = *p++;
        *port = '\0';
    } else {
        strcpy(port, "80");
    }

    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
        pathname[0] = '\0';
    }
    else {
        pathbegin++;
        strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), the number of bytes
 * from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                      char *uri, size_t size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));
    
    /*
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 12, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %zu", time_str, a, b, c, d, uri, size);
}


void doit(int clientfd,struct sockaddr_in sockaddr)
{
    /*
       (client) <=====> clientfd(client_rio) <-(proxy)-> serverfd(server_rio) <=====> (server)
    */
    char client_buf[MAXLINE],server_buf[MAXLINE];
    char method[MAXLINE],uri[MAXLINE],version[MAXLINE];
    char target[MAXLINE],path[MAXLINE],port[MAXLINE];
    int read_bytes,write_bytes ; //Get the written or readed bytes number for debugging.
    rio_t client_rio,server_rio;
    /* Read request line and headers */
    Rio_readinitb(&client_rio,clientfd);
    if(Rio_readlineb(&client_rio,client_buf,MAXLINE) < 0)
        return;
    
    sscanf(client_buf,"%s %s %s",method,uri,version);

    if(parse_uri(uri,target,path,port)!=0){
        fprintf(stderr,"parse_uri error\n");
        return;
    }

    //Construct a request.
    char request[MAXLINE];
    int request_bodylen = 0;

    sprintf(request,"%s /%s %s\r\n",method,path,version);
    /* Read request header */
    for(read_bytes = Rio_readlineb(&client_rio,client_buf,MAXLINE);read_bytes>0;read_bytes = Rio_readlineb(&client_rio,client_buf,MAXLINE))
    {
        //
        if (!strncasecmp(client_buf, "Content-Length", 14)){
			request_bodylen = atoi(client_buf + 15);
        }
        
        sprintf(request,"%s%s",request,client_buf);
        
        //End of the header
        if(!strcmp(client_buf,"\r\n")){
            break;
        }
    }

    // Error(-1) or EOF(0), then a header error occurs. Return directly.
    if(read_bytes<=0)
        return;

    int serverfd;

    serverfd = Open_clientfd(target,port);
    Rio_readinitb(&server_rio,serverfd);

    Rio_writen(serverfd,request,strlen(request));
    /* Send request body to server*/
    if(request_bodylen != 0)
    {
        for(int written_size = 0; written_size < request_bodylen; written_size++){
            read_bytes = Rio_readnb(&client_rio,client_buf,1);
            if(read_bytes<0 && written_size< request_bodylen-1)
            {
                Close(serverfd);
                return;
            }
            // Serverfd needs to be closed before returning.
            write_bytes = rio_writen(serverfd,client_buf,1);
            if(write_bytes!=1){
                Close(serverfd);
                return;
            }
        }
    }
    /* Receive response from server*/
    /* Response header */
    int response_bodylen = 0,response_headerlen=0;
    char response[MAXLINE];
    memset(response,0,MAXLINE);
    for(read_bytes= rio_readlineb(&server_rio,server_buf,MAXLINE);read_bytes>0;read_bytes=rio_readlineb(&server_rio,server_buf,MAXLINE))
    {   
        if(read_bytes<=0){
            Close(serverfd);
            return;
        }
        response_headerlen += read_bytes;
        if (!strncasecmp(server_buf, "Content-Length", 14)){
			response_bodylen = atoi(server_buf + 15);
        }
        sprintf(response,"%s%s",response,server_buf);
        
        if(!strcmp(server_buf,"\r\n")){
            break;
        }
    }
  
    Rio_writen(clientfd,response,strlen(response));
    if(read_bytes<=0){
        Close(serverfd);
        return;
    }

    if(response_bodylen != 0)
    {
        for(int writtensize = 0;writtensize < response_bodylen;writtensize++){
            read_bytes=Rio_readnb(&server_rio,server_buf,1);
            if(read_bytes<=0 && writtensize<response_bodylen-1){
                Close(serverfd);
                return;
            }

            write_bytes = rio_writen(clientfd,server_buf,1);
            if(write_bytes!=1){
                Close(serverfd);
                return;
            }
        } 
    }

    Close(serverfd);
    char log[MAXLINE];
    format_log_entry(log,&sockaddr,uri,response_bodylen+response_headerlen);
    
    P(&mutex);
    printf("%s\n",log);
    V(&mutex);
}