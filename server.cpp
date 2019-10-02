//
// Created by Ilya on 10/1/19.
//

#include <cstdlib>
#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <cstring>

#include "proto.h"

volatile bool shutdown_required = false;

struct file_metadata_t {
    int file_size;
    int chunk_size;
    char *file_name;
    int received_bytes;
    FILE *local_file;
};

struct worker_info_t {
    pthread_t thread;
    int client_socket;
    file_metadata_t *incoming_file;
};

char *make_next_file_name(const char *orig_file_name) {
    auto buffer = new char[strlen(orig_file_name) + 16];
    auto index = 0;
    FILE *check_file = nullptr;
    do {
        if (check_file != nullptr) {
            fclose(check_file);
        }
        sprintf(buffer, "%s.%d", orig_file_name, ++index);
        check_file = fopen(buffer, "r");
    } while (check_file != nullptr);
    return buffer;
}

int send_error(int socket, uint32_t code, const char *message) {
    auto len = strlen(message);
    code = htonl(code);
    len = htonl(len);
    send(socket, &code, 4, 0);
    send(socket, &len, 4, 0);
    send(socket, message, len, 0);
    return 0;
}

int serve_send_file(worker_info_t *info) {
    printf("Received send file command\n");
    ssize_t read_bytes = 0;
    uint32_t file_size = 0, chunk_size = 0, file_name_length;
    read_bytes += recv(info->client_socket, &file_size, 4, 0);
    read_bytes += recv(info->client_socket, &chunk_size, 4, 0);
    read_bytes += recv(info->client_socket, &file_name_length, 4, 0);
    if (read_bytes != 12) {
        printf("Error occurred while reading file metadata: received %zd bytes instead of 12", read_bytes);
        send_error(info->client_socket, RESPONSE_FAIL, "metadata read fail");
        return 1;
    }
    file_name_length = ntohl(file_name_length);
    file_size = ntohl(file_size);
    chunk_size = ntohl(chunk_size);
    //TODO: make safety checks
    char file_name[file_name_length + 1];
    file_name[file_name_length] = '\0';
    read_bytes = recv(info->client_socket, &file_name, file_name_length, 0);
    if (read_bytes != file_name_length) {
        printf("Unable to receive file name: got %zd bytes instead of %u", read_bytes, file_name_length);
        send_error(info->client_socket, RESPONSE_FAIL, "file name read fail");
        return 2;
    }

    //TODO: check file name
    FILE *save_file;
    char *save_file_name = nullptr;
    save_file = fopen(file_name, "r");
    if (save_file) {
        fclose(save_file);
        // make a new name
        save_file_name = make_next_file_name(file_name);
    } else {
        save_file_name = strdup(file_name);
    }
    save_file = fopen(save_file_name, "wb");
    if (save_file == nullptr) {
        printf("Unable to save file %s", file_name);
    }

    auto metadata = new file_metadata_t{};
    metadata->file_size = file_size;
    metadata->chunk_size = chunk_size;
    metadata->file_name = save_file_name;
    metadata->received_bytes = 0;
    metadata->local_file = save_file;

    info->incoming_file = metadata;
    uint32_t command = RESPONSE_SEND_OK;
    uint32_t save_file_size = (strcmp(save_file_name, file_name) == 0) ? 0 : strlen(save_file_name);
    command = htonl(command);
    auto save_file_size_n = htonl(save_file_size);
    send(info->client_socket, &command, 4, 0);
    send(info->client_socket, &save_file_size_n, 4, 0);
    if (save_file_size > 0) {
        send(info->client_socket, save_file_name, save_file_size, 0);
    }
    printf("Opened send file transaction %s -> %s, size %u, maximum chunk size %u\n",
           file_name, save_file_name, file_size, chunk_size);
    return 0;
}

int serve_send_chunk(worker_info_t *info) {
    printf("Received send file command on socket %d\n", info->client_socket);
    if (info->incoming_file == nullptr || info->incoming_file->local_file == nullptr) {
        printf("unexpected SEND CHUNK, use SEND first");
        send_error(info->client_socket, RESPONSE_FAIL, "unexpected SEND CHUNK, use SEND first");
        return 1;
    }
    ssize_t read_bytes = 0;
    uint32_t chunk_size = 0;
    read_bytes += recv(info->client_socket, &chunk_size, 4, 0);
    if (read_bytes != 4) {
        printf("expected 4 bytes, got %u\n", chunk_size);
        send_error(info->client_socket, RESPONSE_FAIL, "invalid SEND CHUNK request size");
        return 2;
    }
    chunk_size = ntohl(chunk_size);
    char data[chunk_size];
    read_bytes = recv(info->client_socket, data, chunk_size, 0);
    if (read_bytes != chunk_size) {
        printf("expected %zd bytes of chunk payload, got %d\n", read_bytes, chunk_size);
        send_error(info->client_socket, RESPONSE_FAIL, "mismatched chunk size and received data size");
        return 3;
    }
    fwrite(data, 1, chunk_size, info->incoming_file->local_file);

    //fflush(info->incoming_file->local_file);

    uint32_t response = htonl(RESPONSE_SEND_CHUNK_OK);
    send(info->client_socket, &response, 4, 0);
    return 0;
}

int serve_send_commit(worker_info_t *info) {
    uint32_t response = htonl(RESPONSE_SEND_COMMIT_OK);
    send(info->client_socket, &response, 4, 0);
    printf("Transaction on socket %d have been committed\n", info->client_socket);
    if (info->incoming_file != nullptr) {
        delete[] info->incoming_file->file_name;
        fclose(info->incoming_file->local_file);
        delete info->incoming_file;
        info->incoming_file = nullptr;
    }
    return 0;
}

int serve_send_reset(worker_info_t *info) {
    uint32_t response = htonl(RESPONSE_SEND_RESET_OK);
    send(info->client_socket, &response, 4, 0);
    if (info->incoming_file != nullptr && info->incoming_file->file_name != nullptr) {
        printf("Transaction SEND -> %s have been aborted\n", info->incoming_file->file_name);
    } else {
        printf("Transaction from socket %d was aborted\n", info->client_socket);
    }
    if (info->incoming_file != nullptr && info->incoming_file->file_name != nullptr) {
        FILE *file_check = fopen(info->incoming_file->file_name, "r");
        if (file_check != nullptr) {
            fclose(file_check);
            remove(info->incoming_file->file_name);
        }
        delete[] info->incoming_file->file_name;
        delete info->incoming_file;
    }
    return 0;
}

void *serve(void *data) {
    auto info = static_cast<worker_info_t *>(data);
    int comm_socket = info->client_socket;
    uint32_t command = 0;
    while (true) {
        ssize_t read_bytes = recv(comm_socket, &command, 4, 0);
        if (read_bytes <= 0) {
            break;
        }
        command = ntohl(command);
        switch (command) {
            case COMMAND_SEND: {
                serve_send_file(info);
                break;
            }
            case COMMAND_SEND_CHUNK: {
                serve_send_chunk(info);
                break;
            }
            case COMMAND_SEND_COMMIT: {
                serve_send_commit(info);
                break;
            }
            case COMMAND_SEND_RESET: {
                serve_send_reset(info);
                break;
            }
            default: {
                printf("client send invalid request, ignoring\n");
            }
        }
    }
    return nullptr;
}

int main(int argc, const char **argv) {
    if (argc < 2) {
        printf("argument <port> is required\n");
        exit(10);
    }
    uint16_t port = 9911;
    sscanf(argv[1],"%hd",&port);
    printf("Starting new file server on port %d\n", port);
    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == -1) {
        printf("Unable to allocate server socket\n");
        exit(1);
    }
    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int));
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t sock_len = sizeof(sockaddr);
    if (bind(server_socket, (const sockaddr *) &server_addr, sock_len) == -1) {
        printf("Server socket bind failed, exiting\n");
        exit(2);
    }
    if (getsockname(server_socket, (sockaddr *) &server_addr, &sock_len) == -1) {
        printf("getsockname failed, something wrong\n");
        exit(3);
    }
    if (listen(server_socket, 5) < 0) {
        printf("Unable to start listening");
        exit(4);
    }

    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(sockaddr_in);
    while (!shutdown_required) {
        int accepted_socket = accept(server_socket, (struct sockaddr *) &client_addr, &addr_len);
        if (accepted_socket < 0) {
            printf("Invalid communication socket: %i\n", accepted_socket);
            continue;
        }
        auto info = new worker_info_t;
        info->client_socket = accepted_socket;
        pthread_create(&info->thread, nullptr, serve, info);
    }
    return 0;
}

