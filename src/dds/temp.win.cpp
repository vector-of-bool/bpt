#ifdef _WIN32
#include "./temp.hpp"

#include <memory>

#include <rpc.h>

#include <cassert>

using namespace dds;

temporary_dir temporary_dir::create() {
    auto base = fs::temp_directory_path();

    ::UUID uuid;
    auto   status = ::UuidCreate(&uuid);
    assert(status == RPC_S_OK || status == RPC_S_UUID_LOCAL_ONLY
           || status == RPC_S_UUID_NO_ADDRESS);

    RPC_CSTR uuid_str;
    ::UuidToStringA(&uuid, &uuid_str);
    std::string uuid_std_str(reinterpret_cast<const char*>(uuid_str));
    ::RpcStringFree(&uuid_str);

    auto            new_dir = base / ("dds-" + uuid_std_str);
    std::error_code ec;
    fs::create_directory(new_dir);
    return std::make_shared<impl>(std::move(new_dir));
}
#endif