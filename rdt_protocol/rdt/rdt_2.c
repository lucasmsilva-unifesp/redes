#include "rdt_2.h"

int biterror_inject = TRUE;

#define PKT_ACK 0
#define PKT_DATA 1

hseq_t _snd_seqnum = 1;
hseq_t _rcv_seqnum = 1;

unsigned short checksum(unsigned short *buf, int nbytes){
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

int iscorrupted(pkt *pr){
	pkt pl = *pr;
	pl.h.csum = 0;

	unsigned short csuml;
	csuml = checksum((void *)&pl, pl.h.pkt_size);
	if (csuml != pr->h.csum){
		return TRUE;
	}

	return FALSE;
}

int make_pkt(pkt *p, htype_t type, hseq_t seqnum, void *msg, int msg_len, htime_t time) {
	if (msg_len > MAX_MSG_LEN) {
		printf("make_pkt: tamanho da msg (%d) maior que limite (%d).\n",
		msg_len, MAX_MSG_LEN);
		return ERROR;
	}

	p->h.pkt_size = sizeof(hdr);
	p->h.csum = 0;
	p->h.pkt_type = type;
	p->h.pkt_seq = seqnum;
	p->h.pkt_time = time;

	if (msg_len > 0) {
		p->h.pkt_size += msg_len;
		memset(p->msg, 0, MAX_MSG_LEN);
		memcpy(p->msg, msg, msg_len);
	}

	p->h.csum = checksum((unsigned short *)p, p->h.pkt_size);

	return SUCCESS;
}

int has_ackseq(pkt *p, hseq_t seqnum) {
	if (p->h.pkt_type != PKT_ACK || p->h.pkt_seq != seqnum)
		return FALSE;
	return TRUE;
}

int rdt_send(int sockfd, void *buf, int buf_len, struct sockaddr_in *dst) {
	pkt p, ack;
	struct sockaddr_in dst_ack;
	int ns, nr, addrlen;
	struct timeval ts, te, timeout;
	struct timezone tz;

	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	// TODO: error
	gettimeofday(&ts, &tz);

	if (make_pkt(&p, PKT_DATA, _snd_seqnum, buf, buf_len, ts.tv_sec + ts.tv_usec/1e6) < 0)
		return ERROR;

resend:
	ns = sendto(sockfd, &p, p.h.pkt_size, 0,
			(struct sockaddr *)dst, sizeof(struct sockaddr_in));
	if (ns < 0) {
		perror("rdt_send: sendto(PKT_DATA):");
		return ERROR;
	}

	addrlen = sizeof(struct sockaddr_in);

	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(timeout)) < 0) {
		perror("setsockopt(..., SO_RCVTIMEO, ...)");
		return ERROR;
	}

	nr = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&dst_ack,
		(socklen_t *)&addrlen);

	// TODO: error
	gettimeofday(&te, &tz);
	
	if (nr < 0) {
    	if (errno == EAGAIN || errno == EWOULDBLOCK) {
			printf("Timeout ocorreu em recvfrom\n");
			goto resend;
		} else {
			printf("rdt_send: recvfrom(PKT_ACK): %d\n", nr);
			perror("rdt_send: recvfrom(PKT_ACK)");
			return ERROR;
		}
	}

	if (iscorrupted(&ack) || !has_ackseq(&ack, _snd_seqnum)) {
		printf("rdt_send: iscorrupted || !has_ackseq");
		goto resend;
	}

	_snd_seqnum++;

	printf("time send: %f\n", (te.tv_sec + te.tv_usec/1e6) - ack.h.pkt_time);

	return buf_len;
}

int has_dataseqnum(pkt *p, hseq_t seqnum) {
	if (p->h.pkt_seq != seqnum || p->h.pkt_type != PKT_DATA)
		return FALSE;
	return TRUE;
}

int rdt_recv(int sockfd, void *buf, int buf_len, struct sockaddr_in *src) {
	pkt p, ack;
	int nr, ns;
	int addrlen;
	struct timeval ts, te;
	struct timezone tz;

	memset(&p, 0, sizeof(hdr));

	// TODO: error
	gettimeofday(&ts, &tz);
	
	if (make_pkt(&ack, PKT_ACK, _rcv_seqnum - 1, NULL, 0, ts.tv_sec + ts.tv_usec/1e6) < 0)
		return ERROR;

rerecv:
	addrlen = sizeof(struct sockaddr_in);

	nr = recvfrom(sockfd, &p, sizeof(pkt), 0, (struct sockaddr*)src,
		(socklen_t *)&addrlen);
	if (nr < 0) {
		perror("recvfrom():");
		return ERROR;
	}

	if (!biterror_inject && (iscorrupted(&p) || !has_dataseqnum(&p, _rcv_seqnum))) {
		printf("rdt_recv: iscorrupted || has_dataseqnum \n");

		// enviar ultimo ACK (_rcv_seqnum - 1)
		ns = sendto(sockfd, &ack, ack.h.pkt_size, 0,
			(struct sockaddr*)src, (socklen_t)sizeof(struct sockaddr_in));
		if (ns < 0) {
			perror("rdt_rcv: sendto(PKT_ACK - 1)");
			return ERROR;
		}

		goto rerecv;
	}

	int msg_size = p.h.pkt_size - sizeof(hdr);
	if (msg_size > buf_len) {
		printf("rdt_rcv(): tamanho insuficiente de buf (%d) para payload (%d).\n", 
			buf_len, msg_size);
		return ERROR;
	}

	memcpy(buf, p.msg, msg_size);

    // TODO: error
	gettimeofday(&ts, &tz);
	
	if (make_pkt(&ack, PKT_ACK, p.h.pkt_seq, NULL, 0, ts.tv_sec + ts.tv_usec/1e6) < 0)
		return ERROR;

	if (!biterror_inject) {
		ns = sendto(sockfd, &ack, ack.h.pkt_size, 0,
			(struct sockaddr*)src, (socklen_t)sizeof(struct sockaddr_in));
		if (ns < 0) {
			perror("rdt_rcv: sendto(PKT_ACK)");
			return ERROR;
		}
	}

	printf("rdt_recv: Recebido pacote com seqnum=%d\n", p.h.pkt_seq);
	printf("rdt_recv: Enviando ACK com seqnum=%d\n", ack.h.pkt_seq);

	_rcv_seqnum++;

	return p.h.pkt_size - sizeof(hdr);
}