#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import os
import pathlib
import threading
import time
from tempfile import TemporaryDirectory
from unittest.mock import MagicMock, patch

import h5py
import zmq
import numpy as np
import pytest
from conftest import mocket, wait_for_state
from constellation.core.broadcastmanager import DiscoveredService
from constellation.core.cdtp import CDTPMessageIdentifier, DataTransmitter
from constellation.core.chirp import CHIRPServiceIdentifier, get_uuid
from constellation.core.cscp import CommandTransmitter
from constellation.core.datareceiver import H5DataReceiverWriter
from constellation.core.datasender import DataSender
from constellation.core import __version__

DATA_PORT = 50101
CMD_PORT = 10101
FILE_NAME = "mock_file_{run_identifier}.h5"


@pytest.fixture
def mock_data_transmitter(mock_socket_sender: mocket):
    mock_socket_sender.port = DATA_PORT
    t = DataTransmitter("mock_sender", mock_socket_sender)
    yield t


@pytest.fixture
def data_transmitter():
    ctx = zmq.Context()
    socket = ctx.socket(zmq.PUSH)
    socket.bind(f"tcp://127.0.0.1:{DATA_PORT}")
    t = DataTransmitter("simple_sender", socket)
    yield t


@pytest.fixture
def commander():
    ctx = zmq.Context()
    socket = ctx.socket(zmq.REQ)
    socket.connect(f"tcp://127.0.0.1:{CMD_PORT}")
    commander = CommandTransmitter("cmd", socket)
    yield commander


@pytest.fixture
def mock_data_receiver(mock_socket_receiver: mocket):
    mock_socket_receiver.port = DATA_PORT
    r = DataTransmitter("mock_receiver", mock_socket_receiver)
    yield r


@pytest.fixture
def mock_sender_satellite(mock_chirp_socket):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        m.endpoint = 1
        return m

    class MockSenderSatellite(DataSender):
        def do_run(self, payload: any):
            self.payload_id = 0
            while self.payload_id < 10:
                self.data_queue.put((f"mock payload {self.payload_id}", {}))
                self.payload_id += 1
                time.sleep(0.02)
            return "Send finished"

    with patch("constellation.core.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = MockSenderSatellite(
            name="mydevice1",
            group="mockstellation",
            cmd_port=CMD_PORT,
            mon_port=22222,
            hb_port=33333,
            data_port=DATA_PORT,
            interface="127.0.0.1",
        )
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


@pytest.fixture
def receiver_satellite():
    """A receiver Satellite."""

    class MockReceiverSatellite(H5DataReceiverWriter):
        pass

    s = MockReceiverSatellite(
        name="mock_receiver",
        group="mockstellation",
        cmd_port=CMD_PORT,
        mon_port=22222,
        hb_port=33333,
        interface="127.0.0.1",
    )
    t = threading.Thread(target=s.run_satellite)
    t.start()
    # give the threads a chance to start
    time.sleep(0.2)
    yield s


@pytest.mark.forked
def test_datatransmitter(
    mock_data_transmitter: DataTransmitter, mock_data_receiver: DataTransmitter
):
    sender = mock_data_transmitter
    rx = mock_data_receiver

    sender.send_start("mock payload")
    msg = rx.recv()

    assert msg.payload == "mock payload"
    assert msg.msgtype == CDTPMessageIdentifier.BOR


@pytest.mark.forked
def test_sending_package(
    mock_sender_satellite,
    mock_data_receiver,
):
    mock = mocket()
    mock.return_value = mock
    mock.endpoint = 0
    mock.port = CMD_PORT

    commander = CommandTransmitter("cmd", mock)
    transmitter = mock_sender_satellite
    rx = mock_data_receiver

    commander.send_request("initialize", {"mock key": "mock argument string"})
    wait_for_state(transmitter.fsm, "INIT")
    commander.send_request("launch")
    wait_for_state(transmitter.fsm, "ORBIT")
    commander.send_request("start", "100102")
    wait_for_state(transmitter.fsm, "RUN")
    assert (
        transmitter.fsm.current_state.id == "RUN"
    ), "Could not set up test environment"

    BOR = True
    for idx in range(11):
        msg = rx.recv()
        if BOR:
            assert msg.msgtype == CDTPMessageIdentifier.BOR
            assert msg.payload != f"mock payload {idx}"
            BOR = False
        else:
            assert msg.msgtype == CDTPMessageIdentifier.DAT
            assert msg.payload == f"mock payload {idx - 1}"
            # assert msg.sequence_number == idx, "Sequence number not expected order"
            # assert msg.name == "mock sender"

    time.sleep(0.2)
    # still in RUN?
    assert transmitter.fsm.current_state.id == "RUN", "Ended RUN state early"
    # go to stop to send EOR
    commander.send_request("stop", "")
    wait_for_state(transmitter.fsm, "ORBIT")
    # EOR
    msg = rx.recv()
    assert msg.msgtype == CDTPMessageIdentifier.EOR
    assert transmitter.payload_id == 10


@pytest.mark.forked
def test_receive_writing_package(
    receiver_satellite,
    data_transmitter,
    commander,
):
    service = DiscoveredService(
        get_uuid("simple_sender"),
        CHIRPServiceIdentifier.DATA,
        "127.0.0.1",
        port=DATA_PORT,
    )

    receiver = receiver_satellite
    tx = data_transmitter
    with TemporaryDirectory() as tmpdir:
        commander.request_get_response(
            "initialize", {"file_name_pattern": FILE_NAME, "output_path": tmpdir}
        )
        wait_for_state(receiver.fsm, "INIT", 1)
        receiver._add_sender(service)
        commander.request_get_response("launch")
        wait_for_state(receiver.fsm, "ORBIT", 1)

        payload = np.array(np.arange(1000), dtype=np.int16)
        assert receiver.run_identifier == ""

        for run_num in range(1, 3):
            # Send new data to handle
            tx.send_start(["mock_start"])
            # send once as byte array with and once w/o dtype
            tx.send_data(payload.tobytes(), {"dtype": f"{payload.dtype}"})
            tx.send_data(payload.tobytes())
            tx.send_end(["mock_end"])
            time.sleep(0.1)

            # Running satellite
            commander.request_get_response("start", str(run_num))
            wait_for_state(receiver.fsm, "RUN", 1)
            assert (
                receiver.fsm.current_state.id == "RUN"
            ), "Could not set up test environment"
            commander.request_get_response("stop")
            wait_for_state(receiver.fsm, "ORBIT", 1)
            assert receiver.run_identifier == str(run_num)

            # Does file exist and has it been written to?
            bor = "BOR"
            eor = "EOR"
            dat = [f"data_{run_num}_{i}" for i in range(1, 3)]

            file = FILE_NAME.format(run_identifier=run_num)
            assert os.path.exists(os.path.join(tmpdir, file))
            h5file = h5py.File(tmpdir / pathlib.Path(file))
            assert "simple_sender" in h5file.keys()
            assert bor in h5file["simple_sender"].keys()
            assert "mock_start" in str(
                h5file["simple_sender"][bor]["payload"][0], encoding="utf-8"
            )
            assert eor in h5file["simple_sender"].keys()
            assert "mock_end" in str(
                h5file["simple_sender"][eor]["payload"][0], encoding="utf-8"
            )
            assert set(dat).issubset(
                h5file["simple_sender"].keys()
            ), "Data packets missing in file"
            assert (payload == h5file["simple_sender"][dat[0]]).all()
            # interpret the uint8 values again as uint16:
            assert (
                payload == np.array(h5file["simple_sender"][dat[1]]).view(np.uint16)
            ).all()
            assert (
                h5file["MockReceiverSatellite.mock_receiver"]["constellation_version"][
                    ()
                ]
                == __version__.encode()
            )
            h5file.close()
