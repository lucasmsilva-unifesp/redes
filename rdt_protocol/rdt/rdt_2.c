#include "rdt_2.h"

int biterror_inject = FALSE;

hseq_t _snd_seqnum = 1;
hseq_t _rcv_seqnum = 1;

void handle_error(const char *message) {
    perror(message);
    exit(ERROR);
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

static int make_pkt(packet *packet, PacketType type, hseq_t seqNum, void *msg, int msg_len) {
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
	packet->header.pkt_time = time_start.tv_sec + time_start.tv_usec/1e6;

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
	struct timeval time_end, timeout;

	if (make_pkt(&packet, PKT_DATA, _snd_seqnum, buf, buf_len) < 0)
		handle_error("rdt_send: make_pkt failed");

resend:
	ns = sendto(sockfd, &packet, packet.header.pkt_size, 0,
			(struct sockaddr *)dest, sizeof(struct sockaddr_in));
	if (ns < 0) {
		handle_error("rdt_send: sendto(PKT_DATA):");
	}

	addrlen = sizeof(struct sockaddr_in);

	timeout.tv_sec = 0;
	timeout.tv_usec = 500000;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(timeout)) < 0) {
		handle_error("setsockopt(..., SO_RCVTIMEO, ...)");
	}

	nr = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&dest_ack,
		(socklen_t *)&addrlen);

    printf("errno: %d \n", errno);
	
	if (nr < 0) {
    	if (errno == EAGAIN || errno == EWOULDBLOCK) {
			printf("Timeout reached. Resending packet.\n");
			goto resend;
		} else {
			printf("rdt_send: recvfrom(PKT_ACK): %d\n", nr);
			handle_error("rdt_send: recvfrom(PKT_ACK)");
		}
	}

	if (is_corrupted(&ack) || !has_ackseq(&ack, _snd_seqnum)) {
		printf("rdt_send: is_corrupted || !has_ackseq");
		goto resend;
	}

    gettimeofday(&time_end, NULL);

	_snd_seqnum++;

	printf("time send: %f\n", (time_end.tv_sec + time_end.tv_usec/1e6) - ack.header.pkt_time);

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

	if (make_pkt(&ack, PKT_ACK, _rcv_seqnum - 1, NULL, 0) < 0)
		handle_error("rdt_recv: make_pkt failed");

rerecv:
	addrLen = sizeof(struct sockaddr_in);

	if (recvfrom(sockfd, &data, sizeof(packet), 0, (struct sockaddr*)src,
		(socklen_t *)&addrLen) < 0) {
		handle_error("reccfrom():");
	}

	if (!biterror_inject && (is_corrupted(&data) || !has_dataseqnum(&data, _rcv_seqnum))) {
		printf("rdt_recv: iscorrupted || has_dataseqnum \n");

		if (sendto(sockfd, &ack, ack.header.pkt_size, 0,
			(struct sockaddr*)src, (socklen_t)sizeof(struct sockaddr_in)) < 0) {
			handle_error("rdt_rcv: sendto(PKT_ACK - 1)");
		}

		goto rerecv;
	}

	int msg_size = data.header.pkt_size - sizeof(header);
	if (msg_size > buf_len) {
		printf("rdt_rcv(): tamanho insuficiente de buf (%d) para payload (%d).\n", 
			buf_len, msg_size);
		handle_error("rdt_rcv: buffer size");
	}

	memcpy(buf, data.payload, msg_size);
	
	if (make_pkt(&ack, PKT_ACK, data.header.pkt_seq_num, NULL, 0) < 0)
		handle_error("rdt_recv: make_pkt failed");

	if (!biterror_inject) {
		if (sendto(sockfd, &ack, ack.header.pkt_size, 0,
			(struct sockaddr*)src, (socklen_t)sizeof(struct sockaddr_in)) < 0) {
			handle_error("rdt_rcv: sendto(PKT_ACK)");
		}
	}

	printf("rdt_recv: Packet received with seqnum=%d\n", data.header.pkt_seq_num);
	printf("rdt_recv: Sending ACK with seqnum=%d\n", ack.header.pkt_seq_num);

	_rcv_seqnum++;

	return data.header.pkt_size - sizeof(header);
}