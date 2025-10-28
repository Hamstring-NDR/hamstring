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
    def get(self) -> dict[tuple[int, int], SingleMetadataInformation]:
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
        self.total_duration = end_time - start_time
        self.start_time = start_time
        self.end_time = end_time

    def get(self) -> dict[tuple[int, int], SingleMetadataInformation]:
        return {
            # (1, 1): IntegerMetadataInformation(
            #     title="Total ingoing loglines",
            #     value=self.total_ingoing_loglines,
            # ),
            (1, 2): DurationMetadataInformation(
                title="Total duration",
                value=self.total_duration,
            ),
            (2, 2): NumberPerTimeMetadataInformation(
                title="Ingoing logline rate",
                value=self.total_ingoing_loglines / self.total_duration.total_seconds(),
                per="s",
            ),
            (1, 1): HourMinuteSecondMetadataInformation(
                title="Start time",
                value=self.start_time,
                include_date=(self.start_time.date() != self.end_time.date()),
            ),
            (2, 1): HourMinuteSecondMetadataInformation(
                title="End time",
                value=self.end_time,
                include_date=(self.start_time.date() != self.end_time.date()),
            ),
        }
