#ifndef _RDT_3_H_
#define _RDT_3_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#define MAX_MSG_LEN 1000
#define ERROR -1
#define TRUE 1
#define FALSE 0
#define SUCCESS 1
#define DEBUG 1
#define ALPHA 0.125
#define BETA 0.25

#define WINDOW_SIZE 10

typedef uint16_t hsize_t;
typedef uint16_t hcsum_t;
typedef uint32_t hseq_t;
typedef struct timeval htime_t;

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
    int pkt_acked;
};

typedef struct hdr header;

struct pkt {
	header header;
	unsigned char payload[MAX_MSG_LEN];
};

typedef struct pkt packet;

typedef struct {
    packet *packets;
    int total_packets;
} chunks_info;

extern double estimatedRTT, devRTT;
extern struct timeval timeOutInterval;

extern hseq_t _snd_seqnum;
extern hseq_t _rcv_seqnum;

extern hseq_t snd_base;
extern hseq_t rcv_base;
extern hseq_t next_seq_num;

int rdt_send(int sockfd, void *buf, int buf_len, struct sockaddr_in *dest);
int rdt_recv(int sockfd, void *buf, int buf_len, struct sockaddr_in *src);

chunks_info divide_file_to_chunks(int buf_len, void *buf);

void handle_error(const char *message);
void set_window();

#endif