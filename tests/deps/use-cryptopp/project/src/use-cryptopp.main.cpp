#include <cryptopp/osrng.h>

#include <string>

int main() {
    std::string arr;
    arr.resize(256);
    CryptoPP::OS_GenerateRandomBlock(false,
                                     reinterpret_cast<CryptoPP::byte*>(arr.data()),
                                     arr.size());
    for (auto b : arr) {
        if (b != '\x00') {
            return 0;
        }
    }
    return 1;
}
