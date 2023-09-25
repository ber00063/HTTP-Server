#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5

int keep_going = 1;

void handle_sigint(int signo) {
    keep_going = 0;
}

int main(int argc, char **argv) {
    // First command is directory to serve, second command is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }

    struct sigaction sact;
    sact.sa_handler = handle_sigint;
    if(sigemptyset(&sact.sa_mask) == -1){
        perror("sigemptyset");
        return 1;
    }
    sact.sa_flags = 0;
    //Setup SIGINT handler
    if (sigaction(SIGINT, &sact, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    const char *serve_dir = argv[1];
    const char *port = argv[2];

    //Setup addrinfo structs
    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int status = getaddrinfo(NULL,port,&hints,&res);
    if (status != 0) {
        perror("getaddrinfo\n");
        return 1;
    }
    //creates socket
    int sockfd = socket(res->ai_family,res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }
    //binds to socket
    if (bind(sockfd,res->ai_addr,res->ai_addrlen) == -1) {
        perror("bind");
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }

    if (listen(sockfd,LISTEN_QUEUE_LEN) == -1) { //backlog = 5 maximum connections is 10
        perror("listen\n");
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }

    freeaddrinfo(res);

    printf("Server listening on port %s\n",port);
    while(keep_going) { //Server loop
        //get client info
        struct sockaddr_storage clientaddr;
        socklen_t addr_size = sizeof(clientaddr);
        //accept and get client fd
        int client_fd = accept(sockfd,(struct sockaddr *)&clientaddr,&addr_size);
        if (client_fd == -1) {
            if (errno != EINTR) { // Checks whether accept failed or was interrupted
                perror("accept\n");
            }
            break;
        }
        printf("Client connected\n");
        //Call read_http_request()
        char localpath[BUFSIZE];
        if (read_http_request(client_fd,localpath) != 0) {
            fprintf(stderr,"Read http request failed\n");
            continue;
        }
        //Convert requested resource name to proper file path
        char fullPath[strlen(serve_dir)+strlen(localpath)+1];
        strcpy(fullPath,serve_dir);
        strcat(fullPath,localpath);

        //Call write_http_response()
        if (write_http_response(client_fd,fullPath) != 0) {
            fprintf(stderr,"Failed to write http request\n");
            continue;
        }
        printf("Response sent\n");

        //if(
        close(client_fd);// != 0){
            //printf("%d\n",client_fd);
            //close(sockfd);
            //return 1;
        //}
    }
    
    if(close(sockfd) != 0){
        perror("close");
    }

    return 0;
    
}
