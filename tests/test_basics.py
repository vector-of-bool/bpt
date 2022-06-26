from pathlib import Path
import time
from subprocess import CalledProcessError

import pytest
from bpt_ci import paths
from bpt_ci.bpt import BPTWrapper
from bpt_ci.testing import Project, ProjectYAML
from bpt_ci.testing.error import expect_error_marker
from bpt_ci.testing.fixtures import ProjectOpener
from bpt_ci.testing.fs import render_into


def test_build_empty(tmp_project: Project) -> None:
    """Check that bpt is okay with building an empty project directory"""
    tmp_project.build()


def test_lib_with_app_only(tmp_project: Project) -> None:
    """Test that bpt can build a simple application"""
    tmp_project.write('src/foo.main.cpp', r'int main() {}')
    tmp_project.build()
    assert (tmp_project.build_root / f'foo{paths.EXE_SUFFIX}').is_file()


def test_build_simple(tmp_project: Project) -> None:
    """
    Test that bpt can build a simple library, and handles rebuilds correctly.
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
    Test that bpt can build a simple library withsome actual content, and that
    the manifest files will affect the output name.
    """
    tmp_project.write('src/foo.cpp', 'int the_answer() { return 42; }')
    tmp_project.bpt_yaml = {
        'name': 'test-project',
        'version': '0.0.0',
    }
    tmp_project.build()
    assert (tmp_project.build_root / 'compile_commands.json').is_file(), 'compdb was not created'
    assert list(tmp_project.build_root.glob('libtest-project.*')) != [], 'No archive was created'


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
    tmp_project.bpt_yaml = {'name': 'test', 'version': '1.2.3', 'dependencies': [{'dep': 'invalid name@1.2.3'}]}
    with expect_error_marker('invalid-pkg-dep-name'):
        tmp_project.build()
    with expect_error_marker('invalid-pkg-dep-name'):
        tmp_project.pkg_create()
    with expect_error_marker('invalid-dep-shorthand'):
        tmp_project.bpt.build_deps(['invalid name@1.2.3'])

    tmp_project.bpt_yaml['name'] = 'invalid name'
    tmp_project.bpt_yaml['dependencies'] = []
    with expect_error_marker('invalid-name'):
        tmp_project.build()
    with expect_error_marker('invalid-name'):
        tmp_project.pkg_create()


TEST_PACKAGE: ProjectYAML = {
    'name': 'test-pkg',
    'version': '0.2.2',
}


def test_empty_with_pkg_json(tmp_project: Project) -> None:
    tmp_project.bpt_yaml = TEST_PACKAGE
    tmp_project.build()


def test_empty_sdist_create(tmp_project: Project) -> None:
    tmp_project.bpt_yaml = TEST_PACKAGE
    tmp_project.pkg_create()
    assert tmp_project.build_root.joinpath('test-pkg@0.2.2~1.tar.gz').is_file(), \
        'The expected sdist tarball was not generated'


def test_project_with_meta(tmp_project: Project) -> None:
    tmp_project.bpt_yaml = {
        'name': 'foo',
        'version': '1.2.3',
        'license': 'MIT-1.2.3',
    }
    with expect_error_marker('invalid-spdx'):
        tmp_project.build()
    tmp_project.bpt_yaml = {
        'name': 'foo',
        'version': '1.2.3',
        'license': 'MIT',  # A valid license string
    }
    tmp_project.build()


def test_link_interdep(project_opener: ProjectOpener) -> None:
    proj = project_opener.render(
        'test-interdep', {
            'foo': {
                'src': {
                    'foo.test.cpp':
                    r'''
                    #include <stdexcept>

                    extern int bar_func();

                    int main() {
                        if (bar_func() != 1729) {
                            std::terminate();
                        }
                    }
                    '''
                },
            },
            'bar': {
                'src': {
                    'bar.cpp':
                    r'''
                    int bar_func() {
                        return 1729;
                    }
                    ''',
                },
            },
        })
    proj.bpt_yaml = {
        'name': 'test',
        'version': '1.2.3',
        'libraries': [{
            'path': 'foo',
            'name': 'foo',
        }, {
            'path': 'bar',
            'name': 'bar',
        }]
    }
    with expect_error_marker('link-failed'):
        proj.build()

    # Update with a 'using' of the library that was required:
    proj.bpt_yaml['libraries'][0]['using'] = ['bar']
    print(proj.bpt_yaml)
    # Build is okay now
    proj.build()


def test_build_other_dir(project_opener: ProjectOpener, tmp_path: Path, bpt: BPTWrapper):
    render_into(
        tmp_path, {
            'build-other-dir': {
                'src': {
                    'foo/': {
                        'foo.hpp':
                        r'''
                        extern int get_value();
                        ''',
                        'foo.cpp':
                        r'''
                        #include <foo/foo.hpp>
                        int get_value() { return 42; }
                        ''',
                        'foo.test.cpp':
                        r'''
                        #include <foo/foo.hpp>
                        int main() {
                            return get_value() != 42;
                        }
                        '''
                    }
                }
            }
        })
    proj = Project(Path('./build-other-dir/'), bpt)
    proj.build(cwd=tmp_path)


def test_build_with_explicit_libs_ignores_default_lib(project_opener: ProjectOpener, tmp_path: Path, bpt: BPTWrapper):
    render_into(
        tmp_path, {
            'src': {
                'bad.cpp': r'''
                #error This file is invalid
                '''
            },
            'okay-lib': {
                'src': {
                    'okay.cpp': r'''
                    // This file is okay
                    '''
                }
            }
        })
    proj = Project(tmp_path, bpt)
    with expect_error_marker('compile-failed'):
        proj.build()

    proj.bpt_yaml = {
        'name': 'mine',
        'version': '1.2.3',
    }
    with expect_error_marker('compile-failed'):
        proj.build()

    # Setting an explicit library list does not generate the default library
    proj.bpt_yaml['libraries'] = [{
        'name': 'something',
        'path': 'okay-lib',
    }]
    proj.build()


def test_new_then_build(bpt: BPTWrapper, tmp_path: Path) -> None:
    bpt.run(['new', 'test-project', '--dir', tmp_path, '--split-src-include=no'])
    proj = Project(tmp_path, bpt)
    proj.build()
