#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "http.h"

#define BUFSIZE 512

const char *get_mime_type(const char *file_extension) {
    if (strcmp(".txt", file_extension) == 0) {
        return "text/plain";
    } else if (strcmp(".html", file_extension) == 0) {
        return "text/html";
    } else if (strcmp(".jpg", file_extension) == 0) {
        return "image/jpeg";
    } else if (strcmp(".png", file_extension) == 0) {
        return "image/png";
    } else if (strcmp(".pdf", file_extension) == 0) {
        return "application/pdf";
    }

    return NULL;
}

int read_http_request(int fd, char *resource_name) {
    char buf[BUFSIZE];
    if (read(fd,buf,sizeof(buf)) <= 0) {
        perror("Read 0 bytes\n");
        return -1;
    }
    //printf("Read request:\n%s",buf);
    // get name of requested file
    if (sscanf(buf, "GET %s HTTP/1.0\r\n", resource_name) != 1) {
        fprintf(stderr, "Invalid request\n");
        return -1;
    }
    while (strstr(buf,"\r\n\0") == NULL) {
        if (read(fd,buf,sizeof(buf)) < 0) {
            perror("read\n");
            return -1;
        }

    }

    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    if (access(resource_path, F_OK) != -1) {
        //File exists
        int localfd = open(resource_path, O_RDONLY);
        if (localfd == -1) {
            fprintf(stderr,"open\n");
            return -1;
        }
        struct stat st;
        if (fstat(localfd,&st) == -1) {
            perror("fstat\n");
            close(localfd);
            return -1;
        }
        long fileSize = st.st_size;
        long fileBytesWritten = 0;
        char response[BUFSIZE] = "HTTP/1.0 200OK\r\n";
        char str[100];
        const char *dot =  strrchr(resource_path,'.');
        sprintf(str,"Content-Type %s\r\n",get_mime_type(dot));
        strcat(response,str);
        sprintf(str,"Content-Length: %ld\r\n\r\n",fileSize);
        strcat(response,str);

        //printf("- - - - -\nResponding header length %ld:\n%s",strlen(response),response);
        if (write(fd,response,strlen(response)) <= 0) {
            perror("writing header failed\n");
            close(localfd);
            return -1;
        }
        char buf[BUFSIZE];
        int bytes_read;

        while ((bytes_read = read(localfd, buf, sizeof(buf))) > 0) {
            int bytes_written = write(fd, buf, bytes_read);
            fileBytesWritten += bytes_written;
            //printf("Written bytes: %d, Total bytes: %ld",bytes_read,fileBytesWritten);
            if (bytes_written <= 0 || bytes_written != bytes_read) {
                perror("Write failed\n");
                close(localfd);
                return -1;
            }

        }
        if (bytes_read < 0) {
            perror("read");
            close(localfd);
            return -1;
        }
        if (fileBytesWritten != fileSize) {
            printf("Sent wrong number of bytes\n");
        }
        memset(buf,0,sizeof(buf));
    }
    else {
        //File doesn't exist, write 404 error back
        const char* error404 = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        if (write(fd,error404,strlen(error404)) <= 0) {
            perror("404 message failed to write\n");
            return -1;
        }
        
    }
    
    if(close(fd) != 0){
        perror("close");
        return -1;
    }

    return 0;
}
