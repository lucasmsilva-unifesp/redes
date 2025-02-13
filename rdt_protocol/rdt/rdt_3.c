#include "rdt_3.h"
#include "utils.h"
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

hseq_t snd_base = 0;
hseq_t rcv_base = 0;
hseq_t next_seq_num = 0;
int packets_sent = 0;

double estimatedRTT = 0.0, devRTT = 0.0;
struct timeval timeOutInterval = {0, 500000};

hseq_t _snd_seqnum = 0;
hseq_t _rcv_seqnum = 0;

packet *recv_window[WINDOW_SIZE];

void set_window() {
	for (int i = 0; i < WINDOW_SIZE; i++) {
        recv_window[i] = NULL;
    }
}

static void verify_acks(int sockfd, packet** sliding_window) {
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
						(struct sockaddr *)&ack_addr, (socklen_t *restrict)&addr_len);
		
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

int make_pkt(packet *packet, PacketType type, hseq_t seqNum, void *msg, int msg_len, htime_t * time) {
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
	int ns;
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

		verify_acks(sockfd, sliding_window);

		// Verifica se o pacote já foi reconhecido, possuiu um ACK
		// Avança janela
		while (snd_base < _snd_seqnum && 
			sliding_window[snd_base % WINDOW_SIZE]->header.pkt_acked) {

			sliding_window[snd_base % WINDOW_SIZE] = NULL;
			snd_base++;
			packets_sent++;
			
			printf("\nWindow advanced: base=%d, next=%d\n", snd_base, _snd_seqnum);
		}

		// Verifica se houve timeout
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

	return buf_len;
}

int rdt_recv(int sockfd, void *buf, int buf_len, struct sockaddr_in *src) {
	int total_received = 0;

    while (1) {
        fd_set readfds;
        packet data, ack;
        int addrLen = sizeof(struct sockaddr_in);

		// Loop para verificar o caso em que há um conteúdo no buff de recebimento.
		if (recv_window[_rcv_seqnum % WINDOW_SIZE] != NULL) {
			int msg_size = recv_window[_rcv_seqnum % WINDOW_SIZE]->header.pkt_size - sizeof(header);
			
			if (msg_size > buf_len) {
				handle_error("rdt_recv: buffer receive overflow");
			}

			memcpy(buf, 
					recv_window[_rcv_seqnum % WINDOW_SIZE]->payload, 
					msg_size);

			total_received = msg_size;
			
			free(recv_window[_rcv_seqnum % WINDOW_SIZE]);
			recv_window[_rcv_seqnum % WINDOW_SIZE] = NULL;

			if (DEBUG) {
				printf("rdt_recv: Buffer filled with size=%d and index=%d\n", msg_size, _rcv_seqnum % WINDOW_SIZE);
			}

			_rcv_seqnum++;
			break;
		}

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        // Wait for packet or timeout
        int ready = select(sockfd + 1, &readfds, NULL, NULL, &timeOutInterval);

        if (ready < 0) {
            handle_error("select error in rdt_recv");
        }

        if (ready == 0) {
            continue;
        }

        if (recvfrom(sockfd, &data, sizeof(packet), 0, 
                     (struct sockaddr*)src, (socklen_t *)&addrLen) < 0) {
            handle_error("recvfrom in rdt_recv");
        }

        if (is_corrupted(&data)) {
            continue;
        }

		if (data.header.pkt_seq_num < _rcv_seqnum) {
            make_pkt(&ack, PKT_ACK, data.header.pkt_seq_num, NULL, 0, &data.header.pkt_time);
            sendto(sockfd, &ack, ack.header.pkt_size, 0, 
                   (struct sockaddr*)src, addrLen);

            if (DEBUG) {
                printf("rdt_recv: Duplicate ACK for packet %d (current base: %d)\n", 
                       data.header.pkt_seq_num, _rcv_seqnum);
            }
            
            continue;
        }

		// Verifica se o pkt está dentro da janela de recebimento
        if (data.header.pkt_seq_num >= _rcv_seqnum && 
            data.header.pkt_seq_num < _rcv_seqnum + WINDOW_SIZE) {
            
            int window_index = data.header.pkt_seq_num % WINDOW_SIZE;
            
			// Caso em que não existe conteúdo no atual índice da janela. Acontece no início e a cada transmissão sucedida, já que após receber o pacote.
            if (recv_window[window_index] == NULL) {
                recv_window[window_index] = malloc(sizeof(packet));
                memcpy(recv_window[window_index], &data, sizeof(packet));

                make_pkt(&ack, PKT_ACK, data.header.pkt_seq_num, NULL, 0, &data.header.pkt_time);
                sendto(sockfd, &ack, ack.header.pkt_size, 0, 
                       (struct sockaddr*)src, addrLen);

				if (DEBUG) {
					printf("rdt_recv: Packet received with seqnum=%d\n", data.header.pkt_seq_num);
					printf("rdt_recv: Sending ACK with seqnum=%d\n", ack.header.pkt_seq_num);
				}
            }

			// Loop para verificar o caso em que há um conteúdo no buff de recebimento.
            if (recv_window[_rcv_seqnum % WINDOW_SIZE] != NULL) {
                int msg_size = recv_window[_rcv_seqnum % WINDOW_SIZE]->header.pkt_size - sizeof(header);
                
                if (msg_size > buf_len) {
                    handle_error("rdt_recv: buffer receive overflow");
                }

                memcpy(buf, 
                       recv_window[_rcv_seqnum % WINDOW_SIZE]->payload, 
                       msg_size);

				total_received = msg_size;
                
                free(recv_window[_rcv_seqnum % WINDOW_SIZE]);
                recv_window[_rcv_seqnum % WINDOW_SIZE] = NULL;

				if (DEBUG) {
					printf("rdt_recv: Buffer filled with size=%d and index=%d\n", msg_size, _rcv_seqnum % WINDOW_SIZE);
				}

                _rcv_seqnum++;
            }
        }

		break;
    }

    return total_received;
}
