# /usr/bin/python3
import argparse
import os

import numpy as np
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", "-i", type=str, required=True, help="input path")
    args = parser.parse_args()

    root, ext = os.path.splitext(args.input)
    x = np.fromfile(args.input, dtype=np.float64)

    fig, ax = plt.subplots()
    fig.suptitle(f"Plot of {os.path.basename(root)}")
    ax.plot(x)

    # Save figure
    fig.savefig(f"{root}.pdf")


if __name__ == "__main__":
    main()
