#include "./rdt/rdt_2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

void server(int port) {
    int sockfd;
    struct sockaddr_in saddr, client_addr;

    char buffer[MAX_MSG_LEN];
    int msg_len;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("server: socket() ");
        return;
    }

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        perror("server: bind() ");
        return;       
    }

    printf("Server is running and waiting for messages on port %d...\n", port);

    while (1) {
        memset(buffer, 0, MAX_MSG_LEN);

        if ((msg_len = rdt_recv(sockfd, buffer, MAX_MSG_LEN, &client_addr)) < 0) {
            printf("Server: rdt_recv() failed\n");
            continue;
        }

        printf("Received message: %s\n", buffer);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 0;
    }

    server(atoi(argv[1]));

    return 0;
}