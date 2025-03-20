#pragma once

#include <string>

class Config {
public:
    Config(int p, const std::string& pw);
    int getPort() const;
    std::string getPassword() const;
    void reconfigure(int newPort, const std::string& newPassword);

private:
    int port;
    std::string password;
};
