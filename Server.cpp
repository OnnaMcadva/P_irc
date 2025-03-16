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

    void Server::handleClientData(int clientSocket, std::vector<pollfd>& fds) {
        char buffer[1024];
        static std::map<int, std::string> clientBuffers; // Буфер для каждого клиента
    
        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].fd == clientSocket) {
                if (fds[i].revents & POLLIN) {
                    ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
                    if (bytesRead <= 0) {
                        std::cout << "Client disconnected.\n";
                        removeClient(clientSocket, fds);
                        clientBuffers.erase(clientSocket);
                        return;
                    }
    
                    buffer[bytesRead] = '\0';
                    clientBuffers[clientSocket] += buffer;
    
                    size_t pos = clientBuffers[clientSocket].find("\r\n");
                    if (pos == std::string::npos) {
                        pos = clientBuffers[clientSocket].find("\n");
                    }
    
                    while (pos != std::string::npos) {
                        std::string input = clientBuffers[clientSocket].substr(0, pos);
                        size_t len_to_erase = (clientBuffers[clientSocket][pos] == '\r') ? pos + 2 : pos + 1;
                        clientBuffers[clientSocket].erase(0, len_to_erase);
    
                        if (!input.empty()) {
                            std::cout << "Received message: " << input << "\n";
    
                            std::map<int, Client>::iterator it = m_clients.find(clientSocket);
                            if (it == m_clients.end()) {
                                std::cerr << "Error: Unknown client\n";
                                removeClient(clientSocket, fds);
                                clientBuffers.erase(clientSocket);
                                return;
                            }
    
                            Client& client = it->second;
                            if (!client.passwordEntered) {
                                if (input == m_password) {
                                    client.passwordEntered = true;
                                    std::cout << "Client authenticated with password: " << input << "\n";
                                    // Временное имя, если ник не задан
                                    std::stringstream ss;
                                    ss << clientSocket;
                                    std::string clientName = client.nickname.empty() ? "guest" + ss.str() : client.nickname;
                                    std::string response = ":server 001 " + clientName + " :Welcome to the IRC server\r\n";
                                    send(clientSocket, response.c_str(), response.length(), 0);
                                } else {
                                    client.passwordAttempts--;
                                    if (client.passwordAttempts > 0) {
                                        std::stringstream ss;
                                        ss << clientSocket;
                                        std::string clientName = client.nickname.empty() ? "guest" + ss.str() : client.nickname;
                                        std::stringstream attempts;
                                        attempts << client.passwordAttempts;
                                        std::string response = ":server 464 " + clientName + " :Wrong password. Attempts left: " + attempts.str() + "\r\n";
                                        send(clientSocket, response.c_str(), response.length(), 0);
                                    } else {
                                        std::stringstream ss;
                                        ss << clientSocket;
                                        std::string clientName = client.nickname.empty() ? "guest" + ss.str() : client.nickname;
                                        std::string response = ":server 464 " + clientName + " :Too many wrong attempts. Disconnecting\r\n";
                                        send(clientSocket, response.c_str(), response.length(), 0);
                                        removeClient(clientSocket, fds);
                                        close(clientSocket);
                                    }
                                }
                            } else {
                                if (strncmp(input.c_str(), "QUIT", 4) == 0) {
                                    std::cout << "Client requested to quit.\n";
                                    std::string goodbyeMessage = ":server 221 " + client.nickname + " :Goodbye\r\n";
                                    send(clientSocket, goodbyeMessage.c_str(), goodbyeMessage.length(), 0);
                                    close(clientSocket);
                                    fds.erase(fds.begin() + i);
                                    removeClient(clientSocket, fds);
                                    clientBuffers.erase(clientSocket);
                                    return;
                                } else if (input.rfind("NICK", 0) == 0) {
                                    std::string nickname = input.substr(5);
                                    if (nickname.empty()) {
                                        std::string response = ":server 431 " + client.nickname + " :No nickname given\r\n";
                                        send(clientSocket, response.c_str(), response.length(), 0);
                                    } else {
                                        client.nickname = nickname;
                                        std::cout << "Client set nickname: " + nickname + "\n";
                                        std::string response = ":server 001 " + nickname + " :Nickname set\r\n";
                                        send(clientSocket, response.c_str(), response.length(), 0);
                                    }
                                } else if (input.rfind("USER", 0) == 0) {
                                    std::istringstream iss(input.substr(5));
                                    std::string username, mode, unused, realname;
                                    // Пропускаем пробелы и парсим
                                    iss >> username >> mode >> unused;
                                    // Ищем двоеточие и realname
                                    std::string rest;
                                    std::getline(iss, rest);
                                    size_t colonPos = rest.find(':');
                                    if (colonPos != std::string::npos) {
                                        realname = rest.substr(colonPos + 1);
                                        // Удаляем лишние пробелы
                                        while (!realname.empty() && realname[0] == ' ') {
                                            realname.erase(0, 1);
                                        }
                                        client.username = username;
                                        std::cout << "Client set username: " << username << "\n";
                                        // Приветствие после NICK и USER
                                        if (!client.nickname.empty()) {
                                            std::string response = ":server 001 " + client.nickname + " :Welcome to the IRC server " + client.nickname + "!\r\n";
                                            send(clientSocket, response.c_str(), response.length(), 0);
                                        } else {
                                            std::string response = ":server 451 " + client.nickname + " :You have not registered\r\n";
                                            send(clientSocket, response.c_str(), response.length(), 0);
                                        }
                                    } else {
                                        std::string response = ":server 461 " + client.nickname + " USER :Syntax error\r\n";
                                        send(clientSocket, response.c_str(), response.length(), 0);
                                    }
                                
                                } else if (input.rfind("JOIN", 0) == 0) {
                                    std::string channelName = input.substr(5);
                                    if (channelName.empty() || channelName[0] != '#') {
                                        std::string response = ":server 403 " + client.nickname + " " + channelName + " :No such channel\r\n";
                                        send(clientSocket, response.c_str(), response.length(), 0);
                                    } else {
                                        joinChannel(clientSocket, channelName);
                                        std::cout << "Client joined channel: " << channelName << "\n";
                                        std::string response = ":" + client.nickname + " JOIN " + channelName + "\r\n";
                                        send(clientSocket, response.c_str(), response.length(), 0);
                                        // Уведомление другим в канале
                                        broadcastMessage(clientSocket, "JOIN " + channelName);
                                    }
                                } else if (input.rfind("PRIVMSG", 0) == 0) {
                                    size_t spacePos = input.find(' ');
                                    size_t colonPos = input.find(':');
                                    if (spacePos != std::string::npos && colonPos != std::string::npos && colonPos > spacePos) {
                                        std::string target = input.substr(spacePos + 1, colonPos - spacePos - 1);
                                        std::string message = input.substr(colonPos + 1);
                                        std::cout << "Received private message to " << target << ": " << message << "\n";
                                        broadcastMessage(clientSocket, "PRIVMSG " + target + " :" + message);
                                        std::string response = ":server 001 " + client.nickname + " :Message sent\r\n";
                                        send(clientSocket, response.c_str(), response.length(), 0);
                                    } else {
                                        std::string response = ":server 401 " + client.nickname + " :No recipient or message\r\n";
                                        send(clientSocket, response.c_str(), response.length(), 0);
                                    }
                                } else {
                                    std::string response = ":server 421 " + client.nickname + " " + input + " :Unknown command\r\n";
                                    send(clientSocket, response.c_str(), response.length(), 0);
                                }
                            }
                        }
    
                        pos = clientBuffers[clientSocket].find("\r\n");
                        if (pos == std::string::npos) {
                            pos = clientBuffers[clientSocket].find("\n");
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
    // Разбираем сообщение (например, "PRIVMSG #test :hello" или "JOIN #test")
    std::string command, target, text;
    std::istringstream iss(message);
    iss >> command >> target;
    if (command == "PRIVMSG") {
        size_t colonPos = message.find(':');
        if (colonPos != std::string::npos) {
            text = message.substr(colonPos + 1);
        } else {
            return; // Нет текста сообщения
        }
    }

    // Ищем канал
    for (std::vector<Channel>::iterator it = channels.begin(); it != channels.end(); ++it) {
        if (it->name == target) {
            for (std::vector<int>::iterator memberIt = it->members.begin(); memberIt != it->members.end(); ++memberIt) {
                if (*memberIt != senderSocket) {
                    std::string senderNick = m_clients[senderSocket].nickname;
                    std::string response;
                    if (command == "PRIVMSG") {
                        response = ":" + senderNick + " PRIVMSG " + target + " :" + text + "\r\n";
                    } else if (command == "JOIN") {
                        response = ":" + senderNick + " JOIN " + target + "\r\n";
                    }
                    send(*memberIt, response.c_str(), response.length(), 0);
                }
            }
            break;
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
