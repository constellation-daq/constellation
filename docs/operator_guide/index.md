# Constellation Operator Guide

```{raw} latex
\chapter*{Preface}
\addcontentsline{toc}{chapter}{Preface}
```

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

Throughout this guide, buttons of graphical user interfaces will be displayed as {bdg-primary}`Buttons`, configuration
parameters and commands typeset as inline code such as `parameter_name`, and states of satellites or the entire Constellation
will be denominated e.g. by {bdg-secondary}`ORBIT`. Sequences of keystrokes are rendered as individual keys such as {kbd}`Control-c`.

```{warning}
This software framework is still under construction and no stable version has been released yet.
Features, protocols and the behavior of individual components may still change.
```

```{raw} latex
\part{Get started}
```

```{toctree}
:caption: Get started

get_started/install_from_flathub
get_started/install_from_pypi
get_started/install_from_source
```

```{raw} latex
\part{Tutorials}
```

```{toctree}
:caption: Tutorials

tutorials/single_satellite
tutorials/missioncontrol
```

```{raw} latex
\part{Concepts}
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

```{raw} latex
\part{How-To Guides}
```

```{toctree}
:caption: How-To Guides

howtos/startup_order
howtos/setup_influxdb_grafana
```
