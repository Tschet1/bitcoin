#include "net_relay.h"
#include <sys/socket.h>
#include "util.h"
#include <stdlib.h>
#include <fcntl.h>
#include <net.h>
#include <chainparams.h>
#include <netmessagemaker.h>
#include <chrono>
#include <thread>
#include <validation.h>
#include <netaddress.h>

/**
 * simple helper function for sending messages
 */
void Net_relay::sendMessageToRelay(CConnman* connman, CNode* rnode, unsigned char* data, unsigned int dataLength){
    LogPrint(BCLog::RELAY, "Relay: send message %.3s\n", data);

    // assemble the message
    std::vector<unsigned char> serializedMessage;
    serializedMessage.insert(serializedMessage.end(), data, data + dataLength);

    // pipe into sending bytes
    {
        LOCK(rnode->cs_vSend);
	bool optimisticSend(rnode->vSendMsg.empty());
        rnode->nSendSize += serializedMessage.size();
        rnode->vSendMsg.push_back(std::move(serializedMessage));
	// If write queue empty, attempt "optimistic write"
        if (optimisticSend == true)
            connman->SocketSendData(rnode);
    }
}


/**
 * This function creates a waitset for collecting the parts of a block.
 */
void Net_relay::addInv(uint256 hash, uint16_t frags){
    NetRelayWaitset waitset;
    waitset.hash = hash;
    waitset.numFragments = frags;
    LogPrint(BCLog::RELAY, "Relay: created waitset with %d segments\n", waitset.numFragments);
    waitset.timeout = std::chrono::system_clock::now() + std::chrono::minutes{2};
    {
        LOCK(cs_waitset);
        if(blockWaitSet.size() < NET_RELAY_MAX_CONCURRENT_INV){
            blockWaitSet.push_back(waitset);
        } else {
            error("Relay: too many blocks requested but not received.");
        }
    }
}

struct RGETDATAMessage {
    netRelayCommand command = NET_RELAY_MESSAGE_TYPE_RGETDATA;
    char hash[32];
    netRelaySegcount segCount;
} __attribute__((__packed__));

/**
 * request a single fragment of a block
 */
static inline void sendRGETDATAMessage(CNode* rnode, uint256* hash, netRelaySegId num){
    // assemble the message
    struct RelayMsgSEG msg;
    memcpy(msg.cmd, RelayMsgType::SEG, NET_RELAY_MESSAGE_TYPE_LENGTH);
    msg.hash = *hash;
    msg.segId = htons(num);

    std::vector<unsigned char> serializedMessage;
    serializedMessage.insert(serializedMessage.end(), (unsigned char*) &msg, ((unsigned char*) &msg) + sizeof(struct RelayMsgSEG));

    // pipe into sending bytes
    {
        LOCK(rnode->cs_vSend);
        rnode->nSendSize += serializedMessage.size();
        rnode->vSendMsg.push_back(std::move(serializedMessage));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

/**
 * send a single fragment of a block
 */
inline void sendBLOCKMessage(CNode* rnode, uint256* hash, unsigned int num, unsigned char* data, unsigned int dataLength){
    // assemble the message
    std::vector<unsigned char> serializedMessage;
    serializedMessage.push_back((unsigned char) NET_RELAY_MESSAGE_TYPE_RBLOCK);
    serializedMessage.insert(serializedMessage.end(), (unsigned char*) hash, ((unsigned char*) hash) + (sizeof(uint256)));
    serializedMessage.insert(serializedMessage.end(), (unsigned char*) &num, ((unsigned char*) &num) + NET_RELAY_FRAGNO_BYTES);
    serializedMessage.insert(serializedMessage.end(), data, data + dataLength);

    // pipe into sending bytes
    {
        LOCK(rnode->cs_vSend);
        rnode->nSendSize += serializedMessage.size();
        rnode->vSendMsg.push_back(std::move(serializedMessage));
    }
}

/**
 * This thread handles the retransmission and the timeout of the fragment transmission
 */
void Net_relay::RGETDATATimeoutHandler(uint256 hash)
{
    LogPrint(BCLog::RELAY, "Relay: start timeout handler\n");
    while(true){
        while(true){
            // check every n seconds if we are still waiting for fragments
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // check if we are still waiting for this block
            {
                LOCK(cs_waitset);
                auto elemInWaitset = std::find(blockWaitSet.begin(), blockWaitSet.end(), hash);
                if(elemInWaitset == blockWaitSet.end() || elemInWaitset->fragments.size() == elemInWaitset->numFragments){
                    // no. we were not waiting for this
                    break;
                }
                //check if timeout exceeded
                if(elemInWaitset->timeout < std::chrono::system_clock::now()){
                    LogPrint(BCLog::RELAY, "Relay: stop waiting for block %s. timed out.\n", (elemInWaitset->hash).GetHex());
                    blockWaitSet.erase(elemInWaitset);
                    break;
                }
                unsigned int totalFrags;
                if(elemInWaitset->lastFragmentTimestamp + std::chrono::seconds{5} > std::chrono::system_clock::now()){
                    auto lastelem = elemInWaitset->fragments.back();
                    LogPrint(BCLog::RELAY, "Relay: trigger partial retransmission? %d %d\n", lastelem.num, elemInWaitset->fragments.size());
                    if(lastelem.num < elemInWaitset->fragments.size()){
                        continue;
                    }
                    LogPrint(BCLog::RELAY, "Relay: trigger partial retransmission\n");
                    totalFrags = lastelem.num>10 ? lastelem.num-10 : 0u;
                } else {
                    LogPrint(BCLog::RELAY, "Relay: trigger retransmission\n");
                    totalFrags = elemInWaitset->numFragments;
                    elemInWaitset->lastFragmentTimestamp = std::chrono::system_clock::now();
                }
                // perform retransmissions
                unsigned int counter = 0;
                auto it = elemInWaitset->fragments.begin();
                auto endIt = elemInWaitset->fragments.end();
                for (; counter < totalFrags; ++counter){
                    if (it == endIt || it->num != counter){
                        // this one is missing. retransmit
                        sendRGETDATAMessage(rnode, &hash, counter);
                    } else {
                        ++it;
                    }
                }
	    }
        }

        //check if there is more work to do
        {
            LOCK(cs_waitset);
            if(blockWaitSet.size() != 0){
                // there is another INV waiting
                auto elemInWaitset = blockWaitSet.front();
                hash = elemInWaitset.hash;
                unsigned int totalFrags = elemInWaitset.numFragments;
                for (unsigned int counter = 0; counter < totalFrags; ++counter){
                    sendRGETDATAMessage(rnode, &hash, counter);
                }
            } else {
                // we have finished here
                return;
            }
        }
    }
}

void Net_relay::blacklist(const CNetAddr& addr){
    //TODO: do we need to have a timely limited blacklist?
    //TODO: delete from collected metadata?
    assert(isMasterNode);
    if (!addr.IsIPv4()){
        LogPrint(BCLog::RELAY, "Relay: Blacklisting of non IPv4 peer is not supported.\n");
    }

    LogPrint(BCLog::RELAY, "Relay: Blacklist peer\n");

    struct in_addr ipaddr;
    addr.GetInAddr(&ipaddr);

    struct RelayMsgBCL tx_msg;
    memcpy(tx_msg.cmd, RelayMsgType::BCL, 3);
    strcpy(tx_msg.ip,inet_ntoa(ipaddr));

    sendMessageToRelay(connman, rnode, (unsigned char*) &tx_msg, sizeof(tx_msg));
}

uint16_t Net_relay::sendFragmentsToSwitch(uint256 hash, const std::shared_ptr<const CBlock> block){
    // create the message
    CSerializedNetMsg msg = CNetMsgMaker(INIT_PROTO_VERSION).Make(NetMsgType::BLOCK, block);

    // get the size
    size_t nMessageSize = msg.data.size();
    size_t nTotalSize = nMessageSize + CMessageHeader::HEADER_SIZE;

    // create the header
    uint256 msghash = Hash(msg.data.data(), msg.data.data() + nMessageSize);
    CMessageHeader hdr(Params().MessageStart(), msg.command.c_str(), nMessageSize);
    memcpy(hdr.pchChecksum, msghash.begin(), CMessageHeader::CHECKSUM_SIZE);


    // write data to stream
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    hdr.Serialize(stream);
    block->Serialize(stream);

    // check how many fragments we need
    uint32_t fragmentNumber = stream.size();
    if(fragmentNumber % NET_RELAY_SEGMENT_SIZE != 0){
        fragmentNumber = fragmentNumber/NET_RELAY_SEGMENT_SIZE + 1;
    } else {
        fragmentNumber/=NET_RELAY_SEGMENT_SIZE;
    }
    LogPrint(BCLog::RELAY, "Relay: send block hash=%s size=%u in %u fragments\n", hash.ToString(), (unsigned int) stream.size(),  (unsigned int) fragmentNumber);

    struct RelayMsgBLKData tx_msg;
    memcpy(tx_msg.cmd, RelayMsgType::BLK, 3);
    // do the sending
    for(uint16_t i=0; i<fragmentNumber; ++i){
        // set the id
        tx_msg.segId = htons(i);
        // add the data
        stream.read(tx_msg.data, std::min((size_t) NET_RELAY_SEGMENT_SIZE,stream.size()));
        // add the checksum
        tx_msg.checksum = udpPreChecksum(&tx_msg, sizeof(struct RelayMsgBLKData) - 2 );
        // send
        sendMessageToRelay(connman, rnode, (unsigned char*) &tx_msg, sizeof(struct RelayMsgBLKData));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return fragmentNumber;
}

/**
 * send an RINV message to the switch.
 */
void Net_relay::sendRINVtoSwitch(uint256 hash, CNode* pnode){
    if(!isMasterNode){
        LogPrint(BCLog::RELAY, "Relay: I found this new block the relay should know about: %s\n", hash.GetHex());

        struct RelayMsgADV tx_msg;
        memcpy(tx_msg.cmd, RelayMsgType::ADV, 3);
        tx_msg.hash = hash;

        //send
        sendMessageToRelay(connman, rnode, (unsigned char*) &tx_msg, sizeof(struct RelayMsgADV));
    } else {
        LogPrint(BCLog::RELAY, "Relay: I found this new block the relay should know about: %s aaand I'm the master, so feed it to the switch\n", hash.GetHex());

        struct RelayMsgUPD tx_msg;
        memcpy(tx_msg.cmd, RelayMsgType::UPD, 3);
        tx_msg.hash = hash;

        //send
        sendMessageToRelay(connman, rnode, (unsigned char*) &tx_msg, sizeof(struct RelayMsgUPD));

        // find the block
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(hash);

        // design: send in best effort approach, this way, the switch can just ignore the block if it does not want it.
        // we don't care about reliable delivery
        // we have the requested block (not pruned)
        uint16_t fragments = 0;
        if (mi != mapBlockIndex.end() && mi->second->nStatus & BLOCK_HAVE_DATA) {
            // get the block
            std::shared_ptr<const CBlock> block;

            // Send block from disk
            LogPrint(BCLog::RELAY, "Relay: load block with hash %s\n", hash.ToString());
            std::shared_ptr<CBlock> pblockRead = std::make_shared<CBlock>();
            Consensus::Params consensusParams = Params().GetConsensus();
            if (!ReadBlockFromDisk(*pblockRead, (*mi).second, consensusParams))
                assert(!"cannot load block from disk");
            block = pblockRead;

            fragments = sendFragmentsToSwitch(hash, block);
        } else {
            LogPrint(BCLog::RELAY, "Relay: Error: could not send block to the switch because it was not found.\n");
            return;
        }

        // send INV to all clients
        struct RelayMsgINV tx_msg_inv;
        memcpy(tx_msg_inv.cmd, RelayMsgType::INV, 3);
        tx_msg_inv.hash = hash;
        tx_msg_inv.segCount = fragments;

        struct sockaddr_in clientAddr;
        clientAddr.sin_family = AF_INET;

        // disconnect the client again
        LogPrint(BCLog::RELAY, "Relay: disconnect peer\n");
        pnode->CloseSocketDisconnect();
        
        for(struct connectionMetadata con : connections) {
            clientAddr.sin_port = *((uint32_t*) con.srcPort);
            char tmpIp[50];
            inet_ntop(AF_INET, con.srcIP, tmpIp, 50);

            inet_aton(tmpIp, &(clientAddr.sin_addr));

            LogPrint(BCLog::RELAY, "Relay: Send INV to %s\n", tmpIp);
            //TODO: error handling
            if (sendto(fd, (unsigned char*)&tx_msg_inv, sizeof(tx_msg_inv),0, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) == -1){
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

    }

}


void Net_relay::createRGETDATAMessages(const uint256 &waitHash){
    // this function creates and adds it to the bits to be sent for this CNode
    NetRelayWaitset waitset = blockWaitSet.front();
    if(waitHash != waitset.hash){
        // there is already another waitset being handled
        return;
    }
    uint256* hash = &waitset.hash;
    unsigned int numFrags = waitset.numFragments;
    for(unsigned int i=0; i<numFrags; ++i){
        sendRGETDATAMessage(rnode, hash, i);
    }

    // start retransmission handler
    std::thread(&TraceThread<std::function<void()> >, "RGETDATATimeoutHandler", std::function<void()>(std::bind(&Net_relay::RGETDATATimeoutHandler, this, *hash))).detach();
}

void Net_relay::HandleMessage(char* bytes_in, unsigned int len, bool& notify, CNode* pfrom){
    // check if message should be handled
    char* msgPtr = bytes_in;

    notify = false;
    LogPrint(BCLog::RELAY, "Relay: Received message\n");

    // translate the messages into regular bitcoin messages
    // We don't have to check if check if we have a started message because the message is guaranteed to fit into a single packet. For blocks, this is handled using fragments.
    if(memcmp(msgPtr, RelayMsgType::INV, NET_RELAY_MESSAGE_TYPE_LENGTH) == 0 && !isMasterNode && len == sizeof(struct RelayMsgINV)){
        // We received a RINV message. Translate to INV message
        LogPrint(BCLog::RELAY, "Relay: Received RINV\n");

        struct RelayMsgINV* rx_msg = (struct RelayMsgINV*) msgPtr;

        LogPrint(BCLog::RELAY, "Relay: hash of block is %s\n", rx_msg->hash.GetHex());

        // create message
        CNetMessage msg = CNetMessage(Params().MessageStart(), SER_NETWORK, INIT_PROTO_VERSION);

        // create the INV
        std::vector<CInv> vInv;
        vInv.push_back(CInv(MSG_BLOCK, rx_msg->hash));
        CSerializedNetMsg serMsg = CNetMsgMaker(INIT_PROTO_VERSION).Make(NetMsgType::INV, vInv);

        // header
        msg.hdr = CMessageHeader(Params().MessageStart(), NetMsgType::INV, serMsg.data.size() + NET_RELAY_SEGCOUNT_BYTES);
        msg.in_data = true;

        // message
        char* buf = (char*) malloc(serMsg.data.size());
        std::copy(serMsg.data.begin(), serMsg.data.end(), buf);
        msg.readData(buf, serMsg.data.size());
        free(buf);

        msg.readData((const char*) &(rx_msg->segCount), NET_RELAY_SEGCOUNT_BYTES);

        // set hash
        uint256 val = msg.GetMessageHash();
        memcpy(msg.hdr.pchChecksum, val.begin(), CMessageHeader::CHECKSUM_SIZE);
        LogPrint(BCLog::RELAY, "Relay: hash of message is %s\n", msg.GetMessageHash().GetHex());
        LogPrint(BCLog::RELAY, "Relay: hash in header is %d\n", *((int*) msg.hdr.pchChecksum));

        // handle the message
        {
            LOCK(pfrom->cs_vRecv);
            pfrom->vRecvMsg.push_back(msg);
        }
        notify = true;

    } else if (memcmp(msgPtr, RelayMsgType::BLK, NET_RELAY_MESSAGE_TYPE_LENGTH) == 0 && !isMasterNode /*TODO: && len == sizeof(struct RelayMsgBLK)*/) {
        //TODO: make sure that messages have the right length! (Security)
        struct RelayMsgBLK* rx_msg = (struct RelayMsgBLK*) msgPtr;

        if(blockWaitSet.empty()) return;

        // check if we requested that fragment
        LogPrint(BCLog::RELAY, "Relay: received fragment %u of current block\n", ntohs(rx_msg->segId));

        // check if we are waiting for this fragment
        {
            LOCK(cs_waitset);
            auto elemInWaitset = blockWaitSet.begin(); // instead, we now just take the first in the list

            // ok
            // add the fragment to the set of fragments
            // check if is first
            int dataLength = len - sizeof(struct RelayMsgBLK);
            msgPtr += sizeof(struct RelayMsgBLK);

            // create the fragment struct
            struct NetRelayFragment fragment;
            fragment.data = (char*) malloc(dataLength);
            memcpy(fragment.data, msgPtr, dataLength);
            fragment.size = dataLength;
            fragment.num = ntohs(rx_msg->segId);

            // insert ordered and check if we are finished
            if(elemInWaitset->fragments.empty()){
                elemInWaitset->fragments.push_back(fragment);
            } else {
                bool done = false;
                for (auto fragmentIt = elemInWaitset->fragments.rbegin(); fragmentIt != elemInWaitset->fragments.rend(); ++fragmentIt ) {
                    if(fragmentIt->num < fragment.num){
                        // found the spot
                        done = true;
                        elemInWaitset->fragments.insert(fragmentIt.base(), std::move(fragment));
                        break;
                    } else if(fragmentIt->num == fragment.num){
                        // received duplicate
                        done = true;
                        free(fragment.data);
                        break;
                    }
                }
                if (!done){
                    // add at beginning
                    elemInWaitset->fragments.push_front(fragment);
                }
            }

            elemInWaitset->lastFragmentTimestamp = std::chrono::system_clock::now();

            // as we enter the fragments ordered and make sure that there is no copy of the fragments, it is enough to have a look
            // at the size to make sure that we received all the fragments
            if(elemInWaitset->fragments.size() == elemInWaitset->numFragments){
                // we received everything.
                // create message
                CNetMessage msg = CNetMessage(Params().MessageStart(), SER_NETWORK, INIT_PROTO_VERSION);
                // header

                // write message piece by piece
                for(auto it = elemInWaitset->fragments.begin(); it != elemInWaitset->fragments.end(); ++it){
                    if(!msg.in_data){
                        msg.readHeader(it->data, CMessageHeader::HEADER_SIZE);
                        msg.readData(it->data + CMessageHeader::HEADER_SIZE, it->size - CMessageHeader::HEADER_SIZE);
                    } else {
                        msg.readData(it->data, it->size);
                    }
                    free(it->data);
                }

                // remove block from list
                blockWaitSet.erase(elemInWaitset);

                pfrom->vRecvMsg.push_back(msg);
                notify = true;
                LogPrint(BCLog::RELAY, "Relay: Block finished\n");
            }
        }
    } else if (memcmp(msgPtr, RelayMsgType::CTR, NET_RELAY_MESSAGE_TYPE_LENGTH) == 0 && !isMasterNode && len == sizeof(struct RelayMsgCTR)) {
        LogPrint(BCLog::RELAY, "Relay: Received CTR\n");
        // we should connect to the specified client
        // TODO: NOTE: this is a security risk. Anyone could send us a ip/port pair.
        struct RelayMsgCTR* rx_msg = (struct RelayMsgCTR*) msgPtr;
        CAddress addr(CService(), NODE_NONE);
        // get the address
        char tmpAddr[50];
        inet_ntop(AF_INET,&(rx_msg->ip), tmpAddr, 50);

        char ipString[100];
        sprintf(ipString, "%s:%u", tmpAddr, (unsigned int)ntohs(*((uint16_t*)(rx_msg->port+1))));

        LogPrint(BCLog::RELAY, "Relay: Try to connect to %s\n", ipString);

        connman->OpenNetworkConnection(addr, false, nullptr, ipString, false, false, true);

    } else if (memcmp(msgPtr, RelayMsgType::CON, NET_RELAY_MESSAGE_TYPE_LENGTH) == 0 && isMasterNode && len == sizeof(struct RelayMsgCON)) {
        LogPrint(BCLog::RELAY, "Relay: Received CON\n");
        struct RelayMsgCON* rx_msg = (struct RelayMsgCON*) msgPtr;

        struct connectionMetadata ipEntry;
        *((uint32_t*) ipEntry.srcPort) = *((uint32_t*) rx_msg->port);
        *((uint64_t*) ipEntry.srcIP) = *((uint64_t*) rx_msg->ip);

        char tmpAddr[50];
        inet_ntop(AF_INET,&(rx_msg->ip), tmpAddr, 50);

        char ipString[100];
        sprintf(ipString, "%s:%u", tmpAddr, ntohs(*((uint32_t*) ipEntry.srcPort)));

        auto ret = connections.insert(ipEntry);
        if(ret.second){
            LogPrint(BCLog::RELAY, "Relay: store metadata for %s\n", ipString);
        } else {
            LogPrint(BCLog::RELAY, "Relay: this host is already known: %s\n", ipString);
        }
    } else {
        error("Relay: received unhandled or malformed message of type %3.s. length is %u", msgPtr, len);
        return;
    }
}

bool Net_relay::connect(){
    //TODO: IPv6?
    //TODO: windows support?

    if(this->connected) return true;

    // create a UDP socket
    if (this->fd == -1){
        if ((this->fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
            error("Relay: Not able to create socket for relay network. Error: %s\n", std::strerror(errno));
            this->connected = false;
            return false;
        }

        // make socket non-blocking
        if (fcntl(this->fd, F_SETFL, fcntl(this->fd, F_GETFL, 0) | O_NONBLOCK) == -1){
            error("Relay: error during call to fcntl: %s", std::strerror(errno));
            this->connected = false;
            close(this->fd);
            this->fd = -1;
            return false;
        }

        // bind the socket to IP address and port
        if (bind(this->fd, (struct sockaddr *) &(this->netMyAddr), sizeof(this->netMyAddr)) < 0){
            error("Relay: Unable to bind to relay network. Error: %s\n", std::strerror(errno));
            this->connected = false;
            close(this->fd);
            this->fd = -1;
            return false;
        }
        rnode->hSocket = fd;
        LogPrint(BCLog::RELAY, "Relay: Created socket for relay network\n");
    }

    if(isMasterNode){
        LogPrint(BCLog::RELAY, "Relay: I am the master and don't need a handshake\n");
        connected=true;
        return true;
    }

    // send SYN
    char buf[NET_RELAY_HANDSHAKE_LENGTH];
    unsigned int adlen = sizeof(this->netRelayAddr);
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

    if(!this->requestSent || now > this->synRequestTimestamp){
        // set the SYN message
        memset(buf, 0, NET_RELAY_HANDSHAKE_LENGTH);
        strcpy(buf, RelayMsgType::REY);
        buf[NET_RELAY_MESSAGE_TYPE_LENGTH] = 0x10;
        LogPrint(BCLog::RELAY, "Relay: I will send %s now.\n", buf);
        if(sendto(this->fd, buf, NET_RELAY_HANDSHAKE_LENGTH,0, (struct sockaddr *)&(this->netRelayAddr), sizeof(this->netRelayAddr)) == -1){
            error("Relay: Could not send SYN message to relay network. Error: %s", std::strerror(errno));
            this->connected = false;
            return false;
        }
        LogPrint(BCLog::RELAY, "Relay: SYN sent. Wait for ACK from relay.\n");
        this->requestSent = true;
        this->synRequestTimestamp = now + std::chrono::seconds{60};
    }

    // check for SYNACK
    int recvlen = recvfrom(this->fd, buf, NET_RELAY_HANDSHAKE_LENGTH, 0, (struct sockaddr *)&(this->netRelayAddr), &adlen);

    // Check SYNACK and reply with ACK
    if (recvlen >= 0) {
        if (strstr(buf, "REY") != NULL && (buf[NET_RELAY_MESSAGE_TYPE_LENGTH] & 0xf0) == 0x20) {
            LogPrint(BCLog::RELAY, "Relay: received SYNACK: %s\n", buf);
            buf[NET_RELAY_MESSAGE_TYPE_LENGTH] = (buf[NET_RELAY_MESSAGE_TYPE_LENGTH] & 0x0f) + 0x30;
            LogPrint(BCLog::RELAY, "Relay: send ACK: %s\n", buf);
            if (sendto(this->fd, buf, NET_RELAY_HANDSHAKE_LENGTH, 0, (struct sockaddr *)&(this->netRelayAddr), adlen)==-1) {
                error("Relay:  error while sending ACK. Error: %s", std::strerror(errno));
                this->connected = false;
                return false;
            }
        }
    } else {
        this->connected = false;
        return false;
    }

    LogPrint(BCLog::RELAY, "Relay: Handshake complete\n");
    this->connected = true;
    return true;
}

Net_relay::Net_relay(std::string type, CConnman* cm)
{
    if(type == NET_RELAY_NODE_STANDARD){
        assert(!"node of type NET_RELAY_NODE_STANDARD should not exist");
    } else if (type == NET_RELAY_NODE_MASTER) {
        isMasterNode = true;
    }

    connman = cm;

    // set parameters
    this->connected = false;
    this->fd = -1;

    // bind socket to all local addresses and any port
    this->netMyAddr.sin_family = AF_INET;
    this->netMyAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    this->netMyAddr.sin_port = htons(gArgs.GetArg("-relaymyport", NET_RELAY_MY_UDP_PORT));
    LogPrint(BCLog::RELAY, "Relay: my addr is %s:%u\n", inet_ntoa(netMyAddr.sin_addr), ntohs(netMyAddr.sin_port));

    // set remote
    this->netRelayAddr.sin_family = AF_INET;
    this->netRelayAddr.sin_port = htons(gArgs.GetArg("-relayport", NET_RELAY_PORT));
    if (inet_aton(gArgs.GetArg("-relayaddr", NET_RELAY_IP).c_str(), &(this->netRelayAddr.sin_addr))==0) {
        assert(!"Relay: invalid ip address for relay node.\n");
    }
    LogPrint(BCLog::RELAY, "Relay: relay addr is %s:%u\n", inet_ntoa(netRelayAddr.sin_addr), ntohs(netRelayAddr.sin_port));

    // create CNode
    // TODO: what else do we have to set here?
    CAddress addr_bind;
    CAddress addr = CAddress(CService(netRelayAddr.sin_addr, ntohs(netRelayAddr.sin_port)), GetDesirableServiceFlags(NODE_NONE));
    //TODO: how should we set the id/version?
    //TODO: should we add the node like we add a regular one?
    rnode = new CNode(7777, NODE_NONE, 0, fd, addr, 0, 0, addr_bind);
    rnode->isRelay = true;
    rnode->nVersion = 7777;

    // connect
    this->connect();
}

Net_relay::~Net_relay()
{
    LogPrint(BCLog::RELAY, "Relay: cleanup\n");
    // close socket
    close(this->fd);
    delete rnode;
}

bool Net_relay::isConnected()
{
    return this->connected;
}

bool Net_relay::isMaster()
{
    return isMasterNode;
}

/**
 * This is taken from udp.c, calculates the first part of the checksum
 */
uint16_t udpPreChecksum(const void *buff, size_t len)
{
    // case iterate through the buffer byte by byte.
    const uint16_t *buf=(uint16_t*)buff;
    uint32_t sum;
    size_t length=len;

    // Calculate the sum
    sum = 0;
    while (len > 1)
    {
        sum += htons(*buf++);
        if (sum & 0x80000000)
            sum = (sum & 0xFFFF) + (sum >> 16);
        len -= 2;
    }

    if ( len & 1 )
        // Add the padding if the packet lenght is odd
        sum += *((uint8_t *)buf);

    // Add the pseudo-header
    sum += IPPROTO_UDP;
    sum += length + 8;
    sum += length + 8;

    // Add the carries
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return ntohs((uint16_t)(sum));
}
