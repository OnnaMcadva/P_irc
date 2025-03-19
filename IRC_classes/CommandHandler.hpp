#pragma once

#include <string>
#include <vector>
#include <poll.h>

class Server;

class CommandHandler {
public:
    CommandHandler(Server& s);
    void processCommand(int clientSocket, const std::string& input, std::vector<pollfd>& fds, size_t i);
    void broadcastMessage(int senderSocket, const std::string& message);

private:
    Server& server;
};
