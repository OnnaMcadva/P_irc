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
        std::string response = ":server@localhost 001 " + clientName + " :✅ Great, that's the correct password, champ!\r\n";
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
            std::string response = ":server@localhost 464 " + clientName + " :Wrong password. Attempts left: " + ossAttempts.str() + "\r\n";
            client.appendOutputBuffer(response);
            fds[i].events |= POLLOUT;
        } else {
            std::string response = ":server@localhost 464 " + clientName + " :Too many wrong attempts. Disconnecting\r\n";
            client.appendOutputBuffer(response);
            fds[i].events |= POLLOUT;
            server.removeClient(clientSocket, fds);
        }
    }
}

/* This piece of code must remain untouched under any circumstances. */
void CommandHandler::handleQuit(int clientSocket, Client& client, std::vector<pollfd>& fds) {
    std::cout << "Client requested to quit.\n";
    (void)client;

    // std::string quitMessage = ":" + client.getNickname() + " QUIT :Quit\r\n";
    // for (std::map<int, Client>::iterator it = server.m_clients.begin(); it != server.m_clients.end(); ++it) {
    //     if (it->first != clientSocket) {
    //         it->second.appendOutputBuffer(quitMessage);
    //         for (size_t j = 0; j < fds.size(); ++j) {
    //             if (fds[j].fd == it->first) {
    //                 fds[j].events |= POLLOUT;
    //                 break;
    //             }
    //         }
    //     }
    // }
    server.removeClient(clientSocket, fds);
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
        std::string response = ":server@localhost 461 " + client.getNickname() + " NICK :Not enough parameters\r\n";
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
        std::string response = ":server@localhost 431 " + client.getNickname() + " :No nickname given\r\n";
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
            std::string response = ":server@localhost 433 * " + nickname + " :Nickname is already in use\r\n";
            client.appendOutputBuffer(response);
            fds[i].events |= POLLOUT;
        } else {
            std::string oldNick = client.getNickname();
            client.setNickname(nickname);
            std::cout << "Client set nickname: " + nickname + "\n";
            if (oldNick.empty()) {
                std::string response = ":server@localhost 001 " + nickname + " :Good NickName ✨\r\n";
                client.appendOutputBuffer(response);
            } else {
                std::string response = ":" + oldNick + "!" + client.getUsername() + "@localhost NICK " + nickname + "\r\n";
                broadcastMessage(clientSocket, "NICK " + nickname, fds);
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
        std::string response = ":server@localhost 461 " + client.getNickname() + " USER :Not enough parameters\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string userPart = input.substr(5);
    size_t firstSpace = userPart.find(' ');
    if (firstSpace == std::string::npos) {
        std::string response = ":server@localhost 461 " + client.getNickname() + " USER :Syntax error\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string username = userPart.substr(0, firstSpace);
    size_t colonPos = userPart.find(':');
    if (colonPos == std::string::npos) {
        std::string response = ":server@localhost 461 " + client.getNickname() + " USER :Syntax error\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string realname = userPart.substr(colonPos + 1);
    while (!realname.empty() && realname[0] == ' ') {
        realname.erase(0, 1);
    }

    client.setUsername(username);
    client.setRealname(realname);
    std::cout << "Client set username: " << username << "\n";
    if (client.isPasswordEntered() && !client.getNickname().empty()) {
        std::string response = ":server@localhost 001 " + client.getNickname() + " :🦋 Welcome to the IRC server🦋 " + client.getNickname() + "!" + client.getUsername() + "@localhost\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
    } else if (client.getNickname().empty()) {
        std::string response = ":server@localhost 451 * :Please set a nickname with NICK command first\r\n";
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
        client.appendOutputBuffer(":server 461 " + client.getNickname() + " JOIN :Not enough parameters\r\n");
        fds[i].events |= POLLOUT;
        return;
    }

    std::string params = input.substr(5);
    size_t spacePos = params.find(' ');
    std::string channelName = spacePos == std::string::npos ? params : params.substr(0, spacePos);
    std::string key = spacePos != std::string::npos ? params.substr(spacePos + 1) : "";

    while (!channelName.empty() && channelName[0] == ' ') channelName.erase(0, 1);
    while (!channelName.empty() && channelName[channelName.length() - 1] == ' ') channelName.erase(channelName.length() - 1);
    while (!key.empty() && key[0] == ' ') key.erase(0, 1);

    if (channelName.empty() || channelName[0] != '#') {
        client.appendOutputBuffer(":server 403 " + client.getNickname() + " " + channelName + " :No such channel\r\n");
        fds[i].events |= POLLOUT;
        return;
    }

    // bool channelExists = false;
    for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
        if (it->getName() == channelName) {
            if (it->isInviteOnly() && !it->isOperator(clientSocket) && !it->isInvited(clientSocket)) {
                client.appendOutputBuffer(":server 473 " + client.getNickname() + " " + channelName + " :Cannot join channel (+i)\r\n");
            } else if (!it->getKey().empty() && key != it->getKey()) {
                client.appendOutputBuffer(":server 475 " + client.getNickname() + " " + channelName + " :Cannot join channel (+k)\r\n");
            } else if (it->getUserLimit() > 0 && static_cast<int>(it->getMembers().size()) >= it->getUserLimit()) {
                client.appendOutputBuffer(":server 471 " + client.getNickname() + " " + channelName + " :Cannot join channel (+l)\r\n");
            } else {
                it->join(clientSocket);
                client.appendOutputBuffer(":" + client.getNickname() + "!" + client.getUsername() + "@localhost JOIN " + channelName + "\r\n");
                broadcastMessage(clientSocket, ":" + client.getNickname() + "!" + client.getUsername() + "@localhost JOIN " + channelName, fds);
            }
            fds[i].events |= POLLOUT;
            return;
        }
    }

    Channel newChannel(channelName);
    newChannel.join(clientSocket);
    server.channels.push_back(newChannel);

    client.appendOutputBuffer(":" + client.getNickname() + "!" + client.getUsername() + "@localhost JOIN " + channelName + "\r\n");
    broadcastMessage(clientSocket, ":" + client.getNickname() + "!" + client.getUsername() + "@localhost JOIN " + channelName, fds);
    fds[i].events |= POLLOUT;
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

        if (target[0] == '#') {
            bool isMember = false;
            for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
                if (it->getName() == target) {
                    std::map<int, bool> members = it->getMembers();
                    std::cout << "Checking if socket " << clientSocket << " is in channel " << target << ", members: " << members.size() << std::endl;
                    if (members.find(clientSocket) != members.end()) {
                        isMember = true;
                        std::cout << "Socket " << clientSocket << " is a member of " << target << std::endl;
                    } else {
                        std::cout << "Socket " << clientSocket << " is NOT a member of " << target << std::endl;
                    }
                    break;
                }
            }
            if (!isMember) {
                std::string response = ":server 404 " + client.getNickname() + " " + target + " :Cannot send to channel\r\n";
                client.appendOutputBuffer(response);
                fds[i].events |= POLLOUT;
                return;
            }
        }

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

void CommandHandler::handleWhois(const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
    if (input.length() <= 6) {
        std::string response = ":server@localhost 461 " + client.getNickname() + " WHOIS :Not enough parameters\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string targetNick = input.substr(6);
    while (!targetNick.empty() && (targetNick[0] == ' ' || targetNick[targetNick.length() - 1] == ' '))
        targetNick.erase(targetNick[0] == ' ' ? 0 : targetNick.length() - 1, 1);

    for (std::map<int, Client>::iterator it = server.m_clients.begin(); it != server.m_clients.end(); ++it) {
        if (it->second.getNickname() == targetNick) {
            std::string response = ":server@localhost 311 " + client.getNickname() + " " + targetNick + " " + 
                                  it->second.getUsername() + " localhost * :" + it->second.getRealname() + "\r\n";
            client.appendOutputBuffer(response);
            response = ":server@localhost 318 " + client.getNickname() + " " + targetNick + " :End of /WHOIS list\r\n";
            client.appendOutputBuffer(response);
            fds[i].events |= POLLOUT;
            return;
        }
    }
    std::string response = ":server@localhost 401 " + client.getNickname() + " " + targetNick + " :No such nick/channel\r\n";
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

void CommandHandler::handlePing(const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
    if (input.length() <= 5) { // "PING " = 5 символов
        std::string response = ":server 461 " + client.getNickname() + " PING :Not enough parameters\r\n";
        client.appendOutputBuffer(response);
        fds[i].events |= POLLOUT;
        return;
    }
    std::string token = input.substr(5);
    std::string response = ":server PONG server :" + token + "\r\n";
    client.appendOutputBuffer(response);
    fds[i].events |= POLLOUT;
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
    std::string firstParam = params.substr(0, spacePos);
    std::string rest = params.substr(spacePos + 1);
    std::string channelName, targetNick;
    size_t colonPos = rest.find(':');
    std::string reason;

    if (firstParam[0] == '#') {
        channelName = firstParam;
        if (colonPos != std::string::npos) {
            targetNick = rest.substr(0, colonPos);
            reason = (colonPos + 1 < rest.length()) ? rest.substr(colonPos + 1) : "Kicked by operator";
        } else {
            targetNick = rest;
            reason = "Kicked by operator";
        }
    } else {
        targetNick = firstParam;
        size_t secondSpace = rest.find(' ');
        if (secondSpace == std::string::npos && colonPos == std::string::npos) {
            channelName = rest;
            reason = "Kicked by operator";
        } else {
            if (colonPos != std::string::npos) {
                channelName = rest.substr(0, colonPos);
                reason = (colonPos + 1 < rest.length()) ? rest.substr(colonPos + 1) : "Kicked by operator";
            } else {
                channelName = rest.substr(0, secondSpace);
                reason = rest.substr(secondSpace + 1);
            }
        }
    }

    while (!targetNick.empty() && targetNick[0] == ' ') targetNick.erase(0, 1);
    while (!targetNick.empty() && targetNick[targetNick.length() - 1] == ' ') targetNick.erase(targetNick.length() - 1, 1);
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
            std::string response = ":" + client.getNickname() + "!" + client.getUsername() + "@localhost KICK " + channelName + " " + targetNick + " :" + reason + "\r\n";
            std::cout << "Sending KICK to kicked client: " << targetSocket << std::endl;
            server.m_clients[targetSocket].appendOutputBuffer(response);
            for (size_t j = 0; j < fds.size(); ++j) {
                if (fds[j].fd == targetSocket) {
                    fds[j].events |= POLLOUT;
                    break;
                }
            }
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

void CommandHandler::handleUnknownCommand(const std::string& input, Client& client, std::vector<pollfd>& fds, size_t i) {
    std::string response = ":server 421 " + client.getNickname() + " " + input + " :Unknown command\r\n";
    client.appendOutputBuffer(response);
    fds[i].events |= POLLOUT;
}

void CommandHandler::processCommand(int clientSocket, const std::string& input, std::vector<pollfd>& fds, size_t i) {
    Client* client;
    if (!checkClient(clientSocket, fds, client)) {
        return;
    }

    if (!client->isPasswordEntered()) {
        handlePassword(clientSocket, input, *client, fds, i);
    } else {
        if (input == server.config.getPassword()) {
            std::cout << "Ignoring repeated password input: " << input << "\n";
            return;
        }
        if (input.rfind("CAP LS", 0) == 0) {
            std::string response = ":server CAP * LS :\r\n"; /* иначе ирсси ругаеца */
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
        } else if (input.rfind("WHOIS", 0) == 0) { /* иначе ирсси ругаеца */
            handleWhois(input, *client, fds, i);
        } else if (input.rfind("MODE ", 0) == 0) {
            handleMode(clientSocket, input, *client, fds, i);
        } else if (input.rfind("PING", 0) == 0) {
            handlePing(input, *client, fds, i);
        } else if (input.rfind("KICK", 0) == 0) {
            handleKick(clientSocket, input, *client, fds, i);
        } else if (input.rfind("INVITE", 0) == 0) {
            handleInvite(clientSocket, input, *client, fds, i);
        } else if (input.rfind("TOPIC", 0) == 0) {
            handleTopic(clientSocket, input, *client, fds, i);
        } else {
            handleUnknownCommand(input, *client, fds, i);
        }
    }
}

void CommandHandler::broadcastMessage(int senderSocket, const std::string& message, std::vector<pollfd>& fds) {
    std::string command, target, text, kickedNick;
    size_t firstSpace = message.find(' ');

    if (message[0] == ':' && firstSpace != std::string::npos) {
        size_t commandStart = message.find(' ', firstSpace + 1); 
        if (commandStart != std::string::npos) {
            command = message.substr(firstSpace + 1, commandStart - firstSpace - 1); 
            size_t secondSpace = message.find(' ', commandStart + 1);
            if (command == "JOIN" || command == "MODE") {
                target = message.substr(commandStart + 1); 
            } else if (command == "PRIVMSG") {
                if (secondSpace == std::string::npos) return;
                target = message.substr(commandStart + 1, secondSpace - commandStart - 1); 
                size_t colonPos = message.find(':');
                if (colonPos == std::string::npos) return;
                text = message.substr(colonPos + 1); 
            } else if (command == "KICK") {
                if (secondSpace == std::string::npos) return;
                size_t thirdSpace = message.find(' ', secondSpace + 1);
                if (thirdSpace == std::string::npos) return;
                target = message.substr(commandStart + 1, secondSpace - commandStart - 1);
                kickedNick = message.substr(secondSpace + 1, thirdSpace - secondSpace - 1);
                size_t colonPos = message.find(':');
                text = (colonPos != std::string::npos && colonPos + 1 < message.length() && message.substr(colonPos + 1).find_first_not_of(" \r\n") != std::string::npos) ? message.substr(colonPos + 1) : "Kicked by operator";
            }
        } else {
            return;
        }
    } else {
        if (firstSpace == std::string::npos) return;
        command = message.substr(0, firstSpace);
        size_t secondSpace = message.find(' ', firstSpace + 1);
        if (secondSpace == std::string::npos && command != "JOIN" && command != "MODE" && command != "KICK") return;
        if (command == "JOIN" || command == "MODE") {
            target = message.substr(firstSpace + 1);
        } else if (command == "PRIVMSG") {
            target = message.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            size_t colonPos = message.find(':');
            if (colonPos == std::string::npos) return;
            text = message.substr(colonPos + 1);
        } else if (command == "KICK") {
            size_t thirdSpace = message.find(' ', secondSpace + 1);
            if (thirdSpace == std::string::npos) return;
            target = message.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            kickedNick = message.substr(secondSpace + 1, thirdSpace - secondSpace - 1);
            size_t colonPos = message.find(':');
            text = (colonPos != std::string::npos && colonPos + 1 < message.length() && message.substr(colonPos + 1).find_first_not_of(" \r\n") != std::string::npos) ? message.substr(colonPos + 1) : "Kicked by operator";
        }
    }

    std::string senderNick = server.m_clients[senderSocket].getNickname();
    std::string senderUser = server.m_clients[senderSocket].getUsername();

    if (command == "PRIVMSG") {
        if (target[0] == '#') {
            for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
                if (it->getName() == target) {
                    std::map<int, bool> members = it->getMembers();
                    for (std::map<int, bool>::iterator memberIt = members.begin(); memberIt != members.end(); ++memberIt) {
                        if (memberIt->first != senderSocket) {
                            std::string response = ":" + senderNick + "!" + senderUser + "@localhost PRIVMSG " + target + " :" + text + "\r\n";
                            server.m_clients[memberIt->first].appendOutputBuffer(response);
                            for (size_t i = 0; i < fds.size(); ++i) {
                                if (fds[i].fd == memberIt->first) {
                                    fds[i].events |= POLLOUT;
                                    break;
                                }
                            }
                        }
                    }
                    return; 
                }
            }
        } else {
            for (std::map<int, Client>::iterator it = server.m_clients.begin(); it != server.m_clients.end(); ++it) {
                if (it->second.getNickname() == target && it->first != senderSocket) {
                    std::string response = ":" + senderNick + "!" + senderUser + "@localhost PRIVMSG " + target + " :" + text + "\r\n";
                    server.m_clients[it->first].appendOutputBuffer(response);
                    for (size_t i = 0; i < fds.size(); ++i) {
                        if (fds[i].fd == it->first) {
                            fds[i].events |= POLLOUT;
                            break;
                        }
                    }
                    return;
                }
            }
        }
    } else if (command == "JOIN") {
        std::cout << "Broadcasting JOIN for channel: " << target << std::endl;
        for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
            if (it->getName() == target) {
                std::map<int, bool> members = it->getMembers();
                std::cout << "Channel " << target << " has " << members.size() << " members" << std::endl;
                for (std::map<int, bool>::iterator memberIt = members.begin(); memberIt != members.end(); ++memberIt) {
                    if (memberIt->first != senderSocket) {
                        std::string response = ":" + senderNick + "!" + senderUser + "@localhost JOIN " + target + "\r\n";
                        server.m_clients[memberIt->first].appendOutputBuffer(response);
                        for (size_t i = 0; i < fds.size(); ++i) {
                            if (fds[i].fd == memberIt->first) {
                                fds[i].events |= POLLOUT;
                                break;
                            }
                        }
                    }
                }
                break;
            }
        }
    } else if (command == "MODE") {
        size_t spacePos = target.find(' ');
        if (spacePos == std::string::npos) return;
        std::string channelName = target.substr(0, spacePos);
        for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
            if (it->getName() == channelName) {
                std::map<int, bool> members = it->getMembers();
                for (std::map<int, bool>::iterator memberIt = members.begin(); memberIt != members.end(); ++memberIt) {
                    if (memberIt->first != senderSocket) {
                        std::string response = ":" + senderNick + "!" + senderUser + "@localhost MODE " + target + "\r\n";
                        server.m_clients[memberIt->first].appendOutputBuffer(response);
                        for (size_t i = 0; i < fds.size(); ++i) {
                            if (fds[i].fd == memberIt->first) {
                                fds[i].events |= POLLOUT;
                                break;
                            }
                        }
                    }
                }
                break;
            }
        }
    } else if (command == "KICK") {
        for (std::vector<Channel>::iterator it = server.channels.begin(); it != server.channels.end(); ++it) {
            if (it->getName() == target) {
                std::map<int, bool> members = it->getMembers();
                std::string response = ":" + senderNick + "!" + senderUser + "@localhost KICK " + target + " " + kickedNick + " :" + text + "\r\n";
                for (std::map<int, bool>::iterator memberIt = members.begin(); memberIt != members.end(); ++memberIt) {
                    std::cout << "Sending KICK to member: " << memberIt->first << std::endl;
                    server.m_clients[memberIt->first].appendOutputBuffer(response);
                    for (size_t i = 0; i < fds.size(); ++i) {
                        if (fds[i].fd == memberIt->first) {
                            fds[i].events |= POLLOUT;
                            break;
                        }
                    }
                }
                break; 
            }
        }
    }
}