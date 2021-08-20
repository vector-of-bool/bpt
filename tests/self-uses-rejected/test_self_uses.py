from dds_ci.testing import Project, error


def test_self_referential_uses_fails(tmp_project: Project) -> None:
    tmp_project.write('src/a.cpp', 'int answer() { return 42; }')
    tmp_project.package_json = {'name': 'mylib',
                                'namespace': 'mylib', 'version': '0.1.0'}
    tmp_project.library_json = {'name': 'mylib', 'uses': ['mylib/mylib']}

    with error.expect_error_marker_re(r'library-json-cyclic-dependency'):
        tmp_project.build()


def test_self_referential_uses_cycle_fails(tmp_project: Project) -> None:
    tmp_project.package_json = {'name': 'mylib',
                                'namespace': 'mylib', 'version': '0.1.0'}
    liba = tmp_project.lib('liba')
    liba.write('src/a.cpp', 'int answera() { return 42; }')
    liba.library_json = {'name': 'liba', 'uses': ['mylib/libb']}

    libb = tmp_project.lib('libb')
    libb.write('src/b.cpp', 'int answerb() { return 24; }')
    libb.library_json = {'name': 'libb', 'uses': ['mylib/liba']}

    with error.expect_error_marker_re(r'library-json-cyclic-dependency'):
        tmp_project.build()


def test_self_referential_uses_for_libs_fails(tmp_project: Project) -> None:
    tmp_project.package_json = {'name': 'mylib',
                                'namespace': 'mylib', 'version': '0.1.0'}
    lib = tmp_project.lib('liba')
    lib.write('src/a.cpp', 'int answer() { return 42; }')
    lib.library_json = {'name': 'liba', 'uses': ['mylib/liba']}

    with error.expect_error_marker_re(r'library-json-cyclic-dependency'):
        tmp_project.build()
