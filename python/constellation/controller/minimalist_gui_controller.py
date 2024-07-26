#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides a minimalist graphical user interface to control a Constellation.

FIXME: In its current state, this has to be considered a proof-of-concept.
"""

import sys
import time
import os
import uuid
from typing import Any
from functools import partial

from PyQt5.QtCore import QSize, QTimer, Qt
from PyQt5.QtWidgets import (
    QApplication,
    QPushButton,
    QHBoxLayout,
    QVBoxLayout,
    QWidget,
    QStyle,
    QMainWindow,
    QGridLayout,
    QLCDNumber,
    QLabel,
    QGroupBox,
    QMenu,
    QFileDialog,
    QMessageBox,
    QLineEdit,
)

from constellation.core.base import (
    EPILOG,
    ConstellationArgumentParser,
    setup_cli_logging,
)
from constellation.core.controller import BaseController, ControllerState
from constellation.core.configuration import load_config
from constellation.core.fsm import SatelliteState


class MinimalistGUIController(QMainWindow):
    """Class handling the main window of the minimalist GUI controller."""

    # styles for displaying Controller state
    state_style_map = {
        ControllerState.NEW: "font-size: 30pt; background-color:LightSkyBlue",
        ControllerState.INIT: "font-size: 30pt; background-color:LightSkyBlue",
        ControllerState.ORBIT: "font-size: 30pt; background-color:Green",
        ControllerState.RUN: "font-size: 30pt; background-color:DarkViolet",
        ControllerState.ERROR: "font-size: 30pt; background-color: FireBrick",
        ControllerState.TRANSITIONING: "font-size: 30pt; background-color: Gold",
    }

    # default background for buttons (when they are active)
    default_background = "LightGray"

    def __init__(self, parent=None, ctrl=None, cfg_dir=None):
        super().__init__(parent)

        if not ctrl:
            raise RuntimeError("Need a Constellation Controller to function")
        self.ctrl = ctrl
        self.in_progress = False

        # configuration variables
        self.cfg_dir = (
            os.path.dirname(os.path.abspath(__file__)) if not cfg_dir else cfg_dir
        )
        self.cfg_file = None
        self.cfg = {}

        # run variables
        self.run_id = ""
        self.run_sequence = 0

        # set up main control buttons
        pixmapi = QStyle.SP_MediaStop
        icon = self.style().standardIcon(pixmapi)

        self.stopBtn = QPushButton(icon=icon, text="Stop", parent=self)
        self.stopBtn.setIconSize(QSize(40, 40))
        self.stopBtn.setFixedWidth(150)
        self.stopBtn.setStyleSheet(f"background-color:{self.default_background}")

        self.stopBtn.clicked.connect(self.onStopBtnClicked)

        pixmapi = QStyle.SP_DialogApplyButton
        icon = self.style().standardIcon(pixmapi)

        self.configBtn = QPushButton(icon=icon, text="Configure", parent=self)
        self.configBtn.setIconSize(QSize(40, 40))
        self.configBtn.setFixedWidth(150)
        self.configBtn.setStyleSheet(f"background-color:{self.default_background}")
        self.configBtn.clicked.connect(self.onConfigBtnClicked)
        pixmapi = QStyle.SP_MediaPlay
        icon = self.style().standardIcon(pixmapi)

        self.startBtn = QPushButton(icon=icon, text="Start", parent=self)
        self.startBtn.setIconSize(QSize(40, 40))
        self.startBtn.setFixedWidth(150)
        self.startBtn.setStyleSheet(f"background-color:{self.default_background}")
        self.startBtn.clicked.connect(self.onStartBtnClicked)

        # configuration load and select buttons
        self.selectCfgBtn = QPushButton(text="Select configuration file", parent=self)

        self.cfgMenu = QMenu(self)
        self.updateCfgMenu()
        self.selectCfgBtn.setMenu(self.cfgMenu)

        pixmapi = QStyle.SP_DialogOpenButton
        icon = self.style().standardIcon(pixmapi)

        self.configDirSelectBtn = QPushButton(
            icon=icon, text="Select Directory", parent=self
        )
        self.configDirSelectBtn.setIconSize(QSize(40, 40))
        self.configDirSelectBtn.setFixedWidth(150)
        self.configDirSelectBtn.clicked.connect(self.onConfigDirSelectBtnClicked)

        conf_layout = QHBoxLayout()
        conf_layout.addWidget(self.selectCfgBtn)
        conf_layout.addWidget(self.configDirSelectBtn)

        # run identifier
        run_id_line = QLineEdit()
        run_id_line.setPlaceholderText("Run Base Identifier")
        run_id_line.textChanged.connect(self.onRunIdFieldEdit)
        self.run_sequence_line = QLineEdit(str(self.run_sequence))
        self.run_sequence_line.setReadOnly(True)
        self.run_sequence_line.setEnabled(False)
        self.run_sequence_line.setFixedWidth(150)

        runid_layout = QHBoxLayout()

        label = QLabel(self)
        label.setText("Run Id:")
        runid_layout.addWidget(label)
        runid_layout.addWidget(run_id_line)
        label = QLabel(self)
        label.setText("_")
        runid_layout.addWidget(label)
        runid_layout.addWidget(self.run_sequence_line)

        # state row
        self.state = QLabel()
        state = ctrl.state
        self.state.setText(state.name)
        self.state.setStyleSheet(self.state_style_map[state])
        self.state.setAlignment(Qt.AlignCenter)

        self.lcd = QLCDNumber()
        self.lcd.setDigitCount(2)
        self.lcd.setFixedHeight(100)

        lcd_label = QLabel()
        lcd_label.setText("connected")

        lcd_layout = QVBoxLayout()
        lcd_layout.addWidget(self.lcd)

        lcd_groupbox = QGroupBox()
        lcd_groupbox.setTitle("connected")
        lcd_groupbox.setLayout(lcd_layout)

        # layout
        layout = QGridLayout()
        control_row = 0
        layout.addWidget(self.configBtn, control_row, 0)
        layout.addWidget(self.startBtn, control_row, 1)
        layout.addWidget(self.stopBtn, control_row, 2)

        layout.addLayout(conf_layout, 1, 0, 1, -1)
        layout.addLayout(runid_layout, 2, 0, 1, -1)

        status_row = 3
        layout.addWidget(lcd_groupbox, status_row, 0)
        layout.addWidget(self.state, status_row, 1, 1, -1)
        layout.setSpacing(10)

        self.main_widget = QWidget(self)
        self.main_widget.setLayout(layout)
        self.setCentralWidget(self.main_widget)

        # set up a timer to regularly update the state fields
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_state_and_status)
        # check every second
        self.timer.start(1000 * 1)

        self.statusBar().showMessage("System starting up...")
        self.setWindowTitle(f"Minimalist {self.ctrl.group.capitalize()} Controller")

    def update_state_and_status(self):
        """Update the state and status fields."""
        self.statusBar().showMessage(f"{self.ctrl.status}")
        self.lcd.display(len(self.ctrl.constellation.satellites))
        state = self.ctrl.state
        self.state.setText(state.name)
        self.state.setStyleSheet(self.state_style_map[state])
        self.update_buttons()

    def onConfigDirSelectBtnClicked(self):
        """Select a configuration directory."""
        self.cfg_dir = QFileDialog.getExistingDirectory(
            self,
            "Select Configuration Directory",
            self.cfg_dir,
            QFileDialog.ShowDirsOnly,
        )
        self.updateCfgMenu()

    def updateCfgMenu(self):
        """Update the configuration selection menu with available config files."""
        self.cfgMenu.clear()
        files = [
            os.path.join(self.cfg_dir, f)
            for f in os.listdir(self.cfg_dir)
            if os.path.isfile(os.path.join(self.cfg_dir, f))
        ]
        configs = [c for c in files if os.path.splitext(c)[1] == ".toml"]
        for cfg in configs:
            action = self.cfgMenu.addAction(cfg)
            action.triggered.connect(partial(self.onConfigSelectClicked, cfg))

    def onConfigSelectClicked(self, cfg):
        """Load a selected configuration file."""
        self.selectCfgBtn.setText(os.path.basename(cfg))
        try:
            self.cfg = load_config(cfg)
        except Exception as e:
            print(f"Encountered error loading config '{cfg}': {repr(e)}")
            dlg = QMessageBox(self)
            dlg.setWindowTitle("Could not load configuration!")
            dlg.setText(f"Loading '{cfg}' resulted in an Exception: '{repr(e)}'")
            dlg.setStandardButtons(QMessageBox.Abort)
            dlg.setIcon(QMessageBox.Warning)
            dlg.exec()

    def onRunIdFieldEdit(self, text):
        """Update the run identifier after line was edited."""
        self.run_id = text
        self.run_sequence = 0
        self.run_sequence_line.setText(str(self.run_sequence))

    def onConfigBtnClicked(self):
        """Apply a configuration to the Constellation."""
        if not self.cfg:
            dlg = QMessageBox(self)
            dlg.setWindowTitle("No configuration selected!")
            dlg.setText(
                "No configuration file has been loaded yet. "
                "Do you want to continue with an empty configuration?"
            )
            dlg.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
            dlg.setIcon(QMessageBox.Warning)
            button = dlg.exec()
            if button == QMessageBox.No:
                return
        sat_states = self.ctrl.states
        for sat, state in sat_states.items():
            if (
                state == SatelliteState.NEW
                or state == SatelliteState.INIT
                or state == SatelliteState.ERROR
            ):
                getattr(self.ctrl.constellation.satellites[sat], "initialize")(self.cfg)
            elif state == SatelliteState.ORBIT:
                getattr(self.ctrl.constellation.satellites[sat], "reconfigure")(
                    self.cfg
                )
            elif state == SatelliteState.SAFE:
                getattr(self.ctrl.constellation.satellites[sat], "recover")(self.cfg)

    def onStartBtnClicked(self):
        """Execute Start."""
        if not self.run_id:
            rand_uuid = str(uuid.uuid1())
            dlg = QMessageBox(self)
            dlg.setWindowTitle("No run identifier entered!")
            dlg.setText(
                "No run identifier has been entered yet. "
                "Do you want to continue with an random string?\n"
                f"  e.g. '{rand_uuid}'?"
            )
            dlg.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
            dlg.setIcon(QMessageBox.Warning)
            button = dlg.exec()
            if button == QMessageBox.No:
                return
            run_id = rand_uuid
        else:
            # FIXME this should probably be incremented somewhere else; when stopping a run?
            self.run_sequence += 1
            self.run_sequence_line.setText(str(self.run_sequence))
            run_id = f"{self.run_id}_{self.run_sequence}"

        self.in_progress = True
        self.update_buttons()
        # launch
        for sat, state in self.ctrl.states.items():
            if state == SatelliteState.INIT:
                getattr(self.ctrl.constellation.satellites[sat], "launch")()
        # wait for transitions to finish
        while SatelliteState.launching in self.ctrl.states.values():
            time.sleep(0.2)
        # wait a little bit extra
        time.sleep(0.2)
        # start
        for sat, state in self.ctrl.states.items():
            if state == SatelliteState.ORBIT:
                getattr(self.ctrl.constellation.satellites[sat], "start")(run_id)
        self.in_progress = False

    def onStopBtnClicked(self):
        """Execute Stop."""
        sat_states = self.ctrl.states
        for sat, state in sat_states.items():
            if state == SatelliteState.RUN:
                getattr(self.ctrl.constellation.satellites[sat], "stop")()

    def update_buttons(self):
        """Update all main control buttons' states."""
        states = self.ctrl.states.values()
        self.configure_button_state(self.stopBtn, states, [SatelliteState.RUN])
        self.configure_button_state(
            self.startBtn, states, [SatelliteState.ORBIT, SatelliteState.INIT]
        )
        self.configure_button_state(
            self.configBtn,
            states,
            [
                SatelliteState.ERROR,
                SatelliteState.SAFE,
                SatelliteState.NEW,
                SatelliteState.INIT,
                SatelliteState.ORBIT,
            ],
        )

    def configure_button_state(self, btn, states, expected_states):
        """Configure a single button's state."""
        if self.in_progress or not any(state in states for state in expected_states):
            # nothing to act on or busy
            btn.setEnabled(False)
            btn.setStyleSheet(f"background-color:{self.default_background}")
        elif all(s in expected_states for s in states):
            # button active, usual situation
            btn.setEnabled(True)
            btn.setStyleSheet(f"background-color:{self.default_background}")
        else:
            # mixed situation; can only stop some
            btn.setEnabled(True)
            btn.setStyleSheet("background-color:orange")


def main(args: Any = None) -> None:
    """Start the Minimalist GUI Controller for a given Constellation."""
    parser = ConstellationArgumentParser(description=main.__doc__, epilog=EPILOG)
    parser.add_argument(
        "-c",
        "--config-dir",
        type=str,
        help="Path to the directory keeping the TOML configuration files.",
    )
    # set the default arguments
    parser.set_defaults(name="controller")
    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # set up logging
    logger = setup_cli_logging(args["name"], args.pop("log_level"))

    cfg_dir = args.pop("config_dir")

    logger.debug("Starting up CLI Controller!")

    # start server with args
    ctrl = BaseController(**args)

    app = QApplication(sys.argv)
    window = MinimalistGUIController(ctrl=ctrl, cfg_dir=cfg_dir)
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
