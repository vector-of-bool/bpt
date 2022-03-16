from pathlib import PurePath
from os import PathLike
from typing import Union, MutableSequence, MutableMapping

#: A path, string, or convertible-to-Path object
Pathish = Union[PurePath, str]

JSONishValue = Union[None, float, str, bool, MutableSequence['JSONishValue'], MutableMapping[str, 'JSONishValue']]
JSONishDict = MutableMapping[str, JSONishValue]
JSONishArray = MutableSequence[JSONishValue]
