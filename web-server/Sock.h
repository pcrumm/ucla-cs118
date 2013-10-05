#ifndef Sock
#define Sock

#include <sys/socket.h> // Socky stuff
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h>
#include <string> // std::string

/**
 * When we read in data, we need to know how large
 * our data bits can be. Different browsers have
 * different limits. By my research, FF is largest
 * at 8kb.
 */
 #define MAX_REQUEST_SIZE 8192

class Socket {
public:
    Socket();
    virtual ~Socket();

    bool create();
    bool bind( int port_number );
    bool listen();

    bool send_data ( std::string data );
    bool receive_data ( std::string& data );

private:
    int sock; // the fd for our socket
    sockaddr_in sock_addr;
};

#endif