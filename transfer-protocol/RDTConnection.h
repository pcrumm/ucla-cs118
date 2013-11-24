#ifndef RDTConn
#define RDTConn
#include <netinet/in.h> // sockaddr_in
#include <string> // std::string

// We want to send one RDT segment per UDP packet and avoid IP fragmentation
// #define MTU 576 // minimum MTU guaranteed by IPv4
#define MTU 1500 // ethernet MTU is defined as 1500 bytes
#define IP_HEADER 20
#define UDP_HEADER 8
#define MSS MTU - IP_HEADER - UDP_HEADER // Max payload size for an actual segment

#define EOF_MASK    1 << 5; // Used to represent the last packet in a transmission
#define FINACK_MASK 1 << 4; // Separate ACK for FIN to avoid confusion from ACK delays
#define SYNACK_MASK 1 << 3; // Separate ACK for SYN to avoid confusion from ACK delays
#define ACK_MASK    1 << 2;
#define SYN_MASK    1 << 1;
#define FIN_MASK    1 << 0;

#define RDT_MAGIC_NUM 0xCABBA6E5
#define RDT_TIMEOUT_SEC 1
#define RDT_TIMEOUT_USEC 500000 // 500ms
#define USEC_CONVERSION 1000000

#define MAX_TIMEOUTS 3
#define MAX_DUPLICATE_ACK 3

class RDTConnection {
public:
    RDTConnection(int w_size);
    virtual ~RDTConnection();

    bool connect( std::string const &afnet_address, int port );
    void close();
    bool listen( int port );
    bool accept();

    bool send_data( std::string const &data );
    bool receive_data( std::string &data );

    int port_number();

private:
    bool is_listener;
    bool listener_connected;
    bool got_FIN;
    int sock_fd;
    sockaddr_in remote_addr;
    sockaddr_in local_addr;

    size_t window_size;

    struct rdt_header_t {
        uint32_t magic_num; // Used for packet alignment when reading from network
        uint16_t src_port;
        uint16_t dst_port;
        uint16_t seq_num;
        uint16_t ack_num;
        uint16_t data_len;
        uint16_t flags;
    };

    // Do not exceed the max MSS allowed
    struct rdt_packet_t {
        rdt_header_t header;
        char data[ MSS - sizeof(rdt_header_t) ];
    };

    bool isEOF(rdt_packet_t &pkt) { return pkt.header.flags & EOF_MASK; }
    bool isFINACK(rdt_packet_t &pkt) { return pkt.header.flags & FINACK_MASK; }
    bool isSYNACK(rdt_packet_t &pkt) { return pkt.header.flags & SYNACK_MASK; }
    bool isACK(rdt_packet_t &pkt) { return pkt.header.flags & ACK_MASK; }
    bool isSYN(rdt_packet_t &pkt) { return pkt.header.flags & SYN_MASK; }
    bool isFIN(rdt_packet_t &pkt) { return pkt.header.flags & FIN_MASK; }

    void setEOF(rdt_packet_t &pkt) { pkt.header.flags |= EOF_MASK; }
    void setFINACK(rdt_packet_t &pkt) { pkt.header.flags |= FINACK_MASK; }
    void setSYNACK(rdt_packet_t &pkt) { pkt.header.flags |= SYNACK_MASK; }
    void setACK(rdt_packet_t &pkt) { pkt.header.flags |= ACK_MASK; }
    void setSYN(rdt_packet_t &pkt) { pkt.header.flags |= SYN_MASK; }
    void setFIN(rdt_packet_t &pkt) { pkt.header.flags |= FIN_MASK; }

    int  build_network_packet(rdt_packet_t &pkt, std::string const &data, size_t max_data_len = 0, size_t data_offset = 0);
    bool broadcast_network_packet(rdt_packet_t const &pkt);
    bool read_network_packet(rdt_packet_t &pkt, bool verify_remote = true, sockaddr_in *ain = NULL);
    void drop_packet(rdt_packet_t &pkt, std::string const &reason);

    bool connect(std::string const &afnet_address, int port, bool sendSYNACK);
    bool bind(int port = 0);
    void close(bool force_teardown);

    void log_event(std::string const &msg);
};

#endif