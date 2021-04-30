import pytest
import subprocess

from dds_ci.testing import ProjectOpener


def test_dependent_src_header_fails(project_opener: ProjectOpener, capfd) -> None:
    proj = project_opener.open('bad_proj')
    with pytest.raises(subprocess.CalledProcessError):
        proj.build()
    out = capfd.readouterr()
    errmsg = out.out + '\n' + out.err

    assert 'Syntax check failed' in errmsg
    assert 'bad_src.hpp' in errmsg


def test_dependent_include_header_fails(project_opener: ProjectOpener, capfd) -> None:
    proj = project_opener.open('bad_proj_include')
    with pytest.raises(subprocess.CalledProcessError):
        proj.build()
    out = capfd.readouterr()
    errmsg = out.out + '\n' + out.err

    assert 'Syntax check failed' in errmsg
    assert 'depends_src.hpp' in errmsg
