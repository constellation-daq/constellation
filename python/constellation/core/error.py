"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides error handling decorators and exceptions.
"""

from functools import wraps
import traceback
from statemachine.exceptions import TransitionNotAllowed


def handle_error(func):
    """Catch and handle exceptions in method calls inside a Satellite."""

    @wraps(func)
    def wrapper(self, *args, **kwargs):
        try:
            return func(self, *args, **kwargs)
        except TransitionNotAllowed as exc:
            err_msg = (
                f"Unable to execute {func.__name__} to transition to {exc.event}: "
            )
            err_msg += f"Not possible in {exc.state.name} state."
            raise RuntimeError(err_msg) from exc
        except Exception as exc:
            err_msg = f"Unable to execute {func.__name__}: {exc}"
            # set the FSM into failure
            self.fsm.failure(err_msg)
            self._wrap_failure()
            self.log.error(err_msg + traceback.format_exc())
            return None

    return wrapper


def debug_log(func):
    """Add debug messages to methods calls inside a Satellite."""

    @wraps(func)
    def wrapper(self, *args, **kwargs):
        self.log.debug("-> Entering %s with args: %s", func.__name__, args)
        output = func(self, *args, **kwargs)
        self.log.debug("<- Exiting %s with output: %s", func.__name__, output)
        return output

    return wrapper
