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
#include <utility>
#include <sstream>

Server::Server(int port, const std::string& password) 
    : m_port(port), m_password(password), m_serverSocket(-1) {}

bool Server::initialize() {
    std::cout << "Initializing server on port " << m_port << " with password " << m_password << "\n";
    return setupSocket();
}

/* Server::run() запускает сервер и обрабатывает входящие соединения и данные от клиентов:
    Инициализация: Добавляет серверный сокет (m_serverSocket) в массив отслеживаемых файловых дескрипторов (fds).
    Цикл работы: Постоянно вызывает poll, чтобы мониторить активности на сокетах (новые подключения или данные от клиентов).
    Обработка событий:
        Если событие пришло от серверного сокета, вызывается handleNewConnection для обработки нового подключения.
        Если событие пришло от клиентского сокета, вызывается handleClientData для обработки данных клиента.*/

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

/* handleNewConnection обрабатывает подключение нового клиента к серверу.
    Принимает подключение: Использует accept, чтобы установить связь с клиентом.
    Устанавливает неблокирующий режим: Настраивает сокет клиента в неблокирующий режим с помощью fcntl.
    Отправляет приветственное сообщение: Запрашивает у клиента пароль с помощью send.
    Добавляет клиента в список отслеживаемых файловых дескрипторов: Добавляет pollfd объекта клиента в массив fds для последующей работы (например, чтения данных).
    Выводит информацию о клиенте: Логирует IP-адрес и порт нового клиента.*/

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

    std::string passwordPrompt = "Enter password: ";
    send(clientSocket, passwordPrompt.c_str(), passwordPrompt.length(), 0);

    m_clients.insert(std::make_pair(clientSocket, Client(clientSocket)));

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
                    removeClient(clientSocket, fds);
                    return;
                }

                buffer[bytesRead] = '\0';
                std::string input = buffer;

                size_t pos = input.find("\r\n");
                if (pos != std::string::npos) {
                    input = input.substr(0, pos);
                }
                if (input.empty()) {
                    return;
                }

                std::cout << "Received message: " << input << "\n";

                std::map<int, Client>::iterator it = m_clients.find(clientSocket);
                if (it == m_clients.end()) {
                    std::cerr << "Error: Unknown client\n";
                    removeClient(clientSocket, fds);
                    return;
                }

                Client& client = it->second;

                if (!client.passwordEntered) {
                    if (input == m_password) {
                        client.passwordEntered = true;
                        std::string response = "Password accepted\n";
                        send(clientSocket, response.c_str(), response.length(), 0);
                    } else {
                        client.passwordAttempts--;
                        if (client.passwordAttempts > 0) {
                            std::stringstream ss;
                            ss << client.passwordAttempts;
                            std::string response = "Wrong password. Attempts left: " + ss.str() + "\n";
                            send(clientSocket, response.c_str(), response.length(), 0);
                        } else {
                            std::string response = "Too many wrong attempts. Disconnecting.\n";
                            send(clientSocket, response.c_str(), response.length(), 0);
                            removeClient(clientSocket, fds);
                            close(clientSocket);
                        }
                    }
                    return;
                }
                
                if (strncmp(buffer, "QUIT", 4) == 0) {
                        std::cout << "Client requested to quit.\n";
                        std::string goodbyeMessage = "Goodbye!\n";
                        send(clientSocket, goodbyeMessage.c_str(), goodbyeMessage.length(), 0);
                        close(clientSocket);
                        fds.erase(fds.begin() + i);
                        removeClient(clientSocket, fds);
                        return;
                } else if (input.rfind("NICK", 0) == 0) {
                    std::string nickname = input.substr(5);
                    std::cout << "Client set nickname: " << nickname << "\n";
                    std::string response = "Nickname set to: " + nickname + "\n";
                    send(clientSocket, response.c_str(), response.length(), 0);
                } else if (input.rfind("USER", 0) == 0) {
                    std::string username = input.substr(5);
                    std::cout << "Client set username: " << username << "\n";
                    std::string response = "Username set to: " + username + "\n";
                    send(clientSocket, response.c_str(), response.length(), 0);
                } else if (input.rfind("JOIN", 0) == 0) {
                    std::string channelName = input.substr(5);
                    std::cout << "Client joined channel: " << channelName << "\n";
                    joinChannel(clientSocket, channelName);
                } else if (input.rfind("PRIVMSG", 0) == 0) {
                    std::string message = input.substr(8);
                    std::cout << "Received private message: " << message << "\n";
                    broadcastMessage(clientSocket, message);
                } else {
                    std::string response = "Message received: " + input + "\n";
                    send(clientSocket, response.c_str(), response.length(), 0);
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

void Server::joinChannel(int clientSocket, const std::string& channelName) {
    bool channelExists = false;
    for (std::vector<Channel>::iterator it = channels.begin(); it != channels.end(); ++it) {
        if (it->name == channelName) {
            it->members.push_back(clientSocket);
            channelExists = true;
            break;
        }
    }

    if (!channelExists) {
        Channel newChannel(channelName);
        newChannel.members.push_back(clientSocket);
        channels.push_back(newChannel);
    }

    std::string response = "Joined channel: " + channelName + "\n";
    send(clientSocket, response.c_str(), response.length(), 0);
}

void Server::broadcastMessage(int senderSocket, const std::string& message) {
    for (std::vector<Channel>::iterator it = channels.begin(); it != channels.end(); ++it) {
        for (std::vector<int>::iterator memberIt = it->members.begin(); memberIt != it->members.end(); ++memberIt) {
            if (*memberIt != senderSocket) {
                send(*memberIt, message.c_str(), message.length(), 0);
            }
        }
    }
}

/* setupSocket настраивает серверный сокет для приёма входящих подключений. Вот её ключевые шаги:
    Создаёт сокет: Создаёт сокет с протоколом TCP (SOCK_STREAM).
    Устанавливает неблокирующий режим: Настраивает сокет на неблокирующий режим с помощью fcntl.
    Позволяет повторное использование порта: Использует setsockopt с флагом SO_REUSEADDR, чтобы избежать ошибок при повторном запуске.
    Привязывает сокет к адресу: Привязывает сокет к указанному порту (m_port) и адресу (INADDR_ANY — любые IP-адреса машины).
    Начинает слушать: Запускает ожидание входящих подключений (listen).*/

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