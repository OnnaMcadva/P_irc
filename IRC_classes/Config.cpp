#include "Config.hpp"
#include <iostream>
#include <cstdlib>

Config::Config(int p, const std::string& pw) {
    if (p <= 0 || p > 65535) {
        std::cerr << "Error: Port must be between 1 and 65535" << std::endl;
        exit(1);
    }
    if (pw.empty()) {
        std::cerr << "Error: Password cannot be empty" << std::endl;
        exit(1);
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