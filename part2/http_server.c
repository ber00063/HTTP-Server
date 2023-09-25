#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "connection_queue.h"
#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define N_THREADS 5

int keep_going = 1;
const char *serve_dir;


void handle_sigint(int signo) {
    keep_going = 0;
    //printf("SIGINT Received\n");
}

void *thread_func(void *queue){
        queue = (connection_queue_t *)queue;
        while(keep_going){
            int client_fd = connection_dequeue(queue);
            if(client_fd == -1){
                return (void *)1;
            }

            //Call read_http_request()
            char localpath[BUFSIZE];
            if (read_http_request(client_fd,localpath) != 0) {
                fprintf(stderr,"Read http request failed\n");
                continue;
            }
            //printf("thread func %s\n%s\n",localpath,serve_dir);
            //Convert requested resource name to proper file path
            char fullPath[strlen(serve_dir)+strlen(localpath)+1];
            strcpy(fullPath,serve_dir);
            strcat(fullPath,localpath);
            //Call write_http_response()
            if (write_http_response(client_fd,fullPath) != 0) {
                fprintf(stderr,"Failed to write http request\n");
                continue;
            }
            //if(
            close(client_fd);
                //perror("close");
                //return (void *)1;
            //}
            //printf("Response sent\n");

        }

        return (void *)0;
}



int main(int argc, char **argv) {
    // First command is directory to serve, second command is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }
    
    int error;

    connection_queue_t queue;
    if(connection_queue_init(&queue) != 0){
        return 1;
    }


    struct sigaction sact;
    sact.sa_handler = handle_sigint;
    if(sigemptyset(&sact.sa_mask) != 0){
        perror("sigemptyset");
        connection_queue_free(&queue);
        return 1;
    }
    sact.sa_flags = 0;
    //Setup SIGINT handler
    if (sigaction(SIGINT, &sact, NULL) == -1) {
        perror("sigaction\n");
        connection_queue_free(&queue);
        return 1;
    }

    serve_dir = argv[1];
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
        connection_queue_free(&queue);
        return 1;
    }
    //creates socket
    int sockfd = socket(res->ai_family,res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket\n");
        freeaddrinfo(res);
        connection_queue_free(&queue);
        return 1;
    }
    //binds to socket
    if (bind(sockfd,res->ai_addr,res->ai_addrlen) == -1) {
        perror("bind\n");
        close(sockfd);
        freeaddrinfo(res);
        connection_queue_free(&queue);
        return 1;
    }

    if (listen(sockfd,LISTEN_QUEUE_LEN) == -1) { //backlog = 5 maximum connections is 10
        perror("listen\n");
        close(sockfd);
        freeaddrinfo(res);
        connection_queue_free(&queue);
        return 1;
    }

    freeaddrinfo(res);

    //set process mask to all signals before creating threads
    sigset_t oldset;
    sigset_t newset;
    if(sigfillset(&newset) != 0){
        perror("sigfillset");
        close(sockfd);
        connection_queue_free(&queue);
        return 1;
    }
    if(sigprocmask(SIG_SETMASK, &newset, &oldset) != 0){
        perror("sigprocmask");
        close(sockfd);
        connection_queue_free(&queue);
        return 1;
    }
    pthread_t threads[N_THREADS];
    for(int i = 0; i < N_THREADS; i++){
        if((error = pthread_create(threads+i, NULL, thread_func, &queue)) != 0){
            fprintf(stderr, "pthread_create failed: %s\n", strerror(error));
            for(int y = 0; y < i; y++){
                pthread_cancel(threads[y]);
            }
            close(sockfd);
            connection_queue_free(&queue);
            return 1;
        }
    }
    //return process mask after creating threads
    if(sigprocmask(SIG_SETMASK, &oldset, NULL) != 0){
        perror("sigprocmask");
        close(sockfd);
        connection_queue_free(&queue);
        return 1;
    }

    while(keep_going) { //Server loop
        //get client info
        struct sockaddr_storage clientaddr;
        socklen_t addr_size = sizeof(clientaddr);
        //accept and get client fd
        int client_fd = accept(sockfd,(struct sockaddr *)&clientaddr,&addr_size);
        if (client_fd == -1) {
            if (errno != EINTR) { // Checks whether accept failed or was interrupted
                perror("accept");
                for(int y = 0; y < N_THREADS; y++){
                    pthread_cancel(threads[y]);
                }
                close(sockfd);
                connection_queue_free(&queue);
                return 1;
            }
            break;
        }
        //printf("Client connected\n");
        if(connection_enqueue(&queue, client_fd) == -1){
            for(int y = 0; y < N_THREADS; y++){
                pthread_cancel(threads[y]);
            }
            close(sockfd);
            connection_queue_free(&queue);
            return 1;

        }

    }
    //shutdown queue
    if(connection_queue_shutdown(&queue) != 0){
        for(int i = 0; i < N_THREADS; i++){
            pthread_cancel(threads[i]);
        }
        close(sockfd);
        connection_queue_free(&queue);
        return 1;
    }

    //join threads
    for(int i = 0; i < N_THREADS; i++){
        if((error = pthread_join(threads[i], NULL)) != 0){
            fprintf(stderr, "pthread_join failed: %s\n", strerror(error));
            for(int j = i; j < N_THREADS; j++){
                pthread_cancel(threads[j]);
            }
            close(sockfd);
            connection_queue_free(&queue);
            return 1;
        }
        
    }

    
    //close server fd
    if(close(sockfd) == -1){
        perror("close");
        connection_queue_free(&queue);
        return 1;
    }

    //free queue
    if(connection_queue_free(&queue) != 0){
        return 1;
    }

    return 0;
}

