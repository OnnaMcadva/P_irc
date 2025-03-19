#include "Server.hpp"
#include "CommandHandler.hpp" // Полное определение CommandHandler здесь
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <utility>

Server::Server(const Config& cfg)
    : m_serverSocket(-1), config(cfg), cmdHandler(new CommandHandler(*this)) {}

Server::~Server() {
    delete cmdHandler; // Освобождаем память
    if (m_serverSocket != -1) {
        close(m_serverSocket);
    }
}

bool Server::initialize() {
    std::cout << "Initializing server on port " << config.getPort() << " with password " << config.getPassword() << "\n";
    return setupSocket();
}

/* Функция `run()` запускает сервер и обрабатывает входящие соединения и данные от клиентов.  

1. Выводит сообщение `"Server is running..."`.  
2. Создаёт список файловых дескрипторов (`fds`) и добавляет в него **серверный сокет** для отслеживания новых подключений.  
3. В бесконечном цикле:  
   - **poll()** ждёт активности на сокетах (100 мс).  
   - Если `poll()` неудачен, выводит ошибку и завершает сервер.  
   - Если есть данные:  
     - Если это **серверный сокет** → вызывает `handleNewConnection(fds)`, чтобы обработать нового клиента.  
     - Если это **клиентский сокет** → вызывает `handleClientData(fds[i].fd, fds)`, чтобы обработать данные от клиента. */

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
    serverAddr.sin_port = htons(config.getPort());

    if (bind(m_serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error: bind failed\n";
        return false;
    }

    if (listen(m_serverSocket, 10) < 0) {
        std::cerr << "Error: listen failed\n";
        return false;
    }

    std::cout << "Server is listening on port " << config.getPort() << "\n";
    return true;
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

    Client newClient(clientSocket);
    newClient.appendOutputBuffer("Enter password: ");
    m_clients.insert(std::pair<int, Client>(clientSocket, newClient));

    pollfd clientFd;
    clientFd.fd = clientSocket;
    clientFd.events = POLLIN | POLLOUT;
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
                    removeClient(clientSocket, fds);
                    return;
                }

                buffer[bytesRead] = '\0';
                std::cout << "Raw data received: " << buffer << " (bytesRead: " << bytesRead << ")" << std::endl;
                m_clients[clientSocket].appendInputBuffer(buffer);

                std::cout << "Current buffer: " << m_clients[clientSocket].getInputBuffer() << std::endl;

                size_t pos = m_clients[clientSocket].getInputBuffer().find("\r\n");
                if (pos == std::string::npos) {
                    pos = m_clients[clientSocket].getInputBuffer().find("\n");
                }

                while (pos != std::string::npos) {
                    std::string input = m_clients[clientSocket].getInputBuffer().substr(0, pos);
                    size_t len_to_erase = (m_clients[clientSocket].getInputBuffer()[pos] == '\r') ? pos + 2 : pos + 1;
                    std::string temp = m_clients[clientSocket].getInputBuffer();
                    temp.erase(0, len_to_erase);
                    m_clients[clientSocket].clearInputBuffer();
                    m_clients[clientSocket].appendInputBuffer(temp);

                    if (!input.empty()) {
                        std::cout << "Processed input: " << input << std::endl;
                        cmdHandler->processCommand(clientSocket, input, fds, i); // Используем указатель
                    }

                    pos = m_clients[clientSocket].getInputBuffer().find("\r\n");
                    if (pos == std::string::npos) {
                        pos = m_clients[clientSocket].getInputBuffer().find("\n");
                    }
                }

                if (!m_clients[clientSocket].getInputBuffer().empty()) {
                    std::string input = m_clients[clientSocket].getInputBuffer();
                    while (!input.empty() && (input[input.length() - 1] == '\r' || input[input.length() - 1] == '\n')) {
                        input.erase(input.length() - 1);
                    }
                    if (!input.empty()) {
                        std::cout << "Processed input (no newline): " << input << std::endl;
                        m_clients[clientSocket].clearInputBuffer();
                        cmdHandler->processCommand(clientSocket, input, fds, i); // Используем указатель
                    }
                }
            }

            if (fds[i].revents & POLLOUT) {
                std::cout << "POLLOUT triggered for client " << clientSocket << ", buffer size: " << m_clients[clientSocket].getOutputBuffer().length() << std::endl;
                if (m_clients[clientSocket].getOutputBuffer().empty()) {
                    fds[i].events &= ~POLLOUT;
                } else {
                    ssize_t bytesSent = send(clientSocket, m_clients[clientSocket].getOutputBuffer().c_str(), m_clients[clientSocket].getOutputBuffer().length(), 0);
                    std::cout << "Bytes sent: " << bytesSent << std::endl;
                    if (bytesSent <= 0) {
                        std::cout << "Failed to send data to client.\n";
                        removeClient(clientSocket, fds);
                        return;
                    }
                    m_clients[clientSocket].eraseOutputBuffer(bytesSent);
                    if (m_clients[clientSocket].getOutputBuffer().empty()) {
                        fds[i].events &= ~POLLOUT;
                    }
                }
            }
            break;
        }
    }
}

void Server::removeClient(int clientSocket, std::vector<pollfd>& fds) {
    close(clientSocket);
    for (std::vector<pollfd>::iterator it = fds.begin(); it != fds.end(); ++it) {
        if (it->fd == clientSocket) {
            fds.erase(it);
            break;
        }
    }
    m_clients.erase(clientSocket);
    std::cout << "Client " << clientSocket << " removed.\n";
}
