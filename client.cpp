//
// Created by ilya on 10/2/19.
//

#include <cstdlib>
#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <cstring>
#include <netdb.h>
#include "proto.h"


int connect_to(const char *ip, uint16_t port) {
    struct hostent *host;
    host = gethostbyname(ip);
    if (!host)
        return -1;
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    dest.sin_addr.s_addr = *((in_addr_t *) host->h_addr);
    int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(socket_fd, (sockaddr *) &dest, sizeof(sockaddr));
    return socket_fd;
}

int main(int argc, const char **argv) {
    if (argc < 4) {
        printf("arguments <file_name> <server_ip> <server_port> are required\n");
        exit(1);
    }
    auto file_name = argv[1];
    auto server_address = argv[2];
    auto server_port = argv[3];
    uint16_t port = 0;
    sscanf(server_port, "%hu", &port);
    int socket = connect_to(server_address, port);
    if (socket < 0) {
        printf("Unable to connect to server : %d\n", socket);
        exit(2);
    }
    FILE *file = fopen(file_name, "rb");
    if (!file) {
        printf("Unable to open file in rb mode\n");
        exit(3);
    }
    fseek(file, 0, SEEK_END);
    long file_size_l = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint32_t command = COMMAND_SEND;
    uint32_t response = 0;
    auto file_size = static_cast<uint32_t>(file_size_l);
    uint32_t chunk_size = 1024;
    auto file_name_len = strlen(file_name);
    command = htonl(command);
    file_size = htonl(file_size);
    chunk_size = htonl(chunk_size);
    file_name_len = htonl(file_name_len);

    // make request
    send(socket, &command, 4, 0);
    send(socket, &file_size, 4, 0);
    send(socket, &chunk_size, 4, 0);
    send(socket, &file_name_len, 4, 0);
    file_name_len = ntohl(file_name_len);
    send(socket, file_name, file_name_len, 0);


    // receive answer
    recv(socket, &response, 4, 0);
    response = ntohl(response);
    if (response != RESPONSE_SEND_OK) {
        uint32_t message_size = 0;
        recv(socket, &message_size, 4, 0);
        message_size = ntohl(message_size);
        char error_message[message_size + 1];
        error_message[message_size] = '\0';
        recv(socket, error_message, message_size, 0);
        printf("SEND failed with code %d : %s\n", response, error_message);
        exit(4);
    }

    printf("Server accepted SEND request\n");
    uint32_t save_file_name_size = 0;
    recv(socket, &save_file_name_size, 4, 0);
    save_file_name_size = ntohl(save_file_name_size);
    if (save_file_name_size != 0) {
        char save_file_name_offer[save_file_name_size + 1];
        save_file_name_offer[save_file_name_size] = '\0';
        recv(socket, save_file_name_offer, save_file_name_size, 0);
        printf("File with name %s already exists, server offers name %s\n", file_name, save_file_name_offer);
        printf("Accept? (y/n) : ");
        char answer = 0;
        while (scanf("%c", &answer) == 0) {
            printf("Input \'y\' or \'n\'\n");
        }
        if (answer == 'y') {
            printf("Continue file transaction\n");
        } else {
            printf("Aborting file transaction...\n");
            uint32_t code = htonl(COMMAND_SEND_RESET);
            send(socket, &code, 4, 0);
            // send reset code and receive ok response
            recv(socket, &code, 4, 0);
            printf("Transaction aborted\n");
            exit(5);
        }
    }


    // send file chunks
    int32_t chunk_counter = 1;
    int32_t bytes_left = ntohl(file_size);
    uint32_t chunk_max_length = ntohl(chunk_size);
    char buffer[chunk_max_length];
    printf("Expected to send %d chunks\n", (int)(double(bytes_left) / chunk_max_length) + (bytes_left % chunk_max_length ? 1 : 0));
    while (bytes_left > 0) {
        uint32_t curr_chunk_size = fread(buffer, 1, chunk_max_length, file);
        command = htonl(COMMAND_SEND_CHUNK);
        bytes_left -= curr_chunk_size;
        auto curr_chunk_size_n = htonl(curr_chunk_size);
        send(socket, &command, 4, 0);
        send(socket, &curr_chunk_size_n, 4, 0);
        send(socket, buffer, curr_chunk_size, 0);
        recv(socket, &command, 4, 0);
        if (command != 0) {
            printf("Error while sending chunk %d", chunk_counter);
        } else {
            printf("Sent chunk %d, size %d\n", chunk_counter, curr_chunk_size);
        }
        chunk_counter++;
    }

    //send commit
    uint32_t data = htonl(COMMAND_SEND_COMMIT);
    send(socket, &data, 4, 0);
    recv(socket, &data, 4, 0);
    if (ntohl(data) != 0) {
        printf("Commit was failed : %d\n", ntohl(data));
    } else {
        printf("Commited \n");
    }
    return 0;
}
