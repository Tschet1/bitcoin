#!/bin/bash
#rsync -a --progress --delete /home/p4/bitcoin/bitcoin_client/data_bak/ /home/p4/bitcoin/bitcoin_client_standard/data

/home/p4/btc_relay/client/src/bitcoind -conf=/home/p4/bitcoin/bitcoin_client_standard/bitcoin.conf -datadir=/home/p4/bitcoin/bitcoin_client_standard/data -daemon #-loadblock=/home/p4/bitcoin/bitcoin_client_standard/blk01234.dat
