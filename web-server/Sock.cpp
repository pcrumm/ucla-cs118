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
 * Accept a connection on a socket.
 */
bool Socket::accept( Socket& new_sock ) {
    int addr_length = sizeof( sock_addr );
    new_sock.sock = ::accept(
        sock,
        ( sockaddr * ) &sock_addr,
        ( socklen_t * ) &addr_length
    );

    return new_sock.sock != -1;
}

/**
 * Make a binded socket listen.
 */
bool Socket::listen( int backlog ) {
    return ( ::listen( sock, backlog ) == 0 );
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
    memset( buffer, '\0', sizeof( buffer ) );

    int receive_status = recv( sock, buffer, MAX_REQUEST_SIZE, 0 );

    switch( receive_status ) {
        case -1:
        case 0:
            data = "";
            return false;
        default:
            data = buffer;
            return true;
    }
}