""" @brief  Common functions and data structures. """

import logging
from shlex import split
from subprocess import CompletedProcess, run
from pathlib import Path

import numpy as np

TRACE_DTYPES: dict[str, np.dtype] = {
    "Kia": np.dtype(
        [
            ("timestamp", np.uint64),
            ("command", np.uint8),
            ("key", np.uint64),
            ("size", np.uint32),
            ("ttl", np.uint32),
        ],
    ),
    # As per https://github.com/SariSultan/TTLsMatter-EuroSys24
    "Sari": np.dtype(
        [
            ("timestamp", np.uint32),
            ("key", np.uint64),
            ("size", np.uint32),
            ("eviction_time", np.uint32),
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

########################################################################
# COMMON FUNCTIONS
########################################################################


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


def abspath(path: str | Path) -> Path:
    """Create an absolute path."""
    return Path(path).resolve()


def sh(cmd: str, **kwargs) -> CompletedProcess:
    """
    Automatically run nohup on every script. This is because I have
    a bad habit of killing my scripts on hangup.

    @note   I will echo the commands to terminal, so don't stick your
            password in commands, obviously!
    """
    my_cmd = f"nohup {cmd}"
    print(f"Running: '{my_cmd}'")
    r = run(split(my_cmd), capture_output=True, text=True, **kwargs)
    if r.returncode != 0:
        print(
            "=============================================================================="
        )
        print(f"Return code: {r.returncode}")
        print(
            "----------------------------------- stderr -----------------------------------"
        )
        print(r.stderr)
        print(
            "----------------------------------- stdout -----------------------------------"
        )
        print(r.stdout)
        print(
            "=============================================================================="
        )
    return r


def practice_sh(cmd: str, **kwargs) -> CompletedProcess:
    # Adding the '--help' flag should call the executable but return
    # very quickly!
    return sh(cmd=f"{cmd} --help", **kwargs)
