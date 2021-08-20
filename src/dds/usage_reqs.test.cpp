#include "./usage_reqs.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <range/v3/view/transform.hpp>

#include <algorithm>

#include <catch2/catch.hpp>

using Catch::Matchers::Contains;

namespace {
class IsRotation : public Catch::MatcherBase<std::vector<lm::usage>> {
    std::vector<lm::usage> cycle;

public:
    IsRotation(std::vector<lm::usage> cycle)
        : cycle(std::move(cycle)) {}

    bool match(const std::vector<lm::usage>& other) const override {
        if (cycle.size() != other.size())
            return false;
        if (cycle.empty())
            return true;

        std::vector<lm::usage> other_cpy(other);

        auto cycle_start = std::find(other_cpy.begin(), other_cpy.end(), cycle.front());
        std::rotate(other_cpy.begin(), cycle_start, other_cpy.end());

        return std::equal(cycle.begin(), cycle.end(), other_cpy.begin());
    }

    std::string describe() const override {
        return fmt::format("is a rotation of: {}",
                           fmt::join(cycle | ranges::views::transform([](const lm::usage& usage) {
                                         return fmt::format("'{}/{}'",
                                                            usage.namespace_,
                                                            usage.name);
                                     }),
                                     ", "));
    }
};
}  // namespace

TEST_CASE("Cyclic dependencies are rejected") {
    dds::usage_requirement_map reqs;
    {
        std::vector<lm::usage> previous;
        for (int i = 0; i < 10; ++i) {
            std::string dep_name = fmt::format("dep{}", i + 10);

            reqs.add(dep_name, dep_name, lm::library{.name = dep_name, .uses = previous});

            previous.push_back(lm::usage{dep_name, dep_name});
        }
    }

    reqs.add("dep1", "dep1", lm::library{.name = "dep1", .uses = {lm::usage{"dep2", "dep2"}}});
    reqs.add("dep2", "dep2", lm::library{.name = "dep2", .uses = {lm::usage{"dep3", "dep3"}}});
    reqs.add("dep3", "dep3", lm::library{.name = "dep3", .uses = {lm::usage{"dep1", "dep1"}}});

    REQUIRE_THROWS_WITH(dds::usage_requirements(reqs),
                        Contains("'dep1/dep1' uses 'dep2/dep2' uses 'dep3/dep3' uses 'dep1/dep1'"));
}

TEST_CASE("Self-referential `uses` are rejected") {
    dds::usage_requirement_map reqs;

    reqs.add("dep1", "dep1", lm::library{.name = "dep1", .uses = {lm::usage{"dep1", "dep1"}}});

    REQUIRE_THROWS_WITH(dds::usage_requirements(reqs), Contains("'dep1/dep1' uses 'dep1/dep1'"));
}

TEST_CASE("Cyclic dependencies can be found - Self-referential") {
    dds::usage_requirement_map reqs;
    reqs.add("dep1", "dep1", lm::library{.name = "dep1", .uses = {lm::usage{"dep1", "dep1"}}});

    auto cycle = reqs.find_usage_cycle();
    REQUIRE(cycle.has_value());
    CHECK_THAT(*cycle, IsRotation({lm::usage{"dep1", "dep1"}}));
}

TEST_CASE("Cyclic dependencies can be found - Longer loops") {
    dds::usage_requirement_map reqs;
    reqs.add("dep1", "dep1", lm::library{.name = "dep1", .uses = {lm::usage{"dep2", "dep2"}}});
    reqs.add("dep2", "dep2", lm::library{.name = "dep2", .uses = {lm::usage{"dep3", "dep3"}}});
    reqs.add("dep3", "dep3", lm::library{.name = "dep3", .uses = {lm::usage{"dep1", "dep1"}}});

    auto cycle = reqs.find_usage_cycle();
    REQUIRE(cycle.has_value());
    CHECK_THAT(*cycle,
               IsRotation({lm::usage{"dep1", "dep1"},
                           lm::usage{"dep2", "dep2"},
                           lm::usage{"dep3", "dep3"}}));
}

TEST_CASE("Cyclic dependencies can be found - larger case") {
    dds::usage_requirement_map reqs;

    {
        std::vector<lm::usage> previous;
        for (int i = 0; i < 10; ++i) {
            std::string dep_name = fmt::format("dep{}", i + 10);

            reqs.add(dep_name, dep_name, lm::library{.name = dep_name, .uses = previous});

            previous.push_back(lm::usage{dep_name, dep_name});
        }
    }

    reqs.add("dep1", "dep1", lm::library{.name = "dep1", .uses = {lm::usage{"dep2", "dep2"}}});
    reqs.add("dep2", "dep2", lm::library{.name = "dep2", .uses = {lm::usage{"dep3", "dep3"}}});
    reqs.add("dep3", "dep3", lm::library{.name = "dep3", .uses = {lm::usage{"dep1", "dep1"}}});

    auto cycle = reqs.find_usage_cycle();
    REQUIRE(cycle.has_value());
    CHECK_THAT(*cycle,
               IsRotation({lm::usage{"dep1", "dep1"},
                           lm::usage{"dep2", "dep2"},
                           lm::usage{"dep3", "dep3"}}));
}