#!/bin/bash

configFolder=~/.pingpong
dataFolder=~/.ping_data

if [ ! -d "$configFolder" ]; then
	mkdir $configFolder
	cat << EOF > $configFolder/bitcoin.conf
port=18333
prune=550
rpcport=18332
connect=127.0.0.1:8333
EOF
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
$DIR/src/bitcoind -conf="$configFolder/bitcoin.conf" -datadir="$dataFolder"
