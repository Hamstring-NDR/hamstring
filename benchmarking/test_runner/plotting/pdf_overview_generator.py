import datetime
import os.path
import sys
from pathlib import Path

import pandas as pd
import pymupdf

sys.path.append(os.getcwd())
from src.base.log_config import get_logger
from benchmarking.test_runner.plotting.boxes import (
    MainTitleBox,
    SectionTitleBox,
    SectionContentImageBox,
    SectionSubtitleBox,
    SectionDoubleTitleBox,
    SectionDoubleSubtitleBox,
    SectionContentMetadataBox,
)
from benchmarking.test_runner.plotting.metadata_configuration import (
    MetadataConfiguration,
    RampUpMetadata,
)
from benchmarking.utils import ReadWriteUtils
from benchmarking.test_runner.plotting.plot_generator import PlotGenerator

logger = get_logger()

BASE_DIR = Path(__file__).resolve().parent.parent.parent.parent  # heiDGAF directory


class PDFOverviewGenerator:
    """Combines multiple plots and test information in a PDF document."""

    def __init__(self, metadata_configuration: MetadataConfiguration):
        self.page_width, self.page_height = 595, 842  # page dimension: A4 portrait
        self.standard_page_margin = {"left": 50, "right": 50, "top": 50, "bottom": 50}

        self.document = pymupdf.open()
        self.boxes = {}
        self.row_heights = {}

        self.metadata_configuration = metadata_configuration

    def setup_first_page_layout(
        self,
        test_directory_identifier: str = None,
        input_file_paths: dict[str, Path] = None,
        benchmark_test_date: datetime.date = datetime.date.today(),
    ):
        """Adds the first page and configures its layout.

        Args:
            test_directory_identifier (str): Identifying name of the benchmark_results directory for this test. Usually
                                             of the form "20250101_120000_ramp_up".
            input_file_paths (dict[str, Path]): Dictionary of input file paths for each figure position. Is set
                                                automatically to point to the graphs in
                                                'heiDGAF/benchmark_results/[TEST_DIRECTORY_IDENTIFIER]/graphs' and
                                                the metadata file
                                                'heiDGAF/benchmark_results/[TEST_DIRECTORY_IDENTIFIER]/metadata.yml'.
            benchmark_test_date: Date on which the test finished.
        """
        if input_file_paths is None and test_directory_identifier is not None:
            input_file_paths = {
                "main_graph": Path(
                    BASE_DIR
                    / "benchmark_results"
                    / test_directory_identifier
                    / "graphs"
                    / "latency_comparison.png"
                ),
                "first_detail_graph": Path(
                    BASE_DIR
                    / "benchmarking/graphs/latencies_boxplot.png"  # TODO: Update
                ),
                "second_detail_graph": Path(
                    BASE_DIR
                    / "benchmark_results"
                    / test_directory_identifier
                    / "graphs"
                    / "fill_levels_comparison.png"
                ),
                "third_detail_graph": Path(
                    BASE_DIR
                    / "benchmark_results"
                    / test_directory_identifier
                    / "graphs"
                    / "entering_processed_comparison.png"
                ),
                "fourth_detail_graph": Path(
                    BASE_DIR
                    / "benchmarking/graphs/entering_processed_bars.png"  # TODO: Update
                ),
                "metadata": Path(
                    BASE_DIR
                    / "benchmark_results"
                    / test_directory_identifier
                    / "metadata.yml"
                ),
            }
        elif input_file_paths is None and test_directory_identifier is None:
            raise ValueError(
                "Either test_directory_identifier or input_file_paths must be set"
            )

        metadata = ReadWriteUtils.load_metadata(input_file_paths["metadata"])

        page_margin, usable_width, usable_height = self.__prepare_overview_page()

        self.boxes["overview_page"]["main_title_row"].append(
            MainTitleBox(
                page=self.document[0],  # first page
                page_margin=page_margin,
                width=usable_width,
                height=self.row_heights["overview_page"][0] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:0]) * usable_height,
            ).fill(
                test_name="ramp-up",
                test_date=benchmark_test_date,
                generation_date=datetime.date.today(),
            )
        )

        self.boxes["overview_page"]["metadata_title_row"].append(
            SectionTitleBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width,
                height=self.row_heights["overview_page"][1] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:1]) * usable_height,
            ).fill(text="Metadata and parameters")
        )

        self.boxes["overview_page"]["metadata_row"].append(
            SectionContentMetadataBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width,
                height=self.row_heights["overview_page"][2] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:2]) * usable_height,
            ).fill(self.metadata_configuration.get(metadata))
        )

        self.boxes["overview_page"]["main_graph_title_row"].append(
            SectionTitleBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width,
                height=self.row_heights["overview_page"][3] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:3]) * usable_height,
            ).fill(text="Latency graphs")
        )

        self.boxes["overview_page"]["main_graph_subtitle_row"].append(
            SectionSubtitleBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width,
                height=self.row_heights["overview_page"][4] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:4]) * usable_height,
            ).fill(text="Comparison of all modules")
        )

        self.boxes["overview_page"]["main_graph_row"].append(
            SectionContentImageBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width,
                height=self.row_heights["overview_page"][5] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:5]) * usable_height,
            ).fill(file_path=input_file_paths["main_graph"])
        )

        self.boxes["overview_page"]["first_detail_graphs_titles_row"].append(
            SectionDoubleTitleBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width,
                height=self.row_heights["overview_page"][6] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:6]) * usable_height,
            ).fill(text_1="Latency comparison boxplot", text_2="Fill levels")
        )

        self.boxes["overview_page"]["first_detail_graphs_subtitles_row"].append(
            SectionDoubleSubtitleBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width,
                height=self.row_heights["overview_page"][7] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:7]) * usable_height,
            ).fill(
                text_1="Min/max/median per module", text_2="Comparison of all modules"
            )
        )

        self.boxes["overview_page"]["first_detail_graphs_row"].append(
            SectionContentImageBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width / 2,
                height=self.row_heights["overview_page"][8] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:8]) * usable_height,
            ).fill(file_path=input_file_paths["first_detail_graph"])
        )
        self.boxes["overview_page"]["first_detail_graphs_row"].append(
            SectionContentImageBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width / 2,
                height=self.row_heights["overview_page"][8] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:8]) * usable_height,
                left_padding=usable_width / 2,
            ).fill(file_path=input_file_paths["second_detail_graph"])
        )

        self.boxes["overview_page"]["second_detail_graphs_title_row"].append(
            SectionTitleBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width,
                height=self.row_heights["overview_page"][9] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:9]) * usable_height,
            ).fill(text="Total number of incoming and completely processed loglines")
        )

        self.boxes["overview_page"]["second_detail_graphs_subtitles_row"].append(
            SectionDoubleSubtitleBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width,
                height=self.row_heights["overview_page"][10] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:10]) * usable_height,
            ).fill(text_1="Accumulated to points in time", text_2="Per time period")
        )

        self.boxes["overview_page"]["second_detail_graphs_row"].append(
            SectionContentImageBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width / 2,
                height=self.row_heights["overview_page"][11] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:11]) * usable_height,
            ).fill(file_path=input_file_paths["third_detail_graph"])
        )
        self.boxes["overview_page"]["second_detail_graphs_row"].append(
            SectionContentImageBox(
                page=self.document[0],
                page_margin=page_margin,
                width=usable_width / 2,
                height=self.row_heights["overview_page"][11] * usable_height,
                top_padding=sum(self.row_heights["overview_page"][:11]) * usable_height,
                left_padding=usable_width / 2,
            ).fill(file_path=input_file_paths["fourth_detail_graph"])
        )

    def __prepare_overview_page(self):
        page_margin = self.standard_page_margin.copy()

        usable_width = (
            self.page_width - page_margin.get("left") - page_margin.get("right")
        )
        usable_height = (
            self.page_height - page_margin.get("top") - page_margin.get("bottom")
        )

        page = self.document.new_page(  # noqa
            0,  # insertion point: begin of document
            width=self.page_width,
            height=self.page_height,
        )

        self.__prepare_overview_page_boxes()
        self.__prepare_overview_page_row_heights()

        return page_margin, usable_width, usable_height

    def __prepare_overview_page_boxes(self):
        self.boxes["overview_page"] = {
            "main_title_row": [],  # 1st row
            "metadata_title_row": [],  # 2nd row
            "metadata_row": [],  # 3rd row
            "main_graph_title_row": [],  # 4th row
            "main_graph_subtitle_row": [],  # 5th row
            "main_graph_row": [],  # 6th row
            "first_detail_graphs_titles_row": [],  # 7th row
            "first_detail_graphs_subtitles_row": [],  # 8th row
            "first_detail_graphs_row": [],  # 9th row
            "second_detail_graphs_title_row": [],  # 10th row
            "second_detail_graphs_subtitles_row": [],  # 11th row
            "second_detail_graphs_row": [],  # 12th row
        }

    def __prepare_overview_page_row_heights(self):
        self.row_heights["overview_page"] = [
            0.045,  # 1st row: main_title_row
            0.025,  # 2nd row: metadata_title_row
            0.1,  # 3rd row: metadata_row
            0.025,  # 4th row: main_graph_title_row
            0.025,  # 5th row: main_graph_subtitle_row
            0.28,  # 6th row: main_graph_row
            0.025,  # 7th row: first_detail_graphs_titles_row
            0.025,  # 8th row: first_detail_graphs_subtitles_row
            0.2,  # 9th row: first_detail_graphs_row
            0.025,  # 10th row: second_detail_graphs_title_row
            0.025,  # 11th row: second_detail_graphs_subtitles_row
            0.2,  # 12th row: second_detail_graphs_row
        ]

    def save_file(self, relative_output_directory_path: Path, output_filename: str):
        """Stores the document as a file.

        Args:
            relative_output_directory_path (Path): Path to the directory in which to store the output file, relative to
                        the project base directory
            output_filename (str): Filename the output file should have, without file type
        """
        absolute_output_directory_path = BASE_DIR / relative_output_directory_path
        os.makedirs(absolute_output_directory_path, exist_ok=True)

        relative_output_filename = (
            relative_output_directory_path / f"{output_filename}.pdf"
        )
        absolute_output_filename = (
            absolute_output_directory_path / f"{output_filename}.pdf"
        )

        try:
            self.document.save(absolute_output_filename, garbage=4, deflate=True)
            logger.info(
                f"Successfully stored document under {relative_output_filename}"
            )
        except ValueError as err:  # includes zero page error
            logger.error(err)


# Only for testing
if __name__ == "__main__":
    plot_generator = PlotGenerator()

    MODULE_TO_CSV_FILENAME: dict[str, str] = {
        "Batch Handler": "batch_handler.csv",
        "Collector": "collector.csv",
        "Detector": "detector.csv",
        "Inspector": "inspector.csv",
        "Log Server": "logserver.csv",
        "Prefilter": "prefilter.csv",
    }

    module_to_filepath = (
        MODULE_TO_CSV_FILENAME.copy()
    )  # keep original dictionary unchanged
    for module in MODULE_TO_CSV_FILENAME.keys():
        filename = MODULE_TO_CSV_FILENAME[module]
        module_to_filepath[module] = str(
            Path(BASE_DIR / "benchmark_results/20251117_200230_ramp_up/data")
            / "latencies"
            / filename
        )

    plot_generator.plot_latency(
        datafiles_to_names=module_to_filepath,
        relative_output_directory_path=Path(
            BASE_DIR / "benchmark_results/20251117_200230_ramp_up/graphs"
        ),
        start_time=pd.Timestamp(
            datetime.datetime(
                year=2025, month=11, day=17, hour=18, minute=54, second=58
            )
        ),
        median_smooth=True,
        intervals_in_sec=[30, 30, 30, 30, 30, 30],
        datarates_per_interval=[1, 10, 50, 100, 150, 200],
    )

    MODULE_TO_CSV_FILENAME: dict[str, str] = {
        "Collector": "collector.csv",
        "Detector": "detector.csv",
        "Inspector": "inspector.csv",
        "Prefilter": "prefilter.csv",
        "Batch Handler (Batch)": "batch_handler_batch.csv",
        "Batch Handler (Buffer)": "batch_handler_buffer.csv",
    }

    module_to_filepath = (
        MODULE_TO_CSV_FILENAME.copy()
    )  # keep original dictionary unchanged
    for module in MODULE_TO_CSV_FILENAME.keys():
        filename = MODULE_TO_CSV_FILENAME[module]
        module_to_filepath[module] = str(
            Path(BASE_DIR / "benchmark_results/20251117_200230_ramp_up/data")
            / "log_volumes"
            / filename
        )

    plot_generator.plot_fill_levels(
        datafiles_to_names=module_to_filepath,
        relative_output_directory_path=Path(
            BASE_DIR / "benchmark_results/20251117_200230_ramp_up/graphs"
        ),
        start_time=pd.Timestamp(
            datetime.datetime(
                year=2025, month=11, day=17, hour=18, minute=54, second=58
            )
        ),
        median_smooth=True,
        intervals_in_sec=[30, 30, 30, 30, 30, 30],
        fig_width=8.35,
        fig_height=4.8,
    )

    MODULE_TO_CSV_FILENAME: dict[str, str] = {
        "Entering": "entering_total.csv",
        "Processed": "processed_total.csv",
    }

    module_to_filepath = (
        MODULE_TO_CSV_FILENAME.copy()
    )  # keep original dictionary unchanged
    for module in MODULE_TO_CSV_FILENAME.keys():
        filename = MODULE_TO_CSV_FILENAME[module]
        module_to_filepath[module] = str(
            Path(BASE_DIR / "benchmark_results/20251117_200230_ramp_up/data")
            / "entering_processed"
            / filename
        )

    plot_generator.plot_entering_processed(
        datafiles_to_names=module_to_filepath,
        relative_output_directory_path=Path(
            BASE_DIR / "benchmark_results/20251117_200230_ramp_up/graphs"
        ),
        start_time=pd.Timestamp(
            datetime.datetime(
                year=2025, month=11, day=17, hour=18, minute=54, second=58
            )
        ),
        intervals_in_sec=[30, 30, 30, 30, 30, 30],
        fig_width=8.35,
        fig_height=4.8,
    )

    generator = PDFOverviewGenerator(
        metadata_configuration=RampUpMetadata(),
    )

    generator.setup_first_page_layout(
        test_directory_identifier="20251117_200230_ramp_up",
        benchmark_test_date=datetime.date(2025, 11, 17),
    )

    generator.save_file(Path("benchmarking/testing_reports"), "report")
