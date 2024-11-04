# User Guide

Welcome to the Constellation User Guide. This guide is intended to provide a comprehensive overview of the framework for
operators as well as for people who want to integrate their own hardware or develop new satellites. For the development of
Constellation itself and for more in-depth technical information, there is a separate [Developer's Guide](../reference/index.md)
which should be consulted.

This guide is structured in four different parts, each of which serve a different purpose:

* The installation and initial setup of Constellation is described in the **Getting Started** section.

* **Tutorials** teach how to use Constellation using practical examples, starting from simple situations such as starting and
  controlling a single satellite, and gradually moving to more complex examples & setups.

* The **Concepts** section provides detailed explanation of the workings of the framework and the thoughts behind its structure.
  This is not the technical documentation of the Constellation core components, it describes their functionality and helps
  in developing an understanding of the system.

* Finally, the **How-To Guides** provide concise answers on how to achieve a specific goal, such as the implementation of a
  new satellite, or the extension of a satellite with custom commands.


```{warning}
This software framework is still under construction and no stable version has been released yet.
Features, protocols and the behavior of individual components may still change.
```

```{toctree}
:caption: Get started

install
```

```{toctree}
:caption: Tutorials

tutorials/single_satellite
```

```{toctree}
:caption: Concepts

concepts/constellation
concepts/logging
concepts/satellite
concepts/controller
concepts/listener
concepts/autonomy
concepts/statistics
```

```{toctree}
:caption: How-To Guides

howtos/satellite_cxx
howtos/satellite_py
howtos/custom_commands
howtos/port_eudaq
howtos/receiver_cxx
howtos/data_transmission_speed
```
