#include <stdio.h>
#include <string.h>
#include "connection_queue.h"

int connection_queue_init(connection_queue_t *queue) {
    //initialize length and shutdown to zero
    queue->length = 0;
    queue->shutdown = 0;
    int error;

    //initialize mutex and conditions
    if((error = pthread_mutex_init(&queue->lock, NULL)) != 0){
        fprintf(stderr, "pthread_mutex_init failed: %s\n", strerror(error));
        return -1;
    }
    if((error = pthread_cond_init(&queue->queue_full, NULL)) != 0){
        fprintf(stderr, "pthread_cond_init failed: %s\n", strerror(error));
        pthread_mutex_destroy(&queue->lock);
        return -1;
    }
    if((error = pthread_cond_init(&queue->queue_empty, NULL)) != 0){
        fprintf(stderr, "pthread_cond_init failed: %s\n", strerror(error));
        pthread_mutex_destroy(&queue->lock);
        pthread_cond_destroy(&queue->queue_full);
        return -1;
    }
    return 0;
}

int connection_enqueue(connection_queue_t *queue, int connection_fd) {
    int error;
    if((error = pthread_mutex_lock(&queue->lock)) != 0){
       fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(error));
        return -1;
    }

    //wait while queue is full
    while(queue->length == CAPACITY){
        if((error = pthread_cond_wait(&queue->queue_full, &queue->lock)) != 0){
            fprintf(stderr, "pthread_cond_wait failed: %s\n", strerror(error));
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }
    }

    //check if shutdown occurred
    if(queue->shutdown == 1){
        if((error = pthread_mutex_unlock(&queue->lock)) != 0){
            fprintf(stderr, "pthread_mutex_unlock failed: %s\n", strerror(error));
        }
        return 1;
    }

    queue->client_fds[queue->length]=connection_fd;
    queue->length++;
    
    //signal queue_empty
    if((error = pthread_cond_signal(&queue->queue_empty)) != 0){
        fprintf(stderr, "pthread_cond_signal failed: %s\n", strerror(error));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    if((error = pthread_mutex_unlock(&queue->lock)) != 0){
        fprintf(stderr, "pthread_mutex_unlock failed: %s\n", strerror(error));
        return -1;
    }
    return 0;
}

int connection_dequeue(connection_queue_t *queue) {
    int error;


    if((error = pthread_mutex_lock(&queue->lock)) != 0){
        fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(error));
        return -1;
    }

    

    //wait while queue is empty 
    while(queue->length == 0){
        if((error = pthread_cond_wait(&queue->queue_empty, &queue->lock)) != 0){
            fprintf(stderr, "pthread_cond_wait failed: %s\n", strerror(error));
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }
        if (queue->shutdown == 1) {
            break;
        }
    }

    //chek if shutdown occured and queue is empty
    if((queue->shutdown == 1) && (queue->length == 0)){
        if((error = pthread_mutex_unlock(&queue->lock)) != 0){
            fprintf(stderr, "pthread_mutex_unlock failed: %s\n", strerror(error));
        }
        return -1;
    }

    queue->length--;
    int dequeued_fd = queue->client_fds[queue->length];
    queue->client_fds[queue->length]=-1;
    
    //signal queue_full
    if((error = pthread_cond_signal(&queue->queue_full)) != 0){
        fprintf(stderr, "pthread_cond_signal failed: %s\n", strerror(error));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }


    if((error = pthread_mutex_unlock(&queue->lock)) != 0){
        fprintf(stderr, "pthread_mutex_unlock failed: %s\n", strerror(error));
        return -1;
    }

    return dequeued_fd;
}

int connection_queue_shutdown(connection_queue_t *queue) {
    int error;

    if((error = pthread_mutex_lock(&queue->lock)) != 0){
        fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(error));
        return -1;
    }

    //set shutdown to 1 to let threads know shutdown occurred
    queue->shutdown = 1;

    //broadcast threads waiting on queue_full
    if((error = pthread_cond_broadcast(&queue->queue_full)) != 0){
        fprintf(stderr, "pthread_cond_broadcast failed: %s\n", strerror(error));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    //broadcast threads waiting on queue_empty
    if((error = pthread_cond_broadcast(&queue->queue_empty)) != 0){
        fprintf(stderr, "pthread_cond_broadcast failed: %s\n", strerror(error));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }


    if((error = pthread_mutex_unlock(&queue->lock)) != 0){
        fprintf(stderr, "pthread_mutex_unlock failed: %s\n", strerror(error));
        return -1;
    }
    
    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    int error;

    //destroy mutex and both conditions
    if((error = pthread_mutex_destroy(&queue->lock)) != 0){
        fprintf(stderr, "pthread_mutex_destroy failed: %s\n", strerror(error));
        pthread_cond_destroy(&queue->queue_full);
        pthread_cond_destroy(&queue->queue_empty);
        return -1;
    }
    if((error = pthread_cond_destroy(&queue->queue_full)) != 0){
        fprintf(stderr, "pthread_cond_destroy: %s\n", strerror(error));
        pthread_cond_destroy(&queue->queue_empty);
        return -1;
    }
    if((error = pthread_cond_destroy(&queue->queue_empty)) != 0){
        fprintf(stderr, "pthread_cond_destroy failed: %s\n", strerror(error));
        return -1;
    }
    return 0;
}
