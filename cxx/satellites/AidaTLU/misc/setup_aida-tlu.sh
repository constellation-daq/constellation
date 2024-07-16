## setup script for AIDA TLU

# Add cactus to path
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/cactus/lib

# start controlhub for stable AIDA TLU TCP/IP communication
/opt/cactus/bin/controlhub_start
/opt/cactus/bin/controlhub_status
