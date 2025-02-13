#include "./rdt/rdt_3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

void server(int port) {
    int sockfd;
    struct sockaddr_in sock_addr, client_addr;
    FILE *file;

    char buffer[MAX_MSG_LEN];
    int msg_len;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        handle_error("server: socket() ");
    }

    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = INADDR_ANY;
    sock_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
        handle_error("server: bind() ");      
    }

    printf("Server is running and waiting for messages on port %d...\n", port);

    file = fopen("received.txt", "a");
    if (file == NULL) {
        handle_error("Could not open file");
        close(sockfd);
        return;
    }

    set_window();

    while (1) {
        memset(buffer, 0, MAX_MSG_LEN);

        if ((msg_len = rdt_recv(sockfd, buffer, MAX_MSG_LEN, &client_addr)) < 0) {
            printf("Server: rdt_recv() failed || packet corrupted\n");
            continue;
        }
        // [1,1,4,1,1,1,0,0,0] Se pkt tiver ack, 

        size_t bytes_wrote = fwrite(buffer, 1, msg_len, file);

        if (bytes_wrote < msg_len) {
            handle_error("Error writing to file");
            break;
        }

        fflush(file);

        printf("Received %d bytes and wrote to file\n", msg_len);

        // deal with end of file
        // if (msg_len < MAX_MSG_LEN) {
        //     break;
        // }
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