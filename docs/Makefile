# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

PYTHON     ?= python3
SPHINXOPTS ?=
SOURCEDIR  ?= .
BUILDDIR   ?= build

export PYTHONPATH=../python/

.PHONY: help
help:
	@$(PYTHON) -m sphinx -M help $(SOURCEDIR) $(BUILDDIR)

.PHONY: clean
clean:
	@$(PYTHON) -Bc "import shutil; shutil.rmtree('$(BUILDDIR)', ignore_errors=True)"

.PHONY: doxygen
doxygen:
	doxygen

.PHONY: Makefile
%: Makefile
	@$(PYTHON) -m sphinx -M $@ $(SOURCEDIR) $(BUILDDIR) $(SPHINXOPTS) $(O)
