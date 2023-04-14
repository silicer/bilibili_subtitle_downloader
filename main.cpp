#define CPPHTTPLIB_OPENSSL_SUPPORT

#include <iostream>
#include <httplib.h>
#include <Windows.h>

int main(int, char**) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    // std::cout << "Hello, world!\n";
    httplib::Client cli("https://dcme.harunare.top");
    auto res = cli.Get("/");
    std::cout << res->body << std::endl;
}
