#pragma once
#include <string>
#include <vector>
#include <poll.h>
#include <map>

class Client {
    public:
        Client(int socket) 
            : socket(socket), passwordEntered(false), passwordAttempts(3), nickname(""), username("") {}
        Client() 
            : socket(-1), passwordEntered(false), passwordAttempts(3), nickname(""), username("") {}
        int socket;           // Номер сокета клиента
        bool passwordEntered; // Прошёл ли проверку пароля
        int passwordAttempts; // Сколько попыток осталось
        std::string nickname; // Ник клиента
        std::string username; // Юзернейм клиента
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