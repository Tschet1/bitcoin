btc_relay
=========

This is a fork of the bitcoin core repository. It contains a modified bitcoin client to support the
SABRE framework. Additionally, it contains a controller for the SDN switch used in that framework.

More information about the setup can be found in the thesis in the thesis folder.

## Overview
src:
----
The source code for the bitcoin client. The files net_relay.h and net_relay.cpp contain the additions to the client.
The changes that were made to the existing code are summarised in codediff.diff.

mininet_test:
-------------
In the folder mininet_test are scripts to test the modified client in mininet as described in chapter 5 of the thesis.

profiling:
----------
In the profiling folder there are scripts used to profile the bitcoin client as described in chapter 3 of the thesis.

pseudocode:
-----------
Pseudocode for the switch that was used during the early stages of the development.

## Compile
```
./autogen.sh
./configure --without-gui --enable-debug --disable-wallet
./make
```

## Usage

The modified client can be used the same way as the upstream bitcoin client. The following options were added to bitcoind:
```
-relaytype=<type>	# Type of the node. Possible values: standard, client, master (default: standard). standard means, that
                    # the client should run without the added modifications. Client means that the client should run as switch
                    # client, meaning that the changes that are explained in the thesis are added. Master means that the 
                    # software should act as the controller for a connected SDN switch.
-relayaddr=<addr>	# IP address of the switch (default: 127.0.0.1)
-relayport=<port> 	# Port of the switch (default: 8080)
-relaymyport=<port>	# UDP Port used to connect to the switch (default: 0 (random port))
```

