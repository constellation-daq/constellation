## State -- Immature
* Only initialization implemented for now.

## Needs
IPbus installation. See https://ipbus.web.cern.ch/doc/user/html/index.html.
Provide path when setting up meson. E.g.
```
meson setup build -Dbuildtype=debugoptimized -Dsatellite_aidatlu=enabled -Dcactus_root=/opt/cactus/ % or
meson setup --reconfigure build -Dbuildtype=debugoptimized -Dsatellite_aidatlu=enabled -Dcactus_root=/opt/cactus/
% then
meson compile -C build
```
Set `LD_LIBRARY_PATH`, e.g.
```
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/cactus/lib
```
Run
```
./build/cxx/satellites/AidaTLU/satelliteAidaTLU -g Tatoo
```

## ToDo
* Aida files are copied from EUDAQ2, fixing only compiler warnings. Might need revision
* Make the `meson.build` file nicer, e.g. catch when cactus path is not provided.
* Introduce error handling instead of just printing `CRITICAL`
