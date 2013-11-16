#ifndef RDTConn
#define RDTConn

#include <sys/socket.h> // Socky stuff
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h> // htonl, ntohl, etc.
#include <unistd.h>
#include <string> // std::string

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
};

#endif