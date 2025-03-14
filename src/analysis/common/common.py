"""@brief  Common functions and data structures."""

import logging
from shlex import split
from subprocess import CompletedProcess, run
from pathlib import Path

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


def format_memory_size(size_bytes: float | int) -> str:
    """Format memory sizes."""
    prefices = {
        "B": 1,
        "KiB": 1 << 10,
        "MiB": 1 << 20,
        "GiB": 1 << 30,
        "TiB": 1 << 40,
        "PiB": 1 << 50,
        "EiB": 1 << 60,
    }
    for i, (prefix, multiplier) in enumerate(prefices.items()):
        if size_bytes / multiplier < 1024:
            break
    return f"{size_bytes / multiplier} {prefix}"
