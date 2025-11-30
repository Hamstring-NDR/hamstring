import datetime
from abc import abstractmethod
from pathlib import Path
from typing import Optional

import numpy as np
import pandas as pd
from matplotlib import pyplot as plt, ticker

from src.base.log_config import get_logger
from utils import ReadWriteUtils

logger = get_logger()

BASE_DIR = Path(__file__).resolve().parent.parent.parent  # project root directory

OUTPUT_FILENAMES = {
    "fill_levels_comparison": "fill_levels_comparison.png",
    "entering_processed_comparison": "entering_processed_comparison.png",
    "latencies_boxplot": "latencies_boxplot.png",
    "entering_processed_per_time": "entering_processed_per_minute.png",
}


class PlotGenerator:
    """Plots given data and combines it into figures."""

    def __init__(self, plot_name: str, test_identifier: str):
        self.plot_name = plot_name
        self.test_identifier = test_identifier
        self.metadata = ReadWriteUtils.get_metadata(test_identifier)

        self.default_fig_size = (10, 5)

    @abstractmethod
    def plot(self, *args, **kwargs):
        raise NotImplementedError

    @abstractmethod
    def __plot_core(self, *args, **kwargs):
        raise NotImplementedError

    def plot_entering_processed_per_minute(
        self,
        datafiles_to_names: dict[str, str],
        relative_output_directory_path: Path,
        title: str = None,
        x_label: str = "Minutes since start",
        y_label: str = "Log lines per minute",
        fig_width: int | float = 10,
        fig_height: int | float = 5,
        bar_width: float = 0.7,
        x_major_locator: int = 1,
        y_major_locator: int = 2500,
    ):
        """Creates a bar chart showing entering and processed log lines per minute.
        Processed data is displayed as negative bars for mirror effect.

        Args:
            datafiles_to_names (dict[str, str]): Dictionary of file names to show in the legend and their paths
            relative_output_directory_path (Path): File path at which the figure should be stored
            title (str): Title of the figure, None by default
            x_label (str): Label x-axis, "Minutes since start" by default
            y_label (str): Label y-axis, "Log lines per minute" by default
            fig_width (int | float): Width of the figure, 10 by default
            fig_height (int | float): Height of the figure, 5 by default
            bar_width (float): Width of the bars, 0.7 by default
            x_major_locator (int): Major tick interval for x-axis, 1 by default
            y_major_locator (int): Major tick interval for y-axis, 2500 by default
        """
        plt.figure(figsize=(fig_width, fig_height))

        start_time = None

        # load data from files
        for name, file in datafiles_to_names.items():
            df = pd.read_csv(file)

            if df.empty:
                continue  # skip empty datafiles

            # determine time column name (flexible handling)
            if "time_bucket" in df.columns:
                time_col = "time_bucket"
            elif "timestamp_in" in df.columns:
                time_col = "timestamp_in"
            elif "timestamp" in df.columns:
                time_col = "timestamp"
            else:
                raise ValueError(f"No recognized time column in {file}")

            df[time_col] = pd.to_datetime(df[time_col])
            df = df.sort_values(by=time_col)

            if start_time is None:
                start_time = df[time_col].min()

            df["minutes_since_start"] = (
                df[time_col] - start_time
            ).dt.total_seconds() / 60

            # determine count column name
            if "count" in df.columns:
                count_col = "count"
                is_cumulative = False
            elif "total_count" in df.columns:
                count_col = "total_count"
                is_cumulative = False
            elif "cumulative_count" in df.columns:
                count_col = "cumulative_count"
                is_cumulative = True
            else:
                raise ValueError(f"No recognized count column in {file}")

            # if data is cumulative, we need to convert to per-minute counts
            if is_cumulative:
                # group by minute buckets if not already aggregated
                if time_col != "time_bucket":
                    df["time_bucket"] = df[time_col].dt.floor("min")
                    grouped = df.groupby("time_bucket")[count_col].last().reset_index()
                    grouped["minutes_since_start"] = (
                        grouped["time_bucket"] - start_time
                    ).dt.total_seconds() / 60
                else:
                    grouped = df.copy()
                    grouped["time_bucket"] = grouped[time_col]

                # calculate per-minute count from cumulative values
                grouped["count"] = grouped[count_col].diff()
                grouped.loc[grouped.index[0], "count"] = grouped[count_col].iloc[0]

                # filter out negative time values
                grouped = grouped[grouped["minutes_since_start"] >= 0]

                count_data = grouped["count"]
                time_data = grouped["minutes_since_start"]
            else:
                # filter out negative time values
                df = df[df["minutes_since_start"] >= 0]
                count_data = df[count_col]
                time_data = df["minutes_since_start"]

            if "Entering" in name:
                plt.bar(
                    time_data,
                    count_data,
                    width=bar_width,
                    align="edge",
                    label=name,
                )
            else:
                # plot processed data as negative bars for mirror effect
                plt.bar(
                    time_data,
                    -count_data,
                    width=bar_width,
                    align="edge",
                    label=name,
                )

        # adjust settings
        plt.gca().xaxis.set_major_locator(ticker.MultipleLocator(x_major_locator))
        plt.gca().yaxis.set_major_locator(ticker.MultipleLocator(y_major_locator))

        plt.xlabel(x_label, labelpad=10)
        plt.ylabel(y_label)
        plt.title(title)
        plt.legend()
        plt.grid(axis="y", linestyle="--")

        relative_output_filename = (
            relative_output_directory_path
            / OUTPUT_FILENAMES["entering_processed_per_time"]
        )
        absolute_output_filename = BASE_DIR / relative_output_filename

        plt.savefig(absolute_output_filename, dpi=300, bbox_inches="tight")
        logger.info(f"File saved at {relative_output_filename}")

    def save_to_file(self):
        output_filepath = ReadWriteUtils.get_plot_output_filepath(
            self.plot_name, self.test_identifier
        )

        plt.savefig(output_filepath, dpi=300, bbox_inches="tight")
        logger.info(f"File saved at {output_filepath}")

    def _set_up_initial_figure(self, fig_size: Optional[tuple[float, float]]):
        if fig_size is None:
            fig_size = self.default_fig_size

        fig_width = fig_size[0]
        fig_height = fig_size[1]

        plt.figure(figsize=(fig_width, fig_height))

    @staticmethod
    def _determine_time_unit(max_value: int, input_unit: str) -> [str, int]:
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

    @staticmethod
    def _get_colors():
        return plt.rcParams["axes.prop_cycle"].by_key()["color"]


class GraphPlotGenerator(PlotGenerator):

    def _get_start_time(self) -> datetime.datetime:
        return self.metadata["start_timestamp"]

    @staticmethod
    def _set_x_ticks(x_unit: str):
        if x_unit == "s":
            plt.gca().xaxis.set_major_locator(ticker.MultipleLocator(30))

    @staticmethod
    def _add_interval_lines(intervals_in_sec: Optional[list[int]]):
        if intervals_in_sec is not None:
            x_values = [0]

            for i in intervals_in_sec:
                x_values.append(x_values[-1] + i)

            for x in x_values[1:]:
                plt.axvline(
                    x, color="gray", linestyle="--", linewidth=1
                )  # TODO: Check for different x_units

    @staticmethod
    @abstractmethod
    def _get_x_label() -> str:
        raise NotImplementedError

    @staticmethod
    @abstractmethod
    def _get_y_label() -> str:
        raise NotImplementedError


class LatencyComparisonPlotGenerator(GraphPlotGenerator):

    def __init__(
        self,
        test_identifier: str,
        intervals_in_sec: Optional[list[int]] = None,
        data_rates_per_interval: Optional[list[int]] = None,
    ):
        plot_name = "latency_comparison"
        super().__init__(plot_name=plot_name, test_identifier=test_identifier)

        self.intervals_in_sec = intervals_in_sec
        self.data_rates_per_interval = data_rates_per_interval

        self.default_fig_size = (12.5, 4.5)

    def plot(
        self,
        fig_size: tuple[float, float] = None,
        median_smooth: bool = True,
        y_input_unit: str = "microseconds",
        color_start_index: int = 0,
    ):
        """TODO
        Creates a figure and plots the given latency data as graphs. All graphs are plotted into the same figure,
                which is then stored as a file.

                Args:
                    median_smooth (bool): True if the data should be smoothed, False by default
                    y_input_unit (str): Unit of the data given as input, "microseconds" by default
                    fig_width (int | float): Width of the figure, 10 by default
                    fig_height (int | float): Height of the figure, 5 by default
                    color_start_index (int): First index of the color palette to be used, 0 by default
        """
        self._set_up_initial_figure(fig_size)
        start_time = self._get_start_time()

        colors = self._get_colors()  # initialize color palette for graphs
        cur_color_index = color_start_index

        total_max_time = 0  # seconds
        total_max_value = 0

        dataframes = {}
        modules_to_csv_paths = ReadWriteUtils.get_modules_to_csv_filepaths(
            self.plot_name, self.test_identifier
        )

        for name, file in modules_to_csv_paths.items():
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
            df.loc[df["time"].diff() > 2, "value"] = np.nan  # fill gaps

            df_max_time = df["time"].max()
            if df_max_time > total_max_time:
                total_max_time = df_max_time

            df_max_value = df["value"].max()
            if df_max_value > total_max_value:
                total_max_value = df_max_value

        x_unit, x_scale = self._determine_time_unit(total_max_time, "seconds")
        y_unit, y_scale = self._determine_time_unit(total_max_value, y_input_unit)

        for name, df in dataframes.items():
            self.__plot_core(name, df, x_scale, y_scale, colors[cur_color_index])
            cur_color_index += 1

        self._add_interval_lines(self.intervals_in_sec)
        self._add_interval_markings()

        plt.xlim(left=0)
        self._set_x_ticks(x_unit)

        self._activate_grid()
        self._set_labels(x_unit, y_unit, median_smooth)

        if len(modules_to_csv_paths) > 1:
            plt.legend()

        self.save_to_file()

    def __plot_core(
        self, name: str, df: pd.DataFrame, x_scale: int, y_scale: int, color
    ):
        plt.yscale("log")
        plt.plot(
            df["time"] / x_scale * (10**6),
            df["value"] / y_scale,
            marker="o",
            markersize=1,
            label=name,
            color=color,
        )

    @staticmethod
    def _get_x_label():
        return "Time"

    @staticmethod
    def _get_y_label():
        return "Latency"

    def _set_labels(self, x_unit: str, y_unit: str, median_smooth: bool):
        plt.xlabel(f"{self._get_x_label()} [{x_unit}]", labelpad=10)
        plt.ylabel(
            f"{self._get_y_label()} [{y_unit}]"
            + (" (median-smoothed)" if median_smooth else "")
        )

    @staticmethod
    def _activate_grid():
        plt.grid(color="lightgray")

    def _add_interval_markings(self):
        if (
            self.intervals_in_sec is not None
            and self.data_rates_per_interval is not None
            and len(self.intervals_in_sec) == len(self.data_rates_per_interval)
        ):
            # prepare list of interval starting times
            interval_starts_in_sec = [
                sum(self.intervals_in_sec[:i])
                for i in range(len(self.intervals_in_sec))
            ]
            interval_starts_in_sec.append(
                sum(self.intervals_in_sec[: len(self.intervals_in_sec)])
            )  # add interval after the test

            # prepare list of data rates
            data_rates_per_interval = self.data_rates_per_interval.copy()
            data_rates_per_interval.append(0)  # add 0 after the test

            y_max = plt.ylim()[1]

            for rate, interval_start in zip(
                data_rates_per_interval, interval_starts_in_sec
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


class FillLevelsComparisonPlotGenerator(GraphPlotGenerator):

    def __init__(
        self,
        test_identifier: str,
        intervals_in_sec: Optional[list[int]] = None,
    ):
        plot_name = "fill_levels_comparison"
        super().__init__(plot_name=plot_name, test_identifier=test_identifier)

        self.intervals_in_sec = intervals_in_sec

        self.default_fig_size = (8.35, 4.8)

    def plot(
        self,
        fig_size: tuple[float, float] = None,
        median_smooth: bool = True,
        color_start_index: int = 0,
    ):
        """TODO
        Creates a figure and plots the given fill level data as graphs. All graphs are plotted into the same figure,
        which is then stored as a file.

        Args:
            median_smooth (bool): True if the data should be smoothed, False by default
            color_start_index (int): First index of the color palette to be used, 0 by default
        """
        self._set_up_initial_figure(fig_size)
        start_time = self._get_start_time()

        colors = self._get_colors()  # initialize color palette for graphs
        cur_color_index = color_start_index

        total_max_time = 0  # seconds
        total_max_value = 0

        dataframes = {}
        modules_to_csv_paths = ReadWriteUtils.get_modules_to_csv_filepaths(
            self.plot_name, self.test_identifier
        )

        for name, file in modules_to_csv_paths.items():
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
            df.loc[df["timestamp"].diff() > 2, "entry_count"] = np.nan  # fill gaps

            df_max_time = df["timestamp"].max()
            if df_max_time > total_max_time:
                total_max_time = df_max_time

            df_max_value = df["entry_count"].max()
            if df_max_value > total_max_value:
                total_max_value = df_max_value

        x_unit, x_scale = self._determine_time_unit(total_max_time, "seconds")

        for name, df in dataframes.items():
            self.__plot_core(name, df, x_scale, colors[cur_color_index])
            cur_color_index += 1

        self._add_interval_lines(self.intervals_in_sec)

        plt.xlim(left=0)
        self._set_x_ticks(x_unit)

        self._activate_grid()
        self._set_labels(x_unit, median_smooth)

        if len(modules_to_csv_paths) > 1:
            plt.legend()

        self.save_to_file()

    def __plot_core(self, name: str, df: pd.DataFrame, x_scale: int, color):
        plt.yscale("log")
        plt.plot(
            df["timestamp"] / x_scale * (10**6),
            df["entry_count"],
            marker="o",
            markersize=1,
            label=name,
            color=color,
        )

    @staticmethod
    def _get_x_label():
        return "Time"

    @staticmethod
    def _get_y_label():
        return "Fill Level"

    def _set_labels(self, x_unit: str, median_smooth: bool):
        plt.xlabel(f"{self._get_x_label()} [{x_unit}]", labelpad=10)
        plt.ylabel(
            f"{self._get_y_label()}" + (" (median-smoothed)" if median_smooth else "")
        )

    @staticmethod
    def _activate_grid():
        plt.grid(color="lightgray")


class EnteringProcessedTotalPlotGenerator(GraphPlotGenerator):

    def __init__(
        self,
        test_identifier: str,
        intervals_in_sec: Optional[list[int]] = None,
    ):
        plot_name = "entering_processed_total"
        super().__init__(plot_name=plot_name, test_identifier=test_identifier)

        self.intervals_in_sec = intervals_in_sec

        self.default_fig_size = (8.35, 4.8)

    def plot(
        self,
        fig_size: tuple[float, float] = None,
        color_start_index: int = 0,
        downsample_factor: int = 500,
    ):
        """TODO
        Creates a figure and plots the entering and processed log lines data. All graphs are plotted into the
        same figure, which is then stored as a file.

        Args:
            color_start_index (int): First index of the color palette to be used, 0 by default
            downsample_factor (int): Factor for downsampling data points, 500 by default
        """
        self._set_up_initial_figure(fig_size)
        start_time = self._get_start_time()

        colors = self._get_colors()  # initialize color palette for graphs
        cur_color_index = color_start_index

        modules_to_csv_paths = ReadWriteUtils.get_modules_to_csv_filepaths(
            self.plot_name, self.test_identifier
        )

        for name, file in modules_to_csv_paths.items():
            df = pd.read_csv(file)

            if df.empty:
                continue  # skip empty datafiles

            if "timestamp_in" in df.columns:
                timestamp_col = (
                    "timestamp_in"  # entering_total.csv contains "timestamp_in" column
                )
            elif "timestamp" in df.columns:
                timestamp_col = (
                    "timestamp"  # processed_total.csv contains "timestamp" column
                )
            else:
                logger.warning(f"No valid timestamp column found in {file}")
                continue

            df[timestamp_col] = pd.to_datetime(df[timestamp_col])
            df = df.sort_values(by=timestamp_col)
            df["time"] = (
                df[timestamp_col] - start_time.replace(tzinfo=None)
            ).dt.total_seconds()

            self.__plot_core(name, df, downsample_factor, colors[cur_color_index])
            cur_color_index += 1

        self._add_interval_lines(self.intervals_in_sec)

        plt.xlim(left=0)
        self._set_x_ticks("s")  # TODO: Make more flexible

        self._activate_grid()
        self._set_labels()

        plt.legend()

        self.save_to_file()

    def __plot_core(self, name: str, df: pd.DataFrame, downsample_factor: int, color):
        n = max(1, len(df) // downsample_factor)  # downsample for better performance
        plt.plot(
            df["time"][::n],
            df["cumulative_count"][::n],
            linestyle="-",
            label=name,
            color=color,
        )

    @staticmethod
    def _get_x_label():
        return "Time"

    @staticmethod
    def _get_y_label():
        return "Accumulated number of log lines"

    def _set_labels(self):
        plt.xlabel(f"{self._get_x_label()} [s]", labelpad=10)
        plt.ylabel(self._get_y_label())

    @staticmethod
    def _activate_grid():
        plt.grid(color="lightgray")


class EnteringProcessedPerTimePlotGenerator(PlotGenerator):
    pass


class LatenciesBoxplotGenerator(PlotGenerator):

    def __init__(
        self,
        test_identifier: str,
    ):
        plot_name = "latencies_boxplot"
        super().__init__(plot_name=plot_name, test_identifier=test_identifier)

        self.default_fig_size = (8.35, 4.8)

    def plot(
        self,
        fig_size: tuple[float, float] = None,
        y_input_unit: str = "microseconds",
    ):
        """Creates a boxplot figure showing latency distributions for different modules.

        Args:
            y_input_unit (str): Unit of the data given as input, "microseconds" by default
        """
        self._set_up_initial_figure(fig_size)

        modules_to_csv_paths = ReadWriteUtils.get_modules_to_csv_filepaths(
            self.plot_name, self.test_identifier
        )

        data = []
        labels = []

        for name, file in modules_to_csv_paths.items():
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

        self.__plot_core(data, labels)
        self._set_labels()

        self.save_to_file()

    def __plot_core(self, data: list, labels: list):
        boxplot_style = self._get_boxplot_style()

        plt.yscale("log")
        plt.tight_layout()
        plt.boxplot(
            data,
            labels=labels,
            vert=True,
            patch_artist=False,  # no filled boxes
            boxprops=boxplot_style.get("boxprops"),
            whiskerprops=boxplot_style.get("whiskerprops"),
            capprops=boxplot_style.get("capprops"),
            medianprops=boxplot_style.get("medianprops"),
            flierprops=boxplot_style.get("flierprops"),
        )

    @staticmethod
    def _get_y_label():
        return "Latency in module [s]"  # TODO: Make more flexible

    def _set_labels(self):
        plt.ylabel(self._get_y_label())

    @staticmethod
    def _get_boxplot_style() -> dict:
        return {
            "boxprops": dict(
                color="black",
                linewidth=1,
            ),
            "whiskerprops": dict(
                color="black",
                linewidth=1,
            ),
            "capprops": dict(
                color="black",
                linewidth=1,
            ),
            "medianprops": dict(
                color="black",
                linewidth=1.5,
            ),
            "flierprops": dict(
                marker="x",
                markerfacecolor="lightgray",
                markeredgecolor="lightgray",
                markersize=4,
                linestyle="none",
            ),
        }

    @staticmethod
    def _activate_grid():
        plt.grid(axis="y", linestyle="--", linewidth=0.5, alpha=0.7)

    @staticmethod
    def _set_x_ticks():
        plt.xticks(rotation=30, ha="right")
