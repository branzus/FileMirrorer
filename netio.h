#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include "filester.h"

#ifndef NETIO_H
#define NETIO_H

#define FILE_REQUEST_COMMAND 0xF00D

// set_addr creates a structure of type sockaddr_in with 
// the desired address and port
int set_addr(struct sockaddr_in *addr, char *name, u_int32_t inaddr, short port);

int stream_read(int sockfd, void *buff, int len);

int stream_write(int sockfd, const void *buff, int len);

int send_file(int sockfd, const char *file);

int get_file(int sockfd, const char *file, uint16_t fileIndex);

//int send_fileinfo(int sockfd, const file_info* fileinfo);
int send_filelist(int sockfd, const file_info *filelist, uint32_t length);

//int get_fileinfo(int sockfd, file_info* fileinfo);
int get_filelist(int sockfd, file_info **filelist, uint32_t *length);

#endif
