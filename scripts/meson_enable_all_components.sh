#!/bin/sh
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

MESON_OPTIONS_TXT="$(realpath $(dirname $0)/..)/meson_options.txt"

# Look for all options starting with satellite_ and enable them
grep "option('satellite_" $MESON_OPTIONS_TXT | sed -E -e "s%option\('(satellite_\w+)'.*%\1%g" | sed -E -e "s%(.*)%-D\1=true%g" | paste -sd " " - | cat
# Look for all options starting with controller_ and enable them
grep "option('controller_" $MESON_OPTIONS_TXT | sed -E -e "s%option\('(controller_\w+)'.*%\1%g" | sed -E -e "s%(.*)%-D\1=true%g" | paste -sd " " - | cat
# Look for all options starting with listener_ and enable them
grep "option('listener_" $MESON_OPTIONS_TXT | sed -E -e "s%option\('(listener_\w+)'.*%\1%g" | sed -E -e "s%(.*)%-D\1=true%g" | paste -sd " " - | cat
