from dds_ci.testing.fixtures import ProjectOpener
from dds_ci import paths, proc


def test_lib_with_tweaks(project_opener: ProjectOpener) -> None:
    pr = project_opener.open('projects/tweaks')
    pr.build()
    app = pr.build_root / ('tweakable' + paths.EXE_SUFFIX)
    res = proc.run([app])
    # The default value is 99:
    assert res.returncode == 99
    # Build again, but with an empty/non-existent tweaks directory
    pr.build(tweaks_dir=pr.root / 'conf')
    res = proc.run([app])
    assert res.returncode == 99
    # Now write a tweaks header and rebuild:
    pr.write(
        'conf/tweakable.tweaks.hpp', r'''
    #pragma once

    namespace tweakable {
        namespace config {
            const int value = 41;
        }
    }
    ''')
    pr.build(tweaks_dir=pr.root / 'conf')
    res = proc.run([app])
    assert res.returncode == 41
