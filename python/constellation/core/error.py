"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides error handling decorators and exceptions.
"""

from functools import wraps
import traceback
from statemachine.exceptions import TransitionNotAllowed
from typing import Callable, Any


def handle_error(func: Callable[..., Any]) -> Callable[..., Any]:
    """Catch and handle exceptions in method calls inside a Satellite."""

    @wraps(func)
    def wrapper(self: Any, *args: Any, **kwargs: Any) -> Any:
        try:
            return func(self, *args, **kwargs)
        except TransitionNotAllowed as exc:
            err_msg = (
                f"Unable to execute {func.__name__} to transition to {exc.event}: "
            )
            err_msg += f"Not possible in {exc.state.name} state."
            raise RuntimeError(err_msg) from exc
        except Exception as exc:
            err_msg = f"Unable to execute {func.__name__}: {repr(exc)}"
            # set the FSM into failure
            self.fsm.failure(err_msg)
            self._wrap_failure()
            self.log.critical(err_msg + traceback.format_exc())
            return None

    return wrapper


def debug_log(func: Callable[..., Any]) -> Callable[..., Any]:
    """Add debug messages to methods calls inside a Satellite."""

    @wraps(func)
    def wrapper(self: Any, *args: Any, **kwargs: Any) -> Any:
        self.log.trace(
            "-> Entering %s.%s with args: %s", type(self).__name__, func.__name__, args
        )
        output = func(self, *args, **kwargs)
        self.log.trace(
            "<- Exiting %s.%s with output: %s",
            type(self).__name__,
            func.__name__,
            output,
        )
        return output

    return wrapper
