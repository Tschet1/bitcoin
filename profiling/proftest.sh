#!/bin/bash

set +e

if [ "$#" -eq "0" ]; then
    echo "usage: proftest.sh path_to_bitcoin_base_folder"
    exit
fi

cd $1

tests=(30 400 860)

#rsync -a --progress --delete ~/.bitcoin ~/.ping_data

# build options: export CXXFLAGS="-lprofiler -ltcmalloc"; export CFLAGS="-lprofiler -ltcmalloc"; ./configure --without-gui --enable-debug --disable-wallet

# define ENV variables
export CPUPROFILESIGNAL=12 # signal used to start and stop profiling
export CPUPROFILE=$(pwd)/testresults_prof/gproftools.prof
export CPUPROFILE_FREQUENCY=100
export CPUPROFILE_REALTIME=1
export CPUPROFILE_TIMER_SIGNAL=34
export LC_ALL=C
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

for Run in 9; do
src/bitcoind &
pid=$!

sleep 60s
sleep 300s

for nodeNum in $tests; do
	mkdir -p $(pwd)/testresults_prof/
	
	# reach the level of nodes that we want
	timeout 180m bash -c "addNodes $nodeNum"

	keepNodes $nodeNum &
	keepNodesPID=$!

	# wait certain time
	sleep 15m

	# run profiler
	killall -12 bitcoind
	sleep 4h
	killall -12 bitcoind
	sleep 5m	
	kill $keepNodesPID
	sleep 15m
done
kill $pid
killall bitcoind timeout top ps xargs
sleep 300s

cd $(pwd)/testresults_prof/

for i in $(seq 0 ${#tests[@]}); do echo $i
google-pprof --callgrind ../src/bitcoind gproftools.prof.$i > callgrind${i}.prof
google-pprof --pdf ../src/bitcoind gproftools.prof.$i > callgraph${i}.pdf
/home/user/go/bin/pprof --callgrind ../src/bitcoind gproftools.prof.$i > callgrind${i}go.prof
/home/user/go/bin/pprof --pdf ../src/bitcoind gproftools.prof.$i > callgraph${i}go.pdf
done

cd ..
tar -cjf testresults_prof.tar.bz2 testresults_prof/
mv testresults_prof.tar.bz2 testresults_prof/
done
