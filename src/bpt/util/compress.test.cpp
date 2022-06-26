#include <bpt/error/result.hpp>
#include <bpt/temp.hpp>
#include <bpt/util/compress.hpp>
#include <bpt/util/fs/io.hpp>

#include <boost/leaf.hpp>
#include <catch2/catch.hpp>

template <typename Fun>
void check_voidres(Fun&& f) {
    boost::leaf::try_handle_all(f, [](const boost::leaf::verbose_diagnostic_info& info) {
        FAIL(info);
    });
}

TEST_CASE("Compress/uncompress a file") {
    const std::string_view plain_string = "I am the text content";
    auto                   tdir         = bpt::temporary_dir::create();
    bpt::fs::create_directories(tdir.path());
    const auto test_file = tdir.path() / "test.txt";
    bpt::write_file(test_file, plain_string);
    CHECK(bpt::read_file(test_file) == plain_string);
    auto test_file_gz = bpt::fs::path(test_file) += ".gz";
    check_voidres([&] { return bpt::compress_file_gz(test_file, test_file_gz); });

    auto compressed_content = bpt::read_file(test_file_gz);
    CHECK_FALSE(compressed_content.empty());
    CHECK(compressed_content != plain_string);

    auto decomp_file = bpt::fs::path(test_file) += ".plain";
    check_voidres([&] { return bpt::decompress_file_gz(test_file_gz, decomp_file); });
    CHECK(bpt::read_file(decomp_file) == plain_string);
}

TEST_CASE("Fail to compress a non-existent file") {
    auto tdir = bpt::temporary_dir::create();
    bpt::fs::create_directories(tdir.path());
    boost::leaf::try_handle_all(
        [&]() -> bpt::result<void> {
            BOOST_LEAF_CHECK(bpt::compress_file_gz(tdir.path() / "noexist.txt",
                                                   tdir.path() / "doesn't matter.gz"));
            FAIL("Compression should have failed");
            return {};
        },
        [&](bpt::e_compress_error,
            std::error_code        ec,
            bpt::e_read_file_path  infile,
            bpt::e_write_file_path outfile) {
            CHECK(ec == std::errc::no_such_file_or_directory);
            CHECK(infile.value == (tdir.path() / "noexist.txt"));
            CHECK(outfile.value == (tdir.path() / "doesn't matter.gz"));
        },
        [](const boost::leaf::verbose_diagnostic_info& info) {
            FAIL("Unexpected failure: " << info);
        });
}

TEST_CASE("Decompress a non-compressed file") {
    auto tdir = bpt::temporary_dir::create();
    bpt::fs::create_directories(tdir.path());
    boost::leaf::try_handle_all(
        [&]() -> bpt::result<void> {
            BOOST_LEAF_CHECK(bpt::decompress_file_gz(__FILE__, tdir.path() / "dummy.txt"));
            FAIL("Decompression should have failed");
            return {};
        },
        [&](bpt::e_decompress_error, bpt::e_read_file_path infile) {
            CHECK(infile.value == __FILE__);
        },
        [](const boost::leaf::verbose_diagnostic_info& info) {
            FAIL("Unexpected failure: " << info);
        });
}