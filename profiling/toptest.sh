#!/bin/bash

set +e

if [ "$#" -eq "0" ]; then
    echo "usage: toptest.sh path_to_bitcoin_base_folder output_folder"
    exit
fi

cd $1

function addNodes {
	nodeGoal=$1
	currentNodes=0

	page=1
	echo "go to $nodeGoal nodes"
	while [[ "$currentNodes" -lt "$nodeGoal" ]]; do
		curl https://bitnodes.earn.com/nodes/\?page\=$page |grep -oh "[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*:8333"|uniq|xargs -I {} bash -c "sleep 5; src/bitcoin-cli addnode \"{}\" \"add\""
		currentNodes=$(src/bitcoin-cli getconnectioncount)
		page=$((page + 1))
	done
	for i in $( seq 1 10); do
		curl https://bitnodes.earn.com/nodes/\?page\=$page |grep -oh "[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*:8333"|uniq|xargs -I {} bash -c "sleep 5; src/bitcoin-cli addnode \"{}\" \"add\""
		page=$((page + 1))
	done

}
export -f addNodes

function measure {
	echo "start measuring"
	resultFile=$1/topout
	pid=$2
	while true; do
		top -d0.1 -b -p $pid -H >> $resultFile
	done
}
export -f measure

function getNodeCount {
	resultFile=$1/date_concount
	while true; do
		date >> $resultFile
		src/bitcoin-cli getconnectioncount >> $resultFile 
		sleep 1
	done
}
export -f getNodeCount

mkdir -p ~/testresults
rm ~/.bitcoin/debug.log

# build options: export CXXFLAGS="-lprofiler -ltcmalloc"; export CFLAGS="-lprofiler -ltcmalloc"; ./configure --without-gui --enable-debug --disable-wallet

# define ENV variables

src/bitcoind &
pid=$!

sleep 180s

resultdir=$(pwd)/$2
echo "store results in $resultdir"	
mkdir -p $resultdir
	
measure $resultdir $pid &
measurePID=$!

getNodeCount $resultdir &
nodeCountPID=$!

# reach the level of nodes that we want
timeout 4h bash -c "addNodes 860"

kill -9 $measurePID
kill -9 $nodeCountPID
kill $pid

sleep 180
mv ~/.bitcoin/debug.log $resultdir

