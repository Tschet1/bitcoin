#!/bin/bash
#date -s "17 MAY 2018 19:00:00"
rsync -a --progress --delete /home/p4/bitcoin/bitcoin_client/data_bak/ /home/p4/bitcoin/bitcoin_master/data

/home/p4/btc_relay/client/src/bitcoind -conf=/home/p4/bitcoin/bitcoin_master/bitcoin.conf -datadir=/home/p4/bitcoin/bitcoin_master/data -daemon
