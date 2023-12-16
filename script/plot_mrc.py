import json
from typing import Dict, Union
import matplotlib.pyplot as plt

INFINITY = float("inf")

def input_histogram() -> Dict[float, Union[int, float]]:
    while True:
        histogram = input()
        # NOTE  We assume that there will only be a single JSON. This will be
        #       only line that starts with the character '{' and the entire
        #       JSON string will be on a single line.
        if histogram.startswith(r"{"):
            break
    histogram = json.loads(histogram)
    histogram = {float(key): value for key, value in histogram.items() if value > 0 and key != INFINITY}
    return histogram

def plot_histogram(histogram: Dict[float, Union[float, int]]):
    running_total = 0
    total = sum(histogram.values())
    mrc = {}
    for key, value in histogram.items():
        running_total += value
        mrc[key] = 1 - running_total / total
    plt.figure()
    plt.title("Miss-Rate Curve")
    plt.xlabel("Number of key-value pairs")
    plt.ylabel("Miss-rate")
    plt.plot(mrc.keys(), mrc.values())
    plt.savefig("mrc.png")

def main():
    histogram = input_histogram()
    plot_histogram(histogram)

if __name__ == "__main__":
    main()
