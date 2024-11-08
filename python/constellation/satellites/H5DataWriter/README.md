---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "H5DataWriter"
subtitle: "Satellite receiving data and writing it to HDF5 files "
---

## Description

This satellite receives data from all satellites and stores it in an HDF5 file.

## Requirements

The H5DataWriter satellite requires the `[hdf5]` component, which can be installed with:

::::{tab-set}
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e .[hdf5]
```

:::
:::{tab-item} PyPI
:sync: pypi

```sh
pip install ConstellationDAQ[hdf5]
```

:::
::::

This requires the HDF5 development libraries to be installed, which can be installed with:

::::{tab-set}
:::{tab-item} Debian/Ubuntu

```sh
sudo apt install libhdf5-dev
```

:::
:::{tab-item} ALMA/Fedora

TODO

:::
:::{tab-item} MacOS

TODO

:::
:::{tab-item} Windows

TODO

:::
::::
