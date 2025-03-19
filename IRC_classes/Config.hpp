#pragma once

#include <string>

class Config {
public:
    Config(int p, const std::string& pw);
    int getPort() const;
    std::string getPassword() const;

private:
    int port;
    std::string password;
};

