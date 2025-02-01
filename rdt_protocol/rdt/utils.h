#ifndef UTILS_H
#define UTILS_H

#include "rdt_3.h"

void handle_error(const char *message);
void timeout_interval(double sampleRTT);

hcsum_t checksum(unsigned short *buf, size_t nbytes);

int is_corrupted(packet *packetReceive);
int has_ackseq(packet *packet, hseq_t seqnum);
int has_dataseqnum(packet *packet, hseq_t seqNum);

void check_timeouts(int sockfd, struct sockaddr_in *dest);
void process_ack(packet ack_pkt);

#endif