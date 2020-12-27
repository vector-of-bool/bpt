from pathlib import Path
import platform

import pytest

from dds_ci.testing import RepoFixture, Project
from dds_ci import proc, toolchain, paths

CRYPTOPP_JSON = {
    "packages": {
        "cryptopp": {
            "8.2.0": {
                "remote": {
                    "git": {
                        "url": "https://github.com/weidai11/cryptopp.git",
                        "ref": "CRYPTOPP_8_2_0"
                    },
                    "auto-lib": "cryptopp/cryptopp",
                    "transform": [{
                        "move": {
                            "from": ".",
                            "to": "src/cryptopp",
                            "include": ["*.c", "*.cpp", "*.h"]
                        }
                    }]
                }
            }
        }
    }
}

APP_CPP = r'''
#include <cryptopp/osrng.h>

#include <string>

int main() {
    std::string arr;
    arr.resize(256);
    CryptoPP::OS_GenerateRandomBlock(false,
                                     reinterpret_cast<CryptoPP::byte*>(arr.data()),
                                     arr.size());
    for (auto b : arr) {
        if (b != '\x00') {
            return 0;
        }
    }
    return 1;
}
'''


@pytest.mark.skipif(platform.system() == 'FreeBSD', reason='This one has trouble running on FreeBSD')
def test_get_build_use_cryptopp(test_parent_dir: Path, tmp_project: Project, http_repo: RepoFixture) -> None:
    http_repo.import_json_data(CRYPTOPP_JSON)
    tmp_project.dds.repo_add(http_repo.url)
    tmp_project.package_json = {
        'name': 'usr-cryptopp',
        'version': '1.0.0',
        'namespace': 'test',
        'depends': ['cryptopp@8.2.0'],
    }
    tmp_project.library_json = {
        'name': 'use-cryptopp',
        'uses': ['cryptopp/cryptopp'],
    }
    tc_fname = 'gcc.tc.jsonc' if 'gcc' in toolchain.get_default_test_toolchain().name else 'msvc.tc.jsonc'
    tmp_project.write('src/use-cryptopp.main.cpp', APP_CPP)
    tmp_project.build(toolchain=test_parent_dir / tc_fname)
    proc.check_run([(tmp_project.build_root / 'use-cryptopp').with_suffix(paths.EXE_SUFFIX)])
