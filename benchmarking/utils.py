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


class TimeUtils:
    @staticmethod
    def now() -> datetime.timestamp:
        """Returns the current UTC time as timezone-aware datetime timestamp.
        Must be used for all internal timestamps."""
        return datetime.now(timezone.utc)
