#!/bin/bash
set -x #echo on
while IFS= read -r dest; do
  sshpass -p "root" ssh -t -oStrictHostKeyChecking=no "$dest" <<EOF
  shutdown now
  exit
EOF
done <rplist.txt