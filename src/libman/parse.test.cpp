#include <dds/util.test.hpp>
#include <libman/parse.hpp>

#include <iostream>

using namespace lm;

void test_simple() {
    auto lm_src = "";
    auto kvs    = parse_string(lm_src);
    CHECK(kvs.size() == 0);

    CHECK(parse_string("    ").size() == 0);
    CHECK(parse_string("\n    ").size() == 0);
    CHECK(parse_string("#comment\n    ").size() == 0);

    lm_src = "foo: bar\n ";
    kvs    = parse_string(lm_src);
    CHECK(kvs.size() == 1);
    REQUIRE(kvs.find("foo"));
    CHECK(kvs.find("foo")->value() == "bar");

    lm_src = "foo:bar: baz";
    kvs    = parse_string(lm_src);
    CHECK(kvs.size() == 1);
    REQUIRE(kvs.find("foo:bar"));
    CHECK(kvs.find("foo:bar")->value() == "baz");

    CHECK(parse_string("#comment").size() == 0);
    CHECK(parse_string("\n\n").size() == 0);
    CHECK(parse_string("\n#comment").size() == 0);
    CHECK(parse_string("#comment\n\n").size() == 0);

    std::vector<std::string_view> empty_foos = {
        "Foo:",
        "Foo:   ",
        "Foo:\n",
        "Foo:  \n",
        "\n\nFoo:",
        "   Foo:",
        "   Foo:   ",
        "Foo  :",
        "Foo   :\n",
    };
    for (auto s : empty_foos) {
        kvs = parse_string(s);
        CHECK(kvs.size() == 1);
        REQUIRE(kvs.find("Foo"));
        CHECK(kvs.find("Foo")->value() == "");
    }

    kvs = parse_string("foo: # Not a comment");
    CHECK(kvs.size() == 1);
    REQUIRE(kvs.find("foo"));
    CHECK(kvs.find("foo")->value() == "# Not a comment");
}

void test_multi() {
    auto kvs = parse_string("Foo: bar\nbaz: qux");
    CHECK(kvs.size() == 2);
    REQUIRE(kvs.find("Foo"));
    CHECK(kvs.find("Foo")->value() == "bar");
    REQUIRE(kvs.find("baz"));
    CHECK(kvs.find("baz")->value() == "qux");

    kvs = parse_string("foo: first\nfoo: second\n");
    CHECK(kvs.size() == 2);
    auto iter = kvs.iter("foo");
    REQUIRE(iter);
    CHECK(iter->key() == "foo");
    CHECK(iter->value() == "first");
    ++iter;
    REQUIRE(iter);
    CHECK(iter->key() == "foo");
    CHECK(iter->value() == "second");
    ++iter;
    CHECK(!iter);

    iter = kvs.iter("no-exist");
    CHECK(!iter);
}

void run_tests() {
    test_simple();
    test_multi();
}

DDS_TEST_MAIN;
