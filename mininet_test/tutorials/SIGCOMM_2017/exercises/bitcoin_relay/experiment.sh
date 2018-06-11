#!/bin/bash

sync() {

echo "wait until the chain is synced"
# sync with the chain
sudo /home/p4/btc_relay/client/src/bitcoind -conf=/home/p4/bitcoin/bitcoin_client_standard/sync.conf -datadir=/home/p4/bitcoin/bitcoin_client_standard/data -daemon
sleep 30
while [ -z "$(sudo grep "best.*progress" /home/p4/bitcoin/bitcoin_client_standard/data/debug.log|wc -l)" ] || [ "$(sudo grep "best.*progress" /home/p4/bitcoin/bitcoin_client_standard/data/debug.log |sed -e "s/.*progress=0.//g" -e "s/ .*//g"|tail -n1)" -lt "999990" ]; do
    sleep 1
done
sleep 2m
sudo killall bitcoind
sleep 1m
sudo rm -rf /home/p4/bitcoin/bitcoin_client_standard/data/debug.log
rm ~/test.out
echo "we should be synced now, copy state"

# we should be in a synced state now, copy the state
sudo rsync -a --progress --delete /home/p4/bitcoin/bitcoin_client_standard/data/ /home/p4/bitcoin/bitcoin_client/data_bak

# wait for a new block
sudo /home/p4/btc_relay/client/src/bitcoind -conf=/home/p4/bitcoin/bitcoin_client_standard/sync.conf -datadir=/home/p4/bitcoin/bitcoin_client_standard/data -daemon
echo "wait for a new block"
sleep 30
while [ -z "$(sudo grep "UpdateTip: new best" /home/p4/bitcoin/bitcoin_client_standard/data/debug.log|wc -l)" ] || [ "$(sudo grep "UpdateTip: new best" /home/p4/bitcoin/bitcoin_client_standard/data/debug.log|wc -l)" -eq "0" ]; do
    sleep 1
done
echo "new block received. ready for the experiment"
sudo killall bitcoind
sleep 1m
sudo rm -rf /home/p4/bitcoin/bitcoin_client_standard/data/debug.log

}

test1() {

for i in $(seq 1 20); do
#sudo date -s "17 MAY 2018 19:00:00"
sleep 5
sudo mn -c
printf '%s\n' "h1 startController" "h2 startClient" "h1 sleep 120" "exit" | /home/p4/tutorials/SIGCOMM_2017/exercises/bitcoin_relay/run.sh
sleep 5
sudo killall bitcoind
sleep 20
~/test.sh >> ~/test.out
done
echo "please run: python3 ~/analyse_test_out.py ~/test.out to see the results"

}

test2() {

#sudo date -s "17 MAY 2018 19:00:00"
for i in $(seq 1 3); do
sleep 5
sudo mn -c
printf '%s\n' "h1 startController" "h2 sleep 1m"  "h2 /home/p4/testConnections.py >> /home/p4/1000test.out &" "h2 sleep 8m"  "h2 startClient" "h1 sleep 3m" "exit" | /home/p4/tutorials/SIGCOMM_2017/exercises/bitcoin_relay/run.sh
sleep 5
sudo killall bitcoind 
sleep 20
sudo ~/test.sh >> ~/test.out
done
}

show_help() {
    echo "./experiment.sh "
    echo 
    echo "-s: get a new block"
    echo "-t1: perform test 1"
    echo "-t2: perform test 2"
}


while :; do
    case $1 in
        -h|-\?|--help)
            show_help    # Display a usage synopsis.
            exit
            ;;
        s|-s|--sync)
            sync
            ;;
        t1|-t1|--test1)
            test1
            ;;
        t2|-t2|--test2)
            test2
            ;;
        -?*)
            printf 'WARN: Unknown option (ignored): %s\n' "$1" >&2
            ;;
        *)               # Default case: No more options, so break out of the loop.
            break
    esac

    shift
done






