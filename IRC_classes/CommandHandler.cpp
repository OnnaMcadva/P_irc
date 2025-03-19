#include "CommandHandler.hpp"
#include "Server.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <poll.h>
#include <cstring>
#include <stdio.h>

CommandHandler::CommandHandler(Server& s) : server(s) {}

void CommandHandler::processCommand(int clientSocket, const std::string& input, std::vector<pollfd>& fds, size_t i) {
    std::map<int, Client>::iterator it = server.m_clients.find(clientSocket);
    if (it == server.m_clients.end()) {
        std::cerr << "Error: Unknown client\n";
        server.removeClient(clientSocket, fds);
        return;
    }
    Client& client = it->second;

    if (!client.isPasswordEntered()) {
        std::string password = input;
        if (input.rfind("PASS ", 0) == 0) {
            password = input.substr(5);
        }
        while (!password.empty() && (password[0] == ' ' || password[password.length() - 1] == ' ')) {
            if (password[0] == ' ') password.erase(0, 1);
            if (!password.empty() && password[password.length() - 1] == ' ') password.erase(password.length() - 1);
        }

        if (password == server.config.getPassword()) {
            client.setPasswordEntered(true);
            std::cout << "Client authenticated with password: " << password << "\n";
            char buffer[16];
            sprintf(buffer, "%d", clientSocket);
            std::string clientName = client.getNickname().empty() ? "guest" + std::string(buffer) : client.getNickname();
            std::string response = ":server 001 " + clientName + " :Welcome to the IRC server\r\n";
            std::cout << "Response to send: " << response << std::endl;
            client.appendOutputBuffer(response);
            fds[i].events |= POLLOUT;
        } else {
            client.setPasswordAttempts(client.getPasswordAttempts() - 1);
            if (client.getPasswordAttempts() > 0) {
                char socketBuffer[16];
                sprintf(socketBuffer, "%d", clientSocket);
                std::string clientName = client.getNickname().empty() ? "guest" + std::string(socketBuffer) : client.getNickname();
                char attemptsBuffer[16];
                sprintf(attemptsBuffer, "%d", client.getPasswordAttempts());
                std::string response = ":server 464 " + clientName + " :Wrong password. Attempts left: " + std::string(attemptsBuffer) + "\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
            } else {
                char socketBuffer[16];
                sprintf(socketBuffer, "%d", clientSocket);
                std::string clientName = client.getNickname().empty() ? "guest" + std::string(socketBuffer) : client.getNickname();
                std::string response = ":server 464 " + clientName + " :Too many wrong attempts. Disconnecting\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                server.removeClient(clientSocket, fds);
                return;
            }
        }
    } else {
        if (input == server.config.getPassword()) {
            std::cout << "Ignoring repeated password input: " << input << "\n";
            return;
        }

        if (strncmp(input.c_str(), "QUIT", 4) == 0) {
            std::cout << "Client requested to quit.\n";
            std::string goodbyeMessage = ":server 221 " + client.getNickname() + " :Goodbye\r\n";
            client.appendOutputBuffer(goodbyeMessage);
            fds[i].events |= POLLOUT;
            server.removeClient(clientSocket, fds);
            return;
        } else if (input.rfind("NICK", 0) == 0) {
            if (input.length() <= 5) {
                std::string response = ":server 461 " + client.getNickname() + " NICK :Not enough parameters\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                return;
            }
            std::string nickname = input.substr(5);
            while (!nickname.empty() && (nickname[0] == ' ' || nickname[nickname.length() - 1] == ' ')) {
                if (nickname[0] == ' ') nickname.erase(0, 1);
                if (!nickname.empty() && nickname[nickname.length() - 1] == ' ') nickname.erase(nickname.length() - 1);
            }
            if (nickname.empty()) {
                std::string response = ":server 431 " + client.getNickname() + " :No nickname given\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
            } else {
                bool nickInUse = false;
                for (std::map<int, Client>::iterator it = server.m_clients.begin(); it != server.m_clients.end(); ++it) {
                    if (it->second.getNickname() == nickname && it->first != clientSocket) {
                        nickInUse = true;
                        break;
                    }
                }
                if (nickInUse) {
                    std::string response = ":server 433 * " + nickname + " :Nickname is already in use\r\n";
                    client.appendOutputBuffer(response);
                    fds[i].events |= POLLOUT;
                } else {
                    client.setNickname(nickname);
                    std::cout << "Client set nickname: " + nickname + "\n";
                    std::string response = ":server 001 " + nickname + " :Nickname set\r\n";
                    client.appendOutputBuffer(response);
                    fds[i].events |= POLLOUT;
                }
            }
        } else if (input.rfind("USER", 0) == 0) {
            if (input.length() <= 5) {
                std::string response = ":server 461 " + client.getNickname() + " USER :Not enough parameters\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                return;
            }
            std::string userPart = input.substr(5);
            size_t firstSpace = userPart.find(' ');
            if (firstSpace == std::string::npos) {
                std::string response = ":server 461 " + client.getNickname() + " USER :Syntax error\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                return;
            }
            std::string username = userPart.substr(0, firstSpace);
            size_t colonPos = userPart.find(':');
            if (colonPos == std::string::npos) {
                std::string response = ":server 461 " + client.getNickname() + " USER :Syntax error\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                return;
            }
            std::string realname = userPart.substr(colonPos + 1);
            while (!realname.empty() && realname[0] == ' ') {
                realname.erase(0, 1);
            }
            client.setUsername(username);
            std::cout << "Client set username: " << username << "\n";
            if (!client.getNickname().empty()) {
                std::string response = ":server 001 " + client.getNickname() + " :Welcome to the IRC server " + client.getNickname() + "!\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
            } else {
                std::string response = ":server 451 * :Please set a nickname with NICK command first\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
            }
        } else if (input.rfind("JOIN", 0) == 0) {
            if (input.length() <= 5) {
                std::string response = ":server 461 " + client.getNickname() + " JOIN :Not enough parameters\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                return;
            }
            std::string channelName = input.substr(5);
            while (!channelName.empty() && (channelName[0] == ' ' || channelName[channelName.length() - 1] == ' ')) {
                if (channelName[0] == ' ') channelName.erase(0, 1);
                if (!channelName.empty() && channelName[channelName.length() - 1] == ' ') channelName.erase(channelName.length() - 1);
            }
            if (channelName.empty() || channelName[0] != '#') {
                std::string response = ":server 403 " + client.getNickname() + " " + channelName + " :No such channel\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
            } else {
                bool channelExists = false;
                for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
                    if (it->getName() == channelName) {
                        it->join(clientSocket);
                        channelExists = true;
                        break;
                    }
                }
                if (!channelExists) {
                    Channel newChannel(channelName);
                    newChannel.join(clientSocket);
                    server.channels.push_back(newChannel);
                }
                std::cout << "Client joined channel: " << channelName << "\n";
                std::string response = ":" + client.getNickname() + " JOIN " + channelName + "\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                broadcastMessage(clientSocket, "JOIN " + channelName);
            }
        } else if (input.rfind("PRIVMSG", 0) == 0) {
            if (input.length() <= 5) {
                std::string response = ":server 461 " + client.getNickname() + " PRIVMSG :Not enough parameters\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                return;
            }
            size_t spacePos = input.find(' ');
            size_t colonPos = input.find(':');
            if (spacePos != std::string::npos && colonPos != std::string::npos && colonPos > spacePos) {
                std::string target = input.substr(spacePos + 1, colonPos - spacePos - 1);
                while (!target.empty() && (target[0] == ' ' || target[target.length() - 1] == ' ')) {
                    if (target[0] == ' ') target.erase(0, 1);
                    if (!target.empty() && target[target.length() - 1] == ' ') target.erase(target.length() - 1);
                }
                std::string message = input.substr(colonPos + 1);
                std::cout << "Received private message to " << target << ": " << message << "\n";
                broadcastMessage(clientSocket, "PRIVMSG " + target + " :" + message);
                std::string response = ":server 001 " + client.getNickname() + " :Message sent\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
            } else {
                std::string response = ":server 401 " + client.getNickname() + " :No recipient or message\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
            }
        } else {
            std::string response = ":server 421 " + client.getNickname() + " " + input + " :Unknown command\r\n";
            client.appendOutputBuffer(response);
            fds[i].events |= POLLOUT;
        }
    }
}

/* Функция `broadcastMessage` обрабатывает команды `PRIVMSG` и `JOIN`,
отправляя сообщения всем участникам указанного канала, кроме отправителя.  

1. **Разбирает входное сообщение:**  
   - Извлекает команду (`PRIVMSG` или `JOIN`).  
   - Определяет целевой канал (`target`).  
   - Если это `PRIVMSG`, извлекает текст сообщения.  

2. **Находит канал в списке `server.channels`**  
   - Если канал найден, получает список его участников.  

3. **Отправляет сообщение всем участникам, кроме отправителя:**  
   - Для `PRIVMSG`: пересылает текст от имени отправителя.  
   - Для `JOIN`: уведомляет о присоединении нового пользователя.  

Фактически, функция реализует рассылку сообщений в рамках IRC-канала. */

void CommandHandler::broadcastMessage(int senderSocket, const std::string& message) {
    size_t firstSpace = message.find(' ');
    if (firstSpace == std::string::npos) return;
    std::string command = message.substr(0, firstSpace);
    size_t secondSpace = message.find(' ', firstSpace + 1);
    if (secondSpace == std::string::npos && command != "JOIN") return;
    std::string target = (command == "JOIN") ? message.substr(firstSpace + 1) : message.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    std::string text;
    if (command == "PRIVMSG") {
        size_t colonPos = message.find(':');
        if (colonPos == std::string::npos) return;
        text = message.substr(colonPos + 1);
    }

    for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
        if (it->getName() == target) {
            std::vector<int> members = it->getMembers();
            for (std::vector<int>::iterator memberIt = members.begin(); memberIt != members.end(); ++memberIt) {
                if (*memberIt != senderSocket) {
                    std::string senderNick = server.m_clients[senderSocket].getNickname();
                    std::string response;
                    if (command == "PRIVMSG") {
                        response = ":" + senderNick + " PRIVMSG " + target + " :" + text + "\r\n";
                    } else if (command == "JOIN") {
                        response = ":" + senderNick + " JOIN " + target + "\r\n";
                    }
                    server.m_clients[*memberIt].appendOutputBuffer(response);
                }
            }
            break;
        }
    }
}
