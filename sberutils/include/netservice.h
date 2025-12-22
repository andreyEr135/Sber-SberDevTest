#ifndef netserviceH
#define netserviceH
//-------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <fcntl.h>
#include <ifaddrs.h>

#include <string>
#include <vector>
#include <iterator>

//-------------------------------------------------------------

std::string sockaddrstr( struct sockaddr_in& sockaddr );
struct sockaddr_in sockaddr_ip_port( std::string ip, int port );
WORD InternetChksum( WORD* lpwData, WORD wDataLength );

//-------------------------------------------------------------

class TnetIF;

//===========================================================================
//   Tnet
//---------------------------------------------------------------------------

class Tnet
{
protected:
	std::string  classstr;
	std::string  errorstr;

public:
    Tnet( std::string classstr );
    virtual ~Tnet( );

	typedef enum { netOK=1, netERROR, netSTOPSIG, netTIMEOUT, netSKIP, netDISCONNECT } Tresult;
	int fd;
	int recved;
	struct sockaddr_in addr;
	std::string error( ) { return classstr +": "+ errorstr; }

	void close( );
	bool opened( ) { return ( fd != -1 ); }

    Tresult wait( int timeoutms=0 );

	bool getaddr( );

	void fdzero ( fd_set& fds ) { FD_ZERO( &fds ); }
	void fdset  ( fd_set& fds ) { FD_SET( fd, &fds ); }
	void fdclr  ( fd_set& fds ) { FD_CLR( fd, &fds ); }
	bool fdisset( fd_set& fds ) { return FD_ISSET( fd, &fds ); }

	bool setopt_broadcast( bool on );
	bool setopt_recvtimeout( int timeoutms );
	bool setopt_reuseaddr( bool on );
	bool setopt_device( std::string device );
	bool setopt_recvbuf( int bufsize );
};

//===========================================================================
//   Ticmp
//---------------------------------------------------------------------------

class Ticmp : public Tnet
{
private:
	WORD icmp_id;

public:
    Ticmp( );
    virtual ~Ticmp( ) { }

	bool open( );
	//bool setopt_device( std::string device );
	bool send( struct sockaddr_in& dest, WORD seq, void* data, int datasize, int timeoutms=0 );
	bool send( std::string ip, WORD seq, void* data, int datasize, int timeoutms=0 );
	Tresult recv( struct sockaddr_in& from, WORD& seg, void* data, int datasize, int timeoutms=0 );
	Tresult recv( std::string& from, WORD& seg, void* data, int datasize, int timeoutms=0 );
};


//===========================================================================
//   Tnetaddr
//---------------------------------------------------------------------------

class Tnetaddr
{
private:
	void update( );
public:
	struct in_addr ip;
	struct in_addr mask;
	struct in_addr sub;
	struct in_addr brd;
	
    Tnetaddr( );
    Tnetaddr( std::string str );

	bool set( std::string str );
	void set( struct in_addr ip, struct in_addr mask );
	
	std::string ipstr( );
	std::string maskstr( );
	std::string substr( );
	std::string brdstr( );
	
	bool incsub( );
	void ipnum( int num );
	bool incipnum( );
	operator bool( ) { return ( ip.s_addr != (uint32_t)0 ); }
};
//---------------------------------------------------------------------------

//   TnetIF
//---------------------------------------------------------------------------

class TnetIF
{
private:
	struct ifreq ifr;
	int          sockfd;
	bool         init( );
public:
	struct Tstats
	{
		uint64_t rx_bytes;
		uint64_t rx_compressed;
		uint64_t rx_crc_errors;
		uint64_t rx_dropped;
		uint64_t rx_errors;
		uint64_t rx_fifo_errors;
		uint64_t rx_frame_errors;
		uint64_t rx_length_errors;
		uint64_t rx_missed_errors;
		uint64_t rx_over_errors;
		uint64_t rx_packets;
		uint64_t tx_aborted_errors;
		uint64_t tx_bytes;
		uint64_t tx_carrier_errors;
		uint64_t tx_compressed;
		uint64_t tx_dropped;
		uint64_t tx_errors;
		uint64_t tx_fifo_errors;
		uint64_t tx_heartbeat_errors;
		uint64_t tx_packets;
		uint64_t tx_window_errors;
		uint64_t collisions;
		uint64_t multicast;
		float    time;
	};

	std::string name;
    Tnetaddr  addr;
	BYTE        mac[6];
	int         index;
	short       flags;
	int         speed;
	int         mtu;
	Tstats      stats;

    TnetIF( );
    TnetIF( std::string name );

	bool    ifname( std::string name );
	bool    update( );
	bool    inited;
	
	bool         flink( );
	bool         fup( );
	bool         fmac( );
    bool         setaddr( Tnetaddr addr );
	bool         setaddr( std::string str );
	std::string  getaddr( );
	bool         updown( bool up );
	bool         promisc( bool v );
	bool         rename( std::string newname );

	Tstats       statistics( );
	std::string  rx_speed( Tstats& delta );
	std::string  tx_speed( Tstats& delta );

	operator bool( ) { return inited; }
    bool operator< ( const TnetIF& x ) const { return name < x.name; }
};

TnetIF::Tstats stats_delta( TnetIF::Tstats& curr, TnetIF::Tstats& prev );
uint64_t      stats_errors_count( TnetIF::Tstats& d );
std::string   stats_errors_string( TnetIF::Tstats& delta );

typedef std::vector< TnetIF >  TnetIFs;
typedef TnetIFs::iterator TnetIFsIt;

bool   getIFs( TnetIFs& ifs );
TnetIF* findIFByName( TnetIFs& ifs, std::string name );

//-------------------------------------------------------------
#endif

