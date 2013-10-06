#include "Server.h"
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>

#define HTTP_OK "200 OK"
#define HTTP_NOT_FOUND "404 NOT FOUND"

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
        handle_request( new_sock, sock_results );
    }
}

/**
 * See a request and respond accordingly.
 *
 * Assemble the whole HTTP response, including the headers and content.
 * This should be sent directly back to the browser.
 */
void Server::handle_request( Socket& response_socket, const std::string& request_data ) {
    std::string file_name = extract_requested_file( request_data );
    std::string file_ext  = "";
    std::string mime_type = "";
    std::string response_code = "";

    std::stringstream header;
    std::ifstream ifs;
    std::string response_data;

    // Bind the url root to index.html
    if ( "/" == file_name )
        file_name = "/index.html";

    // Grab the file extention if we find a '.'
    size_t ext_index = file_name.rfind( '.' );
    if ( std::string::npos != ext_index )
        file_ext = file_name.substr( ext_index, std::string::npos );

    // Grab mime type
    std::transform(file_ext.begin(), file_ext.end(), file_ext.begin(), ::tolower);
    if ( ".html" == file_ext )
        mime_type = "text/html; charset=utf-8";
    else if ( ".gif" == file_ext )
        mime_type = "image/gif";
    else if( ".jpeg" == file_ext || ".jpg" == file_ext )
        mime_type = "image/jpeg";
    else
        mime_type = "test/plain";

    // Attempt to open the file if valid name
    if ( file_name.size() > 0 ) {
        file_name = web_root + file_name;
        ifs.open( file_name.c_str(), std::ifstream::in );
    }

    if ( ifs.is_open() )
        response_code = HTTP_OK;
    else
        response_code = HTTP_NOT_FOUND;

    char c = ifs.get();
    while ( ifs.good() ) {
        response_data.push_back(c);
        c = ifs.get();
    }

    ifs.close();

    // Build response header
    header << "HTTP/1.1 " << response_code << std::endl;

    if ( HTTP_NOT_FOUND == response_code ) {
        header << "Content-Length: " << response_code.size() << "\n\n";
        response_socket.send_data( header.str() );
        response_socket.send_data( response_code );
        return;
    }

    header << "Content-Type: " << mime_type << std::endl;
    header << "Content-Length: " << response_data.size() << "\n\n";

    response_socket.send_data( header.str() );
    response_socket.send_data( response_data );
}

/**
 * We need to figure out what file we're after. Here's an easy way.
 * When we parse the HTTP header, we only care about the first line.
 */
std::string Server::extract_requested_file( std::string request_data ) {
    char* request_copy = new char[request_data.size() + 1];
    memset( request_copy, '\0', request_data.size() + 1 );
    strncpy( request_copy, request_data.c_str(), request_data.size() );

    // Token now holds the method like GET or POST
    char *token = strtok( request_copy, " " );

    // Now we have the url
    token = strtok( NULL, " " );

    std::string file_name = "";

    if ( NULL != token )
        file_name = token;

    delete request_copy;
    return file_name;
}
