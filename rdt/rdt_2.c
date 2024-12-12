#include "rdt_2.h"

struct hdr {
	unsigned short pkt_size;
	unsigned short csum;
};
typedef struct hdr hdr;

struct pkt {
	hdr h;
	unsigned char msg[MAX_MSG_LEN];
};
typedef struct pkt pkt;

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

int make_pkt(pkt *p, void *buf, int buf_len) {
	if (buf_len > MAX_MSG_LEN) {
		printf("make_pkt: tamanho da msg (%d) maior que limite (%d).\n",
		buf_len, MAX_MSG_LEN);
		return ERROR;
	}
	p->h.pkt_size = sizeof(hdr) + buf_len;
	p->h.csum = 0;
	memset(p->msg, 0, MAX_MSG_LEN);
	memcpy(p->msg, buf, buf_len);
	p->h.csum = checksum((unsigned short *)p, p->h.pkt_size);
	return SUCCESS;
}

int rdt_send(int sockfd, void *buf, int buf_len, struct sockaddr_in *dst) {
	pkt p;
	int ns;
	if (make_pkt(&p, buf, buf_len) < 0)
		return ERROR;
	if (biterror_inject) {
		memset(p.msg, 0, MAX_MSG_LEN);
	}
	ns = sendto(sockfd, &p, p.h.pkt_size, 0,
			(struct sockaddr *)dst, sizeof(struct sockaddr_in));
	if (ns < 0) {
		perror("sendto():");
		return ERROR;
	}
	// recvfrom ACK ou NAK
	return ns;
}

int iscorrupted(pkt pr){
	pkt pl = pr;
	pl.h.csum = 0;
	unsigned short csuml;
	csuml = checksum((void *)&pl, pl.h.pkt_size);
	if (csuml != pr.h.csum){
		return TRUE;
	}
	return FALSE;
}

int rdt_recv(int sockfd, void *buf, int buf_len, struct sockaddr_in *src) {
	pkt p;
	int nr;
	int addrlen = sizeof(struct sockaddr_in);
	memset(&p, 0, sizeof(hdr));

	nr = recvfrom(sockfd, &p, sizeof(pkt), 0, (struct sockaddr*)src,
		(socklen_t *)&addrlen);
	if (nr < 0) {
		perror("recvfrom():");
		return ERROR;
	}
	if (iscorrupted(p)) {
		printf("checksum: pacote corrompido. \n");
		// enviar NAK
		return ERROR;
	}
	int msg_size = p.h.pkt_size - sizeof(hdr);
	if (msg_size > buf_len) {
		printf("rdt_rcv(): tamanho insuficiente de buf (%d) para payload (%d).\n", 
			buf_len, msg_size);
		return ERROR;
	}
	memcpy(buf, p.msg, msg_size);
	// enviar ACK
	return p.h.pkt_size - sizeof(hdr);
}
