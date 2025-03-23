#pragma once

#include <string>
#include <vector>
#include <poll.h>
#include "Client.hpp" // Добавлено для объявления Client

class Server;

class CommandHandler {
public:
    CommandHandler(Server& s);
    void processCommand(int clientSocket, const std::string& input, std::vector<pollfd>& fds, size_t i);
    void broadcastMessage(int senderSocket, const std::string& message);

private:
    Server& server;

    // Вспомогательные функции для processCommand
    bool checkClient(int clientSocket, std::vector<pollfd>& fds, Client*& client);
    void handlePassword(int clientSocket, const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i);
    void handleQuit(int clientSocket, Client& client, std::vector<pollfd>& fds);
    void handleNick(int clientSocket, const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i);
    void handleUser(const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i); // убрал clientSocket
    void handleJoin(int clientSocket, const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i);
    void handlePrivmsg(int clientSocket, const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i);
    void handleUnknownCommand(const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i);
};
