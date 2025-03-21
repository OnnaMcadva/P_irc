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

// Инициализация статической переменной
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
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "Error: poll failed with errno " << errno << "\n";
            shutdown();
            return;
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

    std::cout << "\n✨ \033[38;5;227mShutting down server...\033[0m\n ✨";
    shutdown();
    return;
}

/* Функция `setupSocket()` настраивает серверный сокет для работы.   
1. Создаёт **TCP-сокет** (`m_serverSocket`).  
2. Устанавливает **неблокирующий режим** для сокета.  
3. Включает опцию **повторного использования адреса** (`SO_REUSEADDR`).  
4. Заполняет структуру `sockaddr_in` и привязывает сокет (`bind()`) к порту из конфига.  
5. Переводит сокет в режим **прослушивания входящих соединений** (`listen()`).  
6. Если всё успешно, сервер начинает слушать порт и выводит сообщение.  
7. В случае ошибки возвращает `false`. */

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
                        cmdHandler->processCommand(clientSocket, input, fds, i);
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
                        cmdHandler->processCommand(clientSocket, input, fds, i);
                    }
                }
            }

            if (fds[i].revents & POLLOUT) {
                std::cout << "POLLOUT triggered for client " << clientSocket << ", buffer size: " << m_clients[clientSocket].getOutputBuffer().length() << std::endl;
                
                // Проверяем, существует ли клиент
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
    m_clients.erase(clientSocket);
    std::cout << "Client " << clientSocket << " removed.\n";
}

void Server::shutdown() {
    for (std::map<int, Client>::iterator it = m_clients.begin(); it != m_clients.end(); ++it) {
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
