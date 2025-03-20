#include "Config.hpp"
#include <stdexcept>
#include <iostream>

Config::Config(int p, const std::string& pw) {
    if (p <= 0 || p > 65535) {
        throw std::runtime_error("Port must be between 1 and 65535");
    }
    if (pw.empty()) {
        throw std::runtime_error("Password cannot be empty");
    }
    port = p;
    password = pw;
}

int Config::getPort() const {
    return port;
}

std::string Config::getPassword() const {
    return password;
}

void Config::reconfigure(int newPort, const std::string& newPassword) {
    if (newPort <= 0 || newPort > 65535) {
        throw std::runtime_error("New port must be between 1 and 65535");
    }
    if (newPassword.empty()) {
        throw std::runtime_error("New password cannot be empty");
    }
    port = newPort;
    password = newPassword;
}
