#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <poll.h>
#include "Config.hpp"
#include "Client.hpp"
#include "Channel.hpp"

class CommandHandler;

class Server {
public:
    Server(const Config& cfg);
    ~Server(); // Добавляем деструктор для очистки cmdHandler
    bool initialize();
    void run();

private:
    int m_serverSocket;
    Config config;
    std::map<int, Client> m_clients;
    std::vector<Channel> channels;
    CommandHandler* cmdHandler; // Теперь указатель

    bool setupSocket();
    void handleNewConnection(std::vector<pollfd>& fds);
    void handleClientData(int clientSocket, std::vector<pollfd>& fds);
    void removeClient(int clientSocket, std::vector<pollfd>& fds);

    friend class CommandHandler;
};

#endif