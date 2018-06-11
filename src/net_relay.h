#ifndef NET_RELAY_H
#define NET_RELAY_H

#include <util.h>
#include <chrono>
#include <threadinterrupt.h>
#include <uint256.h>
#include <primitives/block.h>

class CNode;
class CConnman;
class CNetAddr;

// NODE TYPES
static const std::string NET_RELAY_NODE_STANDARD = "standard";
static const std::string NET_RELAY_NODE_CLIENT = "client";
static const std::string NET_RELAY_NODE_MASTER = "master";

// MESSAGE TYPES
namespace RelayMsgType {
    const char* const UPD = "UPD"; //(Update)
    const char* const SEG = "SEG"; //(Get_seg)
    const char* const REY = "REY"; //(Handshake)
    const char* const ADV = "ADV"; //(Advertisement)
    const char* const INV = "INV"; //(Inventory)
    const char* const BLK = "BLK"; //(Block Data)
    const char* const BCL = "BCL"; //(add to Black List )
    const char* const CON = "CON"; //(new Connection)
    const char* const CTR = "CTR"; //(new Connection)
}
#define NET_RELAY_MESSAGE_TYPE_LENGTH 3

// MESSAGE REY
// 24   :REY
// 4	:flag
// 20 	:secret
#define NET_RELAY_HANDSHAKE_LENGTH 6


// MESSAGE INV
// 24   :INV
// 256	:hash
// 16	:num_segs
struct RelayMsgINV{
    char cmd[3];
    uint256 hash;
    uint16_t segCount;
}__attribute__((__packed__));

// MESSAGE SEG
// 24   :SEG
// 256	:hash
// 16	:seg_id
struct RelayMsgSEG{
    char cmd[3];
    uint256 hash;
    uint16_t segId;
}__attribute__((__packed__));

// MESSAGE BLK
struct RelayMsgBLK{
    char cmd[3];
    uint16_t segId;
}__attribute__((__packed__));

// MESSAGE ADV
// 24   :ADV
// 256	:hash
struct RelayMsgADV{
    char cmd[3];
    uint256 hash;
}__attribute__((__packed__));

// MESSAGE CTR
// 24   :CTR
// 32   :IP
// 24   :port
struct RelayMsgCTR{
    char cmd[3];
    char ip[4];
    char port[3];
}__attribute__((__packed__));

// MESSAGE CON
// 24   :CON
// 24   :port
// 32   :IP
struct RelayMsgCON{
    char cmd[3];
    char dummy;
    char port[2];
    char ip[4];
}__attribute__((__packed__));

// MESSAGE UPD
// 24   :UPD
// 256	:hash
struct RelayMsgUPD{
    char cmd[3];
    uint256 hash;
}__attribute__((__packed__));

// MESSAGE BLK_DATA
// 24   :BLK
// 16   :seg_id
// 499*8:data
// 16   :check
struct RelayMsgBLKData{
    char cmd[3];
    uint16_t segId;
    char data[499];
    uint16_t checksum;
}__attribute__((__packed__));

// MESSAGE BCL
// 24   :BCL
// 32	:ip
struct RelayMsgBCL{
    char cmd[3];
    char ip[4];
}__attribute__((__packed__));

// PRIMITIVES
#define NET_RELAY_HASH_BYTES 32u
#define NET_RELAY_SEGCOUNT_BYTES 2u
typedef uint16_t netRelaySegcount;
typedef uint16_t netRelaySegId;

// SEGMENT SIZES
#define NET_RELAY_SEGMENT_SIZE 499



// TODO: move to config
// CONNECTION SETTINGS
#define NET_RELAY_MY_UDP_PORT 0
#define NET_RELAY_IP "127.0.0.1"
#define NET_RELAY_PORT 8080
#define NET_RELAY_MAX_CONCURRENT_INV 10

// CONNECTION SETUP
#define NET_RELAY_SECRET_LENGTH 1

// MESSAGE STRUCTURE
typedef uint8_t netRelayCommand;
#define NET_RELAY_COMMAND_BYTES 1u


typedef uint16_t netRelayFragno;
#define NET_RELAY_FRAGNO_BYTES 2u

// COMMANDS
#define NET_RELAY_MESSAGE_TYPE_RINV 0x01
#define NET_RELAY_MESSAGE_TYPE_RGETDATA 0x02
#define NET_RELAY_MESSAGE_TYPE_RBLOCK 0x03

struct connectionMetadata{
    char srcPort[2];
    char srcIP[4];
    //uint32_t dstIP;
    //char[3] dstPort;

    bool operator==(const struct connectionMetadata &i) const
    {
        return (memcmp(this, &i, sizeof(struct connectionMetadata)) == 0);
    }
    bool operator<(const struct connectionMetadata &i) const
    {
        return (memcmp(this, &i, sizeof(struct connectionMetadata)) < 0);
    }
};

struct NetRelayFragment{
    char* data;
    unsigned int size;
    unsigned int num;

    bool operator==(const unsigned int &i) const
    {
        return num == i;
    }
};

typedef struct NetRelayWaitset{
    uint256 hash;
    unsigned int numFragments;

    bool operator==(const uint256 &r) const
    {
        return this->hash == r;
    }

    std::list<struct NetRelayFragment>fragments;
    std::chrono::system_clock::time_point lastFragmentTimestamp;
    std::chrono::system_clock::time_point timeout;
} NetRelayWaitset;


/*
 * Connection handler for connections to the relay network.
 * TODO: add more information
 */
class Net_relay{
private:
    bool requestSent = false;
    std::chrono::system_clock::time_point synRequestTimestamp;
    void RGETDATATimeoutHandler(uint256 hash);
    bool isMasterNode = false;
    void sendMessageToRelay(CConnman* connman, CNode* rnode, unsigned char* data, unsigned int dataLength);
    uint16_t sendFragmentsToSwitch(uint256 hash, const std::shared_ptr<const CBlock> block);
    std::set<struct connectionMetadata> connections;
public:
    struct sockaddr_in  netRelayAddr;
    struct sockaddr_in  netMyAddr;
    socklen_t addrlen;
    unsigned int fd;
    bool connected;
    CNode* rnode;

    CConnman* connman;

    CThreadInterrupt* interruptNet; // used to signal halt

    bool connect();
    void checkSocket();
    void HandleMessage(char* buf, unsigned int len, bool& notify, CNode* pfrom);
    bool isMaster();

    CCriticalSection cs_waitset;
    std::list<NetRelayWaitset> blockWaitSet;
    CCriticalSection cs_sendset;
    std::list<NetRelayWaitset> sendWaitSet;
    void addInv(uint256 hash, uint16_t frags);
    void createRGETDATAMessages(const uint256 &waitHash);

    Net_relay(std::string type, CConnman* cm);
    ~Net_relay();
    bool isConnected();
    void ThreadRelayHandler();

    //RINV sending
    void sendRINVtoSwitch(uint256 hash, CNode* pnode);

    //Blacklisting
    void blacklist(const CNetAddr& addr);
};

uint16_t udpPreChecksum(const void *buff, size_t len);

#endif
