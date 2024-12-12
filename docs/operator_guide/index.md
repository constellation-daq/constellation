# Constellation Operator Guide

Welcome to the Constellation Operator Guide. This guide is intended to provide a comprehensive overview of the framework for
people who will set up and operate Constellations, control satellites and monitor the performance of the system.

```{seealso}
Separate guides are provided for those who intend to [integrate their own hardware or develop new satellites](../application_development/index.md)
as well as for those who wish to contribute to the [development of Constellation](../framework_reference/index.md) and
require more in-depth technical information.
```

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

get_started/install_from_source
get_started/install_from_pypi
```

```{toctree}
:caption: Tutorials

tutorials/single_satellite
```

```{toctree}
:caption: Concepts

concepts/constellation
concepts/satellite
concepts/controller
concepts/autonomy
concepts/logging
concepts/telemetry
concepts/data
```

```{toctree}
:caption: How-To Guides

howtos/setup_influxdb_grafana
```
