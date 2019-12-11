#include <nlohmann/json.hpp>

int main() {
    nlohmann::json j = {
        {"foo", "bar"},
    };
    return j.size() == 1 ? 0 : 12;
}
