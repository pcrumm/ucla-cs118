#include "RDTConnection.h"
#include <iostream> // std::string

RDTConnection::RDTConnection() : sock_fd( -1 ) {
    memset( &remote_addr, 0, sizeof( remote_addr ));
    memset( &local_addr, 0, sizeof( local_addr ));
}

RDTConnection::~RDTConnection() {
    close();
}

/**
 * Creates a socket, binds it, and sends a SYN packet to the remote host
 * Blocks until an ACK is received or connection times out. Only establishes
 * a one way connection from local host to the remote host. read_network_packet()
 * is responsible for responding to any remote SYN requests.
 *
 * Returns true if local-to-remote connection established, false otherwise.
 */
bool RDTConnection::connect( std::string const &afnet_address, int port ) {
    close(); // Close existing connection if any

    local_addr.sin_family = AF_INET;
    local_addr.sin_port = 0;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(port);

    // Parse the IP address string
    if ( inet_pton(AF_INET, afnet_address.c_str(), (void *)&remote_addr.sin_addr.s_addr ) != 1 ) {
        close();
        return false;
    }

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

    // Bind socket so we can listen to the remote host
    if (sock_fd == -1 || bind(sock_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        close();
        return false;
    }

    // Set the socket timeout (inactivity) and bail if setting the option fails
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = RDT_TIMEOUT_USEC;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        close();
        return false;
    }

    // Send a SYN packet to the remote host
    rdt_packet_t pkt;
    build_network_packet(pkt, "");
    setSYN(pkt);

    // Bail on transmission errors
    if ( !broadcast_network_packet(pkt) ) {
        close();
        return false;
    }

    // Wait until remote host SYNACKs our SYN packet
    // SYN packets sent by the remote host are replied by read_network_packet();
    timeval start, now;
    gettimeofday(&start, NULL);
    int delta_sec = 0;
    int delta_usec = RDT_TIMEOUT_USEC;

    do {
        rdt_packet_t recv_pkt;
        if (read_network_packet(pkt) && isSYNACK(pkt))
            return true; // Got the SYNACK, return success!
        else if (errno == EWOULDBLOCK)
            break; // Socket timeout, bail

        // Timeout for getting an ACK
        // this is different from a socket timeout as receiving
        // garbage packets would keep resetting that timer
        gettimeofday(&now, NULL);
    } while (
            now.tv_usec - start.tv_usec < delta_usec
        &&  now.tv_sec - start.tv_sec < delta_sec
    );

    // Timed out
    close();
    return false;
}

void RDTConnection::close() {
    // TODO: properly tear down connection
    if (sock_fd != -1) {
        ::close( sock_fd );
    }

    sock_fd = -1;
    memset( &remote_addr, 0, sizeof( remote_addr ));
    memset( &local_addr, 0, sizeof( local_addr ));
}

/**
 * Initializes a socket and listens for incoming connections
 * If object has an existing connection, it will be closed.
 *
 * Returns a bool indicating successful port binding
 */
bool RDTConnection::listen( int port ) {
    close(); // Close existing connection if any

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(port);

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
        close();
        return false;
    }

    if (bind(sock_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        close();
        return false;
    }

    return true;
}

// RDTConnection RDTConnection::accept() {
    // TODO
// }

bool RDTConnection::send_data( std::string const &data ) {
    // TODO
    return false;
}

bool RDTConnection::receive_data( std::string &data ) {
    // TODO
    data = "";
    return false;
}

/**
 * Initializes a network packet with as much specified data as the packet can hold
 * Returns the amount of data bytes placed into the packet
 */
inline int RDTConnection::build_network_packet(rdt_packet_t &pkt, std::string const &data) {
    memset((void *)&pkt, 0, sizeof(rdt_packet_t));

    pkt.header.magic_num = RDT_MAGIC_NUM;
    pkt.header.src_port  = ntohs(local_addr.sin_port);
    pkt.header.dst_port  = ntohs(remote_addr.sin_port);
    pkt.header.seq_num   = 0;
    pkt.header.ack_num   = 0;
    pkt.header.data_len  = std::min(sizeof(pkt.data), data.size());
    pkt.header.flags     = 0;

    memcpy(&pkt.data, data.c_str(), pkt.header.data_len);
    return pkt.header.data_len;
}

/**
 * Sends a formatted packet to remote_addr.
 * Returns true if packet broadcasted properly, false otherwise
 */
inline bool RDTConnection::broadcast_network_packet(rdt_packet_t const &pkt) {
    size_t len = std::min(sizeof(rdt_header_t) + pkt.header.data_len, sizeof(rdt_packet_t));
    return len == sendto(sock_fd, &pkt, len, 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
}

bool RDTConnection::read_network_packet(rdt_packet_t &pkt) {
    // TODO
    return false;
}

/**
 * Writes to stdout that a packet was dropped for a given reason
 */
void RDTConnection::drop_packet(std::string const &reason) {
    if (reason != "") {
        std::cout << "Dropped packet: " << reason << std::endl;
    }
}