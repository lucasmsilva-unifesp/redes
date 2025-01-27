#include "rdt_2.h"

static int biterror_inject = FALSE;
static int timeout_inject = FALSE;

double estimetedRTT = 0.0, devRTT = 0.0;
struct timeval timeOutInterval = {1, 500000};

hseq_t _snd_seqnum = 1;
hseq_t _rcv_seqnum = 1;

void handle_error(const char *message) {
    perror(message);
    exit(ERROR);
}

static void timeout_interval(double sampleRTT) {
	long double errorRTT, timeOutIntervalRTT;

	estimetedRTT = (1 - ALPHA) * estimetedRTT + ALPHA * sampleRTT;
	errorRTT = fabs(sampleRTT - estimetedRTT);
	devRTT = (1 - BETA) * devRTT + BETA * errorRTT;
	timeOutIntervalRTT = estimetedRTT + 4 * devRTT;

	timeOutInterval.tv_sec = (time_t) timeOutIntervalRTT;
	timeOutInterval.tv_usec = (suseconds_t) ((timeOutIntervalRTT - timeOutInterval.tv_sec) * 1e6);
}

static unsigned short checksum(unsigned short *buf, int nbytes){
	register long sum;
	sum = 0;

	while (nbytes > 1) {
		sum += *(buf++);
		nbytes -= 2;
	}

	if (nbytes == 1)
		sum += *(unsigned short *) buf;
	
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	
	return (unsigned short) ~sum;
}

static int is_corrupted(packet *packetReceive){
	packet pkt_temp = *packetReceive;
	pkt_temp.header.pkt_checksum = 0;

	unsigned short pkt_temp_checksum;
	pkt_temp_checksum = checksum((void *)&pkt_temp, pkt_temp.header.pkt_size);
	if (pkt_temp_checksum != packetReceive->header.pkt_checksum){
		return TRUE;
	}

	return FALSE;
}

static int make_pkt(packet *packet, PacketType type, hseq_t seqNum, void *msg, int msg_len, htime_t * time) {
	struct timeval time_start;

	if (msg_len > MAX_MSG_LEN) {
		printf("make_pkt: message size (%d) bigget than limit - (%d).\n",
		msg_len, MAX_MSG_LEN);
		return ERROR;
	}

	gettimeofday(&time_start, NULL);

	packet->header.pkt_size = sizeof(header);
	packet->header.pkt_checksum = 0;
	packet->header.pkt_type = type;
	packet->header.pkt_seq_num = seqNum;
	packet->header.pkt_time = (time == NULL) ? time_start.tv_sec + time_start.tv_usec/1e6 : *time;

	if (msg_len > 0) {
		packet->header.pkt_size += msg_len;
		memset(packet->payload, 0, MAX_MSG_LEN);
		memcpy(packet->payload, msg, msg_len);
	}

	packet->header.pkt_checksum = checksum((unsigned short *) packet, packet->header.pkt_size);

	return SUCCESS;
}

static int has_ackseq(packet *packet, hseq_t seqnum) {
	if (packet->header.pkt_type != PKT_ACK || packet->header.pkt_seq_num != seqnum)
		return FALSE;
	return TRUE;
}

int rdt_send(int sockfd, void *buf, int buf_len, struct sockaddr_in *dest) {
	packet packet, ack;
	struct sockaddr_in dest_ack;
	int ns, nr, addrlen;
	struct timeval time_end;
	double sampleRTT;

	if (make_pkt(&packet, PKT_DATA, _snd_seqnum, buf, buf_len, NULL) < 0)
		handle_error("rdt_send: make_pkt failed");

resend:
	printf("Sending packet: %d\n", _snd_seqnum);
	ns = sendto(sockfd, &packet, packet.header.pkt_size, 0,
			(struct sockaddr *)dest, sizeof(struct sockaddr_in));
	if (ns < 0) {
		handle_error("rdt_send: sendto(PKT_DATA):");
	}

wait_ack:
	addrlen = sizeof(struct sockaddr_in);

	struct timeval timeout = {timeOutInterval.tv_sec, timeOutInterval.tv_usec};
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(struct timeval)) < 0) {
		handle_error("setsockopt(..., SO_RCVTIMEO, ...)");
	}

	nr = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&dest_ack,
		(socklen_t *)&addrlen);
	
	if (nr < 0) {
    	if (errno == EAGAIN || errno == EWOULDBLOCK) {
			printf("Timeout reached. Resending packet.\n");
			goto resend;
		} else {
			printf("rdt_send: recvfrom(PKT_ACK): %d\n", nr);
			handle_error("rdt_send: recvfrom(PKT_ACK)");
		}
	}

	if (is_corrupted(&ack)) {
		printf("rdt_send: is_corrupted\n");
		goto wait_ack;
	}

	if (!has_ackseq(&ack, _snd_seqnum)) {
		printf("rdt_send: !has_ackseq, _snd_seqnum: %d, ack.header.pkt_seq_num: %d\n", _snd_seqnum, ack.header.pkt_seq_num);
		goto wait_ack;
	}

	if (DEBUG) {
		printf("rdt_send: Packet send with seqnum=%d\n", packet.header.pkt_seq_num);
		printf("rdt_recv: Received ACK with seqnum=%d\n", ack.header.pkt_seq_num);
	}

    gettimeofday(&time_end, NULL);

	_snd_seqnum++;

	sampleRTT = (time_end.tv_sec + time_end.tv_usec/1e6) - ack.header.pkt_time;
	timeout_interval(sampleRTT);

	if (DEBUG) {
		printf("rdt_send: sampleRTT: %f, estimetedRTT: %f, devRTT: %f, timeoutInterval: %ld.%06ld\n",
			sampleRTT, estimetedRTT, devRTT,
			timeOutInterval.tv_sec, timeOutInterval.tv_usec);
	}

	return buf_len;
}

static int has_dataseqnum(packet *packet, hseq_t seqNum) {
	if (packet->header.pkt_seq_num != seqNum || packet->header.pkt_type != PKT_DATA)
		return FALSE;
	return TRUE;
}

int rdt_recv(int sockfd, void *buf, int buf_len, struct sockaddr_in *src) {
	packet data, ack;
	int addrLen;

	memset(&data, 0, sizeof(header));

rerecv:
	addrLen = sizeof(struct sockaddr_in);

	if (recvfrom(sockfd, &data, sizeof(packet), 0, (struct sockaddr*)src,
		(socklen_t *)&addrLen) < 0) {
		handle_error("reccfrom():");
	}

	if (!biterror_inject) {
		if (is_corrupted(&data)) {
			printf("rdt_recv: iscorrupted - expected checksum: %hu, actual checksum: %hu\n",
           		checksum((void *)&data, data.header.pkt_size), data.header.pkt_checksum);

			if (make_pkt(&ack, PKT_ACK, _rcv_seqnum - 1, NULL, 0, &data.header.pkt_time) < 0)
				handle_error("rdt_recv: make_pkt failed");

			if (sendto(sockfd, &ack, ack.header.pkt_size, 0,
				(struct sockaddr*)src, (socklen_t)sizeof(struct sockaddr_in)) < 0) {
				handle_error("rdt_rcv: sendto(PKT_ACK - 1)");
			}

			goto rerecv;
		}

		if (!has_dataseqnum(&data, _rcv_seqnum)) {
			printf("rdt_recv: !has_dataseqnum, _rcv_seqnum: %d, data.header.pkt_seq_num: %d \n", _rcv_seqnum, data.header.pkt_seq_num);
			
			if (make_pkt(&ack, PKT_ACK, _rcv_seqnum-1, NULL, 0, &data.header.pkt_time) < 0)
				handle_error("rdt_recv: make_pkt failed");

			if (sendto(sockfd, &ack, ack.header.pkt_size, 0,
				(struct sockaddr*)src, (socklen_t)sizeof(struct sockaddr_in)) < 0) {
				handle_error("rdt_rcv: sendto(PKT_ACK - 1)");
			}

			goto rerecv;
		}
	}

	int msg_size = data.header.pkt_size - sizeof(header);
	if (msg_size > buf_len) {
		printf("rdt_rcv(): buffer size is insufficient (%d) for payload (%d).\n", 
			buf_len, msg_size);
		handle_error("rdt_rcv: buffer size");
	}

	memcpy(buf, data.payload, msg_size);
	
	if (make_pkt(&ack, PKT_ACK, data.header.pkt_seq_num, NULL, 0, &data.header.pkt_time) < 0)
		handle_error("rdt_recv: make_pkt failed");

	if (timeout_inject) {
		struct timeval time_start, time_end;

		gettimeofday(&time_start, NULL);
		gettimeofday(&time_end, NULL);
		while (time_end.tv_sec + time_end.tv_usec/1e6 - time_start.tv_sec - time_start.tv_usec/1e6 < 1) {
			gettimeofday(&time_end, NULL);
		}
	}

	if (!biterror_inject) {
		if (sendto(sockfd, &ack, ack.header.pkt_size, 0,
			(struct sockaddr*)src, (socklen_t)sizeof(struct sockaddr_in)) < 0) {
			handle_error("rdt_rcv: sendto(PKT_ACK)");
		}
	}

	if (DEBUG) {
		printf("rdt_recv: Packet received with seqnum=%d\n", data.header.pkt_seq_num);
		printf("rdt_recv: Sending ACK with seqnum=%d\n", ack.header.pkt_seq_num);
	}

	_rcv_seqnum++;

	return data.header.pkt_size - sizeof(header);
}