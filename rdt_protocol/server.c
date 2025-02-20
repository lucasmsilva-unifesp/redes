#include "./rdt/rdt_3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/**
 * @brief Função principal do servidor
 * 
 * Esta função espera por mensagens do cliente e as escreve em um arquivo.
 * 
 * @param port número da porta que o servidor ir escutar
 */
void server(int port) {
    int sockfd;
    struct sockaddr_in sock_addr, client_addr;
    FILE *file;

    char buffer[MAX_MSG_LEN];
    int msg_len;

    // Cria o socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        handle_error("server: socket() ");
    }

    // Configura o endereço do servidor
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = INADDR_ANY;
    sock_addr.sin_port = htons(port);

    // Vincula o socket  uma porta
    if (bind(sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
        handle_error("server: bind() ");      
    }

    // Imprime uma mensagem de status
    printf("Server is running and waiting for messages on port %d...\n", port);

    // Abre o arquivo "received.txt" para escrita
    file = fopen("received.txt", "a");
    if (file == NULL) {
        handle_error("Could not open file");
        close(sockfd);
        return;
    }

    // Inicializa a janela de recebimento
    set_window();

    while (1) {
        memset(buffer, 0, MAX_MSG_LEN);

        // Recebe um pacote via RDT 3.0
        if ((msg_len = rdt_recv(sockfd, buffer, MAX_MSG_LEN, &client_addr)) < 0) {
            printf("Server: rdt_recv() failed || packet corrupted\n");
            continue;
        }

        // Escreve no arquivo
        size_t bytes_wrote = fwrite(buffer, 1, msg_len, file);

        if (bytes_wrote < msg_len) {
            handle_error("Error writing to file");
            break;
        }

        // Limpa o buffer do arquivo
        fflush(file);

        printf("Received %d bytes and wrote to file\n", msg_len);
    }

    fclose(file);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 0;
    }

    server(atoi(argv[1]));

    return 0;
}