#pragma once
#include <string>
#include <vector>
#include <poll.h>
#include <map>

class Client {
    public:
        int fd;
        bool passwordEntered;
        int passwordAttempts;
        Client(int fd) : fd(fd), passwordEntered(false), passwordAttempts(3) {}
        Client() : passwordEntered(false), passwordAttempts(3) {}
    };


class Channel {
public:
    std::string name;
    std::vector<int> members;

    Channel(const std::string& name) : name(name) {}
};

class Server {
public:
    Server(int port, const std::string& password);
    bool initialize();
    void run();

private:
    int m_port;
    std::string m_password;
    int m_serverSocket;
    std::map<int, Client> m_clients;
    std::vector<Channel> channels;

    bool setupSocket();
    void handleNewConnection(std::vector<pollfd>& fds);
    void handleClientData(int clientSocket, std::vector<pollfd>& fds);
    void removeClient(int clientSocket, std::vector<pollfd>& fds);
    void joinChannel(int clientSocket, const std::string& channelName);
    void broadcastMessage(int senderSocket, const std::string& message);
};