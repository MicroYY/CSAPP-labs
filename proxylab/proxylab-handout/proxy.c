#include "csapp.h"

#define HOSTNAME_MAX_LEN 63
#define PORT_MAX_LEN 10
#define HEADER_NAME_MAX_LEN 32
#define HEADER_VALUE_MAX_LEN 64
#define MAX_CHACHE_SIZE (1 << 20)
#define MAX_OBJECT_SIZE ((1 << 10) * 100)


/**
 * data structures
 */
typedef struct {
    char host[HOSTNAME_MAX_LEN];
    char port[PORT_MAX_LEN];
    char path[MAXLINE];
} ReqLine;

typedef struct {
    char name[HEADER_NAME_MAX_LEN];
    char value[HEADER_VALUE_MAX_LEN];
} ReqHeader;

typedef struct {
    char *name;
    char *object;
} CacheLine;

typedef struct {
    int used_cnt;
    CacheLine* objects;
} Cache;

/**
 * pre-declaration of functions
 */
void doit(int fd);
void parse_request(int fd, ReqLine *linep, ReqHeader *headers, int *num_hds);
void parse_uri(char *uri, ReqLine *linep);
ReqHeader parse_header(char *line);
int send_to_server(ReqLine *line, ReqHeader *headers, int num_hds);
void *thread(void *vargp);
void init_cache();
int reader(int fd, char *uri);
void writer(char *uri, char *buf);

/**
 * reader-writer model
 */
Cache cache;
int readcnt;
sem_t mutex, w;

int main(int argc, char **argv) 
{
    int listenfd, *connfd;
    pthread_t tid;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(1);
    }

    init_cache();
    listenfd = Open_listenfd(argv[1]);
    while (1) {
	    clientlen = sizeof(clientaddr);
        connfd = Malloc(sizeof(int));
	    *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        Pthread_create(&tid, NULL, thread, connfd);
    }
}

void init_cache() {
    Sem_init(&mutex, 0, 1);
    Sem_init(&w, 0, 1);
    readcnt = 0;
    cache.objects = (CacheLine*)Malloc(sizeof(CacheLine) * 10);
    cache.used_cnt = 0;
    for (int i = 0; i < 10; ++i) {
        cache.objects[i].name = Malloc(sizeof(char) * MAXLINE);
        cache.objects[i].object = Malloc(sizeof(char) * MAX_OBJECT_SIZE);
    }
}

void *thread(void *vargp) {
    int connfd = *((int*)vargp);
    Pthread_detach(pthread_self()); //将线程分离，让这个线程计数结束后自己回收资源
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

void doit(int fd) {
    char buf[MAXLINE], uri[MAXLINE], object_buf[MAX_OBJECT_SIZE];
    int total_size, connfd;
    rio_t rio;
    ReqLine request_line;
    ReqHeader headers[20];
    int num_hds, n;
    parse_request(fd, &request_line, headers, &num_hds);

    strcpy(uri, request_line.host);
    strcpy(uri+strlen(uri), request_line.path);
    if (reader(fd, uri)) {
        fprintf(stdout, "%s from cache\n", uri);
        fflush(stdout);
        return;
    }

    total_size = 0;
    connfd = send_to_server(&request_line, headers, num_hds);
    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE))) {
        Rio_writen(fd, buf, n);
        strcpy(object_buf + total_size, buf);
        total_size += n;
    }
    if (total_size < MAX_OBJECT_SIZE) {
        writer(uri, object_buf);
    }
    Close(connfd);
}

int reader(int fd, char *uri) {
    int in_cache= 0;
    P(&mutex);
    readcnt++;
    if (readcnt == 1) {
        P(&w);
    }
    V(&mutex);

    for (int i = 0; i < 10; ++i) {
        if (!strcmp(cache.objects[i].name, uri)) {
            Rio_writen(fd, cache.objects[i].object, MAX_OBJECT_SIZE);
            in_cache = 1;
            break;
        }
    }
    P(&mutex);
    readcnt--;
    if (readcnt == 0) {
        V(&w);
    }
    V(&mutex);
    return in_cache;
}

void writer(char *uri, char *buf) {
    P(&w);
    strcpy(cache.objects[cache.used_cnt].name, uri);
    strcpy(cache.objects[cache.used_cnt].object, buf);
    ++cache.used_cnt;
    V(&w);
}

void parse_request(int fd, ReqLine *linep, ReqHeader *headers, int *num_hds) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))  
        return;

    //parse request line
    sscanf(buf, "%s %s %s", method, uri, version);
    parse_uri(uri, linep);

    //parse request headers
    *num_hds = 0;
	Rio_readlineb(&rio, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
	    headers[(*num_hds)++] = parse_header(buf);
	    Rio_readlineb(&rio, buf, MAXLINE);
    }
}

void parse_uri(char *uri, ReqLine *linep) {
    if (strstr(uri, "http://") != uri) {
        fprintf(stderr, "Error: invalid uri!\n");
        pthread_exit(NULL);
    }
    uri += strlen("http://");
    char *c = strstr(uri, ":");
    *c = '\0';
    strcpy(linep->host, uri);
    uri = c + 1;
    c = strstr(uri, "/");
    *c = '\0';
    strcpy(linep->port, uri);
    *c = '/';
    strcpy(linep->path, c);
}

ReqHeader parse_header(char *line) {
    ReqHeader header;
    char *c = strstr(line, ": ");
    if (c == NULL) {
        fprintf(stderr, "Error: invalid header: %s\n", line);
        pthread_exit(NULL);
    }
    *c = '\0';
    strcpy(header.name, line);
    strcpy(header.value, c+2);
    return header;
}

int send_to_server(ReqLine *line, ReqHeader *headers, int num_hds) {
    int clientfd;
    char buf[MAXLINE], *buf_head = buf;
    rio_t rio;
    
    clientfd = Open_clientfd(line->host, line->port);
    Rio_readinitb(&rio, clientfd);
    sprintf(buf_head, "GET %s HTTP/1.0\r\n", line->path);
    buf_head = buf + strlen(buf);
    for (int i = 0; i < num_hds; ++i) {
        sprintf(buf_head, "%s: %s", headers[i].name, headers[i].value);
        buf_head = buf + strlen(buf);
    }
    sprintf(buf_head, "\r\n");
    Rio_writen(clientfd, buf, MAXLINE);
    
    return clientfd;
}