These scripts and pseudo clients are used to test the individual components of the client:

## MASTER CLIENT:
### UPDATE/BLOCK_DATA/INV  (CMD: UPD/BLK/INV)
#### implementation:
After receiving a new block, the master will first send the block hash and the precomputed checksum to the switch. It will then send the
individual block segments to the switch. We assume no losses here.
After that, the master sends an INV message to all hosts which it has the metadata of (see CON).
#### testing:
Tested with the real switch.
Checksum verified. Messages are sent. TODO: check if they are understood by the switch.
TODO: INV

### CONNECTION (CMD: CON)
#### implementation:
When a CON message is received, the master will store the IP/port tuple in a list.
Currently, the hosts in this list are never removed. This will have to be changed (at least for blacklisted hosts)
#### testing:
Tested with the real switch. CON messages are received and INVs are sent to the hosts that are CONed.

### BLACKLIST (CMD: BCL)
#### implementation:
Whenever a client is blacklisted in the regular client's workflow, the information about the blacklisted node is pushed to the switch.
#### testing:
TODO

## BTC CLIENT :
### HANDSHAKE (CMD: REY)
#### implementation:
send SYN, wait for ACK, send SYNACK
If there is no ACK, retransmit after n seconds.
#### testing:
Tested using the pseudoclient. The pseudoclient behaves like the P4 switch. A successful handshake is possible between client and pseudoclient.
Tested with the real switch.

### INV  (CMD: INV)
#### implementation:
When an INV message is received, the node checks how many fragment the block is composed of and creates a list to hold the fragments until all 
fragments have been recieved. A thread is started which will request the fragments and trigger retransmissions if needed.
#### testing:
Tested using the pseudoclient: The client responds with SEG messages when it receives an INV.
TODO: test with real switch

### GET_SEG/BLOCK_DATA (CMD: SEG/BLK)
#### implementation:
(see INV). The client puts the fragments in the (ordered) list and checks if all fragments are received. If the block is finished, it is passed
to the processing routine which will treat the block as if originating from a regular client.
#### testing:
The SEG packets are processed by the pseudoclient and the fragment with the requested number is sent back.
Having a look at the logfiles it can be verified that the block gets added to the chain after receiving it from the pseudoclient.
Lost fragments and out of order arrival of segments was tested using the pseudoclient (hardcoded packet losses).
TODO: test with real switch

### ADVERTISEMENT (CMD: ADV)
#### implementation:
When a new block is received which does not originate from the switch, an ADV message is sent to the switch.
This feature is disabled, when the client is not in a synced state. This is checked by looking at the number of blocks that are inflight. If
the client is waiting for 0 blocks, this means that it should be synced.
#### testing:
The pseudoclient sucessfully receives an ADV message when the client finds a new block.
Tested with the real switch.

### CONNECT_TO_CONTROL  (CMD: CTR)
#### implementation:
After parsing the ip and the port, the regular mechanism (of the client) for connecting to a predefined host is used.
#### testing:
Using the according test function in tests.py, a CTR request is sent to the client. By looking at the log files, it can be seen that the client
correctly detects port and ip. Using wireshark, it can be seen that the client tries to do a handshake with the desired host.
It has to be tested, if the resulting connection works.
Tested with the real switch. The client successfully connects to the master.
