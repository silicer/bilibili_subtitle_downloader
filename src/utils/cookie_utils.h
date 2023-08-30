#ifndef COOKIE_UTILS_H
#define COOKIE_UTILS_H

#include <string>

#ifdef _WIN32
#include <Windows.h>
#endif

std::string get_cookie(const std::string &domain, std::string browser);
#endif