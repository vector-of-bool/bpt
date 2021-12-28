from dds_ci.testing import Project, error


def test_self_referential_uses_fails(tmp_project: Project) -> None:
    tmp_project.write('src/a.cpp', 'int answer() { return 42; }')
    tmp_project.project_json = {
        'name': 'mylib',
        'namespace': 'mylib',
        'version': '0.1.0',
        'lib': {
            'name': 'mine',
            'uses': ['mine']
        },
    }

    with error.expect_error_marker_re(r'library-json-cyclic-dependency'):
        tmp_project.build()


def test_self_referential_uses_cycle_fails(tmp_project: Project) -> None:
    tmp_project.project_json = {
        'name':
        'mylib',
        'version':
        '0.1.0',
        'libs': [{
            'name': 'liba',
            'path': 'libs/liba',
            'uses': ['libb'],
        }, {
            'name': 'libb',
            'path': 'libs/libb',
            'uses': ['liba'],
        }],
    }
    liba = tmp_project.lib('liba')
    liba.write('src/a.cpp', 'int answera() { return 42; }')

    libb = tmp_project.lib('libb')
    libb.write('src/b.cpp', 'int answerb() { return 24; }')

    with error.expect_error_marker_re(r'library-json-cyclic-dependency'):
        tmp_project.build()


def test_self_referential_uses_for_libs_fails(tmp_project: Project) -> None:
    tmp_project.project_json = {
        'name': 'mylib',
        'version': '0.1.0',
        'libs': [{
            'name': 'liba',
            'path': 'libs/liba',
            'uses': ['liba'],
        }]
    }
    lib = tmp_project.lib('liba')
    lib.write('src/a.cpp', 'int answer() { return 42; }')

    with error.expect_error_marker_re(r'library-json-cyclic-dependency'):
        tmp_project.build()
