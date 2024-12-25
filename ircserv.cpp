#include <cstdlib>
#include <iostream>
#include <string>
#include "Server.hpp"

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
        return 1;
    }

    int port = atoi(argv[1]);
    std::string password = argv[2];

    std::cout << "Starting IRC server on port " << port << " with password: " << password << std::endl;

    Server server(port, password);
    if (!server.initialize()) {
        return 1;
    }
    server.run();

    return 0;
}
