#include "Server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h> 
#include <poll.h> 
#include <cerrno> 

Server::Server(int port, const std::string& password) 
    : m_port(port), m_password(password), m_serverSocket(-1) {}

bool Server::initialize() {
    std::cout << "Initializing server on port " << m_port << " with password " << m_password << "\n";
    return setupSocket();
}

void Server::run() {
    std::cout << "Server is running...\n";

    std::vector<pollfd> fds;

    pollfd serverFd;
    serverFd.fd = m_serverSocket;
    serverFd.events = POLLIN; 
    fds.push_back(serverFd);

    while (true) {
        int ret = poll(fds.data(), fds.size(), 100);
        if (ret < 0) {
            std::cerr << "Error: poll failed\n";
            break;
        } else if (ret == 0) {
            continue;
        }

        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == m_serverSocket) {
                    handleNewConnection(fds);
                } else {
                    handleClientData(fds[i].fd, fds);
                }
            }
        }
    }
}

void Server::handleNewConnection(std::vector<pollfd>& fds) {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    int clientSocket = accept(m_serverSocket, (struct sockaddr *)&clientAddr, &clientLen);
    if (clientSocket < 0) {
        std::cerr << "Error: Failed to accept client connection\n";
        return;
    }

    if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "Error: fcntl failed to set non-blocking mode for client\n";
        close(clientSocket);
        return;
    }

    pollfd clientFd;
    clientFd.fd = clientSocket;
    clientFd.events = POLLIN;
    fds.push_back(clientFd);

    std::cout << "New client connected: " << inet_ntoa(clientAddr.sin_addr)
              << ":" << ntohs(clientAddr.sin_port) << "\n";
}

void Server::handleClientData(int clientSocket, std::vector<pollfd>& fds) {
    char buffer[1024];

    for (size_t i = 0; i < fds.size(); ++i) {
        if (fds[i].fd == clientSocket) {
            if (fds[i].revents & POLLIN) {
                ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);

                if (bytesRead <= 0) {
                    std::cout << "Client disconnected.\n";
                    close(clientSocket);

                    fds.erase(fds.begin() + i);
                    return;
                } else {
                    buffer[bytesRead] = '\0';
                    std::cout << "Received message: " << buffer << "\n";

                    if (strncmp(buffer, "QUIT", 4) == 0) {
                        std::cout << "Client requested to quit.\n";
                        close(clientSocket);
                        fds.erase(fds.begin() + i);
                        return;
                    } else if (strncmp(buffer, "NICK", 4) == 0) {
                        std::string nickname(buffer + 5);
                        std::cout << "Client set nickname: " << nickname << "\n";
                        std::string response = "Nickname set to: " + nickname;
                        send(clientSocket, response.c_str(), response.length(), 0);
                    } else {
                        std::string response = "Message received: ";
                        response += buffer;
                        send(clientSocket, response.c_str(), response.length(), 0);
                    }
                }
            }
            break;
        }
    }
}

bool Server::setupSocket() {
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket < 0) {
        std::cerr << "Error: Unable to create socket\n";
        return false;
    }

    if (fcntl(m_serverSocket, F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "Error: fcntl failed to set non-blocking mode for server socket\n";
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