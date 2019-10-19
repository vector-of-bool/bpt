#include <browns/md5.hpp>

#include <browns/output.hpp>
#include <neo/buffer.hpp>

#include <iostream>

int check_hash(std::string_view str, std::string_view expect_md5) {
    browns::md5 hash;
    hash.feed(neo::buffer(str));
    hash.pad();
    auto dig = hash.digest();
    auto dig_str = browns::format_digest(dig);
    if (dig_str != expect_md5) {
        std::cerr << "Hash of '" << str << "' did not match. Expected '" << expect_md5
                  << "', but got '" << dig_str << "'\n";
        return 1;
    }
    return 0;
}

int main() {
    browns::md5 hash;

    int n_fails = 0;
    n_fails += check_hash("1234abcd1234abcd1234abcd1234abcd", "67aa636d72b967157c363f0acdf7011b");
    n_fails += check_hash("The quick brown fox jumps over the lazy dog",
                          "9e107d9d372bb6826bd81d3542a419d6");
    n_fails += check_hash("", "d41d8cd98f00b204e9800998ecf8427e");
    n_fails += check_hash("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW", "967ce152b23edc20ebd23b7eba57277c");
    n_fails += check_hash("WWWWWWWWWWWWWWWWWWWWWWWWWWWWW", "9aff577d3248b8889b22f24ee9665c17");
    return n_fails;
}
