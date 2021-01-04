from pathlib import Path
import gzip

ROOT = Path(__file__).absolute().parent.parent
c2_header = ROOT / 'res/catch2.hpp'

buf = c2_header.read_bytes()
compr = gzip.compress(buf, compresslevel=9)
chars = ', '.join(f"'\\x{b:02x}'" for b in compr)


def oct_encode_one(b: int) -> str:
    if b >= 33 and b <= 126:
        c = chr(b)
        if c in ('"', '\\'):
            return '\\' + c
        return c
    else:
        return f'\\{oct(b)[2:]:>03}'


def oct_encode(b: bytes) -> str:
    return ''.join(oct_encode_one(byt) for byt in b)


bufs = []
while compr:
    head = compr[:2000]
    compr = compr[len(head):]
    octl = oct_encode(head)
    bufs.append(f'"{octl}"_buf')

bufs_arr = ',\n    '.join(bufs)

c2_embedded = ROOT / 'src/dds/catch2_embedded.generated.cpp'
c2_embedded.write_text(f'''
#include "./catch2_embedded.hpp"

#include <neo/gzip_io.hpp>
#include <neo/string_io.hpp>

using namespace neo::literals;

namespace dds::detail {{

static const neo::const_buffer catch2_gzip_bufs[] = {{
    {bufs_arr}
}};

}}

std::string_view dds::detail::catch2_embedded_single_header_str() noexcept {{
    static const std::string decompressed = [] {{
        neo::string_dynbuf_io str;
        neo::gzip_decompress(str, catch2_gzip_bufs);
        str.shrink_uncommitted();
        return std::move(str.string());
    }}();
    return decompressed;
}}
''')
