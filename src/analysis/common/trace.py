""" @brief  Code related to reading the binary cache traces. """

import logging
from pathlib import Path

import numpy as np

TRACE_DTYPES: dict[str, np.dtype] = {
    "Kia": np.dtype(
        [
            ("timestamp_ms", np.uint64),
            ("command", np.uint8),
            ("key", np.uint64),
            ("size_b", np.uint32),
            ("ttl_s", np.uint32),
        ],
    ),
    # As per https://github.com/SariSultan/TTLsMatter-EuroSys24
    # NOTE  On the traces I've found on Michael Stumm's servers, Sari
    #       does not store the eviction time, but rather the TTL.
    "Sari": np.dtype(
        [
            ("timestamp_s", np.uint32),
            ("key", np.uint64),
            ("size_b", np.uint32),
            ("ttl_s", np.uint32),
        ]
    ),
}

# NOTE  Default date format is: datefmt=r"%Y-%m-%d %H:%M:%S"
#       according to https://docs.python.org/3/howto/logging.html#formatters.
#       However, there is some number (maybe milliseconds) tacked
#       onto the end. That's why I explicitly specify it.
logging.basicConfig(
    format="[%(levelname)s] [%(asctime)s] [ %(pathname)s:%(lineno)d ] [errno 0: Success] %(message)s",
    level=logging.INFO,
    datefmt=r"%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger(__name__)


def read_trace(path: Path, format: str, mode: str | None):
    """
    @param mode: if None, then load into memory. Otherwise, use it as the mode."""
    assert format in TRACE_DTYPES.keys()
    logger.info(f"Reading from {str(path)} with {format}'s format")
    if mode:
        data = np.memmap(path, dtype=TRACE_DTYPES[format], mode=mode)
    else:
        data = np.fromfile(path, dtype=TRACE_DTYPES[format])
    return data
