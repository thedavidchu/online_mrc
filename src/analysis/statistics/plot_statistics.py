from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


def plot_statistics(
    input_paths: list[Path], labels: list[tuple[str]], output_path: Path
):
    fig, ax = plt.subplots()
    fig.suptitle("Statistics")
    fig.supxlabel("Access Number")
    fig.supylabel("Thresholds")
    for i, input_path in enumerate(input_paths):
        array = np.fromfile(input_path, dtype=np.uint64)
        b64_per_item, items = int(array[0]), array[1:]
        time = items[::b64_per_item]
        for j in range(1, b64_per_item):
            ax.plot(
                time,
                np.log10(items[j::b64_per_item]),
                label=f"{input_path.stem}:{labels[i][j-1]}",
            )
    ax.legend()
    fig.savefig(output_path)


def main():
    input_paths = [
        Path("/home/david/projects/online_mrc/Evicting-Map-stats.bin"),
        Path("/home/david/projects/online_mrc/Fixed-Size-SHARDS-stats.bin"),
    ]
    plot_statistics(
        input_paths,
        [("global threshold", "max hash", "min hash"), ("threshold",)],
        Path(f"plot.pdf"),
    )


if __name__ == "__main__":
    main()
