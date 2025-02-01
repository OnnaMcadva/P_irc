#pragma once
#include <string>
#include <vector>
#include <poll.h> 

class Server {
public:
    Server(int port, const std::string& password);
    bool initialize();
    void run();

private:
    int m_port;
    std::string m_password;
    int m_serverSocket;
    std::vector<int> m_clients;

    bool setupSocket();
    void handleNewConnection(std::vector<pollfd>& fds);
    void handleClientData(int clientSocket, std::vector<pollfd>& fds);
};