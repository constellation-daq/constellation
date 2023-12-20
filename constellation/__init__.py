# Try to get setuptools_scm generated version (package must be installed)
try:
    from ._version import version as __version__
except ImportError:
    __version__ = "0.0+unknown"
