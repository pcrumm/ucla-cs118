#include "RDTConnection.h"

RDTConnection::RDTConnection() : sock_fd( -1 ) {
    memset( &remote_addr, 0, sizeof( remote_addr ));
    memset( &local_addr, 0, sizeof( local_addr ));
}

RDTConnection::~RDTConnection() {
    close();
}

bool RDTConnection::connect( std::string afnet_address, int port ) {
    // TODO
    return false;
}

void RDTConnection::close() {
    if (sock_fd != -1)
        ::close( sock_fd );
}

void RDTConnection::listen( int port ) {
    // TODO
    return;
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