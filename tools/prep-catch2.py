from pathlib import Path

ROOT = Path(__file__).absolute().parent.parent
c2_header = ROOT / 'res/catch2.hpp'

buf = c2_header.read_bytes()

chars = ', '.join(f"'\\x{b:02x}'" for b in buf)

c2_embedded = ROOT / 'src/dds/catch2_embeddead_header.cpp'
c2_embedded.write_text(f'''
#include "./catch2_embedded.hpp"

namespace dds::detail {{

static const char bytes[] = {{
    {chars}, '\\x00'
}};
const char* const catch2_embedded_single_header_str = bytes;

}}
''')