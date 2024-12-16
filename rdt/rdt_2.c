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

int biterror_inject = FALSE;

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

	pkt ack_pkt;
	int nr;
	int ack_len	= sizeof(struct sockaddr_in);
	
	int retries = 0;
	while (retries < 3) {
		memset(&p, 0, sizeof(hdr));

		if (make_pkt(&p, buf, buf_len) < 0)
			return ERROR;
		if (biterror_inject) {
			memset(p.msg, 0, MAX_MSG_LEN);
		}

		// Enviando o pacote; retorna o número de bytes enviados. Se -1, erro no envio.
		ns = sendto(sockfd, &p, p.h.pkt_size, 0,
				(struct sockaddr *)dst, sizeof(struct sockaddr_in));
		if (ns < 0) {
			perror("sendto():");
			return ERROR;
		}

		// recvfrom ACK ou NAK
		nr = recvfrom(sockfd, &ack_pkt, sizeof(pkt), 0, (struct sockaddr*)dst,
			(socklen_t *)&ack_len);
		if (nr < 0) {
			perror("rdt_send: recvfrom():");
			return ERROR;
		}

		
		if (!strncmp((char *)ack_pkt.msg, "ACK", 3)) {
			return ns;
		} else if (!strncmp((char *)ack_pkt.msg, "NAK", 3)) {
			printf("rdt_send(): NAK recebido. Reenviando o pacote.\n");
		}

		retries++;
	}

	printf("rdt_send(): falha ao enviar o pacote depois de 3 vezees.\n");
	return ERROR;
}

// Retorna TRUE (1) se o checksum do pacote estiver corrompido. Senão, FALSE (0).
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

	// Recebendo o pacote; retorna o número de bytes recebidos. Se -1, erro no recebimento.
	nr = recvfrom(sockfd, &p, sizeof(pkt), 0, (struct sockaddr*)src,
		(socklen_t *)&addrlen);
	if (nr < 0) {
		perror("recvfrom():");
		return ERROR;
	}

	// Verificando se o pacote está corrompido. Se estiver, deve-se enviar NAK.
	if (iscorrupted(p)) {
		printf("checksum: pacote corrompido. \n");
		
		// Enviando NAK
		pkt nak_pkt;
		if (make_pkt(&nak_pkt, "NAK", 3) < 0) {
			printf("rdt_rcv(): falha ao criar o pacote NAK.\n");
		}

		int nak_retrives = 0;
		while (nak_retrives < 3) {
			nr = sendto(sockfd, &nak_pkt, nak_pkt.h.pkt_size, 0,
				(struct sockaddr *)src, sizeof(struct sockaddr_in));
			if (nr > 0)
				break;
			printf("rdt_rcv(): falha ao enviar o pacote NAK. Retransmitido (%d/3)\n", ++nak_retrives);
		}

		if (nr < 0) {
			printf("rdt_rcv(): falha ao enviar o pacote NAK depois de 3 vezees.\n");
			return ERROR;
		}

		return ERROR;
	}

	// Caso o pacote não foi corrompido (checksum é igual ao calculado pelo remetente).
	int msg_size = p.h.pkt_size - sizeof(hdr);

	// Verificando se o tamanho do buffer é suficiente para a mensagem, pois o buffer possui um limite no tamanho da mensagem.
	if (msg_size > buf_len) {
		printf("rdt_rcv(): tamanho insuficiente de buf (%d) para payload (%d).\n", 
			buf_len, msg_size);
		return ERROR;
	}

	// Caso o tamanho do buffer seja suficiente, copia a mensagem para o buffer.
	memcpy(buf, p.msg, msg_size);

	// Enviando ACK
	pkt ack_pkt;
	if (make_pkt(&ack_pkt, "ACK", 3) < 0) {
		printf("rdt_rcv(): falha ao criar o pacote ACK.\n");
	}

	int ack_retrives = 0;
	while (ack_retrives < 3) {
		nr = sendto(sockfd, &ack_pkt, ack_pkt.h.pkt_size, 0,
			(struct sockaddr *)src, sizeof(struct sockaddr_in));
		if (nr > 0)
			break;
		printf("rdt_rcv(): falha ao enviar o pacote ACK. Retransmitido (%d/3)\n", ++ack_retrives);
	}

	if (nr < 0) {
		printf("rdt_rcv(): falha ao enviar o pacote ACK depois de 3 vezees.\n");
		return ERROR;
	}

	return p.h.pkt_size - sizeof(hdr);
}
