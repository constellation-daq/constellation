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
from constellation.core.datasender import DataSender
from constellation.core import __version__
from constellation.satellites.H5DataWriter.H5DataWriter import H5DataWriter

DATA_PORT = 50101
MON_PORT = 22222
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
def mock_sender_satellite(mock_chirp_transmitter):
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

    class MockReceiverSatellite(H5DataWriter):
        def do_initializing(self, config: dict[str]) -> str:
            res = super().do_initializing(config)
            # configure monitoring with higher frequency
            self._configure_monitoring(0.1)
            return res

    s = MockReceiverSatellite(
        name="mock_receiver",
        group="mockstellation",
        cmd_port=CMD_PORT,
        mon_port=MON_PORT,
        hb_port=33333,
        interface="127.0.0.1",
    )
    t = threading.Thread(target=s.run_satellite)
    t.start()
    # give the threads a chance to start
    time.sleep(0.2)
    yield s


@pytest.mark.forked
def test_datatransmitter(mock_data_transmitter: DataTransmitter, mock_data_receiver: DataTransmitter):
    sender = mock_data_transmitter
    rx = mock_data_receiver

    # string
    payload = "mock payload"
    sender.send_start(payload)
    msg = rx.recv()
    assert msg.payload == payload
    assert msg.msgtype == CDTPMessageIdentifier.BOR

    # simple list
    payload = ["mock payload", "more mock data"]
    sender.send_data(payload)
    msg = rx.recv()
    assert msg.payload == payload
    assert msg.msgtype == CDTPMessageIdentifier.DAT

    # bytes
    payload = np.arange(0, 1000).tobytes()
    sender.send_data(payload)
    msg = rx.recv()
    assert msg.payload == payload
    assert msg.msgtype == CDTPMessageIdentifier.DAT

    # multi-frame bytes
    payload = [np.arange(0, 1000).tobytes(), np.arange(10000, 20000).tobytes()]
    sender.send_data(payload)
    msg = rx.recv()
    assert msg.payload == payload
    assert msg.msgtype == CDTPMessageIdentifier.DAT

    payload = None
    sender.send_data(payload)
    msg = rx.recv()
    assert msg.payload == payload
    assert msg.msgtype == CDTPMessageIdentifier.DAT

    payload = {"mock": True}
    sender.send_end(payload)
    msg = rx.recv()
    assert msg.payload == payload
    assert msg.msgtype == CDTPMessageIdentifier.EOR


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
    assert transmitter.fsm.current_state_value.name == "RUN", "Could not set up test environment"

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
    assert transmitter.fsm.current_state_value.name == "RUN", "Ended RUN state early"
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
    """Test receiving and writing data, verify state machine of DataSender."""
    service = DiscoveredService(
        get_uuid("simple_sender"),
        CHIRPServiceIdentifier.DATA,
        "127.0.0.1",
        port=DATA_PORT,
    )

    receiver = receiver_satellite
    tx = data_transmitter
    with TemporaryDirectory() as tmpdir:
        commander.request_get_response("initialize", {"file_name_pattern": FILE_NAME, "output_path": tmpdir})
        wait_for_state(receiver.fsm, "INIT", 1)
        receiver._add_sender(service)
        commander.request_get_response("launch")
        wait_for_state(receiver.fsm, "ORBIT", 1)

        payload = np.array(np.arange(1000), dtype=np.int16)
        assert receiver.run_identifier == ""

        for run_num in range(1, 4):
            # Send new data to handle
            tx.send_start({"mock_cfg": run_num, "other_val": "mockval"})
            # send once as byte array with and once w/o dtype
            tx.send_data(payload.tobytes(), {"dtype": f"{payload.dtype}"})
            tx.send_data(payload.tobytes())
            time.sleep(0.1)

            # Running satellite
            commander.request_get_response("start", str(run_num))
            wait_for_state(receiver.fsm, "RUN", 1)
            timeout = 0.5
            while not receiver.active_satellites or timeout < 0:
                time.sleep(0.05)
                timeout -= 0.05
            assert len(receiver.active_satellites) == 1, "No BOR received!"
            assert receiver.fsm.current_state_value.name == "RUN", "Could not set up test environment"
            commander.request_get_response("stop")
            time.sleep(0.5)
            # receiver should still be in 'stopping' as no EOR has been sent
            assert receiver.fsm.current_state_value.name == "stopping", "Receiver stopped before receiving EORE"
            # send EORE
            tx.send_end({"mock_end": f"whatanend{run_num}"})
            wait_for_state(receiver.fsm, "ORBIT", 1)
            assert receiver.run_identifier == str(run_num)

            # Does file exist and has it been written to?
            bor = "BOR"
            eor = "EOR"
            dat = [f"data_{run_num}_{i:09}" for i in range(1, 3)]

            fn = FILE_NAME.format(run_identifier=run_num)
            assert os.path.exists(os.path.join(tmpdir, fn))
            h5file = h5py.File(tmpdir / pathlib.Path(fn))
            assert "simple_sender" in h5file.keys()
            assert bor in h5file["simple_sender"].keys()
            assert h5file["simple_sender"][bor]["mock_cfg"][()] == run_num
            assert eor in h5file["simple_sender"].keys()
            assert "whatanend" in str(h5file["simple_sender"][eor]["mock_end"][()], encoding="utf-8")
            assert set(dat).issubset(h5file["simple_sender"].keys()), "Data packets missing in file"
            assert (payload == h5file["simple_sender"][dat[0]]).all()
            # interpret the uint8 values again as uint16:
            assert (payload == np.array(h5file["simple_sender"][dat[1]]).view(np.uint16)).all()
            assert h5file["MockReceiverSatellite.mock_receiver"]["constellation_version"][()] == __version__.encode()
            h5file.close()


@pytest.mark.forked
def test_receiver_stats(
    receiver_satellite,
    monitoringlistener,
    commander,
):
    """Test the stats sent by DataReceiver are received via CMDP."""
    dp = 23242
    ctx = zmq.Context()
    socket = ctx.socket(zmq.PUSH)
    socket.bind(f"tcp://127.0.0.1:{dp}")
    tx = DataTransmitter("simple_sender", socket)

    service = DiscoveredService(
        get_uuid("simple_sender"),
        CHIRPServiceIdentifier.DATA,
        "127.0.0.1",
        port=dp,
    )

    receiver = receiver_satellite
    ml, tmpdir = monitoringlistener
    commander.request_get_response("initialize", {"file_name_pattern": FILE_NAME, "output_path": tmpdir})
    wait_for_state(receiver.fsm, "INIT", 1)
    receiver._add_sender(service)
    commander.request_get_response("launch")
    wait_for_state(receiver.fsm, "ORBIT", 1)

    payload = np.array(np.arange(1000), dtype=np.int16)

    for run_num in range(1, 3):
        # Send new data to handle
        tx.send_start({"mock_cfg": 1, "other_val": "mockval"})
        # send once as byte array with and once w/o dtype
        tx.send_data(payload.tobytes(), {"dtype": f"{payload.dtype}"})
        tx.send_data(payload.tobytes())

        # Running satellite
        commander.request_get_response("start", str(run_num))
        wait_for_state(receiver.fsm, "RUN", 1)
        time.sleep(0.2)
        commander.request_get_response("stop")
        # send EOR
        tx.send_end({"mock_end": 22})
        wait_for_state(receiver.fsm, "ORBIT", 1)
    timeout = 4
    while timeout > 0 and not len(ml._metric_sockets) > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.receiver_stats) == 2
    assert len(receiver._metrics_callbacks) > 1
    assert len(ml._metric_sockets) == 1
    assert os.path.exists(os.path.join(tmpdir, "stats")), "Stats output directory not created"
    statfile = os.path.join(tmpdir, "stats", "MockReceiverSatellite.mock_receiver.nbytes.csv")
    timeout = 5
    while timeout > 0 and not os.path.exists(statfile):
        time.sleep(0.05)
        timeout -= 0.05
    assert os.path.exists(statfile), "Expected output metrics csv not found"
    # close thread and connections to allow temp dir to be removed
    ml._log_listening_shutdown()
    ml._metrics_listening_shutdown()
