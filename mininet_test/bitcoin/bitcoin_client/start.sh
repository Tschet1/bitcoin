#!/bin/bash

rsync -a --progress --delete /home/p4/bitcoin/bitcoin_client/data_bak/ /home/p4/bitcoin/bitcoin_client/data

/home/p4/btc_relay/client/src/bitcoind -conf=/home/p4/bitcoin/bitcoin_client/bitcoin.conf -datadir=/home/p4/bitcoin/bitcoin_client/data -daemon
