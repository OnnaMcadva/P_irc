#include "Channel.hpp"

Channel::Channel(const std::string& n) : name(n) {}

/* The `join` function adds a client (identified by their socket) to the list of channel members
if they are not already in it. 
clientSocket is the identifier of a specific client 
connected to the server, allowing the server to know who it is communicating with. */

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

void Channel::removeMember(int clientSocket) {
    for (std::vector<int>::iterator it = members.begin(); it != members.end(); ++it) {
        if (*it == clientSocket) {
            members.erase(it);
            break;
        }
    }
}

std::string Channel::getName() const { return name; }

std::vector<int> Channel::getMembers() const { return members; }

