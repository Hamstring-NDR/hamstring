import os
import sys
from datetime import datetime, timezone
from pathlib import Path

import yaml

sys.path.append(os.getcwd())
from src.base.log_config import get_logger
from src.base.utils import setup_config

logger = get_logger()
config = setup_config()

BASE_DIR = Path(__file__).resolve().parent.parent  # project root directory

CONFIG_FILEPATH = os.path.join(os.path.dirname(__file__), "./benchmark_config.yaml")
DIRECTORY_STRUCTURE_FILEPATH = os.path.join(
    os.path.dirname(__file__), "./data_directory_structure.yaml"
)


class ReadWriteUtils:
    @staticmethod
    def setup_config():
        """
        Loads the configuration data from the configuration file and returns it as the corresponding Python object.

        Returns:
             Configuration data as corresponding Python object

        Raises:
            FileNotFoundError: Configuration file could not be opened
        """
        try:
            logger.debug(
                f"Opening benchmark test configuration file at {CONFIG_FILEPATH}..."
            )
            with open(CONFIG_FILEPATH, "r") as file:
                configuration = yaml.safe_load(file)
        except FileNotFoundError:
            logger.critical(f"File {CONFIG_FILEPATH} does not exist. Aborting...")
            raise

        logger.debug("Configuration file successfully opened and information returned.")
        return configuration

    @staticmethod
    def write_metadata(metadata_filepath: Path, data: dict):
        try:
            with open(metadata_filepath, "w") as file:
                yaml.dump(data, file, default_flow_style=False)
        except FileNotFoundError:
            logger.critical(f"File {metadata_filepath} does not exist. Aborting...")
            raise

    @staticmethod
    def get_metadata(test_identifier: str):
        metadata_filepath = Path(
            BASE_DIR / "benchmark_results" / test_identifier / "metadata.yml"
        )

        try:
            with open(metadata_filepath, "r") as file:
                data = yaml.safe_load(file)
        except FileNotFoundError:
            logger.critical(f"File {metadata_filepath} does not exist. Aborting...")
            raise

        return data

    @staticmethod
    def get_modules_to_csv_filepaths(for_plot: str, test_identifier: str):
        try:
            with open(DIRECTORY_STRUCTURE_FILEPATH, "r") as file:
                data = yaml.safe_load(file)
        except FileNotFoundError:
            logger.critical(
                f"File {DIRECTORY_STRUCTURE_FILEPATH} does not exist. Aborting..."
            )
            raise

        try:
            result = data[for_plot]["files"]
        except KeyError:
            logger.critical(
                f"Invalid data directory structure configuration or given plot name does not exist"
            )
            raise

        for module in result.keys():
            filename = result[module]
            result[module] = str(
                Path(
                    BASE_DIR / "benchmark_results" / test_identifier / "data" / filename
                )
            )

        return result

    @staticmethod
    def get_plot_output_filepath(for_plot: str, file_identifier: str):
        try:
            with open(DIRECTORY_STRUCTURE_FILEPATH, "r") as file:
                data = yaml.safe_load(file)
        except FileNotFoundError:
            logger.critical(
                f"File {DIRECTORY_STRUCTURE_FILEPATH} does not exist. Aborting..."
            )
            raise

        try:
            output_filename = data[for_plot]["output_filename"]
        except KeyError:
            logger.critical(
                f"Invalid data directory structure configuration or given plot name does not exist"
            )
            raise

        output_filename = Path(
            BASE_DIR
            / "benchmark_results"
            / file_identifier
            / "graphs"
            / output_filename
        )
        return output_filename


class TimeUtils:
    @staticmethod
    def now() -> datetime.timestamp:
        """Returns the current UTC time as timezone-aware datetime timestamp.
        Must be used for all internal timestamps."""
        return datetime.now(timezone.utc)
