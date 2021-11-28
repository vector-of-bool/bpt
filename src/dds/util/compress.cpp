#include "./compress.hpp"

#include <dds/error/on_error.hpp>
#include <dds/error/result.hpp>
#include <dds/util/fs/io.hpp>

#include <neo/gzip_io.hpp>
#include <neo/io/stream/buffers.hpp>
#include <neo/io/stream/file.hpp>

using namespace dds;

result<void> dds::compress_file_gz(fs::path in_path, fs::path out_path) noexcept {
    DDS_E_SCOPE(e_read_file_path{in_path});
    DDS_E_SCOPE(e_write_file_path{out_path});
    std::error_code ec;

    auto in_file = neo::file_stream::open(in_path, neo::open_mode::read, ec);
    if (ec) {
        return boost::leaf::new_error(ec,
                                      e_compress_error{"Failed to open input file for reading"});
    }

    auto out_file = neo::file_stream::open(out_path, neo::open_mode::write, ec);
    if (ec) {
        return boost::leaf::new_error(ec,
                                      e_compress_error{"Failed to open output file for writing"});
    }

    try {
        neo::gzip_compress(neo::stream_io_buffers{*out_file}, neo::stream_io_buffers{*in_file});
    } catch (const std::system_error& e) {
        return boost::leaf::new_error(e.code(), e_compress_error{e.what()});
    } catch (const std::runtime_error& e) {
        return boost::leaf::new_error(e_compress_error{e.what()});
    }

    return {};
}

result<void> dds::decompress_file_gz(fs::path gz_path, fs::path plain_path) noexcept {
    DDS_E_SCOPE(e_read_file_path{gz_path});
    DDS_E_SCOPE(e_write_file_path{plain_path});

    std::error_code ec;
    auto            in_file = neo::file_stream::open(gz_path, neo::open_mode::read, ec);
    if (ec) {
        return boost::leaf::new_error(ec,
                                      e_decompress_error{"Failed to open input file for reading"});
    }

    auto out_file = neo::file_stream::open(plain_path, neo::open_mode::write, ec);
    if (ec) {
        return boost::leaf::new_error(ec,
                                      e_decompress_error{"Failed to open output file for writing"});
    }

    try {
        neo::gzip_decompress(neo::stream_io_buffers{*out_file}, neo::stream_io_buffers{*in_file});
    } catch (const std::system_error& e) {
        return boost::leaf::new_error(e.code(), e_decompress_error{e.what()});
    } catch (const std::runtime_error& e) {
        return boost::leaf::new_error(e_decompress_error{e.what()});
    }

    return {};
}