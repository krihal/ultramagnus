import logging
import sys


def get_logger() -> logging.Logger:
    logger = logging.getLogger(sys.argv[0])

    if not logger.handlers:
        formatter = logging.Formatter("%(asctime)s: %(message)s")

        handler = logging.StreamHandler()
        handler.setFormatter(formatter)
        logger.addHandler(handler)

    logger.setLevel(logging.INFO)

    return logger
