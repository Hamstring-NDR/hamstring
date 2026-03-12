import os
import sys

sys.path.append(os.getcwd())
from src.alerter.alerter import AlerterBase
from src.base.log_config import get_logger

module_name = "src.alerter.hello_world_alerter"
logger = get_logger(module_name)


class HelloWorldAlerter(AlerterBase):
    """
    Alerter implementation that appends a hello_world attribute to the alert JSON payload
    before passing it onto the base logging implementations.
    """

    def process_alert(self):
        logger.info("HelloWorldAlerter: Appending hello_world information to payload")
        if isinstance(self.alert_data, dict):
            self.alert_data["hello_world"] = "This is a sample payload addition."
        else:
            logger.warning("HelloWorldAlerter: Payload is not a JSON dict, skipping mutation")
