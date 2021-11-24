#include <dds/error/result.hpp>
#include <dds/temp.hpp>
#include <dds/util/compress.hpp>

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
    auto                   tdir         = dds::temporary_dir::create();
    dds::fs::create_directories(tdir.path());
    const auto test_file = tdir.path() / "test.txt";
    check_voidres([&] { return dds::write_file(test_file, plain_string); });
    CHECK(dds::slurp_file(test_file) == plain_string);
    auto test_file_gz = dds::fs::path(test_file) += ".gz";
    check_voidres([&] { return dds::compress_file_gz(test_file, test_file_gz); });

    auto compressed_content = dds::slurp_file(test_file_gz);
    CHECK_FALSE(compressed_content.empty());
    CHECK(compressed_content != plain_string);

    auto decomp_file = dds::fs::path(test_file) += ".plain";
    check_voidres([&] { return dds::decompress_file_gz(test_file_gz, decomp_file); });
    CHECK(dds::slurp_file(decomp_file) == plain_string);
}

TEST_CASE("Fail to compress a non-existent file") {
    auto tdir = dds::temporary_dir::create();
    dds::fs::create_directories(tdir.path());
    boost::leaf::try_handle_all(
        [&]() -> dds::result<void> {
            BOOST_LEAF_CHECK(dds::compress_file_gz(tdir.path() / "noexist.txt",
                                                   tdir.path() / "doesn't matter.gz"));
            FAIL("Compression should have failed");
            return {};
        },
        [&](dds::e_compress_error,
            std::error_code        ec,
            dds::e_read_file_path  infile,
            dds::e_write_file_path outfile) {
            CHECK(ec == std::errc::no_such_file_or_directory);
            CHECK(infile.value == (tdir.path() / "noexist.txt"));
            CHECK(outfile.value == (tdir.path() / "doesn't matter.gz"));
        },
        [](const boost::leaf::verbose_diagnostic_info& info) {
            FAIL("Unexpected failure: " << info);
        });
}

TEST_CASE("Decompress a non-compressed file") {
    auto tdir = dds::temporary_dir::create();
    dds::fs::create_directories(tdir.path());
    boost::leaf::try_handle_all(
        [&]() -> dds::result<void> {
            BOOST_LEAF_CHECK(dds::decompress_file_gz(__FILE__, tdir.path() / "dummy.txt"));
            FAIL("Decompression should have failed");
            return {};
        },
        [&](dds::e_decompress_error, dds::e_read_file_path infile) {
            CHECK(infile.value == __FILE__);
        },
        [](const boost::leaf::verbose_diagnostic_info& info) {
            FAIL("Unexpected failure: " << info);
        });
}