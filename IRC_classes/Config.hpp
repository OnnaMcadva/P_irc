#pragma once

#include <string>

class Config {
public:
    Config(int p, const std::string& pw); // Конструктор с выбросом исключений
    int getPort() const;
    std::string getPassword() const;
    void reconfigure(int newPort, const std::string& newPassword); // Новый метод для изменения конфигурации

private:
    int port;
    std::string password;
};
