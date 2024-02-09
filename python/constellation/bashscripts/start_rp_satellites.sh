while IFS= read -r dest; do
  sshpass -p "root" ssh -t -oStrictHostKeyChecking=no "$dest" <<EOF
    cd constellation
    screen -d -m python -m constellation.rpsatellite
    exit
EOF
done <rplist.txt

