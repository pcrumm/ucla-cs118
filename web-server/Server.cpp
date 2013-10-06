#include "Server.h"
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>

#define HTTP_OK 200
#define HTTP_NOT_FOUND 404

/**
 * Instantiate the values the server will need to operate.
 */
Server::Server( int port, std::string root ) : port_number( port ), web_root( root ) {
    // ensure the web root doesn't end in a /
    if ( web_root.substr( web_root.length() - 2, 1 ) == "/" )
        web_root = web_root.substr( 0, web_root.length() - 1 );

    std::cout << "Starting server on port " << port_number << " with root: " << web_root << std::endl;
}

/**
 * Begin listening for a connection.
 */
void Server::listen() {
    sock.create();
    sock.bind( port_number );
    sock.listen();

    while (true) {
        Socket new_sock;
        sock.accept( new_sock );

        std::string sock_results;
        new_sock.receive_data( sock_results );
        new_sock.send_data( handle_request( sock_results ) );
    }
}

/**
 * See a request and respond accordingly.
 */
std::string Server::handle_request( std::string request_data ) {
    std::string file_content;
    int response_code = retrieve_requested_file( extract_requested_file( request_data ), file_content );

    return file_content;
}

/**
 * Serve the requested file, relative to the web root.
 */
int Server::retrieve_requested_file( std::string file_name, std::string& response_data ) {
    std::string full_path = web_root + file_name;

    std::ifstream ifs;
    ifs.open( full_path.c_str(), std::ifstream::in );

    if ( !ifs.is_open() )
    {
        response_data = "File not found.";
        return HTTP_404;
    }

    char c = ifs.get();
    while ( ifs.good() ) {
        response_data.push_back(c);
        c = ifs.get();
    }

    ifs.close();

    return HTTP_OK;
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
        if ( i == 1 ) {
            std::string file_name( tokens );
            return file_name;
        }
        tokens = strtok( NULL, " " );
        i++;
    }
}