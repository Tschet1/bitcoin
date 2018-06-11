#!/bin/bash

set +e

if [ "$#" -eq "0" ]; then
    echo "usage: stresstest.sh path_to_bitcoin_base_folder"
    exit
fi

cd $1

rm ~/.bitcoin/debug.log
rm ~/.ping_data/debug.log
rm -r ~/.pingpong
rm -r testresults

#rsync -a --progress --delete ~/.bitcoin ~/.ping_data

# build options: export CXXFLAGS="-lprofiler -ltcmalloc"; export CFLAGS="-lprofiler -ltcmalloc"; ./configure --without-gui --enable-debug --disable-wallet

# define ENV variables
export CPUPROFILESIGNAL=12 # signal used to start and stop profiling
export CPUPROFILE=$(pwd)/testresults/gproftools.prof
export CPUPROFILE_FREQUENCY=500

function addNodes {
	nodeGoal=$1
	currentNodes=0

	unset CPUPROFILE
	echo "go to $nodeGoal nodes"
	while [[ "$currentNodes" -lt "$nodeGoal" ]]; do
		currentNodes=$(src/bitcoin-cli getconnectioncount)
		connectTo=$(( nodeGoal-currentNodes > 40 ? 40 : nodeGoal-currentNodes ))
		if [[ "0" -lt "$connectTo" ]]; then
			echo "have $currentNodes, try to connect to another $connectTo nodes"
			rand=$((1 + RANDOM % 200))
			curl https://bitnodes.earn.com/nodes/\?page\=$rand |grep -oh "[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*:8333"|uniq|head -n$connectTo|xargs -I {} src/bitcoin-cli addnode "{}" "add" &
			sleep 180
		fi
		currentNodes=$(src/bitcoin-cli getconnectioncount)
	done
	echo "level reached"

}
export -f addNodes

function keepNodes {
	nodeGoal=$1
	currentNodes=0

	unset CPUPROFILE
	while true; do
		currentNodes=$(src/bitcoin-cli getconnectioncount)
		connectTo=$(( nodeGoal-currentNodes > 40 ? 40 : nodeGoal-currentNodes ))
		if [[ "0" -lt "$connectTo" ]]; then
			echo "have $currentNodes, try to connect to another $connectTo nodes"
			rand=$((1 + RANDOM % 200))
			curl https://bitnodes.earn.com/nodes/\?page\=$rand |grep -oh "[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*:8333"|uniq|head -n$connectTo|xargs -I {} src/bitcoin-cli addnode "{}" "add" &
		fi
		sleep 60
		currentNodes=$(src/bitcoin-cli getconnectioncount)
	done
	echo "level reached"
}
export -f keepNodes

function measure {
	echo "start measuring"
	resultDir=$1
	pid=$2
	while true; do
		date >> $resultDir/date_concount
		src/bitcoin-cli getconnectioncount >> $resultDir/date_concount
		ps -L -p $pid -o %cpu,%mem,nlwp,cmd,tid,comm >> $resultDir/date_concount
		sleep 1
	done
}
export -f measure

for Run in 9; do
src/bitcoind &
pid=$!

sleep 60s

# start pingmachine
~/ping_machine/startPingMachine.sh &
pingPID=$!

sleep 300s

top -p $pid -H -n1 -b|grep -o "^[0-9][0-9]*"|xargs -I {} renice 20 {}

for nodeNum in 30 100 300 500 700 860; do
#for nodeNum in 30 100 300 500 700 860; do
	resultdir=$(pwd)/testresults/testresults_${nodeNum}_${Run}
	echo "store results in $resultdir"	
	mkdir -p $resultdir
	
	# reach the level of nodes that we want
	timeout 180m bash -c "addNodes $nodeNum"

	keepNodes $nodeNum &
	keepNodesPID=$!

	vmstat -t 5 >> $resultdir/vmstat.log &
	vmstatPID=$!
		
	top -H -b -d1 -p $pid >> $resultdir/top.log &
	topPID=$!

	# wait certain time
	sleep 15m

	# do th6 measurement
	timeout 60m bash -c "measure $resultdir $pid"

	# run profiler
	if [[ "$Run" -eq "4" ]]; then
		killall -12 bitcoind
		sleep 90m
		killall -12 bitcoind
	fi
	kill $keepNodesPID
	kill $vmstatPID
	kill $topPID
done
sleep 300s
kill $pid
killall bitcoind timeout
kill $pingPID
sleep 300s
mv ~/.bitcoin/debug.log $(pwd)/testresults/log_${Run}.log
mv ~/.ping_data/debug.log $(pwd)/testresults/pingpong_${Run}.log

done

##for i in $(seq 10 50 1000); do
##echo "test with $i max clients"
##sed -e "s/maxconnections=.*/maxconnections=$i/g" -i ~/.bitcoin/bitcoin.conf
##src/bitcoind &
##pid=$!
##sleep 120
##timeout 20m bash -c "while true; do date >>~/testresults/ps_count_$i; src/bitcoin-cli getconnectioncount >> ~/testresults/ps_count_$i ; ps -L -p $pid -o %cpu,%mem,nlwp,cmd,tid,comm >> ~/testresults/ps_count_$i;sleep 5; done"
##kill $pid
##sleep 120
##mv ~/.bitcoin/debug.log ~/testresults/debug_$i.log
###mv gmon.out ~/testresults/gmon_$i.out
##done
#
#function addNode {
#    rand1=$((1 + RANDOM % 200))
#    rand2=$((1 + RANDOM % 10))
#    #if [[ "$rand2" -lt "3" ]]; then
#    #	curl https://bitnodes.earn.com/nodes/\?page\=$rand1 |grep -oh "[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*:8333"|xargs -I {} src/bitcoin-cli addnode "{}" "add" &
#    #	echo "used random $rand1"
#    #	echo "added Nodes"
#    #fi
#}
#
#export -f addNode
#
#i=0
#echo "test with $i max clients"
#sed -e "s/maxconnections=.*/maxconnections=$i/g" -i ~/.bitcoin/bitcoin.conf
#src/bitcoind &
#pid=$!
#sleep 60
#timeout 240m bash -c "while true;date >>~/testresults/ps_count_$i;  src/bitcoin-cli getconnectioncount >> ~/testresults/ps_count_$i; addNode ; do ps -L -p $pid -o %cpu,%mem,nlwp,cmd,tid,comm >> ~/testresults/ps_count_$i;sleep 5; done"
#kill $pid
#sleep 30
#mv ~/.bitcoin/debug.log ~/testresults/debug_$i.log
#mv gmon.out ~/testresults/gmon_$i.out
