#include "rdt_3.h"

/**
 * @brief Trata erros da aplicação
 * 
 * Esta função é utilizada para imprimir mensagens de erro e finalizar a execução
 * da aplicação em caso de erro.
 * 
 * @param message mensagem de erro a ser impressa
 */
void handle_error(const char *message) {
    perror(message);
    exit(ERROR);
}

/**
 * @brief Calcula o novo tempo de timeout com base no tempo de ida e volta da mensagem
 * 
 * @param sampleRTT tempo de ida e volta da mensagem
 */
void timeout_interval(double sampleRTT) {
	long double errorRTT, timeOutIntervalRTT;

	estimatedRTT = (1 - ALPHA) * estimatedRTT + ALPHA * sampleRTT;
	errorRTT = fabs(sampleRTT - estimatedRTT);
	devRTT = (1 - BETA) * devRTT + BETA * errorRTT;
	timeOutIntervalRTT = estimatedRTT + 4 * devRTT;

	timeOutInterval.tv_sec = (time_t) timeOutIntervalRTT;
	timeOutInterval.tv_usec = (suseconds_t) ((timeOutIntervalRTT - timeOutInterval.tv_sec) * 1e6);
}

/**
 * @brief Calcula o checksum de um buffer
 * 
 * Esta função calcula o checksum de um buffer de dados. O algoritmo utilizado é o
 * somatório de todos os bytes do buffer, tratando cada byte como um short integer.
 * 
 * @param buf buffer de dados
 * @param nbytes tamanho do buffer de dados
 * @return checksum calculado
 */
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

/**
 * @brief Verifica se um pacote foi corrompido
 * 
 * Esta função verifica se um pacote foi corrompido. Para isso, ela calcula o checksum do pacote
 * e compara com o checksum armazenado no próprio pacote. Se os dois checksums forem diferentes,
 * a função retorna TRUE. Caso contrário, retorna FALSE.
 * 
 * @param packetReceive pacote a ser verificado
 * @return TRUE se o pacote foi corrompido, FALSE caso contrário
 */
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

/**
 * @brief Verifica se um pacote é um ACK e se o seu número de sequência é igual
 * ao número de sequência esperado.
 * 
 * @param packet pacote a ser verificado
 * @param seqnum número de sequência esperado
 * @return TRUE se o pacote for um ACK para o seqnum, FALSE caso contrário
 */
int has_ackseq(packet *packet, hseq_t seqnum) {
	if (packet->header.pkt_type != PKT_ACK || packet->header.pkt_seq_num != seqnum)
		return FALSE;
	return TRUE;
}

/**
 * @brief Verifica se um pacote é um pacote de dados e se o seu número de sequência é igual
 * ao número de sequência esperado.
 * 
 * @param packet pacote a ser verificado
 * @param seqNum número de sequência esperado
 * @return TRUE se o pacote for um pacote de dados com o seqnum, FALSE caso contrário
 */
int has_dataseqnum(packet *packet, hseq_t seqNum) {
	if (packet->header.pkt_seq_num != seqNum || packet->header.pkt_type != PKT_DATA)
		return FALSE;
	return TRUE;
}

/**
 * @brief Divide o buffer em chunks e cria pacotes
 * 
 * Esta função divide um buffer de dados em múltiplos chunks, cada um com um tamanho máximo
 * definido por MAX_MSG_LEN. Para cada chunk, um pacote é criado e armazenado em um array de pacotes.
 * 
 * @param buf_len Tamanho total do buffer de entrada
 * @param buf Ponteiro para o buffer de dados a ser dividido
 * @return chunks_info Estrutura contendo o número total de pacotes criados e um ponteiro para o array de pacotes
 */
chunks_info divide_file_to_chunks(int buf_len, void *buf) {
    // Calcula o número total de pacotes necessários
	int total_packets = (buf_len + MAX_MSG_LEN - 1) / MAX_MSG_LEN;
    
    // Aloca memória para o array de pacotes
    packet *packets = (packet *)malloc(total_packets * sizeof(packet));
    if (packets == NULL) {
        handle_error("rdt_send: malloc failed");
    }

    // Divide o buffer em chunks e cria pacotes
	for (int i = 0; i < total_packets; i++) {
        int chunk_size;
        
        // Define o tamanho do chunk
        if (i == total_packets - 1) {
            chunk_size = buf_len - (i * MAX_MSG_LEN);
        } else {
            chunk_size = MAX_MSG_LEN;
        }
        
        if (make_pkt(&packets[i], PKT_DATA, _snd_seqnum + i, buf + (i * MAX_MSG_LEN), chunk_size, NULL) < 0) {
            free(packets);
            handle_error("rdt_send: make_pkt failed");
        }

        if (DEBUG) {
            printf("  Pacote criado com sucesso:\n");
            printf("    Número de sequência: %d\n", packets[i].header.pkt_seq_num);
            printf("    Tamanho total: %d bytes\n", packets[i].header.pkt_size);
            printf("    Checksum: %d\n", packets[i].header.pkt_checksum);
            printf("\n");
        }
    }

    // Retorna o número total de pacotes criados e o array de pacotes
    chunks_info result;
    result.total_packets = total_packets;
    result.packets = packets;

    return result;
}
