# Constellation on RedPitaya

## RedPitaya Constellation Installation
This how-to guide will walk through how to install a new satellite on a RedPitaya SBC.

### Background

In order to run the constellation framework on a RedPitaya we need `python 3.11` or higher. At the time of writing most RedPitaya boards only have `python 3.10` installed and, due to the 32-bit CPU, options like [Anaconda](https://www.anaconda.com/) or [Miniforge](https://github.com/conda-forge/miniforge) are limited. A possible solution is to use [pyenv](https://github.com/pyenv/pyenv) which similarly lets you install and build any python version to use on a per-project basis.

### Install potential dependencies for `pyenv`

```bash
sudo apt-get update
```

```bash
sudo apt-get install -y make build-essential libssl-dev zlib1g-dev libbz2-dev libreadline-dev libsqlite3-dev wget curl llvm libncurses5-dev libncursesw5-dev xz-utils tk-dev libffi-dev liblzma-dev
```

### Install `pyenv`

```bash
curl https://pyenv.run | bash
echo 'export PYENV_ROOT="$HOME/.pyenv"' >> ~/.bashrc
echo 'command -v pyenv >/dev/null || export PATH="$PYENV_ROOT/bin:$PATH"' >> ~/.bashrc
echo 'eval "$(pyenv init -)"' >> ~/.bashrc
echo 'export PYENV_ROOT="$HOME/.pyenv"' >> ~/.profile
echo 'command -v pyenv >/dev/null || export PATH="$PYENV_ROOT/bin:$PATH"' >> ~/.profile
echo 'eval "$(pyenv init -)"' >> ~/.profile
echo 'export PYENV_ROOT="$HOME/.pyenv"' >> ~/.bash_profile
echo '[[ -d $PYENV_ROOT/bin ]] && export PATH="$PYENV_ROOT/bin:$PATH"' >> ~/.bash_profile
echo 'eval "$(pyenv init -)"' >> ~/.bash_profile
```

### Restart shell

```bash
exec "$SHELL"
```

### Install correct `python` version

```bash
pyenv install 3.11.8
```

### Create virtual environment using either

```bash
pyenv virtualenv <python_version> <environment_name>
```

```bash
python3.11 -m venv <environment_name>
```

### Enable virtual environment using either

```bash
pyenv local <environment_name>
pyenv activate <environment_name>
```

```bash
source <environment_name>/bin/activate
```

### Clone **constellation** and enter repository

```bash
git clone https://gitlab.desy.de/constellation/constellation.git
cd constellation
```

### Install build system `meson` and `ninja`

```bash
pip install meson-python meson ninja
```

### Install dependencies for HDF5 and ZMQ

```bash
sudo apt-get install libhdf5-serial-dev libzmq3-dev
```

### Build `python` version

```bash
pip install -e . --no-build-isolation
```

*Note: if ninja is instead installed on a system level, then `pip install -e .` is enough.*
### Set `python` path in `.bashrc` (necessary for RedPitaya API)

```bash
echo 'PYTHONPATH=/opt/redpitaya/lib/python/:$PYTHONPATH' >> ~/.bashrc
```

### Launch satellite

```bash
python -m constellation.core.rpsatellite
```

### Known issues

- When building constellation on the RedPitaya the process is very slow and has a risk of timing out.

## Starting satellite on boot

To launch the satellite on boot of the system, create a `.service`-file with the following service script.

```bash
[Unit]
Description=RedPitaya Satellite service
After=multi-user.target

[Service]
Type=simple
Restart=always
RestartSec=1
ExecStart=/path/to/python -m constellation.<satellite_module> --<arguments>
KillSignal=SIGINT

[Install]
WantedBy=multi-user.target
```

To enable the service, place the file in the `/etc/systemd/system/` directory and call:

```bash
sudo systemctl daemon-reload
sudo systemctl enable <SERVICE_NAME>.service
sudo systemctl start <SERVICE_NAME>.service
```
