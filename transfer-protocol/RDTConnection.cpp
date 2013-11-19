#include "RDTConnection.h"
#include <sys/time.h> // gettimeofday
#include <arpa/inet.h> // htonl, ntohl, etc.
#include <unistd.h>
#include <iostream> // std::cout
#include <sstream> // std::stringstream

RDTConnection::RDTConnection() : sock_fd( -1 ), is_listener( false ) {
    memset( &remote_addr, 0, sizeof( remote_addr ));
    memset( &local_addr, 0, sizeof( local_addr ));
}

RDTConnection::~RDTConnection() {
    close(true); // force teardown, object destroyed
}

/**
 * Public interface for establishing connections
 */
bool RDTConnection::connect( std::string const &afnet_address, int port ) {
    return connect(afnet_address, port, false);
}

/**
 * Sends a SYN packet to the remote host and blocks until an ACK is received or
 * connection times out. Only establishes a one way connection from local host
 * to the remote host. read_network_packet() is responsible for responding to any
 * remote SYN requests.
 *
 * Returns true if local-to-remote connection established, false otherwise.
 */
bool RDTConnection::connect( std::string const &afnet_address, int port, bool sendSYNACK ) {
    // Listeners have already bound a socket, simply try to connect to the remote
    // If we are establishing a brand new connection, a bind is still needed
    if (!is_listener) {
        if (!bind()) {
            std::cout << "Failed to bind connection socket" << std::endl;
            return false;
        }
    }

    // Establish remote host info
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(port);
    if ( inet_pton(AF_INET, afnet_address.c_str(), (void *)&remote_addr.sin_addr.s_addr ) != 1 ) {
        close();
        return false;
    }

    std::cout << "Attempting to connect to " << afnet_address << ":" << port << "..." << std::endl;

    // Set the socket timeout (inactivity) and bail if setting the option fails
    timeval timeout;
    timeout.tv_sec = RDT_TIMEOUT_SEC;
    timeout.tv_usec = RDT_TIMEOUT_USEC;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        close();
        std::cout << "failed to set socket timeout\n";
        return false;
    }

    // Send a SYN packet to the remote host
    rdt_packet_t pkt;
    build_network_packet(pkt, "");
    setSYN(pkt);
    if(sendSYNACK)
        setSYNACK(pkt);

    // Bail on transmission errors
    if ( !broadcast_network_packet(pkt) ) {
        close();
        std::cout << "SYN packet transmission failed\n";
        return false;
    }

    // Wait until remote host SYNACKs our SYN packet
    // SYN packets sent by the remote host are replied by read_network_packet();
    timeval start, now;
    gettimeofday(&start, NULL);
    int delta_sec = RDT_TIMEOUT_SEC;
    int delta_usec = RDT_TIMEOUT_USEC;

    do {
        if (read_network_packet(pkt) && isSYNACK(pkt)) {
            std::cout << "Connected to " << afnet_address << ":" << port << std::endl;
            return true; // Got the SYNACK, return success!
        }
        else if (errno == EWOULDBLOCK) {
            break; // Socket timeout, bail
        }

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
    std::cout << "Connection attempt to " << afnet_address << ":" << port << " timed out" << std::endl;
    return false;
}

/**
 * Creates a system socket to send and receive packets
 */
bool RDTConnection::bind( int port ) {
    close(); // Close existing connection if any

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(port);

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

    // Bind socket so we can receive incoming packets
    if (sock_fd == -1 || ::bind(sock_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        close();
        return false;
    }

    // Double check what port the system gave us
    port = port_number();
    local_addr.sin_port = htons(port);
    return true;
}

/**
 * Public interface for closing connections
 *
 * It will never force listeners to tear down their sockets, which makes
 * it possible to simply close the current connection and continue to listen
 * for additional connection requests
 */
void RDTConnection::close() {
    close(false); // do not force listeners to teardown sockets
}

void RDTConnection::close(bool force_teardown) {
    // TODO: properly tear down connection
    // TODO: log connection close

    memset( &remote_addr, 0, sizeof( remote_addr ));

    // Teardown regular sockets or when listener is destroyed
    if (force_teardown || !is_listener) {
        if (sock_fd != -1)
            ::close( sock_fd );

        sock_fd = -1;
        is_listener = false;
        memset( &local_addr, 0, sizeof( local_addr ));
    }

    if (!force_teardown && is_listener) {
        // Remove the socket timeout (inactivity) to allow blocking while waiting for connections
        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    }
}

/**
 * Establishes the connection object as a listener which can accept
 * arbitrary connection requests
 */
bool RDTConnection::listen( int port ) {
    close(true); // re-establsh listener
    return is_listener = bind(port);
}

/**
 * Accepts a remote connection. If successful, caller can begin writing and reading
 * data from the object. Additional connection requests will be dropped until the
 * current connection is closed.
 *
 * If the object is a listener, function will block until a successful connection
 * is established. If it isn't a listener, function will immediately return false.
 */
bool RDTConnection::accept() {
    bool connected = false;
    char ip_addr[INET_ADDRSTRLEN];
    rdt_packet_t pkt;
    sockaddr_in incoming_addr;

    // Only established listeners can accept connections
    if (!is_listener)
        return false;

    while ( !connected ) {
        memset(&incoming_addr, 0, sizeof(incoming_addr));
        if (!read_network_packet(pkt, false, &incoming_addr))
            continue;

        if(isSYN(pkt)) {
            inet_ntop(AF_INET, &incoming_addr.sin_addr.s_addr, ip_addr, sizeof(ip_addr));
            std::cout << "Connection request from " << ip_addr << ":" << pkt.header.src_port << "...\n";
            connected = connect(ip_addr, pkt.header.src_port, true);
        } else {
            drop_packet(pkt, "non-SYN packet received when awaiting incoming connections");
        }
    }

    return connected;
}

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
 * Returns the port a connection is bound to or -1 on failure
 */
int RDTConnection::port_number() {
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if ( getsockname(sock_fd, (sockaddr *)&addr, &addr_len) == -1 )
        return -1;
    else
        return ntohs( addr.sin_port );
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

/**
 * Function will keep reading from the network until it finds (what it sees) as
 * a valid RDT packet. If no data is left (socket times out), the function will
 * return to its caller.
 */
bool RDTConnection::read_network_packet(rdt_packet_t &pkt, bool verify_remote, sockaddr_in *ain) {
    memset(&pkt, 0, sizeof(pkt));

    sockaddr_in default_addr;
    sockaddr_in *recv_addr = ain ? ain : &default_addr;
    socklen_t recv_addr_len = sizeof(default_addr);

    ssize_t len   = 0;
    size_t offset = 0;
    int max_read  = 0;
    bool valid_header = false;
    bool valid_packet = false;

    // First we try to find a packet header from the UDP buffer
    // We reject packets from unexpected hosts after the *entire* packet
    // is read from the UDP buffer so that we can get rid of the garbage data
    while( !valid_packet ) {
        while( !valid_header ) {
            max_read = std::max(sizeof(pkt.header) - offset, 0ul);
            len = recvfrom(sock_fd, (&pkt.header)+offset, max_read, 0, (sockaddr *)recv_addr, &recv_addr_len);

            // Time out, let caller handle problem
            if (len == -1 && errno == EWOULDBLOCK) {
                drop_packet(pkt, "socket timeout when waiting for packet to arrive");
                return false;
            }

            if (len > 0)
                offset += len;

            // Keep reading until we get the expected size of a packet
            if (offset < sizeof(pkt.header)) {
                continue; // find header loop
            } else if (pkt.header.magic_num == RDT_MAGIC_NUM) {
                // If the magic number is aligned we got a proper header
                valid_header = true;
                break;
            } else { // Misaligned packet, attempt to recover
                const int MAGIC_NUM = RDT_MAGIC_NUM;
                char magicbuf[sizeof(pkt.header.magic_num)];
                memcpy(&magicbuf, &MAGIC_NUM, sizeof(magicbuf));

                // Check if magic number appears in the bytes we've read
                // If so, move the data up to the start of the header
                for (int i = 0; i < offset - sizeof(magicbuf); i++) {
                    if ( memcmp( (&pkt.header)+i, &magicbuf, sizeof(magicbuf) ) == 0 ) {
                        offset -= i;
                        memmove( &pkt.header, (&pkt.header)+i, offset );
                        break; // for loop
                    }
                }

                std::stringstream ss;
                ss << "misaligned packet: " << offset << " bytes dropped";
                drop_packet(pkt, ss.str());

                // Reset offset if no memory was moved and allow outer loop to keep reading
                if (pkt.header.magic_num != RDT_MAGIC_NUM) {
                    offset = 0;
                    valid_header = false;
                }
            }
        }

        // Note: we assume if the header has arrived, so has the rest of the packet
        // They should be sufficiently small so that it would be unlikely for the system
        // to return before the rest of the packet is received. In such a situation, treat
        // the packet as corrupted
        if (valid_header) {
            max_read = (pkt.header.data_len < sizeof(pkt.data) ? pkt.header.data_len : sizeof(pkt.data));
            len = recvfrom(sock_fd, &pkt.data, max_read, 0, NULL, NULL);

            if (len == -1) {
                if (errno == EWOULDBLOCK) {
                    drop_packet(pkt, "socket timeout while receiving packet");
                    return false;
                } else {
                    drop_packet(pkt, "unknown transmission error");
                    valid_header = false;
                }
            } else if (len < max_read) {
                drop_packet(pkt, "received packet was shorter than expected");
                valid_header = false;
            } else if ( verify_remote &&
                        (       recv_addr->sin_addr.s_addr != remote_addr.sin_addr.s_addr
                            ||  htons(pkt.header.src_port) != remote_addr.sin_port
                        )
            ) {
                drop_packet(pkt, "packet received from unexpected host");
                valid_header = false;
            } else if (len == max_read) {
                valid_packet = true;
            }
        } // valid header while loop
    } // valid packet while loop

    if (valid_packet) {
        // If remote host we've already connected to sends a SYN packet at any point
        // (because, say, our prevoius SYNACK was dropped) SYNACK it immediately
        if (isSYN(pkt) && verify_remote) {
            rdt_packet_t ack;
            build_network_packet(ack, "");
            setSYNACK(ack);
            broadcast_network_packet(ack);
        }

        return true;
    }

    return false;
}

/**
 * Writes to stdout that a packet was dropped for a given reason
 */
void RDTConnection::drop_packet(rdt_packet_t &pkt, std::string const &reason) {
    memset(&pkt, 0, sizeof(pkt));

    time_t now;
    char date[32];

    time(&now);
    strftime( date, sizeof(date), "%D %T: ", localtime(&now));

    std::cout << date << (reason == "" ? "unknown error" : reason) << std::endl;
}
