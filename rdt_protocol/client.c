#include "rdt/rdt_3.h"
#include "rdt/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/**
 * @brief Função do lado do cliente que envia um arquivo ao servidor por meio do protocolo RDT 3.0
 * 
 * Esta função lê o arquivo "sent.txt" e o envia ao servidor, que o recebe e salva em um arquivo.
 * 
 * @param server_ip IP do servidor
 * @param port número da porta do servidor
 */
void client(const char *server_ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    // Abre o arquivo "sent.txt" para leitura
    FILE *file = fopen("sent.txt", "r");
    if (file == NULL) {
        handle_error("Could not open file");
        return;
    }
    
    long file_size;
    char *buffer;
    
    // Cria o socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        handle_error("Client: cannot create socket");
    }

    // Configura o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(port);
    
    // Converte o endereço IP do servidor para binário
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        handle_error("Client: Invalid server IP address");
        close(sockfd);
        return;
    }

    printf("Client is ready. Reading file and sending to server %s:%d...\n", server_ip, port);

    // Lê o tamanho do arquivo
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    rewind(file);

    // Aloca memória para armazenar o conteúdo do arquivo
    buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL) {
        handle_error("Memory allocation failed!\n");
        fclose(file);
        close(sockfd);
        return;
    }

    // Lê o conteúdo do arquivo para o buffer
    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';

    // Envia os dados lidos via RDT
    if (rdt_send(sockfd, buffer, file_size, &server_addr) < 0) {
        handle_error("Client: rdt_send() failed");
    }

    // Libera os recursos
    free(buffer);
    fclose(file);
    close(sockfd);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        return 0;
    }

    client(argv[1], atoi(argv[2]));

    return 0;
}