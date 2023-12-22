"""
Mattson's Miss-Rate Curve Generation Algorithm.

NOTE:   This is an intentionally unoptimized version of Mattson whose primary
        purpose is to be easily understood and manually inspected.
"""
from typing import Dict, List, Set, Tuple

SIMPLE_TRACE: List[int] = [1, 2, 3, 1]
# This is the same set of random numbers used to unit test the histogram.
TRACE_100: List[int] = [
    2, 3,  2, 5,  0, 1, 7, 9, 4, 2,  10, 3, 1,  10, 10, 5, 10, 6,  5, 0,
    6, 4,  2, 9,  7, 2, 2, 5, 3, 9,  6,  0, 1,  1,  6,  1, 6,  7,  5, 0,
    0, 10, 8, 3,  1, 2, 6, 7, 3, 10, 8,  6, 10, 6,  6,  2, 6,  0,  7, 9,
    6, 10, 1, 10, 2, 6, 2, 7, 8, 8,  6,  0, 7,  3,  1,  1, 2,  10, 3, 10,
    5, 5,  0, 7,  9, 8, 0, 7, 6, 9,  4,  9, 4,  8,  3,  6, 5,  3,  2, 9,
]


def sort_dict_by_key(my_dict: Dict) -> Dict:
    return dict(sorted(my_dict.items()))


def mattson(trace: List[int]) -> Tuple[Dict[int, int], int]:
    seen_elements: Set[int] = set()
    histogram: Dict[int, int] = {}
    infinite_stack_distances: int = 0
    for i, e in enumerate(trace):
        if e not in seen_elements:
            seen_elements.add(e)
            infinite_stack_distances += 1
            continue
        reuse_distance = list(reversed(trace[:i])).index(e) + 1
        histogram[reuse_distance] = histogram.get(reuse_distance, 0) + 1
    histogram = sort_dict_by_key(histogram)
    return histogram, infinite_stack_distances


def listify_histogram(histogram: Tuple[Dict[int, int], int]) -> Tuple[List[int], int]:
    sparse_histogram, _ = histogram
    dense_histogram = [0] * (max(sparse_histogram.keys()) + 1)
    for e, cnt in sparse_histogram.items():
        dense_histogram[e] = cnt
    return dense_histogram, _


def dictify_histogram(histogram: Tuple[List[int], int]) -> Tuple[Dict[int, int], int]:
    dense_histogram, _ = histogram
    sparse_histogram = {}
    for e, cnt in enumerate(dense_histogram):
        if cnt:
            sparse_histogram[e] = cnt
    return sparse_histogram, _


def main():
    histogram = mattson(SIMPLE_TRACE)
    print(histogram)
    histogram = listify_histogram(histogram)
    print(histogram)
    histogram = dictify_histogram(histogram)
    print(histogram)

    print("---")

    histogram = mattson(TRACE_100)
    print(histogram)
    histogram = listify_histogram(histogram)
    print(histogram)
    histogram = dictify_histogram(histogram)
    print(histogram)


if __name__ == "__main__":
    main()
