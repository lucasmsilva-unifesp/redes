#ifndef _RDT_2_H_
#define _RDT_2_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#define MAX_MSG_LEN 1000
#define ERROR -1
#define TRUE 1
#define FALSE 0
#define SUCCESS 1

typedef uint16_t hsize_t;
typedef uint16_t hcsum_t;
typedef uint16_t hseq_t;
typedef time_t   htime_t;

typedef enum {
    PKT_ACK = 0,
    PKT_DATA = 1
} PacketType;

struct hdr {
	hseq_t  pkt_seq_num;
	hsize_t pkt_size;
	PacketType pkt_type;
	hcsum_t pkt_checksum;
	htime_t pkt_time;
};

typedef struct hdr header;

struct pkt {
	header header;
	unsigned char payload[MAX_MSG_LEN];
};
typedef struct pkt packet;

int rdt_send(int sockfd, void *buf, int buf_len, struct sockaddr_in *dest);
int rdt_recv(int sockfd, void *buf, int buf_len, struct sockaddr_in *src);

void handle_error(const char *message);

#endif