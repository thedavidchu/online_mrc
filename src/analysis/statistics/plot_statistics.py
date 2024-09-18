""" @brief  This is a hacky script to plot the statistics.

I was both trying to be too general and weirdly specific at the same time.
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


def get_file_tree(path: Path | list[Path]) -> list[Path]:
    """
    @brief  Return a list of all the files belonging to a directory.

    @note   Modified from analyze_log.py.
    """
    if isinstance(path, list):
        return sorted(
            [file for dir_or_file in path for file in get_file_tree(dir_or_file)]
        )
    if path.is_file():
        return [path]
    return sorted(
        [
            (dirpath / f).absolute()
            for dirpath, dirnames, filenames in path.resolve().walk()
            for f in filenames
        ]
    )


def plot_statistics(
    input_paths: list[Path], labels: list[tuple[str]], output_path: Path
):
    """
    @note   The format of the input files is as follows:
            [<# uint64 per item>, <number * uint64>, ...]

            Within each item, the first uint64 is treated as the x-axis.
    """
    if len(input_paths) % len(labels) != 0:
        raise ValueError(
            "number of input paths must be a multiple of the number of labels"
        )
    num_plots = len(input_paths) // len(labels)
    fig, ax = plt.subplots(1, num_plots, sharex=False, sharey=True, squeeze=False)
    fig.set_size_inches(6 * num_plots, 6)
    fig.suptitle("Statistics")
    fig.supxlabel("Access Number")
    fig.supylabel("Thresholds")
    # NOTE  I fully copied the code to group the list from Stackoverflow
    #       and in typical ctrl-C/ctrl-V fashion, do not understand the
    #       code... well I could understand the code but instead I took
    #       the time to write that I don't understand the code.
    for i, grouped_input_path in enumerate(zip(*(iter(input_paths),) * len(labels))):
        for k, input_path in enumerate(grouped_input_path):
            array = np.fromfile(input_path, dtype=np.uint64)
            b64_per_item, items = int(array[0]), array[1:]
            time = items[::b64_per_item]
            for j in range(1, b64_per_item):
                ax[0, i].plot(
                    time,
                    np.log10(items[j::b64_per_item]),
                    label=f"{input_path.stem}:{labels[k][j-1]}",
                )
        ax[0, i].legend()
    fig.savefig(output_path)


def main():
    input_paths = get_file_tree(
        Path("/home/david/projects/online_mrc/myresults/tmp/stats")
    )
    [
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
