import datetime
import os
import sys
from abc import abstractmethod

sys.path.append(os.getcwd())
from benchmarking.test_runner.plotting.metadata_information import (
    IntegerMetadataInformation,
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
        total_ingoing_loglines: int,
        total_duration: datetime.timedelta,
        start_time: datetime.datetime,
        end_time: datetime.datetime,
    ):
        self.total_ingoing_loglines = total_ingoing_loglines
        self.total_duration = total_duration
        self.start_time = start_time
        self.end_time = end_time

    def get(self) -> dict[tuple[int, int], SingleMetadataInformation]:
        return {
            (1, 1): IntegerMetadataInformation(
                title="Total ingoing loglines",
                value=self.total_ingoing_loglines,
            ),
            (1, 2): DurationMetadataInformation(
                title="Total duration",
                value=self.total_duration,
            ),
            (1, 3): NumberPerTimeMetadataInformation(
                title="Ingoing logline rate",
                value=self.total_ingoing_loglines / self.total_duration.total_seconds(),
                per="s",
            ),
            (1, 5): HourMinuteSecondMetadataInformation(
                title="Start time",
                value=self.start_time,
            ),
            (2, 5): HourMinuteSecondMetadataInformation(
                title="End time",
                value=self.end_time,
            ),
        }
