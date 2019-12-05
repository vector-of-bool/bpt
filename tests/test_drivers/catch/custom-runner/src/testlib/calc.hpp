#pragma once

namespace stuff {

int calculate(int a, int b) {
    int result = a + b;
    if (result == 42) {
        return result;
    } else {
        return 42;
    }
}

}  // namespace stuff