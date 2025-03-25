#include "Server.hpp"
#include "CommandHandler.hpp"
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
#include <signal.h>
#include <stdexcept>

bool Server::shouldStop = false;

Server::Server(const Config& cfg)
    : m_serverSocket(-1), config(cfg), cmdHandler(new CommandHandler(*this)) {}

Server::~Server() {
    delete cmdHandler;
    if (m_serverSocket != -1) {
        close(m_serverSocket);
    }
}

bool Server::initialize() {
    std::cout << "Initializing server on port " << config.getPort() << " with password " << config.getPassword() << "\n";
    signal(SIGHUP, Server::signalHandler); /* Защита от закрытия терминала с сервером */
    signal(SIGINT, Server::signalHandler);  /* Защита от Ctrl+C или от kill -INT <pid> */
    return setupSocket();
}

void Server::signalHandler(int sig) {
    if (sig == SIGINT || sig == SIGHUP) {
        shouldStop = true;
    }
}

/* Функция `run()` запускает сервер и обрабатывает входящие соединения и данные от клиентов. */
void Server::run() {
    std::cout << "🧠 \033[38;5;219mServer is running...\033[0m\n";

    std::vector<pollfd> fds;
    pollfd serverFd;
    serverFd.fd = m_serverSocket; /* файловый дескриптор, который нужно мониторить */
    serverFd.events = POLLIN; /* ключевое событие для обработки входящих сообщений и команд */
    serverFd.revents = 0; /* события, которые произошли (заполняется системой) */
    fds.push_back(serverFd); /* первый сокет добавлен. это собственно сам сервер */

    while (!shouldStop) {
        int ret = poll(fds.data(), fds.size(), 100);
        if (ret < 0) {
            if (errno == EINTR) continue;
            std::cerr << "Error: poll failed with errno " << errno << "\n";
            shutdown();
            return;
        } else if (ret == 0) {
            continue;
        }

        std::vector<size_t> toRemove; // Индексы сокетов для удаления
        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == m_serverSocket) {
                    handleNewConnection(fds);
                } else {
                    handleClientData(fds[i].fd, fds);
                }
            }
            if (fds[i].revents & POLLOUT && fds[i].fd != m_serverSocket) {
                std::map<int, Client>::iterator clientIt = m_clients.find(fds[i].fd);
                if (clientIt == m_clients.end()) {
                    std::cout << "Client " << fds[i].fd << " not found in m_clients.\n";
                    toRemove.push_back(i);
                    continue;
                }

                Client& client = clientIt->second;
                std::string buffer = client.getOutputBuffer();
                if (!buffer.empty()) {
                    ssize_t bytesWritten = send(fds[i].fd, buffer.c_str(), buffer.size(), 0);
                    if (bytesWritten > 0) {
                        client.eraseOutputBuffer(bytesWritten);
                        std::cout << "Bytes sent: " << bytesWritten << "\n";
                    } else if (bytesWritten < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            std::cout << "Temporary write block for client " << fds[i].fd << ", will retry later.\n";
                        } else {
                            std::cerr << "Error writing to client " << fds[i].fd << ": " << strerror(errno) << "\n";
                            toRemove.push_back(i); // Помечаем для удаления при серьёзной ошибке
                        }
                    } else { // bytesWritten == 0
                        std::cout << "Client " << fds[i].fd << " closed connection.\n";
                        toRemove.push_back(i); // Клиент закрыл соединение
                    }
                }
                if (client.getOutputBuffer().empty()) {
                    fds[i].events = POLLIN; // Снимаем POLLOUT, если буфер пуст
                }
            }
            // Если клиент удалён из m_clients, помечаем сокет для удаления
            if (fds[i].fd != m_serverSocket && m_clients.find(fds[i].fd) == m_clients.end()) {
                toRemove.push_back(i);
            }
        }

        // Удаляем "мёртвые" сокеты
        for (std::vector<size_t>::reverse_iterator it = toRemove.rbegin(); it != toRemove.rend(); ++it) {
            std::cout << "Closing socket " << fds[*it].fd << "\n"; // Для отладки
            close(fds[*it].fd); // Закрываем сокет
            fds.erase(fds.begin() + *it); // Удаляем из fds
        }

        // Обновляем events для оставшихся клиентов
        for (size_t i = 1; i < fds.size(); ++i) {
            Client& client = m_clients[fds[i].fd];
            if (!client.getOutputBuffer().empty()) {
                fds[i].events = POLLIN | POLLOUT;
            } else {
                fds[i].events = POLLIN;
            }
        }
    }

    std::cout << "\n✨ \033[38;5;227mShutting down server...\033[0m\n ✨";
    shutdown();
    return;
}

/* Функция `setupSocket()` настраивает серверный сокет для работы. */

bool Server::setupSocket() {
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    /* AF_INET (IPv4) если есть Wi-Fi (192.168.1.1) и localhost (127.0.0.1) [или AF_INET6 (IPv6)], сервер будет слушать оба, SOCK_STREAM Transmission Control Protocol — протокол управления передачей) */
    if (m_serverSocket < 0) {
        std::cerr << "Error: Unable to create socket\n";
        return false;
    }

    if (fcntl(m_serverSocket, F_SETFL, O_NONBLOCK) < 0) /* Неблокирующий режим: F_SETFL поменять настройки, O_NONBLOCK собственно настройка -1/0 */
    {
        std::cerr << "Error: fcntl failed to set non-blocking mode for server socket\n";
        return false;
    }

    int opt = 1; /* включить SO_REUSEADDR повторно использовать порт, что не работало у чуваков на видео */
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) /* Socket Option Level */
    {
        std::cerr << "Error: setsockopt failed\n";
        return false;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET; /* AF_INET интернет-адреса IPv4 */
    serverAddr.sin_addr.s_addr = INADDR_ANY; /* любой IP-адрес с компа */
    serverAddr.sin_port = htons(config.getPort()); /* host to network short переводит в спец интернет-формат */

    if (bind(m_serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) /* привязка сокета к порту и адресу */
    {
        std::cerr << "Error: bind failed, reason: " << strerror(errno) << "\n";
        return false;
    }

    if (listen(m_serverSocket, 10) < 0) {
        std::cerr << "Error: listen failed, reason: " << strerror(errno) << "\n";
        return false;
    }

    std::cout << "Server is listening on port " << config.getPort() << "\n";
    return true;
}

/* Функция `handleNewConnection()` обрабатывает новое входящее соединение.  
1. **Принимает подключение** нового клиента (`accept()`).  
2. **Устанавливает неблокирующий режим** для клиентского сокета.  
3. **Создаёт объект клиента** и добавляет его в список клиентов (`m_clients`).  
4. **Отправляет приглашение ввести пароль**.  
5. **Добавляет сокет клиента** в список отслеживаемых дескрипторов (`pollfd`) для чтения (`POLLIN`) и записи (`POLLOUT`).  
6. Выводит сообщение о новом подключении.  
7. В случае ошибки закрывает сокет и завершает работу. */

void Server::handleNewConnection(std::vector<pollfd>& fds) {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    int clientSocket = accept(m_serverSocket, (struct sockaddr *)&clientAddr, &clientLen);
    if (clientSocket < 0) {
        std::cerr << "Error: Failed to accept client connection\n";
        return;
    }

    // Проверка на превышение лимита файловых дескрипторов
    long maxFds = sysconf(_SC_OPEN_MAX);
    if (maxFds < 0) {
        std::cerr << "Error: Failed to get max file descriptors limit\n";
        close(clientSocket);
        return;
    }
    // Учитываем серверный сокет и минимум 3 стандартных дескриптора (stdin, stdout, stderr)
    if (fds.size() >= static_cast<size_t>(maxFds) - 4) {
        std::cerr << "Error: Maximum number of file descriptors reached (" << maxFds << ")\n";
        std::cerr << "Rejecting new connection from " << inet_ntoa(clientAddr.sin_addr)
                  << ":" << ntohs(clientAddr.sin_port) << "\n";
        close(clientSocket);
        return;
    }

    if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "Error: fcntl failed to set non-blocking mode for client\n";
        close(clientSocket);
        return;
    }

    m_clients.insert(std::make_pair(clientSocket, Client(clientSocket)));

    pollfd clientFd;
    clientFd.fd = clientSocket;
    clientFd.events = POLLIN | POLLOUT;
    clientFd.revents = 0; // Явно инициализируем
    fds.push_back(clientFd);

    std::cout << "New client connected: " << inet_ntoa(clientAddr.sin_addr)
              << ":" << ntohs(clientAddr.sin_port) << "\n";
}

/* Функция `handleClientData()` обрабатывает данные, полученные от клиента, и отправляет ответы.  
1. **Чтение данных**  
   - Проверяет, есть ли данные для чтения (`POLLIN`).  
   - Читает данные в буфер (`read()`).  
   - Если клиент отключился, удаляет его (`removeClient()`).  
   - Добавляет прочитанные данные в буфер клиента.  

2. **Обработка команд**  
   - Ищет конец строки (`\r\n` или `\n`).  
   - Извлекает и обрабатывает команду (`cmdHandler->processCommand()`).  
   - Очищает обработанные данные из буфера.  

3. **Отправка данных клиенту**  
   - Проверяет, есть ли данные для отправки (`POLLOUT`).  
   - Если буфер не пуст, отправляет данные (`send()`).  
   - Если отправка не удалась, удаляет клиента.  
   - Если буфер пуст, убирает флаг `POLLOUT`. */

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

                std::string inputBuffer = m_clients[clientSocket].getInputBuffer(); // Копируем буфер для обработки
                std::cout << "Current buffer: " << inputBuffer << std::endl;

                size_t pos;
                while ((pos = inputBuffer.find("\r\n")) != std::string::npos || (pos = inputBuffer.find("\n")) != std::string::npos) {
                    std::string command = inputBuffer.substr(0, pos);
                    size_t len_to_erase = (inputBuffer[pos] == '\r') ? pos + 2 : pos + 1;
                    inputBuffer.erase(0, len_to_erase);

                    if (!command.empty()) {
                        std::cout << "Processed input: " << command << std::endl;
                        cmdHandler->processCommand(clientSocket, command, fds, i);
                    }
                }

                // Обновляем буфер клиента остатком
                m_clients[clientSocket].clearInputBuffer();
                if (!inputBuffer.empty()) {
                    m_clients[clientSocket].appendInputBuffer(inputBuffer);
                    std::cout << "Partial data remains in buffer: " << inputBuffer << std::endl;
                }
            }

            if (fds[i].revents & POLLOUT) {
                std::cout << "POLLOUT triggered for client " << clientSocket << ", buffer size: " << m_clients[clientSocket].getOutputBuffer().length() << std::endl;
                
                std::map<int, Client>::iterator clientIt = m_clients.find(clientSocket);
                if (clientIt == m_clients.end()) {
                    std::cout << "Client " << clientSocket << " not found in m_clients.\n";
                    removeClient(clientSocket, fds);
                    return;
                }
                
                if (clientIt->second.getOutputBuffer().empty()) {
                    fds[i].events &= ~POLLOUT; // Снимаем POLLOUT, если буфер пуст
                } else {
                    ssize_t bytesSent = send(clientSocket, clientIt->second.getOutputBuffer().c_str(), clientIt->second.getOutputBuffer().length(), 0);
                    std::cout << "Bytes sent: " << bytesSent << std::endl;
                    
                    if (bytesSent < 0) { // Ошибка отправки
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            std::cout << "Temporary write block for client " << clientSocket << ", will retry later.\n";
                            return; // Не удаляем клиента, ждем следующего POLLOUT
                        } else {
                            std::cout << "Failed to send data to client " << clientSocket << ": errno " << errno << "\n";
                            removeClient(clientSocket, fds);
                            return;
                        }
                    } else if (bytesSent == 0) { // Клиент закрыл соединение
                        std::cout << "Client " << clientSocket << " closed connection.\n";
                        removeClient(clientSocket, fds);
                        return;
                    } else { // Успешная отправка
                        if (static_cast<size_t>(bytesSent) < clientIt->second.getOutputBuffer().length()) {
                            std::cout << "Partial send: " << bytesSent << " of " << clientIt->second.getOutputBuffer().length() << " bytes sent.\n";
                        }
                        clientIt->second.eraseOutputBuffer(bytesSent);
                        if (clientIt->second.getOutputBuffer().empty()) {
                            fds[i].events &= ~POLLOUT; // Снимаем POLLOUT, если буфер опустел
                        }
                    }
                }
            }
            break;
        }
    }
}

void Server::removeClientFromChannels(int clientSocket) {
    for (std::vector<Channel>::iterator it = channels.begin(); it != channels.end(); ++it) {
        it->removeMember(clientSocket);
    }
}

/* Функция `removeClient()` удаляет клиента из сервера.   
1. **Закрывает соединение** с клиентом (`close(clientSocket)`).  
2. **Удаляет клиента из списка опроса (`pollfd`)**, чтобы сервер больше не следил за его событиями.  
3. **Удаляет клиента из списка активных (`m_clients`)**.  
4. **Выводит сообщение** о том, что клиент был удалён. */

void Server::removeClient(int clientSocket, std::vector<pollfd>& fds) {
    close(clientSocket);
    for (std::vector<pollfd>::iterator it = fds.begin(); it != fds.end(); ++it) {
        if (it->fd == clientSocket) {
            fds.erase(it);
            break;
        }
    }
    removeClientFromChannels(clientSocket);
    m_clients.erase(clientSocket);
    std::cout << "Client " << clientSocket << " removed.\n";
}

void Server::shutdown() {
    for (std::map<int, Client>::iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
        // SOOBSHENIE POSLAT
        close(it->first);
    }
    m_clients.clear();

    if (m_serverSocket != -1) {
        close(m_serverSocket);
        m_serverSocket = -1;
    }

    channels.clear();

    std::cout << " \033[38;5;222mServer shutdown complete.\033[0m\n";
    std::cout << "                               🚀\n";
}
