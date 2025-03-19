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
    FD_SET(STDIN_FILENO, &readfds); // Добавляем стандартный ввод в список

    while (true) {
        // Используем poll для асинхронной обработки клиентов и select для обработки ввода с терминала
        int ret = poll(fds.data(), fds.size(), 100);
        if (ret < 0) {
            std::cerr << "Error: poll failed\n";
            break;
        } else if (ret == 0) {
            continue;
        }

        // Обрабатываем ввод с терминала
        fd_set tmpfds = readfds;
        int selectRet = select(STDIN_FILENO + 1, &tmpfds, nullptr, nullptr, nullptr);
        if (selectRet > 0 && FD_ISSET(STDIN_FILENO, &tmpfds)) {
            std::string input;
            std::getline(std::cin, input);
            if (input == "quit") { // Вводим "quit" для завершения работы сервера
                std::cout << "Shutting down the server...\n";
                break;
            }
        }

        // Обрабатываем события для клиентов
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

    // Очистка и завершение работы сервера
    std::cout << "Server shutdown complete.\n";
}
