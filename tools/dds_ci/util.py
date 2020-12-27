from pathlib import PurePath
from os import PathLike
from typing import Union

#: A path, string, or convertible-to-Path object
Pathish = Union[PathLike, PurePath, str]
