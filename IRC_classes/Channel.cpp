#include "Channel.hpp"

Channel::Channel(const std::string& n) : name(n) {}

void Channel::join(int clientSocket) {
    bool clientExists = false;
    for (std::vector<int>::iterator it = members.begin(); it != members.end(); ++it) {
        if (*it == clientSocket) {
            clientExists = true;
            break;
        }
    }
    if (!clientExists) {
        members.push_back(clientSocket);
    }
}

std::string Channel::getName() const { return name; }

std::vector<int> Channel::getMembers() const { return members; }