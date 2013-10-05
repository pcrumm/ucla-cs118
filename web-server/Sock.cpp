#include "Sock.h"

/**
 * Instantiate our socket. Since sockaddr
 * is a C structure we'll need to make sure
 * it's instantiated here. Also, we set socket
 * = -1 so we know what it's up to.
 */
Socket::Socket() : sock( -1 ) {
    memset( &sock_addr, 0, sizeof( sock_addr ));
}

/**
 * To destruct, just close our socket if it's open.
 */
Socket::~Socket() {
    if ( sock != -1 )
        close ( sock );
}

/**
 * Creates a socket. If successful, it'll upate our fd.
 */
bool Socket::create() {
    sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( sock == -1 ) // failed to allocate the fd
        return false;

    return true;
}

/**
 * Bind an already create()ed socket to a given port.
 */
bool Socket::bind( int port_number ) {
    // See man ip for what's going on here
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = INADDR_ANY; // allow connections from any IP
    sock_addr.sin_port = htons( port_number );

    return ( ::bind( sock, ( struct sockaddr * ) &sock_addr, sizeof( sock_addr ) ) == 0 );
}

/**
 * Make a binded socket listen.
 */
bool Socket::listen() {
    return ( ::listen( sock, 1 ) == 0 );
}

/**
 * Send data on a live socket.
 */
bool Socket::send_data( std::string data ) {
    return ( send( sock, data.c_str(), data.size(), 0 ) == 0);
}

/**
 * Receive data on a live socket.
 */
bool Socket::receive_data( std::string& data ) {
    char buffer[ MAX_REQUEST_SIZE + 1 ];

    int receive_status = recv( sock, buffer, MAX_REQUEST_SIZE, 0 );

    switch( receive_status ) {
        case -1:
            return false;
        case 0:
            return false;
        default:
            data = buffer;
            return true;
    }
}