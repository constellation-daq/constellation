"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import tomllib
import json


class ConfigError(Exception):
    """Exception class for general issues with the configuration."""

    pass


class Configuration:
    """Class to track configuration variables and requests."""

    def __init__(self, config: dict = None):
        """Initialize configuration variables"""
        if not config:
            config = {}
        if not isinstance(config, dict):
            raise ConfigError
        self._config = config
        self._requested_keys: set[str] = set()

    def has_unused_values(self):
        """Check if any unused configuration keys exist."""
        return not self._requested_keys == set(self._config.keys())

    def get_unused_keys(self):
        """Return all unused configuration keys"""
        return set(self._config.keys()).difference(self._requested_keys)

    def get_applied(self) -> dict:
        """Return a dictionary of all used configuration items."""
        return {k: self._config[k] for k in self._requested_keys}

    def setdefault(self, key: str, default: any = None):
        """
        Return value from requested key in config with default value if specified.
        Mark key as requested in configuration.
        """
        self._requested_keys.add(key)
        return self._config.setdefault(key, default)

    def __getitem__(self, key: str):
        """
        Return value from requested key in config.
        Mark key as requested in configuration.
        """
        self._requested_keys.add(key)
        return self._config[key]

    def get_keys(self):
        """Return list of keys in config."""
        return list(self._config.keys())

    def get_json(self):
        """Return JSON-encoded configuration data."""
        return json.dumps(self._config)

    def update(self, config: dict) -> None:
        """Update the configuration with a new dict."""
        # update key+values of internal dict
        self._config.update(config)
        # remove all new keys from our set of requested keys
        for key in config.keys():
            self._requested_keys.discard(key)


def load_config(path: str) -> dict:
    """Load a TOML configuration from file."""
    try:
        with open(path, "rb") as f:
            config = tomllib.load(f)
    # TODO: Handle errors FileNotFoundError, TypeError, tomllibDecodeError
    except tomllib.TOMLDecodeError:
        raise
        # TODO: Handle TOMLDecodeError
    return config


def make_lowercase(obj):
    """Recursively lower-case all keys of a nested dictionary."""
    if isinstance(obj, dict):
        ret = {}
        for k, v in obj.items():
            ret[k.lower()] = make_lowercase(v)
        return ret
    else:
        # anything else
        return obj


def flatten_config(
    config: dict,
    sat_class: str,
    sat_name: str | None = None,
):
    """Get configuration of satellite. Specify category to only get part of config."""

    res = {}

    # make all input strings lower case, including keys
    config = make_lowercase(config)
    sat_class = sat_class.lower()
    # set global values
    for category in ["constellation", "satellites"]:
        try:
            for key, value in config[category].items():
                if not isinstance(value, dict):
                    res[key] = value
        except KeyError:
            pass

    # set class values
    for category in ["constellation", "satellites"]:
        try:
            for key, value in config[category][sat_class].items():
                if not isinstance(value, dict):
                    res[key] = value
        except KeyError:
            pass

    if sat_name:
        sat_name = sat_name.lower()
        for category in ["constellation", "satellites"]:
            try:
                for key, value in config[category][sat_class][sat_name].items():
                    if not isinstance(value, dict):
                        res[key] = value
            except KeyError:
                pass

    return res
