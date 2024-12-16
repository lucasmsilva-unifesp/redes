// Include necessary headers
#include "rdt_2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_PORT 8080
#define CLIENT_PORT 8081
#define BUF_SIZE 1024

// Server implementation
void server() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUF_SIZE];

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Server: socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind the socket to the server address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Server: bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server is running and waiting for messages on port %d...\n", SERVER_PORT);

    while (1) {
        // socklen_t client_len = sizeof(client_addr);
        memset(buffer, 0, BUF_SIZE);

        // Receive data from the client
        int received_bytes = rdt_recv(sockfd, buffer, BUF_SIZE, &client_addr);
        if (received_bytes < 0) {
            printf("Server: Failed to receive message\n");
            continue;
        }

        printf("Server: Received message: %s\n", buffer);

        // Respond to the client
        const char *ack_msg = "Message received successfully!";
        if (rdt_send(sockfd, (void *)ack_msg, strlen(ack_msg), &client_addr) < 0) {
            printf("Server: Failed to send ACK\n");
        } else {
            printf("Server: Sent ACK to client\n");
        }
    }

    close(sockfd);
}

// Client implementation
void client(const char *server_ip) {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Client: socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Client: Invalid server IP address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Client is ready. Sending messages to server %s:%d...\n", server_ip, SERVER_PORT);

    while (1) {
        printf("Client: Enter a message to send (or 'exit' to quit): ");
        fgets(buffer, BUF_SIZE, stdin);

        // Remove newline character
        buffer[strcspn(buffer, "\n")] = 0;

        // Exit if the user types "exit"
        if (strcmp(buffer, "exit") == 0) {
            printf("Client: Exiting...\n");
            break;
        }

        // Send the message to the server
        if (rdt_send(sockfd, buffer, strlen(buffer), &server_addr) < 0) {
            printf("Client: Failed to send message\n");
            continue;
        }

        printf("Client: Message sent. Waiting for ACK...\n");

        // Wait for ACK from the server
        struct sockaddr_in ack_addr;
        int received_bytes = rdt_recv(sockfd, buffer, BUF_SIZE, &ack_addr);
        if (received_bytes < 0) {
            printf("Client: Failed to receive ACK\n");
        } else {
            printf("Client: Received response: %s\n", buffer);
        }
    }

    close(sockfd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s [server|client] [server_ip (for client)]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "server") == 0) {
        server();
    } else if (strcmp(argv[1], "client") == 0) {
        if (argc < 3) {
            printf("Client mode requires a server IP address.\n");
            return EXIT_FAILURE;
        }
        client(argv[2]);
    } else {
        printf("Invalid mode. Use 'server' or 'client'.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
