#include <sodium.h>

#include <algorithm>

int main() {
    char arr[256] = {};
    ::randombytes_buf(arr, sizeof arr);
    for (auto b : arr) {
        if (b != '\x00') {
            return 0;
        }
    }
    return 1;
}
