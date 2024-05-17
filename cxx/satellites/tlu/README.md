## State -- Immature
* Compiles, but does not start, because exe can not find uhal libaries.
* Only initialization implemented for now.

## Needs
IPbus installation. See https://ipbus.web.cern.ch/doc/user/html/index.html.
Provide path when setting up meson. E.g.
```
meson setup build -Dcactus_root=/opt/cactus/
```

## ToDo
* Aida files are copied from EUDAQ2, fixing only compiler warnings. Might need revision
* Make the `meson.build` file nicer, e.g. catch when cactus path is not provided.
* Introduce error handling instead of just printing `CRITICAL`
