#ifndef RDTConn
#define RDTConn

#include <sys/socket.h> // Socky stuff
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h> // htonl, ntohl, etc.
#include <unistd.h>
#include <string> // std::string

// We want to send one RDT segment per UDP packet and avoid IP fragmentation
// #define MTU 576 // minimum MTU guaranteed by IPv4
#define MTU 1500 // ethernet MTU is defined as 1500 bytes
#define IP_HEADER 20
#define UDP_HEADER 8
#define MSS MTU - IP_HEADER - UDP_HEADER // Max payload size for an actual segment

#define ACK_MASK 1 << 2;
#define SYN_MASK 1 << 1;
#define FIN_MASK 1 << 0;

class RDTConnection {
public:
    RDTConnection();
    virtual ~RDTConnection();

    bool connect( std::string afnet_address, int port = 0 );
    void close();
    void listen( int port );
    RDTConnection accept();

    bool send_data( std::string const &data );
    bool receive_data( std::string &data );

private:
    int sock_fd;
    sockaddr_in remote_addr;
    sockaddr_in local_addr;

    struct rdt_header_t {
        uint16_t src_port;
        uint16_t dst_port;
        uint16_t seq_num;
        uint16_t ack_num;
        uint16_t data_len;
        uint32_t host_ip; // IP address of host issuing SYN message
        uint16_t flags;
    };

    // Do not exceed the max MSS allowed
    struct rdt_packet_t {
        rdt_header_t header;
        char data[ MSS - sizeof(rdt_header_t) ];
    };

    bool isACK(rdt_packet_t &pkt) { return pkt.header.flags & ACK_MASK; }
    bool isSYN(rdt_packet_t &pkt) { return pkt.header.flags & SYN_MASK; }
    bool isFIN(rdt_packet_t &pkt) { return pkt.header.flags & FIN_MASK; }

    void setACK(rdt_packet_t &pkt) { pkt.header.flags |= ACK_MASK; }
    void setSYN(rdt_packet_t &pkt) { pkt.header.flags |= SYN_MASK; }
    void setFIN(rdt_packet_t &pkt) { pkt.header.flags |= FIN_MASK; }
};

#endif