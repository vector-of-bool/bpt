import json
from pathlib import Path
from typing import NamedTuple, Sequence, List

import pytest

from tests import DDS, fileutil


class DepsCase(NamedTuple):
    dep: str
    usage: str
    source: str

    def setup_root(self, dds: DDS) -> None:
        dds.scope.enter_context(
            fileutil.set_contents(
                dds.source_root / 'package.json',
                json.dumps({
                    'name': 'test-project',
                    'namespace': 'test',
                    'version': '0.0.0',
                    'depends': [self.dep],
                }).encode()))
        dds.scope.enter_context(
            fileutil.set_contents(dds.source_root / 'library.json',
                                  json.dumps({
                                      'name': 'test',
                                      'uses': [self.usage],
                                  }).encode()))
        dds.scope.enter_context(fileutil.set_contents(dds.source_root / 'src/test.test.cpp', self.source.encode()))


CASES: List[DepsCase] = []


def get_default_pkg_versions(pkg: str) -> Sequence[str]:
    catalog_json = Path(__file__).resolve().parent.parent.parent / 'old-catalog.json'
    catalog_dict = json.loads(catalog_json.read_text())
    return list(catalog_dict['packages'][pkg].keys())


def add_cases(pkg: str, uses: str, versions: Sequence[str], source: str) -> None:
    if versions == ['auto']:
        versions = get_default_pkg_versions(pkg)
    for ver in versions:
        CASES.append(DepsCase(f'{pkg}@{ver}', uses, source))


# pylint: disable=pointless-string-statement

# magic_enum tests
"""
##     ##    ###     ######   ####  ######          ######## ##    ## ##     ## ##     ##
###   ###   ## ##   ##    ##   ##  ##    ##         ##       ###   ## ##     ## ###   ###
#### ####  ##   ##  ##         ##  ##               ##       ####  ## ##     ## #### ####
## ### ## ##     ## ##   ####  ##  ##               ######   ## ## ## ##     ## ## ### ##
##     ## ######### ##    ##   ##  ##               ##       ##  #### ##     ## ##     ##
##     ## ##     ## ##    ##   ##  ##    ##         ##       ##   ### ##     ## ##     ##
##     ## ##     ##  ######   ####  ######  ####### ######## ##    ##  #######  ##     ##
"""
add_cases(
    'magic_enum', 'neargye/magic_enum', ['auto'], r'''
    #include <magic_enum.hpp>
    #include <string_view>

    enum my_enum {
        foo,
        bar,
    };

    int main() {
        if (magic_enum::enum_name(my_enum::foo) != "foo") {
            return 1;
        }
    }
    ''')

# Range-v3 tests
"""
########     ###    ##    ##  ######   ########         ##     ##  #######
##     ##   ## ##   ###   ## ##    ##  ##               ##     ## ##     ##
##     ##  ##   ##  ####  ## ##        ##               ##     ##        ##
########  ##     ## ## ## ## ##   #### ######   ####### ##     ##  #######
##   ##   ######### ##  #### ##    ##  ##                ##   ##         ##
##    ##  ##     ## ##   ### ##    ##  ##                 ## ##   ##     ##
##     ## ##     ## ##    ##  ######   ########            ###     #######
"""

add_cases(
    'range-v3', 'range-v3/range-v3', ['auto'], r'''
    #include <range/v3/algorithm/remove_if.hpp>

    #include <vector>
    #include <algorithm>

    int main() {
        std::vector<int> nums = {1, 2, 3, 5, 1, 4, 2, 7, 8, 0, 9};
        auto end = ranges::remove_if(nums, [](auto i) { return i % 2; });
        return std::distance(nums.begin(), end) != 5;
    }
    ''')

# nlohmann-json
"""
##    ## ##        #######  ##     ## ##     ##    ###    ##    ## ##    ##               ##  ######   #######  ##    ##
###   ## ##       ##     ## ##     ## ###   ###   ## ##   ###   ## ###   ##               ## ##    ## ##     ## ###   ##
####  ## ##       ##     ## ##     ## #### ####  ##   ##  ####  ## ####  ##               ## ##       ##     ## ####  ##
## ## ## ##       ##     ## ######### ## ### ## ##     ## ## ## ## ## ## ## #######       ##  ######  ##     ## ## ## ##
##  #### ##       ##     ## ##     ## ##     ## ######### ##  #### ##  ####         ##    ##       ## ##     ## ##  ####
##   ### ##       ##     ## ##     ## ##     ## ##     ## ##   ### ##   ###         ##    ## ##    ## ##     ## ##   ###
##    ## ########  #######  ##     ## ##     ## ##     ## ##    ## ##    ##          ######   ######   #######  ##    ##
"""
add_cases('nlohmann-json', 'nlohmann/json', ['auto'], r'''
    #include <nlohmann/json.hpp>

    int main() {}
    ''')

# ctre
"""
 ######  ######## ########  ########
##    ##    ##    ##     ## ##
##          ##    ##     ## ##
##          ##    ########  ######
##          ##    ##   ##   ##
##    ##    ##    ##    ##  ##
 ######     ##    ##     ## ########
"""
add_cases(
    'ctre', 'hanickadot/ctre', ['auto'], r'''
    #include <ctre.hpp>

    constexpr ctll::fixed_string MY_REGEX{"\\w+-[0-9]+"};

    int main() {
        auto [did_match] = ctre::match<MY_REGEX>("foo-44");
        if (!did_match) {
            return 1;
        }

        auto [did_match_2] = ctre::match<MY_REGEX>("bar-1ff");
        if (did_match_2) {
            return 2;
        }
    }
    ''')

# fmt
"""
######## ##     ## ########
##       ###   ###    ##
##       #### ####    ##
######   ## ### ##    ##
##       ##     ##    ##
##       ##     ##    ##
##       ##     ##    ##
"""
add_cases('fmt', 'fmt/fmt', ['auto'], r'''
    #include <fmt/core.h>

    int main() {
        fmt::print("Hello!");
    }
    ''')

# Catch2
"""
 ######     ###    ########  ######  ##     ##  #######
##    ##   ## ##      ##    ##    ## ##     ## ##     ##
##        ##   ##     ##    ##       ##     ##        ##
##       ##     ##    ##    ##       #########  #######
##       #########    ##    ##       ##     ## ##
##    ## ##     ##    ##    ##    ## ##     ## ##
 ######  ##     ##    ##     ######  ##     ## #########
"""
add_cases(
    'catch2', 'catch2/catch2', ['auto'], r'''
    #include <catch2/catch_with_main.hpp>

    TEST_CASE("I am a test case") {
        CHECK((2 + 2) == 4);
        CHECK_FALSE((2 + 2) == 5);
    }
    ''')

# Asio
"""
   ###     ######  ####  #######
  ## ##   ##    ##  ##  ##     ##
 ##   ##  ##        ##  ##     ##
##     ##  ######   ##  ##     ##
#########       ##  ##  ##     ##
##     ## ##    ##  ##  ##     ##
##     ##  ######  ####  #######
"""
add_cases(
    'asio', 'asio/asio', ['auto'], r'''
    #include <asio.hpp>

    int main() {
        asio::io_context ioc;

        int retcode = 12;
        ioc.post([&] {
            retcode = 0;
        });
        ioc.run();
        return retcode;
    }
    ''')

# Abseil
"""
   ###    ########   ######  ######## #### ##
  ## ##   ##     ## ##    ## ##        ##  ##
 ##   ##  ##     ## ##       ##        ##  ##
##     ## ########   ######  ######    ##  ##
######### ##     ##       ## ##        ##  ##
##     ## ##     ## ##    ## ##        ##  ##
##     ## ########   ######  ######## #### ########
"""
add_cases(
    'abseil', 'abseil/abseil', ['auto'], r'''
    #include <absl/strings/str_cat.h>

    int main() {
        std::string_view foo = "foo";
        std::string_view bar = "bar";
        auto cat = absl::StrCat(foo, bar);
        return cat != "foobar";
    }
    ''')

# Zlib
"""
######## ##       #### ########
     ##  ##        ##  ##     ##
    ##   ##        ##  ##     ##
   ##    ##        ##  ########
  ##     ##        ##  ##     ##
 ##      ##        ##  ##     ##
######## ######## #### ########
"""
add_cases(
    'zlib', 'zlib/zlib', ['auto'], r'''
    #include <zlib.h>
    #include <cassert>

    int main() {
        ::z_stream strm = {};
        deflateInit(&strm, 6);

        const char buffer[] = "foo bar baz";
        strm.next_in        = (Bytef*)buffer;
        strm.avail_in       = sizeof buffer;

        char dest[256] = {};
        strm.next_out  = (Bytef*)dest;
        strm.avail_out = sizeof dest;
        auto ret = deflate(&strm, Z_FINISH);
        deflateEnd(&strm);
        assert(ret == Z_STREAM_END);
        assert(strm.avail_in == 0);
        assert(strm.avail_out != sizeof dest);
    }
    ''')

# sol2
"""
 ######   #######  ##        #######
##    ## ##     ## ##       ##     ##
##       ##     ## ##              ##
 ######  ##     ## ##        #######
      ## ##     ## ##       ##
##    ## ##     ## ##       ##
 ######   #######  ######## #########
"""
add_cases(
    'sol2', 'sol2/sol2', ['3.2.1', '3.2.0', '3.0.3', '3.0.2'], r'''
    #include <sol/sol.hpp>

    int main() {
        sol::state lua;
        int x = 0;
        lua.set_function("beepboop", [&]{ ++x; });
        lua.script("beepboop()");
        return x != 1;
    }
    ''')

# pegtl
"""
########  ########  ######   ######## ##
##     ## ##       ##    ##     ##    ##
##     ## ##       ##           ##    ##
########  ######   ##   ####    ##    ##
##        ##       ##    ##     ##    ##
##        ##       ##    ##     ##    ##
##        ########  ######      ##    ########
"""
add_cases(
    'pegtl', 'tao/pegtl', ['auto'], r'''
    #include <tao/pegtl.hpp>

    using namespace tao::pegtl;

    struct sign : one<'+', '-'> {};
    struct integer : seq<opt<sign>, plus<digit>> {};

    int main() {
        tao::pegtl::string_input str{"+44", "[test string]"};
        tao::pegtl::parse<integer>(str);
    }
    ''')

# Boost.PFR
"""
########   #######   #######   ######  ########     ########  ######## ########
##     ## ##     ## ##     ## ##    ##    ##        ##     ## ##       ##     ##
##     ## ##     ## ##     ## ##          ##        ##     ## ##       ##     ##
########  ##     ## ##     ##  ######     ##        ########  ######   ########
##     ## ##     ## ##     ##       ##    ##        ##        ##       ##   ##
##     ## ##     ## ##     ## ##    ##    ##    ### ##        ##       ##    ##
########   #######   #######   ######     ##    ### ##        ##       ##     ##
"""
add_cases(
    'boost.pfr', 'boost/pfr', ['auto'], r'''
    #include <iostream>
    #include <string>
    #include <boost/pfr/precise.hpp>

    struct some_person {
        std::string name;
        unsigned birth_year;
    };

    int main() {
        some_person val{"Edgar Allan Poe", 1809};

        std::cout << boost::pfr::get<0>(val)                // No macro!
            << " was born in " << boost::pfr::get<1>(val);  // Works with any aggregate initializables!

        return boost::pfr::get<0>(val) != "Edgar Allan Poe";
    }
    ''')

# Boost.LEAF
"""
##       ########    ###    ########
##       ##         ## ##   ##
##       ##        ##   ##  ##
##       ######   ##     ## ######
##       ##       ######### ##
##       ##       ##     ## ##
######## ######## ##     ## ##
"""
add_cases(
    'boost.leaf', 'boost/leaf', ['auto'], r'''
    #include <boost/leaf/all.hpp>

    namespace leaf = boost::leaf;

    int main() {
        return leaf::try_handle_all(
            [&]() -> leaf::result<int> {
                return 0;
            },
            [](leaf::error_info const&) {
                return 32;
            }
        );
    }
    ''')

# Boost.mp11
"""
########   #######   #######   ######  ########     ##     ## ########     ##      ##
##     ## ##     ## ##     ## ##    ##    ##        ###   ### ##     ##  ####    ####
##     ## ##     ## ##     ## ##          ##        #### #### ##     ##    ##      ##
########  ##     ## ##     ##  ######     ##        ## ### ## ########     ##      ##
##     ## ##     ## ##     ##       ##    ##        ##     ## ##           ##      ##
##     ## ##     ## ##     ## ##    ##    ##    ### ##     ## ##           ##      ##
########   #######   #######   ######     ##    ### ##     ## ##         ######  ######
"""
add_cases(
    'boost.mp11', 'boost/mp11', ['auto'], r'''
    #include <boost/mp11.hpp>

    int main() {
        return boost::mp11::mp_false() == boost::mp11::mp_true();
    }
    ''')

# libsodium
"""
##       #### ########   ######   #######  ########  #### ##     ## ##     ##
##        ##  ##     ## ##    ## ##     ## ##     ##  ##  ##     ## ###   ###
##        ##  ##     ## ##       ##     ## ##     ##  ##  ##     ## #### ####
##        ##  ########   ######  ##     ## ##     ##  ##  ##     ## ## ### ##
##        ##  ##     ##       ## ##     ## ##     ##  ##  ##     ## ##     ##
##        ##  ##     ## ##    ## ##     ## ##     ##  ##  ##     ## ##     ##
######## #### ########   ######   #######  ########  ####  #######  ##     ##
"""
add_cases(
    'libsodium', 'sodium/sodium', ['auto'], r'''
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
    ''')

# toml++
"""
########  #######  ##     ## ##
   ##    ##     ## ###   ### ##         ##     ##
   ##    ##     ## #### #### ##         ##     ##
   ##    ##     ## ## ### ## ##       ###### ######
   ##    ##     ## ##     ## ##         ##     ##
   ##    ##     ## ##     ## ##         ##     ##
   ##     #######  ##     ## ########
"""
add_cases(
    'tomlpp', 'tomlpp/tomlpp', ['auto'], r'''
    #include <toml++/toml.h>

    #include <string_view>

    int main() {
        std::string_view sv = R"(
            [library]
            something = "cats"
            person = "Joe"
        )";

        toml::table tbl = toml::parse(sv);
        return tbl["library"]["person"] != "Joe";
    }
    ''')

# Inja
"""
#### ##    ##       ##    ###
 ##  ###   ##       ##   ## ##
 ##  ####  ##       ##  ##   ##
 ##  ## ## ##       ## ##     ##
 ##  ##  #### ##    ## #########
 ##  ##   ### ##    ## ##     ##
#### ##    ##  ######  ##     ##
"""
add_cases(
    'inja', 'inja/inja', ['2.0.0', '2.0.1', '2.1.0', '2.2.0'], r'''
    #include <inja/inja.hpp>
    #include <nlohmann/json.hpp>

    int main() {
        nlohmann::json data;
        data["foo"] = "bar";

        auto result = inja::render("foo {{foo}}", data);
        return result != "foo bar";
    }
    ''')

# Cereal
"""
 ######  ######## ########  ########    ###    ##
##    ## ##       ##     ## ##         ## ##   ##
##       ##       ##     ## ##        ##   ##  ##
##       ######   ########  ######   ##     ## ##
##       ##       ##   ##   ##       ######### ##
##    ## ##       ##    ##  ##       ##     ## ##
 ######  ######## ##     ## ######## ##     ## ########
"""
add_cases(
    'cereal', 'cereal/cereal', ['auto'], r'''
    #include <cereal/types/memory.hpp>
    #include <cereal/types/string.hpp>
    #include <cereal/archives/binary.hpp>

    #include <sstream>

    struct something {
        int a, b, c;
        std::string str;

        template <typename Ar>
        void serialize(Ar& ar) {
            ar(a, b, c, str);
        }
    };

    int main() {
        std::stringstream strm;
        cereal::BinaryOutputArchive ar{strm};

        something s;
        ar(s);

        return 0;
    }
    ''')

# pcg
"""
########   ######   ######
##     ## ##    ## ##    ##
##     ## ##       ##
########  ##       ##   ####
##        ##       ##    ##
##        ##    ## ##    ##
##         ######   ######
"""
add_cases(
    'pcg-cpp', 'pcg/pcg-cpp', ['auto'], r'''
    #include <pcg_random.hpp>

    #include <iostream>

    int main() {
        pcg64 rng{1729};
        return rng() != 14925250045015479985;
    }
    ''')

# spdlog
"""
 ######  ########  ########  ##        #######   ######
##    ## ##     ## ##     ## ##       ##     ## ##    ##
##       ##     ## ##     ## ##       ##     ## ##
 ######  ########  ##     ## ##       ##     ## ##   ####
      ## ##        ##     ## ##       ##     ## ##    ##
##    ## ##        ##     ## ##       ##     ## ##    ##
 ######  ##        ########  ########  #######   ######
"""
add_cases('spdlog', 'spdlog/spdlog', ['auto'], r'''
    #include <spdlog/spdlog.h>

    int main() {
        spdlog::info("Howdy!");
    }
    ''')

# date
"""
########     ###    ######## ########
##     ##   ## ##      ##    ##
##     ##  ##   ##     ##    ##
##     ## ##     ##    ##    ######
##     ## #########    ##    ##
##     ## ##     ##    ##    ##
########  ##     ##    ##    ########
"""
add_cases(
    'hinnant-date', 'hinnant/date', ['auto'], r'''
    #include <date/date.h>
    #include <iostream>

    int main() {
        auto now = std::chrono::system_clock::now();
        using namespace date::literals;
        auto year = date::year_month_day{date::floor<date::days>(now)}.year();
        std::cout << "The current year is " << year << '\n';
        return year < 2020_y;
    }
    ''')


@pytest.mark.deps_test
@pytest.mark.parametrize('case', CASES, ids=[c.dep for c in CASES])
def test_dep(case: DepsCase, dds_pizza_catalog: Path, dds: DDS) -> None:
    case.setup_root(dds)
    dds.build(catalog_path=dds_pizza_catalog)
