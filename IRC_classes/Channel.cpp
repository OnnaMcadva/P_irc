#include "Channel.hpp"

Channel::Channel(const std::string& n) : name(n) {}

/* Функция `join` добавляет клиента (по его сокету) в список участников канала,
если его там еще нет. 
clientSocket — это идентификатор конкретного клиента, 
подключенного к серверу, чтобы сервер знал, с кем он общается. */

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

