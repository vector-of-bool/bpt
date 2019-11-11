#include <browns/md5.hpp>

#include <browns/output.hpp>
#include <neo/as_buffer.hpp>

#include <catch2/catch.hpp>

#include <iostream>

auto md5_hash_str(std::string_view s) {
    browns::md5 hash;
    hash.feed(neo::as_buffer(s));
    hash.pad();
    return browns::format_digest(hash.digest());
}

void check_hash(std::string_view str, std::string_view digest) {
    INFO("Hashed string: " << str);
    CHECK(md5_hash_str(str) == digest);
}

TEST_CASE("Known hashes") {
    check_hash("1234abcd1234abcd1234abcd1234abcd", "67aa636d72b967157c363f0acdf7011b");
    check_hash("The quick brown fox jumps over the lazy dog", "9e107d9d372bb6826bd81d3542a419d6");
    check_hash("", "d41d8cd98f00b204e9800998ecf8427e");
    check_hash(
        "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW",
        "967ce152b23edc20ebd23b7eba57277c");
    check_hash("WWWWWWWWWWWWWWWWWWWWWWWWWWWWW", "9aff577d3248b8889b22f24ee9665c17");
}
