import datetime
from pathlib import Path
from typing import Optional

import numpy as np
import pandas as pd
from matplotlib import pyplot as plt, ticker

from src.base.log_config import get_logger

logger = get_logger()

BASE_DIR = Path(__file__).resolve().parent.parent.parent  # heiDGAF directory

LATENCIES_COMPARISON_FILENAME = "latency_comparison.png"


class PlotGenerator:
    """Plots given data and combines it into figures."""

    def plot_latency(
        self,
        datafiles_to_names: dict[str, str],
        start_time: pd.Timestamp,
        relative_output_directory_path: Path,
        median_smooth: bool,
        title: str = None,
        x_label: str = "Time",
        y_label: str = "Latency",
        y_input_unit: str = "microseconds",
        fig_width: int | float = 12.5,
        fig_height: int | float = 4.5,
        color_start_index: int = 0,
        intervals_in_sec: Optional[list[int]] = None,
        datarates_per_interval: Optional[list[int]] = None,
    ):
        """Creates a figure and plots the given latency data as graphs. All graphs are plotted into the same figure,
        which is then stored as a file.

        Args:
            datafiles_to_names (dict[str, str]): Dictionary of file names to show in the legend and their paths
            start_time (pd.Timestamp): Time to be set as t = 0
            relative_output_directory_path (Path): File path at which the figure should be stored
            median_smooth (bool): True if the data should be smoothed, False by default
            title (str): Title of the figure, None by default
            x_label (str): Label x-axis, "Time" by default
            y_label (str): Label y-axis, "Latency" by default
            y_input_unit (str): Unit of the data given as input, "microseconds" by default
            fig_width (int | float): Width of the figure, 10 by default
            fig_height (int | float): Height of the figure, 5 by default
            color_start_index (int): First index of the color palette to be used, 0 by default
            intervals_in_sec (Optional[list[int]]): Optional list of interval lengths in seconds
            datarates_per_interval (Optional[list[int]]): Optional list of data rates per interval
        """
        plt.figure(figsize=(fig_width, fig_height))

        # initialize color palette
        colors = plt.rcParams["axes.prop_cycle"].by_key()["color"]
        cur_color_index = color_start_index

        # load data from files
        dataframes = {}
        total_max_time = 0  # seconds
        total_max_value = 0

        for name, file in datafiles_to_names.items():
            df = pd.read_csv(file, parse_dates=["time"]).sort_values(by="time")

            if df.empty:
                continue  # skip empty datafiles

            df["time"] = (
                df["time"] - start_time.replace(tzinfo=None)
            ).dt.total_seconds()

            if median_smooth:
                window_size = max(1, len(df) // 100)
                df["value"] = (
                    df["value"]
                    .rolling(window=window_size, center=True, min_periods=1)
                    .median()
                )

            dataframes[name] = df

            df.loc[df["time"].diff() > 2, "value"] = np.nan

            df_max_time = df["time"].max()
            if df_max_time > total_max_time:
                total_max_time = df_max_time

            df_max_value = df["value"].max()
            if df_max_value > total_max_value:
                total_max_value = df_max_value

        x_unit, x_scale = self._determine_time_unit(total_max_time, "seconds")
        y_unit, y_scale = self._determine_time_unit(total_max_value, y_input_unit)

        # plot data
        for name, df in dataframes.items():
            plt.yscale("log")
            plt.plot(
                df["time"] / x_scale * (10**6),
                df["value"] / y_scale,
                marker="o",
                markersize=1,
                label=name,
                color=colors[cur_color_index],
            )
            cur_color_index += 1

        # add interval lines
        if intervals_in_sec is not None:
            x_values = [0]
            for i in intervals_in_sec:
                x_values.append(x_values[-1] + i)
            for x in x_values[1:]:
                plt.axvline(
                    x, color="gray", linestyle="--", linewidth=1
                )  # TODO: Check for different x_units

        if (
            intervals_in_sec is not None
            and datarates_per_interval is not None
            and len(intervals_in_sec) == len(datarates_per_interval)
        ):
            # prepare list of interval starting times
            interval_starts_in_sec = [
                sum(intervals_in_sec[:i]) for i in range(len(intervals_in_sec))
            ]
            interval_starts_in_sec.append(
                sum(intervals_in_sec[: len(intervals_in_sec)])
            )  # add interval after the test

            # prepare list of data rates
            datarates_per_interval.append(0)  # add 0 after the test

            y_max = plt.ylim()[1]

            for rate, interval_start in zip(
                datarates_per_interval, interval_starts_in_sec
            ):
                plt.annotate(
                    f"{rate:,}/s \u2192",  # arrow to the right
                    xy=(interval_start, y_max),
                    xytext=(0, 9),
                    textcoords="offset points",
                    ha="left",
                    va="top",
                    fontsize=7,
                    color="gray",
                )

        # adjust settings
        plt.xlim(left=0)

        if x_unit == "s":
            plt.gca().xaxis.set_major_locator(ticker.MultipleLocator(30))

        y_label_additions = ""
        if median_smooth:
            y_label_additions = " (median-smoothed)"

        plt.title(title)
        plt.xlabel(f"{x_label} [{x_unit}]", labelpad=10)
        plt.ylabel(f"{y_label} [{y_unit}]" + y_label_additions)
        plt.grid(color="lightgray")

        if len(datafiles_to_names) > 1:
            plt.legend()

        relative_output_filename = (
            relative_output_directory_path / LATENCIES_COMPARISON_FILENAME
        )
        absolute_output_filename = BASE_DIR / relative_output_filename

        plt.savefig(absolute_output_filename, dpi=300, bbox_inches="tight")
        logger.info(f"File saved at {relative_output_filename}")

    def plot_fill_levels(
        self,
        datafiles_to_names: dict[str, str],
        start_time: pd.Timestamp,
        relative_output_directory_path: Path,
        median_smooth: bool,
        title: str = None,
        x_label: str = "Time",
        y_label: str = "Fill Level",
        fig_width: int | float = 12.5,
        fig_height: int | float = 4.5,
        color_start_index: int = 0,
        intervals_in_sec: Optional[list[int]] = None,
    ):
        """Creates a figure and plots the given fill level data as graphs. All graphs are plotted into the same figure,
        which is then stored as a file.

        Args:
            datafiles_to_names (dict[str, str]): Dictionary of file names to show in the legend and their paths
            start_time (pd.Timestamp): Time to be set as t = 0
            relative_output_directory_path (Path): File path at which the figure should be stored
            median_smooth (bool): True if the data should be smoothed, False by default
            title (str): Title of the figure, None by default
            x_label (str): Label x-axis, "Time" by default
            y_label (str): Label y-axis, "Fill Level" by default
            y_input_unit (str): Unit of the data given as input, "count" by default
            fig_width (int | float): Width of the figure, 12.5 by default
            fig_height (int | float): Height of the figure, 4.5 by default
            color_start_index (int): First index of the color palette to be used, 0 by default
            intervals_in_sec (Optional[list[int]]): Optional list of interval lengths in seconds
        """
        plt.figure(figsize=(fig_width, fig_height))

        # initialize color palette
        colors = plt.rcParams["axes.prop_cycle"].by_key()["color"]
        cur_color_index = color_start_index

        # load data from files
        dataframes = {}
        total_max_time = 0  # seconds
        total_max_value = 0

        for name, file in datafiles_to_names.items():
            df = pd.read_csv(file, parse_dates=["timestamp"]).sort_values(
                by="timestamp"
            )

            if df.empty:
                continue  # skip empty datafiles

            df["timestamp"] = (
                df["timestamp"] - start_time.replace(tzinfo=None)
            ).dt.total_seconds()

            if median_smooth:
                window_size = max(1, len(df) // 100)
                df["entry_count"] = (
                    df["entry_count"]
                    .rolling(window=window_size, center=True, min_periods=1)
                    .median()
                )

            dataframes[name] = df

            df.loc[df["timestamp"].diff() > 2, "entry_count"] = np.nan

            df_max_time = df["timestamp"].max()
            if df_max_time > total_max_time:
                total_max_time = df_max_time

            df_max_value = df["entry_count"].max()
            if df_max_value > total_max_value:
                total_max_value = df_max_value

        x_unit, x_scale = self._determine_time_unit(total_max_time, "seconds")

        # plot data
        for name, df in dataframes.items():
            plt.yscale("log")
            plt.plot(
                df["timestamp"] / x_scale * (10**6),
                df["entry_count"],
                marker="o",
                markersize=1,
                label=name,
                color=colors[cur_color_index],
            )
            cur_color_index += 1

        # add interval lines
        if intervals_in_sec is not None:
            x_values = [0]
            for i in intervals_in_sec:
                x_values.append(x_values[-1] + i)
            for x in x_values[1:]:
                plt.axvline(x, color="gray", linestyle="--", linewidth=1)

        # adjust settings
        plt.xlim(left=0)

        if x_unit == "s":
            plt.gca().xaxis.set_major_locator(ticker.MultipleLocator(30))

        y_label_additions = ""
        if median_smooth:
            y_label_additions = " (median-smoothed)"

        plt.title(title)
        plt.xlabel(f"{x_label} [{x_unit}]", labelpad=10)
        plt.ylabel(f"{y_label}" + y_label_additions)
        plt.grid(color="lightgray")

        if len(datafiles_to_names) > 1:
            plt.legend()

        relative_output_filename = (
            relative_output_directory_path / "fill_levels_comparison.png"
        )
        absolute_output_filename = BASE_DIR / relative_output_filename

        plt.savefig(absolute_output_filename, dpi=300, bbox_inches="tight")
        logger.info(f"File saved at {relative_output_filename}")

    def plot_entering_processed(
        self,
        datafiles_to_names: dict[str, str],
        start_time: pd.Timestamp,
        relative_output_directory_path: Path,
        title: str = None,
        x_label: str = "Time",
        y_label: str = "Accumulated number of log lines",
        fig_width: int | float = 10,
        fig_height: int | float = 5,
        color_start_index: int = 0,
        intervals_in_sec: Optional[list[int]] = None,
        downsample_factor: int = 500,
    ):
        """Creates a figure and plots the entering and processed log lines data. All graphs are plotted into the
        same figure, which is then stored as a file.

        Args:
            datafiles_to_names (dict[str, str]): Dictionary of file names to show in the legend and their paths
            start_time (pd.Timestamp): Time to be set as t = 0
            relative_output_directory_path (Path): File path at which the figure should be stored
            title (str): Title of the figure, None by default
            x_label (str): Label x-axis, "Time" by default
            y_label (str): Label y-axis, "Number of log lines" by default
            fig_width (int | float): Width of the figure, 10 by default
            fig_height (int | float): Height of the figure, 5 by default
            color_start_index (int): First index of the color palette to be used, 0 by default
            intervals_in_sec (Optional[list[int]]): Optional list of interval lengths in seconds
            downsample_factor (int): Factor for downsampling data points, 500 by default
        """
        plt.figure(figsize=(fig_width, fig_height))

        # initialize color palette
        colors = plt.rcParams["axes.prop_cycle"].by_key()["color"]
        cur_color_index = color_start_index

        # load data from files
        for name, file in datafiles_to_names.items():
            # determine which timestamp column to use
            df = pd.read_csv(file)

            if df.empty:
                continue  # skip empty datafiles

            # check which timestamp column exists
            if "timestamp_in" in df.columns:
                timestamp_col = "timestamp_in"
            elif "timestamp" in df.columns:
                timestamp_col = "timestamp"
            else:
                logger.warning(f"No valid timestamp column found in {file}")
                continue

            # parse timestamp and sort
            df[timestamp_col] = pd.to_datetime(df[timestamp_col])
            df = df.sort_values(by=timestamp_col)

            df["time"] = (
                df[timestamp_col] - start_time.replace(tzinfo=None)
            ).dt.total_seconds()

            # downsample data for better performance
            n = max(1, len(df) // downsample_factor)
            plt.plot(
                df["time"][::n],
                df["cumulative_count"][::n],
                linestyle="-",
                label=name,
                color=colors[cur_color_index],
            )
            cur_color_index += 1

        # add interval lines
        if intervals_in_sec is not None:
            x_values = [0]
            for i in intervals_in_sec:
                x_values.append(x_values[-1] + i)
            for x in x_values[1:]:
                plt.axvline(x, color="gray", linestyle="--", linewidth=1)

        # adjust settings
        plt.xlim(left=0)
        plt.ylim(bottom=0)
        plt.gca().xaxis.set_major_locator(ticker.MultipleLocator(60))

        plt.title(title)
        plt.xlabel(f"{x_label} [s]", labelpad=10)
        plt.ylabel(y_label)
        plt.legend()
        plt.grid(color="lightgray")

        relative_output_filename = (
            relative_output_directory_path / "entering_processed_comparison.png"
        )
        absolute_output_filename = BASE_DIR / relative_output_filename

        plt.savefig(absolute_output_filename, dpi=300, bbox_inches="tight")
        logger.info(f"File saved at {relative_output_filename}")

    def plot_latencies_boxplot(
        self,
        datafiles_to_names: dict[str, str],
        relative_output_directory_path: Path,
        title: str = None,
        y_label: str = "Latency in module [s]",
        y_input_unit: str = "microseconds",
        fig_width: int | float = 7,
        fig_height: int | float = 5,
    ):
        """Creates a boxplot figure showing latency distributions for different modules.

        Args:
            datafiles_to_names (dict[str, str]): Dictionary of file names to show in the legend and their paths
            relative_output_directory_path (Path): File path at which the figure should be stored
            title (str): Title of the figure, "Latencies per module" by default
            y_label (str): Label y-axis, "Latency in module [s]" by default
            y_input_unit (str): Unit of the data given as input, "microseconds" by default
            fig_width (int | float): Width of the figure, 7 by default
            fig_height (int | float): Height of the figure, 5 by default
        """
        plt.figure(figsize=(fig_width, fig_height))

        data = []
        labels = []

        # load data from files
        for name, file in datafiles_to_names.items():
            df = pd.read_csv(file, parse_dates=["time"]).sort_values(by="time")

            if df.empty:
                continue  # skip empty datafiles

            # convert to seconds based on input unit
            if y_input_unit == "microseconds":
                data.append(df["value"] / (10**6))
            elif y_input_unit == "milliseconds":
                data.append(df["value"] / (10**3))
            elif y_input_unit == "seconds":
                data.append(df["value"])
            else:
                data.append(df["value"])

            labels.append(name)

        # define box plot styling
        boxprops = dict(color="black", linewidth=1)
        whiskerprops = dict(color="black", linewidth=1)
        capprops = dict(color="black", linewidth=1)
        medianprops = dict(color="black", linewidth=1.5)
        flierprops = dict(
            marker="x",
            markerfacecolor="lightgray",
            markeredgecolor="lightgray",
            markersize=4,
            linestyle="none",
        )

        plt.boxplot(
            data,
            labels=labels,
            vert=True,
            patch_artist=False,  # no filled boxes
            boxprops=boxprops,
            whiskerprops=whiskerprops,
            capprops=capprops,
            medianprops=medianprops,
            flierprops=flierprops,
        )

        plt.yscale("log")
        plt.ylabel(y_label)
        plt.title(title, fontsize=12)
        plt.grid(axis="y", linestyle="--", linewidth=0.5, alpha=0.7)

        plt.xticks(rotation=30, ha="right")
        plt.tight_layout()

        relative_output_filename = (
            relative_output_directory_path / "latencies_boxplot.png"
        )
        absolute_output_filename = BASE_DIR / relative_output_filename

        plt.savefig(absolute_output_filename, dpi=300, bbox_inches="tight")
        logger.info(f"File saved at {relative_output_filename}")

    @staticmethod
    def _determine_time_unit(max_value: int, input_unit: str):
        max_value_in_seconds = datetime.timedelta(
            **{input_unit: int(max_value)}
        ).total_seconds()
        max_value_in_microseconds = max_value_in_seconds * (10**6)

        units = [
            ("us", 1),
            ("ms", 10**3),
            ("s", 10**6),
            ("min", 60 * (10**6)),
            ("h", 60 * 60 * (10**6)),
            ("d", 24 * 60 * 60 * (10**6)),
        ]
        thresholds = [
            1.3 * (10**3),  # ms for over 1.3 ms
            1.3 * (10**6),  # s for over 1.3 s
            5 * 60 * (10**6),  # min for over 5 min
            3 * 60 * 60 * (10**6),  # h for over 3 h
            3 * 24 * 60 * 60 * (10**6),  # d for over 3 d
            float("inf"),  # d for everything above
        ]

        for (unit, factor), threshold in zip(units, thresholds):
            if max_value_in_microseconds < threshold:
                return unit, factor
