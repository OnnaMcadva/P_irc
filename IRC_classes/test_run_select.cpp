/* with select */

#include <iostream>
#include <sys/select.h>
#include <unistd.h>

void Server::run() {
    std::cout << "Server is running...\n";

    std::vector<pollfd> fds;
    pollfd serverFd;
    serverFd.fd = m_serverSocket;
    serverFd.events = POLLIN;
    fds.push_back(serverFd);

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    while (true) {
        int ret = poll(fds.data(), fds.size(), 100);
        if (ret < 0) {
            std::cerr << "Error: poll failed\n";
            break;
        } else if (ret == 0) {
            continue;
        }

        fd_set tmpfds = readfds;
        int selectRet = select(STDIN_FILENO + 1, &tmpfds, nullptr, nullptr, nullptr);
        if (selectRet > 0 && FD_ISSET(STDIN_FILENO, &tmpfds)) {
            std::string input;
            std::getline(std::cin, input);
            if (input == "quit") { 
                std::cout << "Shutting down the server...\n";
                break;
            }
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

    std::cout << "Server shutdown complete.\n";
}

/* with poll */

#include <iostream>
#include <poll.h>
#include <unistd.h>
#include <cstring>

bool serverRunning = true;

void Server::run() {
    std::cout << "Server is running...\n";

    std::vector<pollfd> fds;
    pollfd serverFd;
    serverFd.fd = m_serverSocket;
    serverFd.events = POLLIN;
    fds.push_back(serverFd);
    
    int terminalFd = open("/dev/tty", O_RDONLY);
    if (terminalFd < 0) {
        std::cerr << "Error: failed to open terminal" << std::endl;
        return;
    }

    pollfd terminalPollFd;
    terminalPollFd.fd = terminalFd;
    terminalPollFd.events = POLLIN;
    fds.push_back(terminalPollFd);

    while (serverRunning) {
        int ret = poll(fds.data(), fds.size(), 100);
        if (ret < 0) {
            std::cerr << "Error: poll failed\n";
            break;
        } else if (ret == 0) {
            continue;
        }

        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == m_serverSocket) {
                    handleNewConnection(fds);
                } else if (fds[i].fd == terminalFd) {
                    char buffer[128];
                    ssize_t bytesRead = read(terminalFd, buffer, sizeof(buffer) - 1);
                    if (bytesRead > 0) {
                        buffer[bytesRead] = '\0';
                        if (std::string(buffer) == "shutdown\n") {
                            std::cout << "Shutdown command received. Stopping server...\n";
                            serverRunning = false;
                        }
                    }
                } else {
                    handleClientData(fds[i].fd, fds);
                }
            }
        }
    }

    close(terminalFd);
    std::cout << "Server shutdown complete.\n";
}

