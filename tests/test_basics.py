import time
from subprocess import CalledProcessError

import pytest
from dds_ci import paths
from dds_ci.testing import Project, PkgYAML
from dds_ci.testing.error import expect_error_marker


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
    tmp_project.pkg_yaml = {
        'name': 'test-project',
        'version': '0.0.0',
        'lib': {
            'name': 'test-library',
        }
    }
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
    tmp_project.pkg_yaml = {'name': 'test', 'version': '1.2.3', 'dependencies': [{'dep': 'invalid name@1.2.3'}]}
    with expect_error_marker('invalid-pkg-dep-name'):
        tmp_project.build()
    with expect_error_marker('invalid-pkg-dep-name'):
        tmp_project.pkg_create()
    with expect_error_marker('invalid-dep-shorthand'):
        tmp_project.dds.build_deps(['invalid name@1.2.3'])

    tmp_project.pkg_yaml['name'] = 'invalid name'
    tmp_project.pkg_yaml['dependencies'] = []
    with expect_error_marker('invalid-name'):
        tmp_project.build()
    with expect_error_marker('invalid-name'):
        tmp_project.pkg_create()


TEST_PACKAGE: PkgYAML = {
    'name': 'test-pkg',
    'version': '0.2.2',
}


def test_empty_with_pkg_json(tmp_project: Project) -> None:
    tmp_project.pkg_yaml = TEST_PACKAGE
    tmp_project.build()


def test_empty_sdist_create(tmp_project: Project) -> None:
    tmp_project.pkg_yaml = TEST_PACKAGE
    tmp_project.pkg_create()
    assert tmp_project.build_root.joinpath('test-pkg@0.2.2~1.tar.gz').is_file(), \
        'The expected sdist tarball was not generated'


def test_project_with_meta(tmp_project: Project) -> None:
    tmp_project.pkg_yaml = {
        'name': 'foo',
        'version': '1.2.3',
        'license': 'MIT-1.2.3',
    }
    with expect_error_marker('invalid-spdx'):
        tmp_project.build()
    tmp_project.pkg_yaml = {
        'name': 'foo',
        'version': '1.2.3',
        'license': 'MIT',  # A valid license string
    }
    tmp_project.build()
