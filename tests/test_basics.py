from subprocess import CalledProcessError
import time

import pytest

from dds_ci import paths
from dds_ci.testing import Project, PackageJSON


def test_build_empty(tmp_project: Project) -> None:
    """Check that dds is okay with building an empty project directory"""
    tmp_project.build()


def test_lib_with_app_only(tmp_project: Project) -> None:
    """Test that dds can build a simple application"""
    tmp_project.write('src/foo.main.cpp', r'int main() {}')
    tmp_project.build()
    assert (tmp_project.build_root / f'foo{paths.EXE_SUFFIX}').is_file()


def test_build_simple(tmp_project: Project) -> None:
    """
    Test that dds can build a simple library, and handles rebuilds correctly.
    """
    # Build a bad project
    tmp_project.write('src/f.cpp', 'syntax error')
    with pytest.raises(CalledProcessError):
        tmp_project.build()
    # Now we can build:
    tmp_project.write('src/f.cpp', r'void f() {}')
    tmp_project.build()
    # Writing again will build again:
    time.sleep(0.2)  # Sleep long enough to register a file change
    tmp_project.write('src/f.cpp', r'bad again')
    with pytest.raises(CalledProcessError):
        tmp_project.build()


def test_simple_lib(tmp_project: Project) -> None:
    """
    Test that dds can build a simple library withsome actual content, and that
    the manifest files will affect the output name.
    """
    tmp_project.write('src/foo.cpp', 'int the_answer() { return 42; }')
    tmp_project.package_json = {
        'name': 'TestProject',
        'version': '0.0.0',
        'namespace': 'test',
    }
    tmp_project.library_json = {'name': 'TestLibrary'}
    tmp_project.build()
    assert (tmp_project.build_root / 'compile_commands.json').is_file()
    assert list(tmp_project.build_root.glob('libTestLibrary.*')) != []


def test_lib_with_just_test(tmp_project: Project) -> None:
    tmp_project.write('src/foo.test.cpp', 'int main() {}')
    tmp_project.build()
    assert tmp_project.build_root.joinpath(f'test/foo{paths.EXE_SUFFIX}').is_file()


TEST_PACKAGE: PackageJSON = {
    'name': 'test-pkg',
    'version': '0.2.2',
    'namespace': 'test',
}


def test_empty_with_pkg_dds(tmp_project: Project) -> None:
    tmp_project.package_json = TEST_PACKAGE
    tmp_project.build()


def test_empty_with_lib_dds(tmp_project: Project) -> None:
    tmp_project.package_json = TEST_PACKAGE
    tmp_project.build()


def test_empty_sdist_create(tmp_project: Project) -> None:
    tmp_project.package_json = TEST_PACKAGE
    tmp_project.sdist_create()


def test_empty_sdist_export(tmp_project: Project) -> None:
    tmp_project.package_json = TEST_PACKAGE
    tmp_project.sdist_export()
