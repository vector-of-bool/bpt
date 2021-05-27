from subprocess import CalledProcessError
import time

import pytest

from dds_ci import paths
from dds_ci.testing.error import expect_error_marker
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
    time.sleep(1)  # Sleep long enough to register a file change
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
        'name': 'test-project',
        'version': '0.0.0',
        'namespace': 'test',
    }
    tmp_project.library_json = {'name': 'test-library'}
    tmp_project.build()
    assert (tmp_project.build_root / 'compile_commands.json').is_file(), 'compdb was not created'
    assert list(tmp_project.build_root.glob('libtest-library.*')) != [], 'No archive was created'


def test_lib_with_just_test(tmp_project: Project) -> None:
    tmp_project.write('src/foo.test.cpp', 'int main() {}')
    tmp_project.build()
    assert tmp_project.build_root.joinpath(f'test/foo{paths.EXE_SUFFIX}').is_file()


def test_lib_with_failing_test(tmp_project: Project) -> None:
    tmp_project.write('src/foo.test.cpp', 'int main() { return 2; }')
    with expect_error_marker('build-failed-test-failed'):
        tmp_project.build()


def test_error_enoent_toolchain(tmp_project: Project) -> None:
    with expect_error_marker('bad-toolchain'):
        tmp_project.build(toolchain='no-such-file', fixup_toolchain=False)


def test_invalid_names(tmp_project: Project) -> None:
    tmp_project.package_json = {
        'name': 'test',
        'version': '1.2.3',
        'namespace': 'test',
        'depends': ['invalid name@1.2.3']
    }
    with expect_error_marker('invalid-pkg-dep-name'):
        tmp_project.build()
    with expect_error_marker('invalid-pkg-dep-name'):
        tmp_project.pkg_create()
    with expect_error_marker('invalid-pkg-dep-name'):
        tmp_project.dds.build_deps(['invalid name@1.2.3'])

    tmp_project.package_json = {
        **tmp_project.package_json,
        'name': 'invalid name',
        'depends': [],
    }
    with expect_error_marker('invalid-pkg-name'):
        tmp_project.build()
    with expect_error_marker('invalid-pkg-name'):
        tmp_project.pkg_create()

    tmp_project.package_json = {
        **tmp_project.package_json,
        'name': 'simple_name',
        'namespace': 'invalid namespace',
    }
    with expect_error_marker('invalid-pkg-namespace-name'):
        tmp_project.build()
    with expect_error_marker('invalid-pkg-namespace-name'):
        tmp_project.pkg_create()

    tmp_project.package_json = {
        'name': 'test',
        'version': '1.2.3',
        'namespace': 'test',
        'depends': [],
    }
    tmp_project.library_json = {'name': 'invalid name'}
    # Need a source directory for dds to load the lib manifest
    tmp_project.write('src/empty.hpp', '')
    with expect_error_marker('invalid-lib-name'):
        tmp_project.build()


TEST_PACKAGE: PackageJSON = {
    'name': 'test-pkg',
    'version': '0.2.2',
    'namespace': 'test',
}


def test_empty_with_pkg_json(tmp_project: Project) -> None:
    tmp_project.package_json = TEST_PACKAGE
    tmp_project.build()


def test_empty_sdist_create(tmp_project: Project) -> None:
    tmp_project.package_json = TEST_PACKAGE
    tmp_project.pkg_create()
    assert tmp_project.build_root.joinpath('test-pkg@0.2.2.tar.gz').is_file(), \
        'The expected sdist tarball was not generated'


def test_new_format(tmp_project: Project)->None:
    tmp_project.package_json = {
        'name': 'test-pkg',
        'version': '1.2.3',
        'namespace': 'testing',
        'libraries': {
            '.': {
                'name': 'test',
                'uses': ['foo/bar']
            }
        }
    }
    tmp_project.pkg_create()
