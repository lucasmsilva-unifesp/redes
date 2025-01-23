#include "./rdt/rdt_2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void client(const char *server_ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    char buffer[MAX_MSG_LEN];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Client: socket creation failed");
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Client: Invalid server IP address");
        close(sockfd);
        return;
    }

    printf("Client is ready. Sending messages to server %s:%d...\n", server_ip, port);

    while (1) {
        memset(buffer, 0, MAX_MSG_LEN);

        printf("Client: Enter a message to send (or 'exit' to quit): ");
        fgets(buffer, MAX_MSG_LEN, stdin);

        if (!strcmp(buffer, "exit\n")) {
            printf("Client: Exiting...\n");
            break;
        }

        // Remove newline character
        buffer[strcspn(buffer, "\n")] = 32;


        if (strlen(buffer) > 1 && rdt_send(sockfd, buffer, strlen(buffer), &server_addr) < 0) {
            perror("Client: rdt_send() failed");
            continue;
        }
    }

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