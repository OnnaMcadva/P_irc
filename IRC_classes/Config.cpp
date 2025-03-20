#include "Config.hpp"
#include <iostream>
#include <stdexcept> 
#include <cstdlib>

/* Конструктор Config инициализирует объект с номером порта и паролем. */

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

/* Функция `reconfigure` обновляет настройки сервера:  
- Проверяет, что новый порт находится в допустимом диапазоне (1–65535).  
- Проверяет, что новый пароль не пустой.  
- Если проверки пройдены, обновляет значения порта и пароля.  
- В случае ошибки выбрасывает исключение. */

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
