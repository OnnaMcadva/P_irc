#include "Server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <unistd.h>

Server::Server(int port, const std::string& password) 
    : _port(port), _password(password), _serverSocket(-1) {}

bool Server::initialize() {
    std::cout << "Initializing server on port " << _port << " with password " << _password << "\n";
    return setupSocket();
}

void Server::run() {
    std::cout << "Server is running...\n";

    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        int clientSocket = accept(_serverSocket, (struct sockaddr *)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            std::cerr << "Error: Failed to accept client connection\n";
            continue;
        }

        _clients.push_back(clientSocket);

        std::cout << "New client connected: " << inet_ntoa(clientAddr.sin_addr)
                  << ":" << ntohs(clientAddr.sin_port) << "\n";

        char buffer[1024];
        ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::cout << "Received message: " << buffer << "\n";
            
            std::string response = "Message received: ";
            response += buffer;
            send(clientSocket, response.c_str(), response.length(), 0);
        }

        close(clientSocket);
    }
}

bool Server::setupSocket() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket < 0) {
        std::cerr << "Error: Unable to create socket\n";
        return false;
    }

    int opt = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error: setsockopt failed\n";
        return false;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(_port);

    if (bind(_serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error: bind failed\n";
        return false;
    }

    if (listen(_serverSocket, 10) < 0) {
        std::cerr << "Error: listen failed\n";
        return false;
    }

    std::cout << "Server is listening on port " << _port << "\n";
    return true;
}
