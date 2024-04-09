#!/bin/bash

clear
source venv/bin/activate
export COLOREDLOGS_DATE_FORMAT='%H:%M:%S'
export COLOREDLOGS_LOG_FORMAT="%(asctime)s %(hostname)s %(name)s %(levelname)s %(message)s"
Controller --group cnstln1
