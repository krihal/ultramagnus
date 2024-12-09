#!/usr/bin/env python3

#
# This script listens for a client to connect to a UNIX socket and
# then starts collecting GRE packets on the specified interface and
# stores them in a dictionary. The sequence numbers of the packets are
# then used to determine if all packets were received in order or lost.
#

import socket
import os
import argparse
import time
import signal

from log import get_logger
from scapy.config import conf
from scapy.all import GRE, IP, AsyncSniffer

conf.use_pcap = True
conf.debug_dissector = 2


prefixes = {}
notification_sock = None

logger = get_logger()


class TimeoutError(Exception):
    pass


def timeout(seconds=60):
    """
    Wrapper to add a timeout to a function
    """

    def decorator(func):
        def __handle_timeout__(signum, frame):
            raise TimeoutError("Function call timed out")

        def wrapper(*args, **kwargs):
            signal.signal(signal.SIGALRM, __handle_timeout__)
            signal.alarm(seconds)

            try:
                func(*args, **kwargs)
            finally:
                signal.alarm(0)

        return wrapper

    return decorator


def create_socket(path: str) -> socket.socket:
    """
    Create a UNIX socket on the given path
    """
    global notification_sock

    if os.path.exists(path):
        os.remove(path)

    # Create a UNIX socket on the given path
    notification_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    notification_sock.bind(path)
    notification_sock.listen(1)

    return notification_sock


def print_packets(prefixes: dict, train_len: int) -> None:
    """
    Analyze and print the received packets
    """

    logger.info(f"Recevied {len(prefixes)} packets")
    logger.info("=" * (os.get_terminal_size().columns - 25))

    for _, value in prefixes.items():
        seq = 0
        missmatch = False
        seqlist = []

        for packet in value:
            seqlist.append(packet["pkt_seq"])
            if packet["pkt_seq"] != seq:
                missmatch = True
            seq += 1

        src = packet["src"]
        dst = packet["dst"]
        sport = packet["sport"]
        dport = packet["dport"]

        line = f"{src:<10}:{sport:<5} ->  {dst}:{dport} : {seqlist}"

        if missmatch:
            line += " (OOO)"

        if len(seqlist) != train_len:
            line += " (!!!)"

        logger.info(line)


def get_notifications(notification_sock: socket.socket, iface: str) -> None:
    """
    Wait for a client to connect and then start receiving GRE packets
    """
    global prefixes

    logger.info("Server started, waiting for client to connect")

    while True:
        prefixes = {}
        conn, addr = notification_sock.accept()
        message = conn.recv(1024).decode()

        if "client_start" not in message:
            continue

        train_len = int(message.split(":")[1])

        logger.info(f"Client connected, {train_len} packets per prefix.")

        try:
            receive_packets(iface)
        except TimeoutError:
            logger.error("Timeout occurred, stopping the sniffer")

        print_packets(prefixes, train_len)


@timeout(60)
def receive_packets(iface: str) -> None:
    """
    Receive GRE packets and store them in a dictionary
    """

    def store_packet(pkt: GRE) -> None:
        global prefixes

        key = str(pkt[GRE][IP].dst) + ":" + str(pkt[GRE][IP].sport)

        if key not in prefixes:
            prefixes[key] = []

        prefixes[key].append(
            {
                "pkt_seq": int(pkt.load.decode()),
                "sport": pkt[GRE][IP].sport,
                "dport": pkt[GRE][IP].dport,
                "src": pkt[GRE][IP].src,
                "dst": pkt[GRE][IP].dst,
            }
        )

    try:
        sniffer = AsyncSniffer(iface=iface, filter="proto 47", prn=store_packet)
        sniffer.start()
    except TimeoutError:
        sniffer.stop()

    logger.info("Sniffer started, waiting for client to stop...")

    conn, addr = notification_sock.accept()
    message = conn.recv(1024).decode()

    if message == "client_done":
        logger.info("Client stopped, waiting for five seconds...")
        time.sleep(5)
        sniffer.stop()


def main() -> None:
    argparser = argparse.ArgumentParser()
    argparser.add_argument(
        "-s",
        "--sockpath",
        help="Path to the UNIX socket",
        required=True,
    )
    argparser.add_argument(
        "-i", "--iface", help="Interface to listen on", required=True
    )

    args = argparser.parse_args()
    conn = create_socket(args.sockpath)

    try:
        get_notifications(conn, args.iface)
    except KeyboardInterrupt:
        print("Bye!")


if __name__ == "__main__":
    main()
