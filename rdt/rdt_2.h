#ifndef _RDT_2_H_
#define _RDT_2_H_

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_MSG_LEN 1000
#define ERROR -1
#define TRUE 1
#define FALSE 0
#define SUCCESS 0

int rdt_send(int sockfd, void *buf, int buf_len, struct sockaddr_in *dst);
int rdt_recv(int sockfd, void *buf, int buf_len, struct sockaddr_in *src);

#endif