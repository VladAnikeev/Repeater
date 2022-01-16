#ifndef BASE_LOG_H
#define BASE_LOG_H
#include <fstream>
#include <iostream>
#include <string>
#include "hex.h"
#include <errno.h>
class base_log
{
private:
    std::ostream *log_file;
    Hex toHex;
    std::string buf;

public:
    base_log(std::ostream &link_file);
    void error(const char *nameError);
    base_log &operator<<(int id);
    base_log &operator<<(char *massage);
    base_log &operator<<(const char *nameError);
    base_log &operator<<(std::ostream &endl(std::ostream &os));
    ~base_log();
};

base_log::base_log(std::ostream &link_file) : log_file(&link_file)
{
}

base_log &base_log::operator<<(const char *massage)
{
    std::cout << massage;
    buf += massage;
    return (*this);
}
base_log &base_log::operator<<(char *massage)
{
    std::cout << massage;
    buf += massage;
    return (*this);
}

base_log &base_log::operator<<(int id)
{
    std::cout << id;
    buf += toHex.intToHEX(id);
    return (*this);
}
base_log &base_log::operator<<(std::ostream &endl(std::ostream &os))
{
    std::cout << std::endl;
    (*log_file) << toHex.stringToHEX(buf) << std::endl;
    buf.clear();
    return (*this);
}

//логирование ошибок
void base_log::error(const char *nameError)
{
    std::cout << nameError << errno << std::endl;
    (*log_file) << toHex.stringToHEX(nameError + toHex.intToHEX(errno)) << std::endl;
}

base_log::~base_log()
{
    log_file->~basic_ostream();
}
#endif
