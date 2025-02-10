#include "rdt/rdt_3.h"
#include "rdt/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void client(const char *server_ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    FILE *file = fopen("sent.txt", "r");
    long file_size;
    char *buffer;
    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        handle_error("Client: cannot create socket");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        handle_error("Client: Invalid server IP address");
        close(sockfd);
        return;
    }

    printf("Client is ready. Reading file and sending to server %s:%d...\n", server_ip, port);

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    rewind(file);

    buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL) {
        handle_error("Memory allocation failed!\n");
        fclose(file);
        close(sockfd);
        return;
    }

    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';

    // todo: handle error on rdt_send
    if (rdt_send(sockfd, buffer, file_size, &server_addr) < 0) {
        handle_error("Client: rdt_send() failed");
    }

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