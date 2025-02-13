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

void format_timestamp(double timestamp) {
    time_t rawtime = (time_t)timestamp;
    double fractional = timestamp - (time_t)timestamp;
    struct tm ts;
    char buf[80];
    
    ts = *localtime(&rawtime);
    double msec = fractional * 1000;
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S", &ts);
    char final_buf[100];
    char tz[6];
    strftime(tz, sizeof(tz), "%Z", &ts);
    snprintf(final_buf, sizeof(final_buf), "%s.%03.0f", buf, msec);
    printf("%s\n", final_buf);
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

chunks_info divide_file_to_chunks(int buf_len, void *buf) {
	int total_packets = (buf_len + MAX_MSG_LEN - 1) / MAX_MSG_LEN;
    
    packet *packets = (packet *)malloc(total_packets * sizeof(packet));
    if (packets == NULL) {
        handle_error("rdt_send: malloc failed");
    }

	for (int i = 0; i < total_packets; i++) {
        int chunk_size;
        
        if (i == total_packets - 1) {
            chunk_size = buf_len - (i * MAX_MSG_LEN);
        } else {
            chunk_size = MAX_MSG_LEN;
        }
        
        if (make_pkt(&packets[i], PKT_DATA, _snd_seqnum + i, buf + (i * MAX_MSG_LEN), chunk_size, NULL) < 0) {
            free(packets);
            handle_error("rdt_send: make_pkt failed");
        }

		printf("  Pacote criado com sucesso:\n");
        printf("    Número de sequência: %d\n", packets[i].header.pkt_seq_num);
        printf("    Tamanho total: %d bytes\n", packets[i].header.pkt_size);
        printf("    Checksum: %d\n", packets[i].header.pkt_checksum);
        printf("\n");
    }

    chunks_info result;
    result.total_packets = total_packets;
    result.packets = packets;
    return result;
}