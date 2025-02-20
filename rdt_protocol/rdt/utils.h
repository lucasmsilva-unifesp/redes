#ifndef UTILS_H
#define UTILS_H

#include "rdt_3.h"

void handle_error(const char *message);
void timeout_interval(double sampleRTT);

hcsum_t checksum(unsigned short *buf, size_t nbytes);

int is_corrupted(packet *packetReceive);
int has_ackseq(packet *packet, hseq_t seqnum);
int has_dataseqnum(packet *packet, hseq_t seqNum);

chunks_info divide_file_to_chunks(int buf_len, void *buf);

#endif