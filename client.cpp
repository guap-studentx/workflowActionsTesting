

// Copyright (C) 1883 Thomas Edison - All Rights Reserved
// You may use, distribute and modify this code under the
// terms of the XYZ license, which unfortunately won't be
// written for another century.
//
// You should have received a copy of the XYZ license with
// this file. If not, please write to: , or visit :

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>

#define DEFAULT_SERVICE_PORT 12345
#define DEFAULT_BUFFER_SIZE 512

int main(int argc, char* argv[]) {
    // Parse command line
    // ------------------
    const char* server_ip;
    int server_port = 0;
    if (argc < 3) {
        std::cerr << "Not enought arguments" << std::endl;
        std::cout << "Usage: client <address> <port>" << std::endl;
        return 1;
    } else {
        server_ip = argv[1];
        if (server_ip == nullptr) {
            std::cerr << "You should pass valid ip-address!" << std::endl;
            return 1;
        }
        server_port = atoi(argv[2]);
        if (server_port <= 0 || server_port >= UINT16_MAX) {
            std::cerr << "the specified port is incorrect" << std::endl;
            return 1;
        }
    }
    /* Создание эфимерного** сокета */
    int net_sock;
    net_sock = socket(
                   AF_INET,       // IPv4
                   SOCK_STREAM,   // TCP
                   IPPROTO_TCP);  // Default
    if (net_sock == -1) {
        std::cerr << "[ERROR] Can't create socket" << std::endl;
        perror(" > Details: ");
        return 1;
    }

    /* Определение адреса, для сокета */
    struct sockaddr_in server_addrress;
    // IPv4 Address type
    server_addrress.sin_family = AF_INET;
    // Указываем порт, не забываем про Network-Byte-Flow (big endian)
    server_addrress.sin_port = htons(DEFAULT_SERVICE_PORT);
    // Из десятично-точечной в двоичную big-endian
    inet_pton(AF_INET, server_ip, &(server_addrress.sin_addr.s_addr));

    /* Подключение к сокету сервера */
    int status;
    printf("Conneting to (%s:%d)...\n", server_ip, server_port);
    status = connect(net_sock,
        (struct sockaddr*)&(server_addrress),
        sizeof(server_addrress));
    if (status == -1) {
        std::cerr << "[ERROR] Can't connect to server" << std::endl;
        perror(" > Details: ");
        return 1;
    } else {
        printf("[+++] Connected to (%s:%d)\n", server_ip, server_port);
    }

    /* Работа с сервером */
    ssize_t bRecv = 0;
    ssize_t bSent = 0;
    std::string command_prompt;
    char* respBuf = new char[DEFAULT_BUFFER_SIZE] {0};
    char* reqBuf = new char[DEFAULT_BUFFER_SIZE] {0};

    while (true) {
        int msglen = 0;
        std::cout << "command> ";
        std::getline(std::cin, command_prompt);

        msglen = command_prompt.size();
        if (msglen >= DEFAULT_BUFFER_SIZE) {
            printf("\t -> [ERROR] Buffer oversize");
            continue;
        }

        memcpy(reqBuf, command_prompt.c_str(), msglen);

        // Отправляем запрос
        bSent = send(net_sock, reqBuf, msglen, 0);
        if (bSent == -1) {
            std::cerr << "[ERROR] Can't send data to server" << std::endl;
            perror(" > Details: ");
            continue;
        }

        // Получаем ответ
        bRecv = recv(net_sock, respBuf, DEFAULT_BUFFER_SIZE, 0);
        if (bRecv == 0) {
            std::cerr << "[ERROR] 0 bytes recieved from server" << std::endl;
            perror(" > Details: ");
            continue;
        }

        printf("%s\n", respBuf);

        // Условия завершения
        if (command_prompt == "quit") {
            printf("Exiting ..\n");
            break;
        }
        memset(respBuf, 0, DEFAULT_BUFFER_SIZE);
        memset(reqBuf, 0, DEFAULT_BUFFER_SIZE);
    }

    /* Чистки */
    shutdown(net_sock, SHUT_RDWR);
    delete respBuf;
    delete reqBuf;

    return 0;
}
