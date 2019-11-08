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

void test_nested_kvlist() {
    auto check_1 = [](auto str) {
        auto result = nested_kvlist::parse(str);
        CHECK(result.primary == "Foo");
        CHECK(result.pairs.size() == 1);
        REQUIRE(result.pairs.find("bar"));
        CHECK(result.pairs.find("bar")->value() == "baz");
    };
    check_1("Foo; bar=baz");
    check_1("Foo ; bar=baz");
    check_1("Foo ;   bar=baz");
    check_1("Foo ;   bar=baz  ");
    check_1("Foo;bar=baz  ");
    check_1("Foo;bar=baz");

    auto check_2 = [](auto str) {
        auto result = nested_kvlist::parse(str);
        CHECK(result.primary == "Foo");
        CHECK(result.pairs.size() == 0);
    };
    check_2("Foo");
    check_2("Foo;");
    check_2("Foo  ;");
    check_2("Foo  ; ");
    check_2("Foo; ");

    auto check_3 = [](auto str) {
        auto result = nested_kvlist::parse(str);
        CHECK(result.primary == "Foo bar");
        CHECK(result.pairs.size() == 2);
        REQUIRE(result.pairs.find("baz"));
        CHECK(result.pairs.find("baz")->value() == "meow");
        REQUIRE(result.pairs.find("quux"));
        CHECK(result.pairs.find("quux")->value() == "");
    };

    check_3("Foo bar; baz=meow quux");
    check_3("Foo bar  ; baz=meow quux=");
    check_3("Foo bar  ; quux= baz=meow");
    check_3("Foo bar  ;quux=   baz=meow");
}

void run_tests() {
    test_simple();
    test_multi();
    test_nested_kvlist();
}

DDS_TEST_MAIN;
