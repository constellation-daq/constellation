# NOTE: This is currently just a template to build on.

import sys
import logging
import readline

from PyQt5.QtWidgets import QApplication, QWidget, QPushButton, QVBoxLayout

from .controller import TrivialController, CliCompleter
from .fsm import SatelliteFSM


class SimpleGUI(QWidget):
    def __init__(self):
        super().__init__()

        self.init_ui()

    def init_ui(self):
        # Create a QPushButton
        button = QPushButton("Click me!", self)

        # Connect the button to a function (slot) that will be called when the button is clicked
        button.clicked.connect(self.on_button_click)

        # Create a QVBoxLayout to arrange the button vertically
        layout = QVBoxLayout()
        layout.addWidget(button)

        # Set the layout for the main window
        self.setLayout(layout)

        # Set window properties
        self.setWindowTitle("Simple PyQt GUI")
        self.setGeometry(300, 300, 300, 200)

        # Show the window
        self.show()

    def on_button_click(self):
        print("Button clicked!")


def main():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--satellite", "--sat", action="append")
    args = parser.parse_args()

    logging.basicConfig(
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        level=args.log_level.upper(),
    )
    if not args.satellite:
        print("No satellites specified! Use '--satellite' to add one.")
        return
    # Set up simple tab completion
    commands = ["exit", "get_state", "transition ", "failure", "register "]
    transitions = [t.name for t in SatelliteFSM.events]

    cliCompleter = CliCompleter(list(set(commands)), list(set(transitions)))
    readline.set_completer_delims(" \t\n;")
    readline.set_completer(cliCompleter.complete)
    readline.parse_and_bind("tab: complete")

    # start server with args
    ctrl = TrivialController(hosts=args.satellite)
    ctrl.run_from_interface()  # Place in thread
    app = QApplication(sys.argv)
    # window = SimpleGUI()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
