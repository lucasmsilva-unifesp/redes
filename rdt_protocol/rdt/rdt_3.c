#include "rdt_3.h"
#include "utils.h"
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

static int biterror_inject = FALSE;
static int timeout_inject = FALSE;

hseq_t snd_base = 0;
hseq_t rcv_base = 0;
hseq_t next_seq_num = 0;
int packets_sent = 0;

double estimatedRTT = 0.0, devRTT = 0.0;
struct timeval timeOutInterval = {0, 500000};

hseq_t _snd_seqnum = 0;
hseq_t _rcv_seqnum = 0;

static int make_pkt(packet *packet, PacketType type, hseq_t seqNum, void *msg, int msg_len, htime_t * time) {
	struct timeval time_start;

	if (msg_len > MAX_MSG_LEN) {
		printf("make_pkt: message size (%d) bigger than limit - (%d).\n",
		msg_len, MAX_MSG_LEN);
		return ERROR;
	}

	gettimeofday(&time_start, NULL);

	packet->header.pkt_size = sizeof(header);
	packet->header.pkt_checksum = 0;
	packet->header.pkt_type = type;
	packet->header.pkt_seq_num = seqNum;
	packet->header.pkt_time = (time == NULL) ? time_start : *time;

	if (msg_len > 0) {
		packet->header.pkt_size += msg_len;
		memset(packet->payload, 0, MAX_MSG_LEN);
		memcpy(packet->payload, msg, msg_len);
	}

	packet->header.pkt_checksum = checksum((unsigned short *) packet, packet->header.pkt_size);

	return SUCCESS;
}

int rdt_send(int sockfd, void *buf, int buf_len, struct sockaddr_in *dest) {
	packet data_pkt, ack; // Podemos apagar isso depois
	struct sockaddr_in dest_ack;
	int ns, nr, addrlen;
	struct timeval time_end;
	double sampleRTT;
	chunks_info chunks;

	chunks = divide_file_to_chunks(buf_len, buf);
	packet *packets = chunks.packets;
	int total_packets = chunks.total_packets;
	
	packet *sliding_window[WINDOW_SIZE];
	for (int i = 0; i < WINDOW_SIZE; i++) {
		sliding_window[i] = NULL;
	}

	while (packets_sent < total_packets) {

		// Envia pacotes enquanto houver espaço na janela
		while (_snd_seqnum < total_packets && 
           _snd_seqnum < snd_base + WINDOW_SIZE) {

			printf("\nSending data_pkt: %d (window position: %d)\n", 
               _snd_seqnum, _snd_seqnum % WINDOW_SIZE);
        
			sliding_window[_snd_seqnum % WINDOW_SIZE] = &packets[_snd_seqnum];
			
			ns = sendto(sockfd, sliding_window[_snd_seqnum % WINDOW_SIZE], 
					sliding_window[_snd_seqnum % WINDOW_SIZE]->header.pkt_size, 0,
					(struct sockaddr *)dest, sizeof(struct sockaddr_in));
			
			if (ns < 0) {
				handle_error("rdt_send: sendto(PKT_DATA):");
			}
			
			// Apenas para verificar o status dos pacotes na janela
			if(DEBUG){
				printf("  Window status:\n");
				for (int i = 0; i < WINDOW_SIZE; i++) {
					if (sliding_window[i] != NULL) {
						printf("  [%d]: seq=%d ", i, 
							sliding_window[i]->header.pkt_seq_num);
						if (sliding_window[i]->header.pkt_acked)
							printf("(ACKed)\n");
						else
							printf("(waiting)\n");
					} else {
						printf("  [%d]: empty\n", i);
					}
				}
			}
			
			_snd_seqnum++;
		}

		// struct timeval carlos;
		// gettimeofday(&carlos, NULL);

		// timeval_compare(&sliding_window[0]->header.pkt_time, &carlos, TRUE);

		// verify_acks(sockfd, sliding_window);

		// // Verifica se o pacote já foi reconhecido, possuiu um ACK
		// // Avança janela
		while (snd_base < _snd_seqnum && 
			sliding_window[snd_base % WINDOW_SIZE]->header.pkt_acked) {

			sliding_window[snd_base % WINDOW_SIZE] = NULL;
			snd_base++;
			packets_sent++;
			
			printf("\nWindow advanced: base=%d, next=%d\n", snd_base, _snd_seqnum);
		}

		// // Verifica se houve timeout
		struct timeval current_time;	
		
		for (int i = snd_base; i < _snd_seqnum; i++) {
	    	gettimeofday(&current_time, NULL);

			packet *current_pkt = sliding_window[i % WINDOW_SIZE];
			if (current_pkt != NULL && !current_pkt->header.pkt_acked) {

				// Verifica se houve timeout
				if (timeval_compare(&current_pkt->header.pkt_time, &current_time, 0) > 0) {

					printf("\nTimeout for packet %d.\n", i);
					
					if(DEBUG){
						double current_time_ts = current_time.tv_sec + current_time.tv_usec/1e6;
						double current_pkt_ts = current_pkt->header.pkt_time.tv_sec + current_pkt->header.pkt_time.tv_usec/1e6;
						double diff_time_ts = current_time_ts - current_pkt_ts;

						printf("	Current time: %f, ", current_time_ts);
						format_timestamp(current_time_ts);
						printf("	Packet time: %f, ", current_pkt_ts);
						format_timestamp(current_pkt_ts);
						printf("	Diff time: %f, ", diff_time_ts);
						format_timestamp(diff_time_ts);
					}

					// Reenvia o pacote
					ns = sendto(sockfd, current_pkt, current_pkt->header.pkt_size, 0,
							(struct sockaddr *)dest, sizeof(struct sockaddr_in));
					
					if (ns < 0) {
						handle_error("rdt_send: sendto(PKT_DATA) resend:");
					}
					
					// Atualiza o timeout
					current_pkt->header.pkt_time = current_time;
				}
				else {
					if(DEBUG){
						printf("\nNo timeout for packet %d.\n", i);
						printf("  Current time: %ld.%06ld\n", current_time.tv_sec, current_time.tv_usec);
						printf("  Packet time: %ld.%06ld\n", current_pkt->header.pkt_time.tv_sec, current_pkt->header.pkt_time.tv_usec);
					}
				}
				
			}
		}
	}

// wait_ack:
// 	addrlen = sizeof(struct sockaddr_in);

// 	struct timeval timeout = {timeOutInterval.tv_sec, timeOutInterval.tv_usec};
// 	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(struct timeval)) < 0) {
// 		handle_error("setsockopt(..., SO_RCVTIMEO, ...)");
// 	}

// 	nr = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&dest_ack,
// 		(socklen_t *)&addrlen);
	
// 	if (nr < 0) {
//     	if (errno == EAGAIN || errno == EWOULDBLOCK) {
// 			printf("Timeout reached. Resending data_pkt.\n");
// 			// goto resend;
// 		} else {
// 			printf("rdt_send: recvfrom(PKT_ACK): %d\n", nr);
// 			handle_error("rdt_send: recvfrom(PKT_ACK)");
// 		}
// 	}

	// if (is_corrupted(&ack)) {
	// 	printf("rdt_send: is_corrupted\n");
	// 	goto wait_ack;
	// }

	// if (!has_ackseq(&ack, _snd_seqnum)) {
	// 	printf("rdt_send: !has_ackseq, _snd_seqnum: %d, ack.header.pkt_seq_num: %d\n", _snd_seqnum, ack.header.pkt_seq_num);
	// 	goto wait_ack;
	// }

	// if (DEBUG) {
	// 	printf("rdt_send: data_pkt send with seqnum=%d\n", data_pkt.header.pkt_seq_num);
	// 	printf("rdt_recv: Received ACK with seqnum=%d\n", ack.header.pkt_seq_num);
	// }

    // gettimeofday(&time_end, NULL);

	// _snd_seqnum++;

	// sampleRTT = (time_end.tv_sec + time_end.tv_usec/1e6) - ack.header.pkt_time;
	// timeout_interval(sampleRTT);

	// if (DEBUG) {
	// 	printf("rdt_send: sampleRTT: %f, estimatedRTT: %f, devRTT: %f, timeoutInterval: %ld.%06ld\n",
	// 		sampleRTT, estimatedRTT, devRTT,
	// 		timeOutInterval.tv_sec, timeOutInterval.tv_usec);
	// }

	return buf_len;
}

void verify_acks(int sockfd, packet** sliding_window) {
	// Verifica ACKs e timeouts
	fd_set readfds;
	struct timeval tv;
	packet ack_pkt;
	struct sockaddr_in ack_addr;
	int addr_len = sizeof(ack_addr);

	// Configura o select
	FD_ZERO(&readfds);
	FD_SET(sockfd, &readfds);

	// Configura timeout para o select (menor timeout entre os pacotes na janela)
	tv.tv_sec = timeOutInterval.tv_sec;
	tv.tv_usec = timeOutInterval.tv_usec;

	// Espera por ACKs ou timeout
	int ready = select(sockfd + 1, &readfds, NULL, NULL, &tv);

	if (ready < 0) {
		handle_error("select error");
	} else if (ready > 0) {
		// Recebe o ACK
		int nr = recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0,
						(struct sockaddr *)&ack_addr, &addr_len);
		
		if (nr < 0) {
			handle_error("rdt_send: recvfrom(ACK)");
		}
		
		printf("\nReceived ACK for packet: %d\n", ack_pkt.header.pkt_seq_num);
		
		// Verifica se o ACK não está corrompido
		if (!is_corrupted(&ack_pkt)) {
			printf("ACK não foi corrompido\n");
			// Encontra o pacote na janela e marca como confirmado
			int ack_seq = ack_pkt.header.pkt_seq_num;
			if (ack_seq >= snd_base && ack_seq < _snd_seqnum) {
				sliding_window[ack_seq % WINDOW_SIZE]->header.pkt_acked = 1;
				printf("Marked packet %d as ACKed\n", ack_seq);
				
				// Atualiza estimativas de RTT se necessário
				struct timeval current_time;
				gettimeofday(&current_time, NULL);

				double sampleRTT = (current_time.tv_sec + current_time.tv_usec/1e6) - 
					(ack_pkt.header.pkt_time.tv_sec + ack_pkt.header.pkt_time.tv_usec/1e6);

				timeout_interval(sampleRTT);
			}
		} else {
			printf("Received corrupted ACK\n");
		}
	}
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

int rdt_recv(int sockfd, void *buf, int buf_len, struct sockaddr_in *src) {
	packet *sliding_window[WINDOW_SIZE];
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

			if (sendto(sockfd, &ack, ack.header.pkt_size, 0,
				(struct sockaddr*)src, (socklen_t)sizeof(struct sockaddr_in)) < 0) {
				handle_error("rdt_rcv: sendto(PKT_ACK - 1)");
			}

			goto rerecv;
		}

		if (!has_dataseqnum(&data, _rcv_seqnum)) {
			printf("rdt_recv: !has_dataseqnum, _rcv_seqnum: %d, data.header.pkt_seq_num: %d \n", _rcv_seqnum, data.header.pkt_seq_num);

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