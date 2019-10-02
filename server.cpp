//
// Created by Ilya on 10/1/19.
//

#include <cstdlib>
#include <cstdio>
#include <bits/socket.h>
#include <netinet/in.h>

int main(int argc, const char **argv){
    uint16_t port = 9911;
    printf("Starting new file server on port %d\n",port);
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
        printf("Server socket bind failed, exiting");
        exit(2);
    }

}

