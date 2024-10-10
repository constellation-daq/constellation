#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides a single-button graphical user interface to send a command to a Constellation.
"""

import sys
from typing import Any

from PyQt6.QtWidgets import (
    QApplication,
    QPushButton,
    QGridLayout,
    QWidget,
    QMainWindow,
)

from constellation.core.base import (
    EPILOG,
    ConstellationArgumentParser,
    setup_cli_logging,
)
from constellation.core.controller import BaseController


class SingleButtonGUIController(QMainWindow):
    """Class handling the main window of the minimalist GUI controller."""

    def __init__(self, parent=None, ctrl=None, cmd=None):
        super().__init__(parent)

        if not ctrl:
            raise RuntimeError("Need a Constellation Controller to function")
        self.ctrl = ctrl

        if not cmd:
            raise RuntimeError("Need a command to call")
        self.cmd = cmd

        self.setFixedWidth(200)
        self.setFixedHeight(200)

        # set up the button
        self.theBtn = QPushButton(text=self.cmd, parent=self)
        self.theBtn.setFixedHeight(150)
        self.theBtn.setFixedWidth(150)
        self.theBtn.setStyleSheet("background-color:FireBrick")
        self.theBtn.clicked.connect(self.onBtnClicked)

        layout = QGridLayout()
        layout.addWidget(self.theBtn, 0, 0)

        self.main_widget = QWidget(self)
        self.main_widget.setLayout(layout)
        self.setCentralWidget(self.main_widget)

        self.setWindowTitle(f"Single-Button {self.ctrl.group.capitalize()} Controller")

    def onBtnClicked(self):
        """Execute Stop."""
        self.ctrl.command(cmd=self.cmd)


def main(args: Any = None) -> None:
    """Start the Minimalist GUI Controller for a given Constellation."""
    parser = ConstellationArgumentParser(description=main.__doc__, epilog=EPILOG)

    parser.add_argument(
        "-c",
        "--command",
        required=True,
        type=str,
        help="Command to be called by the button",
    )

    # set the default arguments
    parser.set_defaults(name="single_button_ctrl")
    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # set up logging
    logger = setup_cli_logging(args["name"], args.pop("log_level"))
    logger.debug("Starting up Single Button Controller!")

    cmd = args.pop("command")

    # start server with args
    ctrl = BaseController(**args)

    app = QApplication(sys.argv)
    window = SingleButtonGUIController(ctrl=ctrl, cmd=cmd)
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
