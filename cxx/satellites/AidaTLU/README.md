## State -- Almost there
Ported EUDAQ2 codes with minimal adaptation. Definitively room for improvement. Note that no data is stored at this point, since this is not yet supported on constellation side.

## Dependencies
Relies on the IPBUS suite as a hardware interface. See [here](https://ipbus.web.cern.ch/doc/user/html/index.html) for installation instructions.

## Building
Building requires two additional build options to be set. E.g.
```
meson setup build -Dbuildtype=debugoptimized -Dsatellite_aidatlu=enabled -Dcactus_root=/opt/cactus/ % or
meson setup --reconfigure build -Dbuildtype=debugoptimized -Dsatellite_aidatlu=enabled -Dcactus_root=/opt/cactus/
% then
meson compile -C build
```
where `/opt/cactus/` is assumed to be where you installed cactus (the IPBUS stuff).

## Running
You need to start the start the cactus control hub, by running
```
/opt/cactus/bin/controlhub_start
```
and add the cactus library to `LD_LIBRARY_PATH`, e.g. by executing
```
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/cactus/lib
```
Alternatively you can source this script `cxx/satellites/AidaTLU/misc/setup_aida-tlu.sh`.

Now you are ready to run the satellite:
```
./build/cxx/satellites/AidaTLU/satelliteAidaTLU -g Tatoo -n Tatooine
```

## Controlling
To start python controller:
```
python -m constellation.core.controller --group Tatoo --config cxx/satellites/AidaTLU/misc/aida_tlu_example_configuration.toml
```
, using an example configuration file that is provided with the framework. Documentation regarding the configuration parameters can be found [here](https://ohwr.org/project/fmc-mtlu/blob/master/Documentation/Main_TLU.pdf). The naming conventions are unaltered with respect to EUDAQ2.

Now you can use the python interface to control the `AidaTLUSatellite`, e.g.
```
constellation.satellites.get('AidaTLU.Tatooine').get_name()
% or
constellation.AidaTLU.Tatooine.get_name()
% or
constellation.AidaTLU.Tatooine.initialize(cfg)
```
Note that the latter option supports tab completion

## ToDo
* Aida files are copied from EUDAQ2, fixing only compiler warnings. Might need revision
  * E.g. handle verbosity... move to proper constellation debug output
  * Proper error catching instead of just printing `CRITICAL`
* Make the `meson.build` file nicer, e.g. catch when cactus path is not provided.
* I am actually not sure of the separation between initialize and configure makes sense, should some more steps be done in initialize? Is there an order that needs to be obeyed?
