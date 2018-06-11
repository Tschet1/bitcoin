# Profiling

The following describes how to setup the framework for testing the modified client in mininet and how to run the experiments.

## setup
0. This setup assumes an active user p4 in a vm as described by p4lang/tutorials and the switch software to be installed.
1. Add the contents of the following folders to the existing folders:
   usr -> /usr
   tutorials -> /home/p4/tutorials
2. Add the new folders/files. Copy:
   bitcoin -> /home/p4/bitcoin
   test.sh -> /home/p4/
   testConnections.py -> /home/p4/
   analyse_test_out.py -> /home/p4
3. Apply the increase_log_accuracy.diff patch
4. Add state of the (pruned) chain to /home/p4/bitcoin/bitcoin_client_standard/data to speed up the initial sync.

## Run experiment

Go to directory /home/p4/tutorials/SIGCOMM_2017/exercises/bitcoin_relay/
Run ./experiment.sh with one or multiple of the following options. The Experiment are done in the order they are specified
-s   : Prepares the state for the following experiments. Has to be run roughly every 2 days. Syncs the chain and adds one additional block to the standard client
-t1  : Run experiment 1: See section 5.1 of the thesis for a description of the setup.
-t2  : Run experiment 1: See section 5.2 of the thesis for a description of the setup.

The results of the experiments can be found in /home/p4/test.out
