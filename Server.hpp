#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>

class Server {
public:
    Server(int port, const std::string& password);
    bool initialize();
    void run();

private:
    int _port;
    std::string _password;
    int _serverSocket;
    std::vector<int> _clients;

    bool setupSocket();
};

#endif
