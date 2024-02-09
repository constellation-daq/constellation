#!/bin/bash
set -x #echo on
while IFS= read -r dest; do
  sshpass -p "root" scp rpsatellite.py "$dest:/root/constellation/python/constellation"
  sshpass -p "root" scp rpdevice.py "$dest:/root/constellation/python/constellation"
  sshpass -p "root" scp config_redpitaya_measure_events_standard.yaml "$dest:/root/constellation/python/constellation"
done <rplist.txt