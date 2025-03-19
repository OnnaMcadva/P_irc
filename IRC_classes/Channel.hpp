#pragma once

#include <string>
#include <vector>

class Channel {
public:
    Channel(const std::string& n);
    void join(int clientSocket);
    std::string getName() const;
    std::vector<int> getMembers() const;

private:
    std::string name;
    std::vector<int> members;
};
