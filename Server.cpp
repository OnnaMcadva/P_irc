#include "Server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <unistd.h>

Server::Server(int port, const std::string& password) 
    : m_port(port), m_password(password), m_serverSocket(-1) {}

bool Server::initialize() {
    std::cout << "Initializing server on port " << m_port << " with password " << m_password << "\n";
    return setupSocket();
}

void Server::run() {
    std::cout << "Server is running...\n";

    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        int clientSocket = accept(m_serverSocket, (struct sockaddr *)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            std::cerr << "Error: Failed to accept client connection\n";
            continue;
        }

        std::cout << "New client connected: " << inet_ntoa(clientAddr.sin_addr)
                  << ":" << ntohs(clientAddr.sin_port) << "\n";

        // Обработка клиента в цикле
        while (true) {
            char buffer[1024];
            ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);

            if (bytesRead <= 0) { // Разрыв соединения
                std::cout << "Client disconnected.\n";
                break;
            }

            buffer[bytesRead] = '\0';
            std::cout << "Received message: " << buffer << "\n";

            std::string response = "Message received: ";
            response += buffer;
            send(clientSocket, response.c_str(), response.length(), 0);
        }

        close(clientSocket);
    }
}


// void Server::run() {
//     std::cout << "Server is running...\n";

//     while (true) {
//         struct sockaddr_in clientAddr;
//         socklen_t clientLen = sizeof(clientAddr);

//         int clientSocket = accept(m_serverSocket, (struct sockaddr *)&clientAddr, &clientLen);
//         if (clientSocket < 0) {
//             std::cerr << "Error: Failed to accept client connection\n";
//             continue;
//         }

//         m_clients.push_back(clientSocket);

//         std::cout << "New client connected: " << inet_ntoa(clientAddr.sin_addr)
//                   << ":" << ntohs(clientAddr.sin_port) << "\n";

//         char buffer[1024];
//         ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
//         if (bytesRead > 0) {
//             buffer[bytesRead] = '\0';
//             std::cout << "Received message: " << buffer << "\n";
            
//             std::string response = "Message received: ";
//             response += buffer;
//             send(clientSocket, response.c_str(), response.length(), 0);
//         }

//         close(clientSocket);
//     }
// }

bool Server::setupSocket() {
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket < 0) {
        std::cerr << "Error: Unable to create socket\n";
        return false;
    }

    int opt = 1;
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error: setsockopt failed\n";
        return false;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(m_port);

    if (bind(m_serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error: bind failed\n";
        return false;
    }

    if (listen(m_serverSocket, 10) < 0) {
        std::cerr << "Error: listen failed\n";
        return false;
    }

    std::cout << "Server is listening on port " << m_port << "\n";
    return true;
}
