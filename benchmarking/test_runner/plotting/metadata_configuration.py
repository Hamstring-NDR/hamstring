import datetime
import os
import sys
from abc import abstractmethod

sys.path.append(os.getcwd())
from benchmarking.test_runner.plotting.metadata_information import (
    DurationMetadataInformation,
    NumberPerTimeMetadataInformation,
    SingleMetadataInformation,
    HourMinuteSecondMetadataInformation,
)


class MetadataConfiguration:
    @abstractmethod
    def get(self, metadata: dict) -> dict[tuple[int, int], SingleMetadataInformation]:
        raise NotImplementedError


class RampUpMetadata(MetadataConfiguration):
    def __init__(
        self,
        # total_ingoing_loglines: int,
        start_time: datetime.datetime,
        end_time: datetime.datetime,
    ):
        if start_time >= end_time:
            raise ValueError("End time must be after start time")

        self.total_ingoing_loglines = 12345  # TODO: Only for testing

    def get(self, metadata: dict) -> dict[tuple[int, int], SingleMetadataInformation]:
        try:
            start_timestamp = metadata["start_timestamp"]
            end_timestamp = metadata["end_timestamp"]
            parameters = metadata["parameters"]

            # calculation
            total_duration = end_timestamp - start_timestamp
        except Exception:
            raise ValueError("Malformed or missing metadata values")

        return {
            # (1, 1): IntegerMetadataInformation(
            #     title="Total ingoing loglines",
            #     value=self.total_ingoing_loglines,
            # ),
            (1, 2): DurationMetadataInformation(
                title="Total duration",
                value=total_duration,
            ),
            (2, 2): NumberPerTimeMetadataInformation(
                title="Ingoing logline rate",
                value=self.total_ingoing_loglines / total_duration.total_seconds(),
                per="s",
            ),
            (1, 1): HourMinuteSecondMetadataInformation(
                title="Start time",
                value=start_timestamp.astimezone(),
                include_date=(start_timestamp.date() != end_timestamp.date()),
            ),
            (2, 1): HourMinuteSecondMetadataInformation(
                title="End time",
                value=end_timestamp.astimezone(),
                include_date=(start_timestamp.date() != end_timestamp.date()),
            ),
        }
