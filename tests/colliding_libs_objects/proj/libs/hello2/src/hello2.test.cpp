#include <iostream>
#include <string>

namespace hello {
std::string message();
}

int main() {
    auto msg = hello::message();
    std::cout << msg << '\n';
    if (msg != "Hello 2")
        return 1;
}