#include <exception>
#include <string>

#pragma once

class bwt_error : public std::exception {
private:
    std::string msg;
public:
    bwt_error(std::string m): msg {m} {}

    virtual const char *what() const throw()
    {
        return msg.c_str();
    }
};

class bad_flag : public std::exception {
private:
    std::string msg;
public:
    bad_flag(std::string m): msg {m} {}

    virtual const char *what() const throw()
    {
        return msg.c_str();
    }
};



