from pathlib import Path

import numpy as np
from shapely import Point, LineString

from plot_predictive_cache import (
    parse_data,
    parse_number,
    get_stat,
    get_scaled_fixed_data,
    CAPACITY_GIB_ARGS,
    COUNT_SHARDS_ARGS,
    HOURS_NO_SHARDS_ARGS,
    GiB_SHARDS_ARGS,
)

################################################################################
### Difference between miss ratio curves.
################################################################################


def trapezoid_mean_absolute_error(a: Point, b: Point, c: Point, d: Point) -> float:
    """
    Find the mean absolute error within a trapezoid (with parallel vertical sides).

    @params a, b: represent points of the oracle.
    @params c, d: represent points of the output.

    ```
    A----B
    |****|
    |****D
    |***/
    |**/
    |*/
    |/
    C
    ```
    """
    calculate_improvement = True
    # We assume it's a trapezoid to simplify the calculations.
    assert a.x == c.x and b.x == d.x
    oracle_ln = LineString([a, b])
    output_ln = LineString([c, d])
    # If the endpoints are equal, then we can solve with the regular
    # formula and avoid infinite recursion.
    if oracle_ln.intersects(output_ln) and not a.equals(c) and not b.equals(d):
        # If there is no intersection, then we get an empty 'LineString'.
        # We know from above that there should be an intersection.
        intersection = oracle_ln.intersection(output_ln)
        if isinstance(intersection, LineString):
            assert not intersection.is_empty()
            return 0.0
        elif isinstance(intersection, Point):
            return trapezoid_mean_absolute_error(
                a, intersection, c, intersection
            ) + trapezoid_mean_absolute_error(intersection, b, intersection, d)
        else:
            raise ValueError("unrecognized intersection type")
    dx = abs(b.x - a.x)
    if calculate_improvement:
        return (dx * (a.y - c.y) + (b.y - d.y)) / 2
    return abs(dx * (abs(a.y - c.y) + abs(b.y - d.y))) / 2


def mean_absolute_error(
    xs: list[float], oracle_ys: list[float], output_ys: list[float]
) -> float:
    mae = 0.0
    for x0, x1, oracle_y0, oracle_y1, out_y0, out_y1 in zip(
        xs, xs[1:], oracle_ys, oracle_ys[1:], output_ys, output_ys[1:]
    ):
        a, b = Point(x0, oracle_y0), Point(x1, oracle_y1)
        c, d = Point(x0, out_y0), Point(x1, out_y1)
        mae += trapezoid_mean_absolute_error(a, b, c, d)
    return mae


def trapezoid_mae_test():
    # Test normal case.
    a, b, c, d = Point(0, 0), Point(1, 0), Point(0, 1), Point(1, 1)
    mae = trapezoid_mean_absolute_error(a, b, c, d)
    print(f"{mae=}")
    assert mae == 1.0

    # Test intersection case.
    a, b, c, d = Point(0, 0), Point(1, 1), Point(0, 1), Point(1, 0)
    mae = trapezoid_mean_absolute_error(a, b, c, d)
    print(f"{mae=}")
    assert mae == 0.5

    # Test identical lines case.
    a, b, c, d = Point(0, 0), Point(1, 0), Point(0, 0), Point(1, 0)
    mae = trapezoid_mean_absolute_error(a, b, c, d)
    print(f"{mae=}")
    assert mae == 0.0

    # Test identical points case.
    a = Point(0, 0)
    mae = trapezoid_mean_absolute_error(*4 * [a])
    print(f"{mae=}")
    assert mae == 0.0


def full_mae_test():
    # Simple test.
    xs = [0.0, 1.0, 2.0, 3.0]
    orc_ys = [1.0, 1.0, 1.0, 1.0]
    out_ys = [0.0, 0.0, 0.0, 0.0]
    mae = mean_absolute_error(xs, orc_ys, out_ys)
    print(f"{mae=}")
    assert mae == 3.0

    # Intersecting test.
    xs = [0.0, 1.0, 2.0, 3.0]
    orc_ys = [1.0, 0.0, 1.0, 0.0]
    out_ys = [0.0, 1.0, 0.0, 1.0]
    mae = mean_absolute_error(xs, orc_ys, out_ys)
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
    @note We only track when the output is larger than the oracle.
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


def get_temporal_sizes(data_list: list[dict[str, object]]) -> list[list[float]]:
    # We assume that all times are equally spaced.
    _get_times_func = get_scaled_fixed_data(
        lambda d: d["CacheStatistics"]["Temporal Times [ms]"],
        *HOURS_NO_SHARDS_ARGS,
        vector=True,
    )
    assert _get_times_func
    get_sizes_func = get_scaled_fixed_data(
        lambda d: d["CacheStatistics"]["Temporal Sizes [B]"],
        *COUNT_SHARDS_ARGS,
        vector=True,
    )
    return [get_sizes_func(d) for d in data_list]


def get_temporal_metadata(data_list: list[dict[str, object]]) -> list[list[float]]:
    # We assume that all times are equally spaced.
    _get_times_func = get_scaled_fixed_data(
        lambda d: d["CacheStatistics"]["Temporal Times [ms]"],
        *HOURS_NO_SHARDS_ARGS,
        vector=True,
    )
    assert _get_times_func
    get_metadata_usage_func = get_scaled_fixed_data(
        lambda d: np.array(d["LRU-TTL Statistics"]["Temporal LRU Sizes [#]"])
        + np.array(d["LRU-TTL Statistics"]["Temporal TTL Sizes [#]"]),
        *COUNT_SHARDS_ARGS,
        vector=True,
    )
    return [get_metadata_usage_func(d) for d in data_list]


def main():
    ipath = Path(
        "/home/david/projects/online_mrc/myresults/lru_ttl/result-lfu-cluster52-v1-s0.01.out"
    )
    data = parse_data(ipath)

    oracle_data_list = data[(0.0, 1.0, "EvictionTime")]
    predict_data_list = data[(0.5, 0.5, "EvictionTime")]

    # Calculate MAE of the MRC.
    oracle_c, oracle_mr = get_mrc(oracle_data_list)
    output_c, output_mr = get_mrc(predict_data_list)
    assert np.all(np.array(oracle_c) == np.array(output_c))
    mae = mean_absolute_error(oracle_c, oracle_mr, output_mr)
    print(f"{mae=} ({100 * mae:.2}%)")

    # Calculate the error of the temporal data.
    oracle_sizes = get_temporal_sizes(oracle_data_list)
    output_sizes = get_temporal_sizes(predict_data_list)
    for oracle_sizes, output_sizes in zip(oracle_sizes, output_sizes):
        err = temporal_error(oracle_sizes, output_sizes)
        print(f"{err['mean_excess_error']=} [B]")

    # Calculate the total memory savings.
    oracle_metadata = get_temporal_metadata(oracle_data_list)
    output_metadata = get_temporal_metadata(predict_data_list)
    for oracle_metadata, output_metadata in zip(oracle_metadata, output_metadata):
        err = temporal_error(oracle_metadata, output_metadata)
        print(f"{err['mean_excess_error']=}")


if __name__ == "__main__":
    if False:
        trapezoid_mae_test()
        full_mae_test()
        test_temporal_error()
    main()
