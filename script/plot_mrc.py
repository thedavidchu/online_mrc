import json
from typing import Dict, Union
import matplotlib.pyplot as plt

INFINITY = float("inf")

def decode_json_histogram(histogram_text: str) -> Dict[float, Union[int, float]]:
    histogram = json.loads(histogram_text)
    histogram = {float(key): value for key, value in histogram.items() if value > 0 and key != INFINITY}
    return histogram

def input_histogram() -> Dict[float, Union[int, float]]:
    while True:
        histogram_text = input()
        # NOTE  We assume that there will only be a single JSON. This will be
        #       only line that starts with the character '{' and the entire
        #       JSON string will be on a single line.
        if histogram_text.startswith(r"{"):
            break
    histogram = decode_json_histogram(histogram_text)
    return histogram

def plot_histogram(histogram: Dict[float, Union[float, int]]):
    plt.figure()
    plt.title("Histogram")
    plt.xlabel("Reuse Stack Distance")
    plt.ylabel("Frequency")
    plt.plot(histogram.keys(), histogram.values())
    plt.savefig("histogram.png")

def plot_miss_rate_curve(histogram: Dict[float, Union[float, int]]):
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
    from_input = True
    if from_input:
        histogram = input_histogram()
    else:
        histogram_text = ""
        histogram = decode_json_histogram(histogram_text)
    plot_histogram(histogram)
    plot_miss_rate_curve(histogram)

if __name__ == "__main__":
    main()
