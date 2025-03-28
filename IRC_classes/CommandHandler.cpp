#include "CommandHandler.hpp"
#include "Server.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <poll.h>
#include <cstring>
#include <stdio.h>
#include <sstream>
#include <cstdlib>
#include <unistd.h>

CommandHandler::CommandHandler(Server& s) : server(s) {}

/* The `checkClient` function verifies whether a client (identified by their socket) 
exists in the server's list of active clients. 
If the client does not exist, an error message is printed, 
the client is removed from the server, and the function returns `false`. 
If the client exists, a reference to the client object is assigned to the provided pointer, 
and the function returns `true`. */

bool CommandHandler::checkClient(int clientSocket, std::vector<pollfd>& fds, Client*& client) {
    std::map<int, Client>::iterator it = server.m_clients.find(clientSocket);
    if (it == server.m_clients.end()) {
        std::cerr << "Error: Unknown client\n";
        server.removeClient(clientSocket, fds);
        return false;
    }
    client = &it->second;
    return true;
}

/* The `handlePassword` function processes the `PASS` command sent by a client. 
It verifies the password provided in the command and takes appropriate actions:
1. If the password matches the server's configured password, the client is authenticated, 
   and the output buffer is updated for further communication.
2. If the password is incorrect, the client is notified, and the remaining attempts 
   are decremented. After exhausting all attempts, the client is disconnected.
3. If the input does not start with "PASS :", the function exits without processing further. */

void CommandHandler::handlePassword(int clientSocket, const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
    // Проверяем, начинается ли строка с "PASS :"
    if (input.rfind("PASS :", 0) != 0) {
        // Если это не команда PASS, просто выходим и ждём следующую команду
        /* не знаю надо ли с этим что- то делаьть*/
        return;
    }

    std::string password = input.substr(6);
    while (!password.empty() && (password[0] == ' ' || password[password.length() - 1] == ' ')) {
        if (password[0] == ' ') password.erase(0, 1);
        if (!password.empty() && password[password.length() - 1] == ' ') password.erase(password.length() - 1);
    }

    if (password == server.config.getPassword()) {
        client.setPasswordEntered(true);
        std::cout << "Client authenticated with password: " << password << "\n";
        
        std::ostringstream oss;
        oss << clientSocket;
        std::string clientName = client.getNickname().empty() ? "guest" + oss.str() : client.getNickname();
        std::string response = ":server 001 " + clientName + " :✅ Great, that's the correct password, champ!\r\n";
        std::cout << "Response to send: " << response << std::endl;
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
    } else {
        client.setPasswordAttempts(client.getPasswordAttempts() - 1);
        std::ostringstream ossClient;
        ossClient << clientSocket;
        std::string clientName = client.getNickname().empty() ? "guest" + ossClient.str() : client.getNickname();

        if (client.getPasswordAttempts() > 0) {
            std::ostringstream ossAttempts;
            ossAttempts << client.getPasswordAttempts();
            std::string response = ":server 464 " + clientName + " :Wrong password. Attempts left: " + ossAttempts.str() + "\r\n";
            client.appendOutputBuffer(response);
            fds[i].events |= POLLOUT;
        } else {
            std::string response = ":server 464 " + clientName + " :Too many wrong attempts. Disconnecting\r\n";
            client.appendOutputBuffer(response);
            fds[i].events |= POLLOUT;
            server.removeClient(clientSocket, fds);
        }
    }
}

/* This piece of code must remain untouched under any circumstances. */
void CommandHandler::handleQuit(int clientSocket, Client& client, std::vector<pollfd>& fds) {
    std::cout << "Client requested to quit.\n";

    // Уведомляем всех остальных клиентов
    std::string quitMessage = ":" + client.getNickname() + " QUIT :Quit\r\n";
    for (std::map<int, Client>::iterator it = server.m_clients.begin(); it != server.m_clients.end(); ++it) {
        if (it->first != clientSocket) { // Не отправляем уходящему клиенту
            it->second.appendOutputBuffer(quitMessage);
            for (size_t j = 0; j < fds.size(); ++j) {
                if (fds[j].fd == it->first) {
                    fds[j].events |= POLLOUT; // Устанавливаем POLLOUT для отправки
                    break;
                }
            }
        }
    }

    // Сразу разрываем связь с клиентом
    server.removeClient(clientSocket, fds); // Удаляем из m_clients и fds
}
/* End of message */

/* The `handleNick` function processes the `NICK` command sent by a client. 
It performs the following steps:
1. If the input length is less than or equal to 5, a response is sent indicating 
   that there are not enough parameters, and the function exits.
2. The provided nickname is extracted and trimmed of leading and trailing spaces.
3. If the nickname is empty after trimming, a response is sent stating that 
   no nickname was given, and the function exits.
4. The function checks if the nickname is already in use by another client.
   - If it is in use, a response is sent indicating that the nickname is unavailable.
   - Otherwise, the client's nickname is updated, and a confirmation message is logged.
5. Updates the output buffer to include the appropriate response for further communication. */

void CommandHandler::handleNick(int clientSocket, const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
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
            std::string oldNick = client.getNickname();
            client.setNickname(nickname);
            std::cout << "Client set nickname: " + nickname + "\n";
            std::string response = ":server 001 " + nickname + " :Good NickName ✨\r\n";
            client.appendOutputBuffer(response);
            if (!oldNick.empty()) {
                std::string response = ":" + oldNick + "'s "/*[aka  + client.getUsername() + ] */"new NICK is " + nickname + "\r\n";
                client.appendOutputBuffer(response);
            }
            fds[i].events |= POLLOUT;
        }
    }
}

/* The `handleUser` function processes the `USER` command sent by a client.
It performs the following steps:
1. If the input length is less than or equal to 5, a response is sent to the client indicating 
   that there are not enough parameters, and the function exits.
2. Extracts the username and real name from the input:
   - If the input format is incorrect (e.g., missing spaces or colon), a syntax error response is sent.
   - Trims any leading spaces from the real name.
3. Sets the client's username using the extracted value.
4. Based on the client's current status:
   - If the password is already entered and the nickname is set, a welcome message is sent.
   - If the nickname is missing, a response is sent instructing the client to set a nickname first.
5. Updates the output buffer with the appropriate response for further communication. */

void CommandHandler::handleUser(const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
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
    if (client.isPasswordEntered() && !client.getNickname().empty()) {
        std::string response = ":server 001 " + client.getNickname() + " :🦋 Welcome to the IRC server🦋 " + client.getNickname() + "!\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
    } else if (client.getNickname().empty()) {
        std::string response = ":server 451 * :Please set a nickname with NICK command first\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
    }
}

/* The `handleJoin` function processes the `JOIN` command sent by a client.
It performs the following steps:
1. Checks if the input length is sufficient:
   - If not, a response is sent indicating that parameters are missing, and the function exits.
2. Extracts and trims the channel name:
   - Removes leading and trailing spaces from the channel name.
3. Validates the channel name:
   - If the channel name is empty or does not start with a '#', a response is sent indicating 
     that the channel does not exist, and the function exits.
4. Checks if the channel already exists:
   - If it exists, adds the client to the channel's member list.
   - If it does not exist, creates a new channel, adds the client as the first member, 
     and registers the channel in the server's channel list.
5. Sends a response to the client confirming the join and logs the action.
6. Broadcasts the join message to other members of the channel.
7. Updates the output buffer to ensure the appropriate responses are sent to the client. */

void CommandHandler::handleJoin(int clientSocket, const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
    if (input.length() <= 5) {
        std::string response = ":server 461 " + client.getNickname() + " JOIN :Not enough parameters\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string params = input.substr(5);
    size_t spacePos = params.find(' ');
    std::string channelName = spacePos == std::string::npos ? params : params.substr(0, spacePos);
    std::string key = spacePos != std::string::npos ? params.substr(spacePos + 1) : "";
    while (!channelName.empty() && (channelName[0] == ' ' || channelName[channelName.length() - 1] == ' '))
        channelName.erase(channelName[0] == ' ' ? 0 : channelName.length() - 1, 1);
    while (!key.empty() && key[0] == ' ') key.erase(0, 1);
    if (channelName.empty() || channelName[0] != '#') {
        std::string response = ":server 403 " + client.getNickname() + " " + channelName + " :No such channel\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
    } else {
        bool channelExists = false;
        for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
            if (it->getName() == channelName) {
                if (it->isInviteOnly() && !it->isOperator(clientSocket) && !it->isInvited(clientSocket)) {
                    std::string response = ":server 473 " + client.getNickname() + " " + channelName + " :Cannot join channel (+i)\r\n";
                    client.appendOutputBuffer(response);
                    fds[i].events |= POLLOUT;
                    return;
                }
                if (!it->getKey().empty() && key != it->getKey()) {
                    std::string response = ":server 475 " + client.getNickname() + " " + channelName + " :Cannot join channel (+k)\r\n";
                    client.appendOutputBuffer(response);
                    fds[i].events |= POLLOUT;
                    return;
                }
                if (it->getUserLimit() > 0 && static_cast<int>(it->getMembers().size()) >= it->getUserLimit()) {
                    std::string response = ":server 471 " + client.getNickname() + " " + channelName + " :Cannot join channel (+l)\r\n";
                    client.appendOutputBuffer(response);
                    fds[i].events |= POLLOUT;
                    return;
                }
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
        std::string response = ":" + client.getNickname() + /* [aka  + client.getUsername() + ]*/" JOIN " + channelName + "\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        broadcastMessage(clientSocket, "JOIN " + channelName, fds);
    }
}

/* The `handlePrivmsg` function processes the `PRIVMSG` command sent by a client.
It performs the following actions:
1. Checks if the input length is sufficient:
   - If not, a response is sent indicating that the parameters are missing, and the function exits.
2. Extracts the target (recipient) and the message:
   - The target and message are separated by a space and a colon, respectively.
   - Leading and trailing spaces in the target are removed.
3. Validates the input format:
   - If the format is correct, the message is broadcast to the target.
   - A confirmation response is added to the client's output buffer.
   - Logs the private message for debugging purposes.
4. Handles incorrect input:
   - If the recipient or message is missing, a response is sent indicating the error.
5. Updates the output buffer to ensure the appropriate response is sent back to the client. */

void CommandHandler::handlePrivmsg(int clientSocket, const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
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
        broadcastMessage(clientSocket, "PRIVMSG " + target + " :" + message, fds);
        std::string response = ":server 001 " + client.getNickname() + " :Message sent\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
    } else {
        std::string response = ":server 401 " + client.getNickname() + " :No recipient or message\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
    }
}

void CommandHandler::handleKick(int clientSocket, const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
    if (input.length() <= 5) {
        std::string response = ":server 461 " + client.getNickname() + " KICK :Not enough parameters\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string params = input.substr(5);
    size_t spacePos = params.find(' ');
    if (spacePos == std::string::npos) {
        std::string response = ":server 461 " + client.getNickname() + " KICK :Not enough parameters\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string channelName = params.substr(0, spacePos);
    std::string rest = params.substr(spacePos + 1);
    size_t nextSpace = rest.find(' ');
    std::string targetNick = nextSpace == std::string::npos ? rest : rest.substr(0, nextSpace);
    std::string reason = nextSpace != std::string::npos && rest.find(':') != std::string::npos ? rest.substr(rest.find(':') + 1) : "Kicked by operator";
    while (!targetNick.empty() && targetNick[0] == ' ') targetNick.erase(0, 1);
    while (!channelName.empty() && (channelName[0] == ' ' || channelName[channelName.length() - 1] == ' '))
        channelName.erase(channelName[0] == ' ' ? 0 : channelName.length() - 1, 1);

    for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
        if (it->getName() == channelName) {
            if (!it->isOperator(clientSocket)) {
                std::string response = ":server 482 " + client.getNickname() + " " + channelName + " :You're not channel operator\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                return;
            }
            int targetSocket = -1;
            for (std::map<int, Client>::iterator clientIt = server.m_clients.begin(); clientIt != server.m_clients.end(); ++clientIt) {
                if (clientIt->second.getNickname() == targetNick) {
                    targetSocket = clientIt->first;
                    break;
                }
            }
            if (targetSocket == -1 || it->getMembers().find(targetSocket) == it->getMembers().end()) {
                std::string response = ":server 441 " + client.getNickname() + " " + targetNick + " " + channelName + " :They aren't on that channel\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                return;
            }
            it->removeMember(targetSocket);
            broadcastMessage(clientSocket, "KICK " + channelName + " " + targetNick + " :" + reason, fds);
            fds[i].events |= POLLOUT;
            return;
        }
    }
    std::string response = ":server 403 " + client.getNickname() + " " + channelName + " :No such channel\r\n";
    client.appendOutputBuffer(response);
    fds[i].events |= POLLOUT;
}

void CommandHandler::handleInvite(int clientSocket, const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
    if (input.length() <= 7) {
        std::string response = ":server 461 " + client.getNickname() + " INVITE :Not enough parameters\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string params = input.substr(7);
    size_t spacePos = params.find(' ');
    if (spacePos == std::string::npos) {
        std::string response = ":server 461 " + client.getNickname() + " INVITE :Not enough parameters\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string targetNick = params.substr(0, spacePos);
    std::string channelName = params.substr(spacePos + 1);
    while (!targetNick.empty() && targetNick[0] == ' ') targetNick.erase(0, 1);
    while (!channelName.empty() && (channelName[0] == ' ' || channelName[channelName.length() - 1] == ' '))
        channelName.erase(channelName[0] == ' ' ? 0 : channelName.length() - 1, 1);

    for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
        if (it->getName() == channelName) {
            if (!it->isOperator(clientSocket)) {
                std::string response = ":server 482 " + client.getNickname() + " " + channelName + " :You're not channel operator\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                return;
            }
            int targetSocket = -1;
            for (std::map<int, Client>::iterator clientIt = server.m_clients.begin(); clientIt != server.m_clients.end(); ++clientIt) {
                if (clientIt->second.getNickname() == targetNick) {
                    targetSocket = clientIt->first;
                    break;
                }
            }
            if (targetSocket == -1) {
                std::string response = ":server 401 " + client.getNickname() + " " + targetNick + " :No such nick\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                return;
            }
            it->invite(targetSocket);
            std::map<int, Client>::iterator targetIt = server.m_clients.find(targetSocket);
            if (targetIt != server.m_clients.end()) {
                std::string response = ":" + client.getNickname() + /*[aka  + client.getUsername() + ]*/" INVITE " + targetNick + " :" + channelName + "\r\n";
                targetIt->second.appendOutputBuffer(response);
                client.appendOutputBuffer(":server 341 " + client.getNickname() + " " + targetNick + " " + channelName + "\r\n");
                fds[i].events |= POLLOUT;
                for (size_t j = 0; j < fds.size(); ++j) {
                    if (fds[j].fd == targetSocket) {
                        fds[j].events |= POLLOUT;
                        break;
                    }
                }
            }
            return;
        }
    }
    std::string response = ":server 403 " + client.getNickname() + " " + channelName + " :No such channel\r\n";
    client.appendOutputBuffer(response);
    fds[i].events |= POLLOUT;
}

void CommandHandler::handleTopic(int clientSocket, const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
    if (input.length() <= 6) {
        std::string response = ":server 461 " + client.getNickname() + " TOPIC :Not enough parameters\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string params = input.substr(6);
    size_t colonPos = params.find(':');
    std::string channelName = colonPos == std::string::npos ? params : params.substr(0, colonPos);
    while (!channelName.empty() && (channelName[0] == ' ' || channelName[channelName.length() - 1] == ' '))
        channelName.erase(channelName[0] == ' ' ? 0 : channelName.length() - 1, 1);

    for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
        if (it->getName() == channelName) {
            if (colonPos == std::string::npos) {
                std::string topic = it->getTopic();
                std::string response = topic.empty() ?
                    ":server 331 " + client.getNickname() + " " + channelName + " :No topic is set\r\n" :
                    ":server 332 " + client.getNickname() + " " + channelName + " :" + topic + "\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
            } else {
                if (it->isTopicRestricted() && !it->isOperator(clientSocket)) {
                    std::string response = ":server 482 " + client.getNickname() + " " + channelName + " :You're not channel operator\r\n";
                    client.appendOutputBuffer(response);
                    fds[i].events |= POLLOUT;
                    return;
                }
                std::string newTopic = params.substr(colonPos + 1);
                it->setTopic(newTopic);
                broadcastMessage(clientSocket, "TOPIC " + channelName + " :" + newTopic, fds);
                fds[i].events |= POLLOUT;
            }
            return;
        }
    }
    std::string response = ":server 403 " + client.getNickname() + " " + channelName + " :No such channel\r\n";
    client.appendOutputBuffer(response);
    fds[i].events |= POLLOUT;
}

void CommandHandler::handleMode(int clientSocket, const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
    if (input.length() <= 5) {
        std::string response = ":server 461 " + client.getNickname() + " MODE :Not enough parameters\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string params = input.substr(5);
    size_t spacePos = params.find(' ');
    if (spacePos == std::string::npos) {
        std::string response = ":server 461 " + client.getNickname() + " MODE :Not enough parameters\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string channelName = params.substr(0, spacePos);
    std::string modeStr = params.substr(spacePos + 1);
    while (!channelName.empty() && (channelName[0] == ' ' || channelName[channelName.length() - 1] == ' '))
        channelName.erase(channelName[0] == ' ' ? 0 : channelName.length() - 1, 1);
    while (!modeStr.empty() && modeStr[0] == ' ') modeStr.erase(0, 1);

    for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
        if (it->getName() == channelName) {
            if (!it->isOperator(clientSocket)) {
                std::string response = ":server 482 " + client.getNickname() + " " + channelName + " :You're not channel operator\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                return;
            }
            bool addMode = (modeStr[0] == '+');
            char mode = modeStr[1];
            std::string arg = modeStr.length() > 2 ? modeStr.substr(3) : "";
            while (!arg.empty() && arg[0] == ' ') arg.erase(0, 1);

            int targetSocket = -1; // Перенесено за межі switch
            switch (mode) {
                case 'i':
                    it->setInviteOnly(addMode);
                    broadcastMessage(clientSocket, "MODE " + channelName + " " + (addMode ? "+i" : "-i"), fds);
                    break;
                case 't':
                    it->setTopicRestricted(addMode);
                    broadcastMessage(clientSocket, "MODE " + channelName + " " + (addMode ? "+t" : "-t"), fds);
                    break;
                case 'k':
                    if (addMode && arg.empty()) {
                        std::string response = ":server 461 " + client.getNickname() + " MODE :Not enough parameters\r\n";
                        client.appendOutputBuffer(response);
                        fds[i].events |= POLLOUT;
                        return;
                    }
                    it->setKey(addMode ? arg : "");
                    broadcastMessage(clientSocket, "MODE " + channelName + " " + (addMode ? "+k " + arg : "-k"), fds);
                    break;
                case 'o':
                    if (arg.empty()) {
                        std::string response = ":server 461 " + client.getNickname() + " MODE :Not enough parameters\r\n";
                        client.appendOutputBuffer(response);
                        fds[i].events |= POLLOUT;
                        return;
                    }
                    for (std::map<int, Client>::iterator clientIt = server.m_clients.begin(); clientIt != server.m_clients.end(); ++clientIt) {
                        if (clientIt->second.getNickname() == arg) {
                            targetSocket = clientIt->first;
                            break;
                        }
                    }
                    if (targetSocket != -1 && it->getMembers().find(targetSocket) != it->getMembers().end()) {
                        it->setOperator(targetSocket, addMode);
                        broadcastMessage(clientSocket, "MODE " + channelName + " " + (addMode ? "+o " : "-o ") + arg, fds);
                    } else {
                        std::string response = ":server 441 " + client.getNickname() + " " + arg + " " + channelName + " :They aren't on that channel\r\n";
                        client.appendOutputBuffer(response);
                        fds[i].events |= POLLOUT;
                        return;
                    }
                    break;
                case 'l':
                    if (addMode && arg.empty()) {
                        std::string response = ":server 461 " + client.getNickname() + " MODE :Not enough parameters\r\n";
                        client.appendOutputBuffer(response);
                        fds[i].events |= POLLOUT;
                        return;
                    }
                    it->setUserLimit(addMode ? atoi(arg.c_str()) : 0);
                    broadcastMessage(clientSocket, "MODE " + channelName + " " + (addMode ? "+l " + arg : "-l"), fds);
                    break;
                default:
                    std::string response = ":server 472 " + client.getNickname() + " " + mode + " :is unknown mode char to me\r\n";
                    client.appendOutputBuffer(response);
                    fds[i].events |= POLLOUT;
                    return;
            }
            fds[i].events |= POLLOUT;
            return;
        }
    }
    std::string response = ":server 403 " + client.getNickname() + " " + channelName + " :No such channel\r\n";
    client.appendOutputBuffer(response);
    fds[i].events |= POLLOUT;
}

void CommandHandler::handleUnknownCommand(const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
    std::string command = input.substr(0, input.find(' '));
    std::string response = ":server 421 " + client.getNickname() + " " + input + " :Unknown command\r\n";
    client.appendOutputBuffer(response);
    fds[i].events |= POLLOUT;
}

void CommandHandler::processCommand(int clientSocket, const std::string& input, std::vector<pollfd>& fds, size_t i) {
    Client* client;
    if (!checkClient(clientSocket, fds, client)) return;
    if (!client->isPasswordEntered()) {
        handlePassword(clientSocket, input, *client, fds, i);
    } else {
        if (input.rfind("CAP LS", 0) == 0) {
            std::string response = ":server CAP * LS :\r\n";
            client->appendOutputBuffer(response);
            fds[i].events |= POLLOUT;
            return;
        }
        if (strncmp(input.c_str(), "QUIT", 4) == 0) {
            handleQuit(clientSocket, *client, fds);
        } else if (input.rfind("NICK", 0) == 0) {
            handleNick(clientSocket, input, *client, fds, i);
        } else if (input.rfind("USER", 0) == 0) {
            handleUser(input, *client, fds, i);
        } else if (input.rfind("JOIN", 0) == 0) {
            handleJoin(clientSocket, input, *client, fds, i);
        } else if (input.rfind("PRIVMSG", 0) == 0) {
            handlePrivmsg(clientSocket, input, *client, fds, i);
        } else if (input.rfind("KICK", 0) == 0) {
            handleKick(clientSocket, input, *client, fds, i);
        } else if (input.rfind("INVITE", 0) == 0) {
            handleInvite(clientSocket, input, *client, fds, i);
        } else if (input.rfind("TOPIC", 0) == 0) {
            handleTopic(clientSocket, input, *client, fds, i);
        } else if (input.rfind("MODE", 0) == 0) {
            handleMode(clientSocket, input, *client, fds, i);
        } else {
            handleUnknownCommand(input, *client, fds, i);
        }
    }
}

void CommandHandler::broadcastMessage(int senderSocket, const std::string& message, std::vector<pollfd>& fds) {
    size_t firstSpace = message.find(' ');
    if (firstSpace == std::string::npos) return;
    std::string command = message.substr(0, firstSpace);
    size_t secondSpace = message.find(' ', firstSpace + 1);
    std::string target = (command == "JOIN" || command == "TOPIC") ? 
        message.substr(firstSpace + 1, secondSpace == std::string::npos ? std::string::npos : secondSpace - firstSpace - 1) : 
        message.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    std::string text = (command == "PRIVMSG" || command == "TOPIC" || command == "KICK") ? 
        message.substr(message.find(':') + 1) : "";
    while (!target.empty() && (target[0] == ' ' || target[target.length() - 1] == ' '))
        target.erase(target[0] == ' ' ? 0 : target.length() - 1, 1);

    std::map<int, Client>::iterator senderIt = server.m_clients.find(senderSocket);
    if (senderIt == server.m_clients.end()) return;
    std::string senderNick = senderIt->second.getNickname();
    std::string senderUser = senderIt->second.getUsername();

    if (command == "PRIVMSG") {
        if (target[0] == '#') {
            for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
                if (it->getName() == target) {
                    std::map<int, bool> members = it->getMembers();
                    for (std::map<int, bool>::iterator memberIt = members.begin(); memberIt != members.end(); ++memberIt) {
                        if (memberIt->first != senderSocket) {
                            std::map<int, Client>::iterator memberClientIt = server.m_clients.find(memberIt->first);
                            if (memberClientIt != server.m_clients.end()) {
                                /* We use a quasi-standard IRC format */
                                std::string response = ":" + senderNick + /*[aka  + senderUser + ] */" PRIVMSG " + target + " :" + text + "\r\n";
                                memberClientIt->second.appendOutputBuffer(response);
                                for (size_t j = 0; j < fds.size(); ++j) {
                                    if (fds[j].fd == memberIt->first) {
                                        fds[j].events |= POLLOUT;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    return;
                }
            }
            std::string response = ":server 403 " + senderNick + " " + target + " :No such channel\r\n";
            senderIt->second.appendOutputBuffer(response);
            for (size_t j = 0; j < fds.size(); ++j) {
                if (fds[j].fd == senderSocket) {
                    fds[j].events |= POLLOUT;
                    break;
                }
            }
        } else {
            // Личное сообщение
            for (std::map<int, Client>::iterator it = server.m_clients.begin(); it != server.m_clients.end(); ++it) {
                if (it->second.getNickname() == target && it->first != senderSocket) {
                    std::string response = ":" + senderNick + /*[aka  + senderUser + ] */ "PRIVMSG " + target + " :" + text + "\r\n";
                    it->second.appendOutputBuffer(response);
                    for (size_t j = 0; j < fds.size(); ++j) {
                        if (fds[j].fd == it->first) {
                            fds[j].events |= POLLOUT;
                            break;
                        }
                    }
                    return;
                }
            }
            std::string response = ":server 401 " + senderNick + " " + target + " :No such nick/channel\r\n";
            senderIt->second.appendOutputBuffer(response);
            for (size_t j = 0; j < fds.size(); ++j) {
                if (fds[j].fd == senderSocket) {
                    fds[j].events |= POLLOUT;
                    break;
                }
            }
        }
    } else if (command == "JOIN" || command == "KICK" || command == "TOPIC" || command == "MODE") {
        for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
            if (it->getName() == target) {
                std::map<int, bool> members = it->getMembers();
                for (std::map<int, bool>::iterator memberIt = members.begin(); memberIt != members.end(); ++memberIt) {
                    if (memberIt->first != senderSocket || command != "JOIN") {
                        std::map<int, Client>::iterator memberClientIt = server.m_clients.find(memberIt->first);
                        if (memberClientIt != server.m_clients.end()) {
                            std::string response;
                            if (command == "JOIN") {
                                response = ":" + senderNick + " JOIN " + target + "\r\n";
                                memberClientIt->second.appendOutputBuffer(response);
                                // Список пользователей в канале
                                std::string names = ":localhost 353 " + senderNick + " = " + target + " :";
                                for (std::map<int, bool>::iterator m = members.begin(); m != members.end(); ++m) {
                                    std::map<int, Client>::iterator c = server.m_clients.find(m->first);
                                    if (c != server.m_clients.end()) names += c->second.getNickname() + " ";
                                }
                                names += "\r\n";
                                memberClientIt->second.appendOutputBuffer(names);
                                // Конец списка
                                std::string end = ":localhost 366 " + senderNick + " " + target + " :End of /NAMES list\r\n";
                                memberClientIt->second.appendOutputBuffer(end);
                            }
                            // if (command == "JOIN")
                            //     response = ":" + senderNick + "/* [aka " + senderUser + "]*/ JOIN " + target + "\r\n";
                            else if (command == "KICK")
                                response = ":" + senderNick + /* [aka  + senderUser + ]*/" KICK " + target + " " + message.substr(secondSpace + 1) + "\r\n";
                            else if (command == "TOPIC")
                                response = ":" + senderNick + /* [aka  + senderUser + ]*/" TOPIC " + target + " :" + text + "\r\n";
                            else
                                response = ":" + senderNick + /* [aka  + senderUser + ]*/" MODE " + target + " " + message.substr(firstSpace + 1) + "\r\n";
                            memberClientIt->second.appendOutputBuffer(response);
                            for (size_t j = 0; j < fds.size(); ++j) {
                                if (fds[j].fd == memberIt->first) {
                                    fds[j].events |= POLLOUT;
                                    break;
                                }
                            }
                        }
                    }
                }
                break;
            }
        }
    }
}