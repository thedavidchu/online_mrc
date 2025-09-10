#!/usr/bin/python3
"""Calculate the accuracy of the prediction mechanism."""
import argparse
from pathlib import Path

import numpy as np
from shapely import Point, LineString

from plot_predictive_cache import (
    parse_data,
    get_stat,
    get_scaled_fixed_data,
    COUNT_SHARDS_ARGS,
    HOURS_NO_SHARDS_ARGS,
    GiB_SHARDS_ARGS,
)

################################################################################
### Difference between miss ratio curves.
################################################################################


def trapezoid_mean_absolute_error(
    a0: Point, a1: Point, b0: Point, b1: Point, absolute: bool
) -> float:
    """
    Find the mean absolute error within a trapezoid (with parallel vertical sides).

    @params a0, a1: represent points of the oracle.
    @params b0, b1: represent points of the output.

    ```
    A1----A1
    |*****|
    |*****B1
    |****/
    |***/
    |**/
    |*/
    B0
    ```
    """
    # We assume it's a trapezoid to simplify the calculations.
    assert a0.x == b0.x and a1.x == b1.x
    assert a0.x <= a1.x
    oracle_ln = LineString([a0, a1])
    output_ln = LineString([b0, b1])
    # If the lines cross over each other, then we need to break the
    # problem into that of two triangles. However, if only the endpoints
    # are equal, then we can solve with the regular trapezoid formula
    # and avoid infinite recursion.
    if oracle_ln.intersects(output_ln) and not a0.equals(b0) and not a1.equals(b1):
        # If there is no intersection, then we get an empty 'LineString'.
        # We know from above that there should be an intersection.
        intersection = oracle_ln.intersection(output_ln)
        if isinstance(intersection, LineString):
            assert not intersection.is_empty()
            return 0.0
        elif isinstance(intersection, Point):
            return trapezoid_mean_absolute_error(
                a0, intersection, b0, intersection, absolute
            ) + trapezoid_mean_absolute_error(
                intersection, a1, intersection, b1, absolute
            )
        else:
            raise ValueError("unrecognized intersection type")
    dx = a1.x - a0.x
    if not absolute:
        return dx * ((a0.y - b0.y) + (a1.y - b1.y)) / 2
    return abs(dx * (abs(a0.y - b0.y) + abs(a1.y - b1.y))) / 2


def mean_absolute_error(
    xs: list[float], oracle_ys: list[float], output_ys: list[float], absolute: bool
) -> float:
    mae = 0.0
    for x0, x1, oracle_y0, oracle_y1, out_y0, out_y1 in zip(
        xs, xs[1:], oracle_ys, oracle_ys[1:], output_ys, output_ys[1:]
    ):
        a, b = Point(x0, oracle_y0), Point(x1, oracle_y1)
        c, d = Point(x0, out_y0), Point(x1, out_y1)
        mae += trapezoid_mean_absolute_error(a, b, c, d, absolute)
    return mae


def trapezoid_mae_test():
    # Test normal case.
    a, b, c, d = Point(0, 0), Point(1, 0), Point(0, 1), Point(1, 1)
    mae = trapezoid_mean_absolute_error(a, b, c, d, absolute=True)
    print(f"{mae=}")
    assert mae == 1.0

    # Test intersection case.
    a, b, c, d = Point(0, 0), Point(1, 1), Point(0, 1), Point(1, 0)
    mae = trapezoid_mean_absolute_error(a, b, c, d, absolute=True)
    print(f"{mae=}")
    assert mae == 0.5

    # Test symmetrical intersection case without absolute.
    mae = trapezoid_mean_absolute_error(a, b, c, d, absolute=False)
    print(f"{mae=}")
    assert mae == 0.0

    # Test identical lines case.
    a, b, c, d = Point(0, 0), Point(1, 0), Point(0, 0), Point(1, 0)
    mae = trapezoid_mean_absolute_error(a, b, c, d, absolute=False)
    print(f"{mae=}")
    assert mae == 0.0

    # Test identical points case.
    a = Point(0, 0)
    mae = trapezoid_mean_absolute_error(*4 * [a], absolute=False)
    print(f"{mae=}")
    assert mae == 0.0


def full_mae_test():
    # Simple test.
    xs = [0.0, 1.0, 2.0, 3.0]
    orc_ys = [1.0, 1.0, 1.0, 1.0]
    out_ys = [0.0, 0.0, 0.0, 0.0]
    mae = mean_absolute_error(xs, orc_ys, out_ys, absolute=False)
    print(f"{mae=}")
    assert mae == 3.0

    # Intersecting test.
    xs = [0.0, 1.0, 2.0, 3.0]
    orc_ys = [1.0, 0.0, 1.0, 0.0]
    out_ys = [0.0, 1.0, 0.0, 1.0]
    mae = mean_absolute_error(xs, orc_ys, out_ys, absolute=True)
    print(f"{mae=}")
    assert mae == 1.5


################################################################################
### Difference between temporal curves.
################################################################################


def mean(*xs: float) -> float | None:
    if not xs:
        return None
    return sum(xs) / len(xs)


def np_median(xs: np.ndarray) -> float | None:
    xs = sorted(xs)
    if len(xs) == 0:
        return None
    if len(xs) % 2 == 0:
        return mean(xs[len(xs) // 2 - 1], xs[len(xs) // 2])
    return xs[len(xs) // 2]


def temporal_error(oracle_ys: list[float], output_ys: list[float]) -> dict[str, float]:
    """
    Calculate the piecewise absolute error, the mean, the median, and the maximum.

    @note We assume that all times are equally spaced.
    """
    a, b = np.array(oracle_ys), np.array(output_ys)
    assert a.shape == b.shape
    # These are errors where we use MORE memory than the oracle.
    # The fairness is whether we allow memory savings relative to the
    # oracle to offset memory losses relative to the oracle.
    fair_ex_errs = b - a
    relu_ex_errs = np.where(a < b, b - a, 0)
    # Absolute error doesn't reflect the fact that it's actually good
    # if we use less memory than the oracle.
    abs_errs = np.abs(a - b)
    abs_err_ratios = abs_errs / np.max(np.concatenate([a, b]))

    return dict(
        # Errors where output memory usage is in excess of the oracle's.
        # We allow for offsetting instances of greater memory usage with
        # instances of less memory usage than the oracle.
        fair_excess_errors=fair_ex_errs,
        mean_fair_excess_error=np.mean(fair_ex_errs),
        median_fair_excess_error=np_median(fair_ex_errs),
        max_fair_excess_error=np.max(fair_ex_errs),
        # Errors where output memory usage is in excess of the oracle's.
        # In situtations where the output uses less memory than the
        # oracle, we do not let that offset situations where we use more.
        relu_excess_errors=relu_ex_errs,
        mean_relu_excess_error=np.mean(relu_ex_errs),
        median_relu_excess_error=np_median(relu_ex_errs),
        max_relu_excess_error=np.max(relu_ex_errs),
        # Absolute errors.
        absolute_errors=abs_errs,
        mean_absolute_error=np.mean(abs_errs),
        median_absolute_error=np_median(abs_errs),
        max_absolute_error=np.max(abs_errs),
        # Ratios.
        # TODO(dchu): Is this the correct logic?
        absolute_error_ratios=abs_err_ratios,
        mean_absolute_error_ratio=np.mean(abs_err_ratios),
        median_absolute_error_ratio=np_median(abs_err_ratios),
        max_absolute_error_ratio=np.max(abs_err_ratios),
    )


def test_temporal_error():
    np.random.seed(0)

    # Simple test without error.
    err = temporal_error([1.0, 2.0, 3.0], [1.0, 2.0, 3.0])
    print(f"{err=}")

    # Simple test with error.
    err = temporal_error([1.0, 2.0, 3.0], [0.0, 0.0, 0.0])
    print(f"{err=}")

    # Larger test with random order.
    oracle = list(range(100))
    np.random.shuffle(oracle)
    err = temporal_error(oracle, 100 * [0.0])
    print(f"{err=}")

    # Larger test with double random order.
    oracle, output = list(range(100)), list(range(100))
    np.random.shuffle(oracle)
    err = temporal_error(oracle, output)
    print(f"{err=}")


################################################################################
### Find the error in the miss ratio curves and temporal data.
################################################################################


def get_mrc(data_list: list[dict[str, object]]) -> tuple[list[float], list[float]]:
    get_mr_func = get_scaled_fixed_data(
        lambda d: get_stat(d, ["CacheStatistics", "Miss Ratio"]),
        # TODO Adjust with SHARDS correction.
        # I'm
    )
    get_c_func = get_scaled_fixed_data(
        lambda d: get_stat(d, ["Capacity [B]"]),
        *GiB_SHARDS_ARGS,
    )
    return [get_c_func(d) for d in data_list], [get_mr_func(d) for d in data_list]


def get_temporal_sizes(data_list: list[dict[str, object]]) -> dict[float, list[float]]:
    """Get the temporal sizes of each different cache capacity."""
    get_cap_func = get_scaled_fixed_data(
        lambda d: get_stat(d, ["Capacity [B]"]),
        *GiB_SHARDS_ARGS,
    )
    # We assume that all times are equally spaced.
    _get_times_func = get_scaled_fixed_data(
        lambda d: d["CacheStatistics"]["Temporal Times [ms]"],
        *HOURS_NO_SHARDS_ARGS,
    )
    assert _get_times_func
    get_sizes_func = get_scaled_fixed_data(
        lambda d: d["CacheStatistics"]["Temporal Sizes [B]"],
        *COUNT_SHARDS_ARGS,
    )
    return {get_cap_func(d): get_sizes_func(d) for d in data_list}


def get_temporal_metadata(
    data_list: list[dict[str, object]],
) -> dict[float, list[float]]:
    get_cap_func = get_scaled_fixed_data(
        lambda d: get_stat(d, ["Capacity [B]"]),
        *GiB_SHARDS_ARGS,
    )
    # We assume that all times are equally spaced.
    _get_times_func = get_scaled_fixed_data(
        lambda d: d["CacheStatistics"]["Temporal Times [ms]"],
        *HOURS_NO_SHARDS_ARGS,
    )
    assert _get_times_func
    get_metadata_usage_func = get_scaled_fixed_data(
        lambda d: np.array(d["Removal Policy Statistics"]["Temporal LRU Sizes [#]"])
        + np.array(d["Removal Policy Statistics"]["Temporal TTL Sizes [#]"]),
        *COUNT_SHARDS_ARGS,
    )
    return {get_cap_func(d): get_metadata_usage_func(d) for d in data_list}


################################################################################
### Utility functions
################################################################################


def calculate_average_error(diff: dict[float, float]) -> float:
    xs, ys = list(diff.keys()), list(diff.values())
    r = 0.0
    for x0, x1, y0, y1 in zip(xs, xs[1:], ys, ys[1:]):
        a, b = Point(x0, y0), Point(x1, y1)
        c, d = Point(x0, 0), Point(x1, 0)
        r += trapezoid_mean_absolute_error(a, b, c, d, absolute=False)
    return r


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", "-i", type=Path, required=True, help="input path")
    parser.add_argument("--verbose", "-v", action="store_true", help="verbosity flag")
    args = parser.parse_args()
    ipath = args.input
    verbose = args.verbose

    data = parse_data(ipath)

    oracle_data_list = data[(0.0, 1.0, "EvictionTime")]
    predict_data_list = data[(0.5, 0.5, "EvictionTime")]

    # Calculate MAE of the MRC.
    print(f"=== PATH: {ipath} ===")
    print("--- IMPROVEMENT OF MISS RATIO CURVE (GREATER IS BETTER) ---")
    oracle_c, oracle_mr = get_mrc(oracle_data_list)
    output_c, output_mr = get_mrc(predict_data_list)
    assert np.all(np.array(oracle_c) == np.array(output_c))
    mae = mean_absolute_error(oracle_c, oracle_mr, output_mr, absolute=False)
    print(f"{mae=} ({100 * mae:.2}%)")

    # Calculate the error of the temporal data.
    print("--- TOTAL MEMORY USAGE (LESSER IS BETTER) ---")
    diff = {}
    oracle_cap_vs_temporal_sizes = get_temporal_sizes(oracle_data_list)
    output_cap_vs_temporal_sizes = get_temporal_sizes(predict_data_list)
    for (oracle_cap, oracle_sizes), (output_cap, output_sizes) in zip(
        oracle_cap_vs_temporal_sizes.items(), output_cap_vs_temporal_sizes.items()
    ):
        assert oracle_cap == output_cap, f"{oracle_cap} vs {output_cap}"
        err = temporal_error(oracle_sizes, output_sizes)
        if verbose:
            print(f"{oracle_cap} [GiB]: {err['mean_fair_excess_error']=} [B]")
        diff[oracle_cap] = err["mean_fair_excess_error"]
    caps = oracle_cap_vs_temporal_sizes.keys()
    print(calculate_average_error(diff) / (max(caps) - min(caps)))

    # Calculate the total memory savings.
    print("--- TOTAL METADATA MEMORY SAVINGS (GREATER IS BETTER) ---")
    diff = {}
    oracle_cap_vs_metadata = get_temporal_metadata(oracle_data_list)
    output_cap_vs_metadata = get_temporal_metadata(predict_data_list)
    for (oracle_cap, oracle_metadata), (output_cap, output_metadata) in zip(
        oracle_cap_vs_metadata.items(), output_cap_vs_metadata.items()
    ):
        assert oracle_cap == output_cap, f"{oracle_cap} vs {output_cap}"
        err = temporal_error(oracle_metadata, output_metadata)
        if verbose:
            print(f"{oracle_cap} [GiB]: {err['mean_fair_excess_error']=} [B]")
        diff[oracle_cap] = err["mean_fair_excess_error"]
    caps = oracle_cap_vs_metadata.keys()
    print(calculate_average_error(diff) / (max(caps) - min(caps)))


if __name__ == "__main__":
    test = False
    if test:
        trapezoid_mae_test()
        full_mae_test()
        test_temporal_error()
        exit()
    main()
