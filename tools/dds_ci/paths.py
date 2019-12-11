import os
from pathlib import Path

TOOLS_DIR = Path(__file__).absolute().parent.parent
PROJECT_ROOT = TOOLS_DIR.parent
BUILD_DIR = PROJECT_ROOT / '_build'
PREBUILT_DIR = PROJECT_ROOT / '_prebuilt'
EXE_SUFFIX = '.exe' if os.name == 'nt' else ''
PREBUILT_DDS = (PREBUILT_DIR / 'dds').with_suffix(EXE_SUFFIX)
CUR_BUILT_DDS = (BUILD_DIR / 'dds').with_suffix(EXE_SUFFIX)
EMBEDDED_REPO_DIR = PROJECT_ROOT / 'external/repo'
SELF_TEST_REPO_DIR = BUILD_DIR / '_self-repo'
