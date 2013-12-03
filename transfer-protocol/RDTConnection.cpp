#include "RDTConnection.h"
#include <sys/time.h> // gettimeofday
#include <arpa/inet.h> // htonl, ntohl, etc.
#include <unistd.h>
#include <cstdlib> // malloc etc.
#include <cstring> // memset, memcpy, etc.
#include <cerrno> // errno
#include <iostream> // std::cout
#include <sstream> // std::stringstream
#include <math.h> // ceil
#include <algorithm> // std::min etc.
#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))

RDTConnection::RDTConnection(int w_size, double ploss, double pcorrupt)
    :   window_size( w_size ),
        prob_loss( std::max(0.0, std::min(100.0, ploss)) ),
        prob_corrupt( std::max(0.0, std::min(pcorrupt, 100.0)) ),
        sock_fd( -1 ),
        got_FIN( false ),
        is_listener( false ),
        listener_connected( false )
{
    memset( &remote_addr, 0, sizeof( remote_addr ));
    memset( &local_addr, 0, sizeof( local_addr ));

    srand(time(0)); // seed for simulating random network errors
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
            log_event("Failed to bind connection socket");
            return false;
        }
    }

    got_FIN = false;

    // Establish remote host info
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(port);
    if ( inet_pton(AF_INET, afnet_address.c_str(), (void *)&remote_addr.sin_addr.s_addr ) != 1 ) {
        close();
        return false;
    }

    std::stringstream ip_ss;
    ip_ss << afnet_address << ":" << port;
    std::string ip_str = ip_ss.str();
    log_event("Attempting to connect to " + ip_str);

    // Set the socket timeout (inactivity) and bail if setting the option fails
    timeval timeout;
    timeout.tv_sec = RDT_TIMEOUT_SEC;
    timeout.tv_usec = RDT_TIMEOUT_USEC;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        close();
        log_event("failed to set socket timeout");
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
        log_event("SYN packet transmission failed");
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
            log_event("Connected to " + ip_str);
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
    log_event("Connection attempt to " + ip_str + " timed out");
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
    // If a non connected listener tries to close the connection it will
    // get stuck as non-connected listener sockets don't have a timeout
    if ((!is_listener && sock_fd != -1) || (is_listener && listener_connected)) {
        std::stringstream ss;
        char ip_addr[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &remote_addr.sin_addr.s_addr, ip_addr, sizeof(ip_addr));
        ss << "Closing connection to " << ip_addr << ":" << ntohs(remote_addr.sin_port);
        log_event(ss.str());

        int num_timeouts = 0;
        bool got_ACK = false;
        bool resend_pkt = true;
        rdt_packet_t pkt;

        // Wait for the remote host to acknowledge our FIN
        while (!got_ACK && num_timeouts < MAX_HANDSHAKE_TIMEOUTS) {
            if (resend_pkt) {
                build_network_packet(pkt, "");
                setFIN(pkt);
                broadcast_network_packet(pkt);
                resend_pkt = false;
            }

            if (read_network_packet(pkt)) {
                got_ACK = isFINACK(pkt);
                got_FIN = got_FIN || isFIN(pkt);
            } else if (errno == EWOULDBLOCK) {
                resend_pkt = true;
                num_timeouts++;
            }
        }

        if (got_ACK) { // Successfully got a FINACK
            num_timeouts = 0;
            while (!got_FIN && num_timeouts < MAX_HANDSHAKE_TIMEOUTS) {
                if (read_network_packet(pkt))
                    got_FIN = isFIN(pkt);
                else if (errno == EWOULDBLOCK)
                    num_timeouts++;
            }
        } else { // Number of tries exhausted, assume connection is gone
            log_event("Timeout while waiting for FINACK, terminating connection");
        }

        if (!got_FIN) {
            got_FIN = true;
            log_event("Timeout while waiting for FIN, terminating connection");
        } else {
            log_event("Connection closed");
        }
    }

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

        listener_connected = false;
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
    char ip_addr[INET_ADDRSTRLEN];
    rdt_packet_t pkt;
    sockaddr_in incoming_addr;

    // Only established listeners can accept connections
    if (!is_listener)
        return false;
    else
        listener_connected = false;

    while ( !listener_connected ) {
        memset(&incoming_addr, 0, sizeof(incoming_addr));
        if (!read_network_packet(pkt, false, &incoming_addr))
            continue;

        if(isSYN(pkt)) {
            inet_ntop(AF_INET, &incoming_addr.sin_addr.s_addr, ip_addr, sizeof(ip_addr));
            std::stringstream ss;
            ss << "Connection request from " << ip_addr << ":" << pkt.header.src_port;
            log_event(ss.str());
            listener_connected = connect(ip_addr, pkt.header.src_port, true);
        } else {
            drop_packet(pkt, "non-SYN packet received when awaiting incoming connections");
        }
    }

    return listener_connected;
}

bool RDTConnection::send_data( std::string const &data ) {
    // First, compute the number of windows we need, then create a little structure 
    // for storing pertinent information.
    size_t necessary_windows = (window_size / MSS) + 1;
    size_t current_window = 0;

    struct connection_window {
        bool is_acked;          // Used to track if the data in this window was acknowledged.
        size_t seq_num;         // The sequence number for this particular data item.
        timeval sent_on_time;   // The time the data was sent on. Used for computing timeout.
    };

    struct connection_window *windows = (struct connection_window*)malloc(sizeof(struct connection_window) * necessary_windows);

    // Configure the default settings of the struct
    for (int i = 0; i < necessary_windows; i++) {
        windows[i].is_acked = true;
        windows[i].seq_num = 0;
        windows[i].sent_on_time.tv_sec = 0;
        windows[i].sent_on_time.tv_usec = 0;
    }

    size_t current_unacknowledged_bytes = 0;
    size_t total_acknowledged_bytes = 0;
    size_t last_ack = 0;
    uint16_t timeout_count = 0;

    size_t data_length = data.length();
    std::stringstream ss;
    ss << "Preparing to transmit " << data_length << " bytes!";
    log_event(ss.str());

    rdt_packet_t pkt;
    size_t current_packet_size;
    size_t current_packet_max_size;

    bool fast_retransmit = false;

    while (true) {
        // If everything is acknowledged, we're done!
        if (total_acknowledged_bytes >= data_length && total_acknowledged_bytes != 0 && data_length != 0) {
            log_event("Transmission complete.");
            return true;
        }

        /**
         * To simplify things, we do this asynchronously. We first send every segment
         * the window can hold. Once we've filled the window size, we start reading.
         * We take care to not transmit over an unACKED window.
         */
        while (current_unacknowledged_bytes + total_acknowledged_bytes < data_length && current_unacknowledged_bytes < window_size && windows[current_window].is_acked) {
            // We need to take care to not try to send any more data than the window will allow.
            current_packet_max_size = std::min((size_t)window_size - current_unacknowledged_bytes, sizeof(pkt.data));
            current_packet_size = build_network_packet(pkt, data, current_packet_max_size, total_acknowledged_bytes + current_unacknowledged_bytes);
            current_unacknowledged_bytes += current_packet_size;

            // We also need to set some clerical data for the packet--namely, the sequence number.
            // The sequence number represents the numerical ID of the /last/ byte of data in the packet.
            pkt.header.seq_num = total_acknowledged_bytes + current_unacknowledged_bytes;
            windows[current_window].is_acked = false;
            windows[current_window].seq_num = pkt.header.seq_num;
            gettimeofday(&windows[current_window].sent_on_time, NULL);

            if ((current_unacknowledged_bytes + total_acknowledged_bytes) >= data_length) {
                setEOF(pkt);
                log_event("Prepared EOF packet for transmission.");
            }

            // Advance the window
            current_window = (current_window + 1) % necessary_windows;

            std::stringstream ss;
            ss << "Preparing to transmit packet with SEQ " << pkt.header.seq_num << " and payload " << current_packet_size;
            ss << " - Current window has " << current_unacknowledged_bytes << " of " << window_size;
            log_event(ss.str());
            broadcast_network_packet(pkt);
        }

        /**
         * Now that we've broken out of here, we've either sent enough data, or ran out of windows.
         * To handle this, we first see if any packets have timed out in the window. If they have,
         * then we reset to that packet and begin resending. If not, we start checking for ACKs.
         */
        for (int i = 0; i < necessary_windows; i++) {
            timeval now;
            gettimeofday(&now, NULL);

            long delta_sec  = now.tv_sec  - windows[i].sent_on_time.tv_sec;
            long delta_usec = now.tv_usec - windows[i].sent_on_time.tv_usec;
            bool timed_out = delta_sec > RDT_TIMEOUT_SEC || (delta_sec == RDT_TIMEOUT_SEC && delta_usec > RDT_TIMEOUT_USEC);

            if (!windows[i].is_acked && timed_out) {
                // The packet has timed out. Resend it, and everything after it. To do this, set the
                // current_unacknowledged_bytes to empty, and make sure every packet after is marked
                // as acked so we can write over it, and rewind the current_window index.
                current_unacknowledged_bytes = 0;
                current_window = (current_window - 1) % necessary_windows;

                for (int j = i; j < necessary_windows; j++)
                    windows[j].is_acked = true;

                std::stringstream ss;
                ss << "SEQ NUM " << windows[i].seq_num << " has timed out. Resend!";
                log_event(ss.str());
                fast_retransmit = true;
                break;
            }
        }

        // Avoid reading another network packet and just resend now
        if (fast_retransmit) {
            fast_retransmit = false;
            continue;
        }

        /**
         * We've sent everything, and nothing has timed out, so let's hunt for an ACK. We're using cumilative ACKS--
         * this means that we assume the client will only ACK bytes it has received. If we receive an ACK, we mark
         * every sequence number less than it is as sent.
         */
        if (read_network_packet(pkt)) {
            if (isFIN(pkt)) {
                log_event("Send data interrupted: remote closed the connection");
                close();
                return false;
            } else if (!isACK(pkt)) {
                drop_packet(pkt, "expected ACK and received non-ACK packet.");
            } else {
                std::stringstream ss;
                ss << "Received ACK " << pkt.header.ack_num;
                log_event(ss.str());

                if (pkt.header.ack_num > current_unacknowledged_bytes + total_acknowledged_bytes) {
                    std::stringstream ss;
                    ss << "received garbage ACK value. god " << pkt.header.ack_num << ", anticipated " << current_unacknowledged_bytes << "+" << total_acknowledged_bytes;
                    drop_packet(pkt, ss.str());
                    continue;
                }

                if (pkt.header.ack_num < last_ack) {
                    drop_packet(pkt, "discarding duplicate ACK");
                } else {
                    for (int i = 0; i < necessary_windows; i++) {
                        if (!windows[i].is_acked && windows[i].seq_num <= pkt.header.ack_num) {
                            windows[i].is_acked = true;

                            std::stringstream ss;
                            ss << "Marking " << pkt.header.ack_num << " as ACKED.";
                            log_event(ss.str());
                        }
                    }
                    total_acknowledged_bytes = pkt.header.ack_num;
                    current_unacknowledged_bytes -= (pkt.header.ack_num - last_ack);
                    last_ack = pkt.header.ack_num;
                    timeout_count = 0;
                }
            }
        }
        else {
            timeout_count++;

            if (timeout_count == MAX_TRANSMIT_TIMEOUTS) {
                log_event("Timeout limit reached. Giving up.");
                close();
                return false;
            }
        }
    }
}

bool RDTConnection::receive_data( std::string &data ) {
    rdt_packet_t pkt;
    rdt_packet_t response_pkt;
    uint16_t timeout_count = 0;
    size_t total_bytes_received = 0;
    data = "";
    bool got_EOF = false;
    while (true) {
        if (read_network_packet(pkt)) {
            if (pkt.header.seq_num <= total_bytes_received) {
                std::stringstream ss;
                ss << "Duplicate packet " << pkt.header.seq_num << " detected. Resending ACK";
                log_event(ss.str());

                build_network_packet(response_pkt, "");
                response_pkt.header.ack_num = pkt.header.seq_num;
                setACK(response_pkt);

                broadcast_network_packet(response_pkt);
                continue;
            }
            else if ((int)(pkt.header.seq_num - MSS) > (int)total_bytes_received) {
                std::stringstream ss;
                ss << "packet SEQ num " << pkt.header.seq_num << " out of desired range " << total_bytes_received << "+" << MSS;
                drop_packet(pkt, ss.str());
                continue;
            }

            // If the above checks pass, this is a valid packet.
            if ( pkt.header.data_len > 0 ) {
                data.append(pkt.data, pkt.header.data_len);
            }

            timeout_count = 0;
            total_bytes_received += pkt.header.data_len;

            // Now send an ACK
            build_network_packet(response_pkt, "");

            std::stringstream ss;
            ss << "ACK " << pkt.header.seq_num;
            log_event(ss.str());

            response_pkt.header.ack_num = pkt.header.seq_num;
            setACK(response_pkt);

            if (isEOF(pkt))
                setEOFACK(response_pkt);

            broadcast_network_packet(response_pkt);

            if (isEOF(pkt)) {
                log_event("Received EOF packet, transmission complete.");
                got_EOF = true;
            }

            if (isFIN(pkt)) {
                log_event("Receive data interrupted: remote closed the connection");
                close();
            }

            if (isEOF(pkt) || isFIN(pkt)) {
                return got_EOF;
            }
        } else {
            timeout_count++;

            std::stringstream ss;
            ss << "Read timeout. Set timeout count to " << timeout_count;
            log_event(ss.str());

            if (timeout_count == MAX_TRANSMIT_TIMEOUTS) {
                std::stringstream ss;
                ss << "Timeout limit " << MAX_TRANSMIT_TIMEOUTS << " exceeded. Giving up.";
                log_event(ss.str());

                return false;
            }
        }
    }
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
inline int RDTConnection::build_network_packet(rdt_packet_t &pkt, std::string const &data, size_t max_data_len, size_t data_offset) {
    memset((void *)&pkt, 0, sizeof(rdt_packet_t));

    pkt.header.magic_num = RDT_MAGIC_NUM;
    pkt.header.src_port  = ntohs(local_addr.sin_port);
    pkt.header.dst_port  = ntohs(remote_addr.sin_port);
    pkt.header.seq_num   = 0;
    pkt.header.ack_num   = 0;
    pkt.header.data_len  = std::max((unsigned int)0, (unsigned int)std::min(sizeof(pkt.data), data.size() - data_offset));
    pkt.header.flags     = 0;

    // First, ensure the offset is valid
    if (data_offset > data.size())
        return 0;

    // Next, compute the correct length based on any caller limitations.
    pkt.header.data_len = (max_data_len == 0) ?
        pkt.header.data_len :
        std::min((uint16_t)max_data_len, (uint16_t)pkt.header.data_len);

    memcpy(&pkt.data, (data.substr(data_offset)).c_str(), pkt.header.data_len);
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
 *
 * Function will automatically SYNACK any SYN packets or FINACK any FIN packets. It is
 * the caller's duty to note any incoming FIN packets and take the appropriate action.
 */
bool RDTConnection::read_network_packet(rdt_packet_t &pkt, bool verify_remote, sockaddr_in *ain) {
    memset(&pkt, 0, sizeof(pkt));

    sockaddr_in default_addr;
    sockaddr_in *recv_addr = ain ? ain : &default_addr;
    socklen_t recv_addr_len = sizeof(default_addr);

    ssize_t len = 0;
    bool valid_header = false;
    bool valid_packet = false;
    bool valid_host   = false;

    if (sock_fd == -1)
        return false;

    // First we try to find a packet header from the UDP buffer
    // We reject packets from unexpected hosts after the *entire* packet
    // is read from the UDP buffer so that we can get rid of the garbage data
    while( !valid_packet ) {
        while( !valid_header ) {
            // Reading the header and payload in two reads seems to cause problems
            // We peek at the header to avoid popping it out of the buffer
            len = recvfrom(sock_fd, &pkt.header, sizeof(pkt.header), MSG_PEEK, (sockaddr *)recv_addr, &recv_addr_len);

            // Time out, let caller handle problem
            if (len == -1 && errno == EWOULDBLOCK) {
                drop_packet(pkt, "socket timeout when waiting for packet to arrive");
                return false;
            }

            // Keep reading until we get the expected size of a packet
            if (len < sizeof(pkt.header)) {
                continue; // find header loop
            } else if (pkt.header.magic_num == RDT_MAGIC_NUM) {
                // If the magic number is aligned we got a proper header
                valid_header = true;
                break;
            } else { // Misaligned packet, attempt to recover
                int bytes_dropped = 0;
                const int MAGIC_NUM = RDT_MAGIC_NUM;
                char magicbuf[sizeof(pkt.header.magic_num)];
                memcpy(&magicbuf, &MAGIC_NUM, sizeof(magicbuf));

                // Check if magic number appears in the bytes we've read
                // If so, move the data up to the start of the header
                for (int i = 0; i < len - sizeof(magicbuf); i++) {
                    if ( memcmp( (&pkt.header)+i, &magicbuf, sizeof(magicbuf) ) == 0 ) {
                        bytes_dropped -= i;
                        memmove( &pkt.header, (&pkt.header)+i, len - bytes_dropped );
                        break; // for loop
                    }
                }

                std::stringstream ss;
                ss << "misaligned packet: " << bytes_dropped << " bytes dropped";
                drop_packet(pkt, ss.str());

                // Reset offset if no memory was moved and allow outer loop to keep reading
                if (pkt.header.magic_num != RDT_MAGIC_NUM)
                    valid_header = false;
            }
        }

        // Note: we assume if the header has arrived, so has the rest of the packet
        // They should be sufficiently small so that it would be unlikely for the system
        // to return before the rest of the packet is received. In such a situation, treat
        // the packet as corrupted
        if (valid_header) {
            size_t max_read = std::min(sizeof(pkt), pkt.header.data_len + sizeof(pkt.header));
            len = recvfrom(sock_fd, &pkt, max_read, 0, NULL, NULL);

            valid_host = recv_addr->sin_addr.s_addr == remote_addr.sin_addr.s_addr
                        && htons(pkt.header.src_port) == remote_addr.sin_port;

            if (len == -1) {
                if (errno == EWOULDBLOCK) {
                    log_event("socket timeout while receiving packet");
                    return false;
                } else {
                    log_event("unknown transmission error");
                    valid_header = false;
                }
            } else if (len < max_read) {
                drop_packet(pkt, "received packet was shorter than expected");
                valid_header = false;
            } else if (verify_remote && !valid_host) {
                drop_packet(pkt, "packet received from unexpected host");
                valid_header = false;
            } else if ( !isEOFACK(pkt) && (random() % 100 < prob_loss) ) {
                // Simulate network packet loss
                // Do not apply this on EOFACK packets to avoid synchronization issues
                drop_packet(pkt, "(simulated) socket timeout while receiving packet");
                errno = EWOULDBLOCK;
                return false;
            } else if ( !isEOFACK(pkt) && (random() % 100 < prob_corrupt) ) {
                // Simulate packet corruption
                // Do not apply this on EOFACK packets to avoid synchronization issues
                drop_packet(pkt, "packet corrupted");
                return false;
            } else if (len == max_read) {
                valid_packet = true;
            }
        } // valid header while loop
    } // valid packet while loop

    if (valid_packet) {
        // If remote host we've already connected to sends a SYN packet at any point
        // (because, say, our prevoius SYNACK was dropped) SYNACK it immediately
        rdt_packet_t ack;
        build_network_packet(ack, "");

        if (isSYN(pkt) && verify_remote) {
            setSYNACK(ack);
            broadcast_network_packet(ack);
            log_event("Received SYN packet");
        } else if (valid_host && isFIN(pkt)) { // Always ignore FIN packets from unknown hosts
            got_FIN = true;
            setFINACK(ack);
            broadcast_network_packet(ack);
            log_event("Received FIN packet, remote host closed connection");
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
    log_event("Dropped packet: " + (reason == "" ? "unknown error" : reason));
}

void RDTConnection::log_event(std::string const &msg) {
    time_t now;
    char date[32];

    time(&now);
    strftime( date, sizeof(date), "%D %T: ", localtime(&now));

    std::cerr << date << msg << std::endl;
}
