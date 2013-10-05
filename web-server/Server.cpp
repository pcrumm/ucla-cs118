#include "Server.h"
#include <iostream>
#include <string>
#include <cstring>

/**
 * Instantiate the values the server will need to operate.
 */
Server::Server( int port, std::string root ) : port_number( port ), web_root( root ) {
}

/**
 * Begin listening for a connection.
 */
void Server::listen() {
    sock.create();
    sock.bind( port_number );
    sock.listen();

    Socket new_sock;
    sock.accept( new_sock );

    while (true) {
        std::string sock_results;
        new_sock.receive_data( sock_results );

        handle_request( sock_results );

        // Now that the request is handled, let's get another!
        sock.accept( new_sock );
    }
}

/**
 * See a request and respond accordingly.
 */
void Server::handle_request( std::string request_data ) {
    std::cout << "File name: " << extract_requested_file( request_data ) << "\n";
}

/**
 * We need to figure out what file we're after. Here's an easy way.
 * When we parse the HTTP header, we only care about the first line.
 */
std::string Server::extract_requested_file( std::string request_data ) {
    char* request = (char*)malloc( request_data.size() + 1 );
    memcpy( request, request_data.c_str(), request_data.size() + 1 );
    char* tokens = strtok( request, " " );

    int i = 0;
    while ( tokens != NULL ) {
        if ( i == 2 ) {
            std::string file_name( tokens );
            return file_name;
        }
        i++;
    }

}