# Building as Flatpak

Constellation can be built and distributed as [Flatpak](https://flatpak.org/). The advantage is that this only requires an
installation of Meson and Flatpak to build and makes it easy to distribute Constellation to the users.

Information on how to start and run the Constellation Flatpak can be found in the
[operator guide](../operator_guide/get_started/install_from_flathub.md#usage-notes).

## Setting up Flatpak

The installation instruction for Flatpak can be found on the [Flatpak website](https://flatpak.org/setup/).
To build the Flatpak, some additional steps have to be taken.

First, [Flathub](https://flathub.org/) needs to be added as a repository user-wide:

```sh
flatpak remote-add --if-not-exists --user flathub https://dl.flathub.org/repo/flathub.flatpakrepo
```

Then the Flatpak Builder needs to be installed:

```sh
flatpak install -y --user flathub org.flatpak.Builder
```

## Preparing the Flatpak

To build the Flatpak, a manifest for the Flatpak needs to be created. It should be named `de.desy.constellation.yml` and
located in a folder named `flatpak` within a clone of the Constellation repository with the following contents:

```yaml
id: de.desy.constellation
runtime: org.kde.Platform
runtime-version: "6.8"
sdk: org.kde.Sdk
command: Satellite
finish-args:
  - --share=network
  - --filesystem=home
  - --share=ipc
  - --socket=fallback-x11
  - --socket=wayland
  - --device=dri
cleanup:
  - /include
  - /lib/pkgconfig
  - /share/pkgconfig
modules:
  - name: Constellation
    buildsystem: meson
    config-opts:
      - -Dbuildtype=release
      - -Dimpl_py=disabled
      - -Dcxx_tools=false
      - -Dcxx_tests=disabled
      - -Dbuild_gui=qt6
      - -Dcontroller_missioncontrol=true
      - -Dlistener_observatory=true
      - -Dsatellite_dev_null_receiver=true
      - -Dsatellite_eudaq_native_writer=true
      - -Dsatellite_random_transmitter=true
      - -Dsatellite_sputnik=true
    sources:
      - type: dir
        path: ..
```

Since internet access is not available during the Flatpak build, all dependencies need to be downloaded beforehand. This can
be easily done with Meson:

```sh
meson subprojects download
```

## Building the Flatpak

The Flatpak can be built by running the following command in the `flatpak` folder:

```sh
flatpak run --user org.flatpak.Builder --force-clean --user --install --install-deps-from=flathub --mirror-screenshots-url=https://dl.flathub.org/media/ --ccache --repo=repo builddir de.desy.constellation.yml
```

The Flatpak should now be installed for the current user.

After the build, the linter can be run to check for any blockers for the submission to Flathub:

```sh
flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest de.desy.constellation.yml
flatpak run --command=flatpak-builder-lint org.flatpak.Builder repo repo
```
