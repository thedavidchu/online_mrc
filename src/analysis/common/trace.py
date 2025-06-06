"""@brief  Code related to reading the binary cache traces."""

import logging
from pathlib import Path
from string import Template
from warnings import warn

import numpy as np

TRACE_DTYPE: dict[str, np.dtype] = {
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
    "YangTwitterX": np.dtype(
        [
            ("timestamp_ms", np.uint32),
            ("key", np.uint64),
            # Upper 10 bits: key size [bytes]
            # Lower 22 bits: value size [bytes]
            ("key_value_size", np.uint32),
            # Upper 8 bits: op
            # Lower 24 bits: TTL [s]
            ("op_ttl_s", np.uint32),
            ("client_id", np.uint32),
        ]
    ),
    "TimeSizeTTL": np.dtype(
        [
            ("timestamp_ms", np.uint64),
            ("size_b", np.uint32),
            ("ttl_ms", np.float64),
        ],
    ),
}

TRACE_DTYPE_KEY = list(TRACE_DTYPE.keys())

KIA_OP_NAMES = {
    0: "GET",
    1: "SET",
}

YANG_OP_NAMES = {
    0: "NOP",
    1: "GET",
    2: "GETS",
    3: "SET",
    4: "ADD",
    5: "CAS",
    6: "REPLACE",
    7: "APPEND",
    8: "PREPEND",
    9: "DELETE",
    10: "INCR",
    11: "DECR",
    12: "READ",
    13: "WRITE",
    14: "UPDATE",
    255: "INVALID",
}


def convert_to_time_size_ttl(
    src: np.ndarray, format: str, dst: np.ndarray, process_ttl: bool = False
):
    """Convert other formats to the TimeSizeTTL format for analysis.

    NOTE    I thought there was a major performance difference (3x)
            between processing the TTL versus not. Initially, I measured
            it to be around 25 s vs 7 s. However, I don't notice the
            difference anymore. I'm leaving the parameter in though, not
            just to pollute my code base with dead code. TBH processing
            the data is useless because the Numpy histogram doesn't
            support NAN, so I need to filter out the NANs anyways.
    """
    match format:
        case "Sari":
            dst[:]["timestamp_ms"] = 1000 * src[:]["timestamp_s"]
            dst[:]["size_b"] = src[:]["size_b"]
            dst[:]["ttl_ms"] = 1000 * src[:]["ttl_s"]
        case "Kia":
            dst[:]["timestamp_ms"] = src[:]["timestamp_ms"]
            dst[:]["size_b"] = src[:]["size_b"]
            if not process_ttl:
                dst[:]["ttl_ms"] = 1000 * src[:]["ttl_s"]
            else:
                dst[:]["ttl_ms"] = np.where(
                    src[:]["command"] == 0,
                    np.nan,
                    np.where(src[:]["ttl_s"] != 0, 1000 * src[:]["ttl_s"], np.inf),
                )
        case "YangTwitterX":
            dst[:]["timestamp_ms"] = src[:]["timestamp_ms"]
            dst[:]["size_b"] = (src[:]["key_value_size"] >> 22) + (
                src[:]["key_value_size"] & 0x003F_FFFF
            )
            if not process_ttl:
                dst[:]["ttl_ms"] = 1000 * (src[:]["op_ttl_s"] & 0x00FF_FFFF)
            else:
                dst[:]["ttl_ms"] = np.where(
                    (src[:]["op_ttl_s"] >> 24) not in range(3, 255),
                    np.nan,
                    np.where(
                        src[:]["op_ttl_s"] & 0x00FF_FFFF != 0,
                        1000 * (src[:]["op_ttl_s"] & 0x00FF_FFFF),
                        np.inf,
                    ),
                )
        case "TimeSizeTTL":
            dst[:] = src[:]
        case _:
            raise ValueError(f"unrecognized format '{format}'")


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
    assert format in TRACE_DTYPE.keys()
    logger.info(f"Reading from {str(path)} with {format}'s format")
    logger.info(f"Input path size: {path.stat().st_size}")
    logger.info(f"Format size: {TRACE_DTYPE[format].itemsize}")
    if mode:
        data = np.memmap(path, dtype=TRACE_DTYPE[format], mode=mode)
    else:
        data = np.fromfile(path, dtype=TRACE_DTYPE[format])
    return data


def get_twitter_path(
    nr: int, format: str, template: Template | None = None
) -> Path | None:
    """
    @brief   Get the Twitter path from Michael's Intel 2023 server.
    @param template:    None => use default; otherwise, replace '$cluster'
                        with the 'nr' variable.
    """
    if template is not None:
        path = Path(template.substitute(cluster=nr)).resolve()
        if path.exists():
            return path
        warn(f"file '{str(path)}' DNE")
        return None
    match format:
        case "Kia":
            # We try to use disk2-8tb because it is faster.
            path = Path(f"/mnt/disk2-8tb/kia/twitter/cluster{nr}.bin")
            if path.exists():
                return path
            path = Path(f"/mnt/disk1-20tb/kia/twitter/cluster{nr}.bin")
            if path.exists():
                return path
            warn(f"file '{str(path)}' DNE")
            return None
        case "Sari":
            path = Path(
                f"/mnt/disk1-20tb/Sari-Traces/FilteredTwitter_Binary/cluster{nr}.bin"
            )
            if path.exists():
                return path
            warn(f"file '{str(path)}' DNE")
            return None
        case _:
            warn(f"unsupported format '{format}'")
            return None
