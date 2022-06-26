from pathlib import PurePath
from typing import Union, MutableSequence, MutableMapping

import tarfile

#: A path, string, or convertible-to-Path object
Pathish = Union[PurePath, str]

JSONishValue = Union[None, float, str, bool, MutableSequence['JSONishValue'], MutableMapping[str, 'JSONishValue']]
JSONishDict = MutableMapping[str, JSONishValue]
JSONishArray = MutableSequence[JSONishValue]


def read_tarfile_member(tarpath: Pathish, member: Pathish) -> bytes:
    with tarfile.open(tarpath, 'r:*') as tf:
        io = tf.extractfile(str(PurePath(member).as_posix()))
        assert io, f'No member of [{tarpath}]: [{member}]'
        return io.read()
