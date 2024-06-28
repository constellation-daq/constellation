=======
Logging
=======

This page documents the logging facilities for the Constellation C++ implementation.

**************
Logging Macros
**************

The following macros should be used for
any logging. They ensure proper evaluation of logging levels and additional conditions before the stream is evaluated.

.. doxygenfile:: constellation/core/logging/log.hpp
   :sections: define

*****************
The Log Namespace
*****************

.. doxygennamespace:: constellation::log
   :members:
   :protected-members:
   :undoc-members:
