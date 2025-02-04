#include "rdt_3.h"

void handle_error(const char *message) {
    perror(message);
    exit(ERROR);
}

void timeout_interval(double sampleRTT) {
	long double errorRTT, timeOutIntervalRTT;

	estimatedRTT = (1 - ALPHA) * estimatedRTT + ALPHA * sampleRTT;
	errorRTT = fabs(sampleRTT - estimatedRTT);
	devRTT = (1 - BETA) * devRTT + BETA * errorRTT;
	timeOutIntervalRTT = estimatedRTT + 4 * devRTT;

	timeOutInterval.tv_sec = (time_t) timeOutIntervalRTT;
	timeOutInterval.tv_usec = (suseconds_t) ((timeOutIntervalRTT - timeOutInterval.tv_sec) * 1e6);
}

hcsum_t checksum(unsigned short *buf, size_t nbytes) {
	register uint32_t sum = 0;
	const uint16_t *ptr = (const uint16_t *)buf;

	while (nbytes > 1) {
		sum += *(ptr++);
		nbytes -= 2;
	}

	if (nbytes == 1)
		sum += *(uint8_t *) ptr;
	
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	
	return (hcsum_t) ~sum;
}

int is_corrupted(packet *packetReceive) {
	packet pkt_temp = *packetReceive;
	pkt_temp.header.pkt_checksum = 0;

	unsigned short pkt_temp_checksum;
	pkt_temp_checksum = checksum((void *)&pkt_temp, pkt_temp.header.pkt_size);
	if (pkt_temp_checksum != packetReceive->header.pkt_checksum){
		return TRUE;
	}

	return FALSE;
}

int has_ackseq(packet *packet, hseq_t seqnum) {
	if (packet->header.pkt_type != PKT_ACK || packet->header.pkt_seq_num != seqnum)
		return FALSE;
	return TRUE;
}

int has_dataseqnum(packet *packet, hseq_t seqNum) {
	if (packet->header.pkt_seq_num != seqNum || packet->header.pkt_type != PKT_DATA)
		return FALSE;
	return TRUE;
}

int timeval_compare(struct timeval *pkt_time, struct timeval *current_time, int print) {
    double pkt_total = pkt_time->tv_sec * 1000000 + pkt_time->tv_usec;
    double timeout_total = timeOutInterval.tv_sec * 1000000 + timeOutInterval.tv_usec;
    double sum = pkt_total + timeout_total;
    double current = current_time->tv_sec * 1000000 + current_time->tv_usec;
    
	if (print) {
		printf("Packet time: %lf microsec\n", pkt_total);
		printf("Timeout interval: %lf microsec\n", timeout_total);
		printf("Sum (pkt + timeout): %lf microsec\n", sum);
		printf("Current time: %lf microsec\n", current);
		printf("Diferenca (sum - current): %lf microsec\n", sum - current);
		printf("Tempo de criação do primeiro pacote: %ld.%06ld\n", pkt_time->tv_sec, pkt_time->tv_usec);
	}

    if(sum < current)
        return 1;
    else
        return 0;
}

// int timeval_compare(struct timeval *t1, struct timeval *t2) {
//     if (t1->tv_sec < t2->tv_sec)
//         return -1;
//     if (t1->tv_sec > t2->tv_sec)
//         return 1;
//     if (t1->tv_usec < t2->tv_usec)
//         return -1;
//     if (t1->tv_usec > t2->tv_usec)
//         return 1;
//     return 0;
// }

// void check_timeouts(int sockfd, struct sockaddr_in *dest) {
//     struct timeval now;
//     gettimeofday(&now, NULL);
    
//     for (int i = snd_base; i < snd_base + WINDOW_SIZE; i++) {
//         if (i >= next_seq_num) break;
        
//         SentPacket *sp = &snd_window[i % WINDOW_SIZE];
//         if (!sp->acked) {
//             double elapsed = (now.tv_sec - sp->send_time.tv_sec) + 
//                            (now.tv_usec - sp->send_time.tv_usec) / 1e6;
                           
//             if (elapsed > TIMEOUT) {
//                 // Retransmissão
//                 sendto(sockfd, &sp->pkt, sp->pkt.header.pkt_size, 0,
//                       (struct sockaddr *)dest, sizeof(struct sockaddr_in));
//                 gettimeofday(&sp->send_time, NULL);
//             }
//         }
//     }
// }

// void process_ack(packet ack_pkt) {
//     if (!is_corrupted(&ack_pkt) && ack_pkt.header.pkt_type == PKT_ACK) {
//         hseq_t ack_num = ack_pkt.header.pkt_seq_num;
        
//         // Verifica se o ACK está dentro da janela atual
//         if ((ack_num - snd_base) % MAX_SEQ_NUM < WINDOW_SIZE) {
//             snd_window[ack_num % WINDOW_SIZE].acked = 1;
            
//             // Avança snd_base enquanto os pacotes estiverem confirmados
//             while (snd_window[snd_base % WINDOW_SIZE].acked) {
//                 snd_base = (snd_base + 1) % MAX_SEQ_NUM;
//             }
//         }
//     }
// }

// void ack_handler(int sockfd) {
//     packet ack_pkt;
//     struct sockaddr_in src;
//     socklen_t addrlen = sizeof(src);
    
//     while(1) {
//         if (recvfrom(sockfd, &ack_pkt, sizeof(packet), 0,
//                    (struct sockaddr *)&src, &addrlen) > 0) {
//             process_ack(ack_pkt);
//         }
//     }
// }