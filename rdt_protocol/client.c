#include "rdt/rdt_3.h"
#include "rdt/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void ack_handler(int sockfd) {
    packet ack_pkt;
    struct sockaddr_in src;
    socklen_t addrlen = sizeof(src);
    
    while(1) {
        if (recvfrom(sockfd, &ack_pkt, sizeof(packet), 0,
                   (struct sockaddr *)&src, &addrlen) > 0) {
            process_ack(ack_pkt);
        }
    }
}

void client(const char *server_ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[MAX_MSG_LEN];
    FILE *file;
    
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

    file = fopen("sent.txt", "r");
    if (file == NULL) {
        handle_error("Could not open file");
        close(sockfd);
        return;
    }

    while (1) {
        memset(buffer, 0, MAX_MSG_LEN);
        
        size_t bytes_read = fread(buffer, 1, MAX_MSG_LEN - 1, file);
        
        if (bytes_read == 0) {
            if (feof(file)) {
                printf("End of file reached\n");
                break;
            }
            if (ferror(file)) {
                handle_error("Error reading file");
                break;
            }
        }

        // todo: handle error on rdt_send
        if (rdt_send(sockfd, buffer, bytes_read, &server_addr) < 0) {
            handle_error("Client: rdt_send() failed");
            continue;
        }

        printf("Sent %zu bytes\n", bytes_read);
    }

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