# Installing from Flathub

Constellation is available on [Flathub](https://flathub.org/apps/de.desy.constellation), an app store for Linux-based
operating systems. Flathub is available for almost all Linux distributions, installation instructions for each distribution
can be found [here](https://flathub.org/setup).

After Flatpak and Flathub is set up, Constellation can be installed either via the app store of the desktop environment or
by running:

```sh
flatpak install flathub de.desy.constellation
```

Afterwards, the graphical user interfaces of Constellation show up in the application search of the
desktop environment.

```{important}
If the system was not restarted after Flatpak was installed, Constellation will not show up in the application search.
```

## Usage Notes

Flatpak is designed for distributing graphical user interfaces, not command-line interfaces. Thus to start a satellite, the
following command can to be used:

```sh
flatpak run de.desy.constellation -t Sputnik -n Flatpak -g edda
```

```{note}
Currently, only C++ satellites are available on Flathub.
```

Flatpak applications run in a sandbox with limited permissions to the file system. By default, Constellation can
only read and write to data in the user's home folder, which means that writing to `/data` with data receivers will fail.
However, file system permissions for specific paths like `/data` can be added to Constellation using e.g.
[Flatseal](https://flathub.org/apps/com.github.tchx84.Flatseal).

Flatpak is not suitable for application development, for which an [installation from source](./install_from_source.md) is
required. Information on how to build the Constellation Flatpak can be found in the
[framework reference](../../framework_reference/flatpak.md).
