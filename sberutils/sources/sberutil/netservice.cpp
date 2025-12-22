#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <linux/if_packet.h>
#include <linux/if_link.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/stat.h>


#include "debugsystem.h"
#include "netservice.h"

//-------------------------------------------------------------

#undef  UNIT
#define UNIT  g_DebugSystem.units[ UNIT_NETSERVICE ]
#include "log_macros.def"

using namespace std;

#define SOCKET_ERROR    -1
#define INVALID_SOCKET  -1

//-------------------------------------------------------------

std::string sockaddrstr( struct sockaddr_in& sockaddr )
{
	return stringformat( "%s:%d", inet_ntoa(sockaddr.sin_addr), ntohs(sockaddr.sin_port) );
}
//---------------------------------------------------------------------------

struct sockaddr_in sockaddr_ip_port( std::string ip, int port )
{
	struct sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons( port );
	sockaddr.sin_addr.s_addr = inet_addr( ip.c_str() );
	return sockaddr;
}
//---------------------------------------------------------------------------

WORD InternetChksum( WORD* lpwData, WORD wDataLength )
{
	WORD	wOddByte;  // Left over byte from the summation
	WORD	wAnswer;   // The 1's complement checksum
	long  lSum = 0L; // Store the summation
	while( wDataLength > 1 )
	{
		lSum += *lpwData++;
		wDataLength -= 2;
	}
	// Handle the odd byte if necessary and make sure the top half is zero
	if( wDataLength == 1 )
	{
		wOddByte = 0;
		*((BYTE*) &wOddByte) = *(BYTE*)lpwData;	// One byte only
		lSum += wOddByte;
	}
	// Add back the carry outs from the 16 bits to the low 16 bits
	lSum = (lSum >> 16) + (lSum & 0xffff);  // Add high-16 to low-16
	lSum += (lSum >> 16);                   // Add carry
	wAnswer = (WORD)~lSum;  // 1's complement, then truncate // to 16 bits
	return wAnswer;
}
//---------------------------------------------------------------------------

//===========================================================================
//   dxNET
//---------------------------------------------------------------------------

Tnet::Tnet( std::string classstr ) : classstr(classstr)
{
	fd = -1;
	errorstr = "closed";
}
//---------------------------------------------------------------------------

Tnet::~Tnet( )
{
	if( opened( ) ) ::close( fd );
}
//---------------------------------------------------------------------------

void Tnet::close( )
{FUNCTION_TRACE
	if( !opened( ) ) return;
	::close( fd );
	fd = -1;
	errorstr = "closed";
}
//---------------------------------------------------------------------------

Tnet::Tresult Tnet::wait( int timeoutms )
{FUNCTION_TRACE
	if( !opened( ) ) return netERROR;
	try
	{
		fd_set reads, exceptfds;
		FD_ZERO( &reads );     FD_SET( fd, &reads );
		FD_ZERO( &exceptfds ); FD_SET( fd, &exceptfds );
		struct timeval *ptv = NULL;
		struct timeval tv = { timeoutms / 1000, ( timeoutms % 1000 ) * 1000 };
		if( timeoutms > 0 ) ptv = &tv;
		int res = select( fd + 1, &reads, 0, &exceptfds, ptv );
		//if( WSAGetLastError() == 10038 ) throw errException( netSTOPSIG, "stopsignal" );
		if( errno== 4 ) { errno=0; throw errException( netSTOPSIG, "stopsignal" ); }
		if( res == -1 ) throw errException( netERROR, "select" );
		if( res ==  0 ) throw errException( netTIMEOUT, "timeout" );
	}
	catch( errException& e )
	{
		errorstr = std::string( "wait: " ) + e.error();
        return (Tnet::Tresult)e.code;
	}
	return netOK;
}
//---------------------------------------------------------------------------

bool Tnet::getaddr( )
{FUNCTION_TRACE
	memset( &addr, 0, sizeof(addr) );
	socklen_t addrlen = sizeof(addr);
	bool ok = ( getsockname( fd, (struct sockaddr*)&addr, &addrlen ) != SOCKET_ERROR );
	if( !ok ) errorstr = errException( "getaddr" ).error();
	return ok;
}
//---------------------------------------------------------------------------

bool Tnet::setopt_broadcast( bool on )
{FUNCTION_TRACE
	if( !opened( ) ) return false;
	int param = on ? 1 : 0;
	bool ok = ( setsockopt( fd, SOL_SOCKET, SO_BROADCAST, (char*)&param, sizeof(param) ) != -1 );
	if( !ok ) errorstr = errException( "setopt_broadcast: SO_BROADCAST '%d'", param ).error();
	return ok;
}
//---------------------------------------------------------------------------

bool Tnet::setopt_recvtimeout( int timeoutms )
{FUNCTION_TRACE
	if( !opened( ) ) return false;
	struct timeval tv = { timeoutms / 1000, ( timeoutms % 1000 ) * 1000 };
	bool ok = ( setsockopt( fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv) ) != -1 ) ;
	if( !ok ) errorstr = errException( "setopt_recvtimeout: SO_RCVTIMEO '%dms'", timeoutms ).error();
	return ok;
}
//---------------------------------------------------------------------------

bool Tnet::setopt_reuseaddr( bool on )
{FUNCTION_TRACE
	if( !opened( ) ) return false;
	int param = on ? 1 : 0;
	bool ok = ( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (char*)&param, sizeof(param) ) != -1 );
	if( !ok ) errorstr = errException( "setopt_reuseaddr: SO_REUSEADDR '%d'", param ).error();
	return ok;
}
//---------------------------------------------------------------------------

bool Tnet::setopt_device( std::string device )
{FUNCTION_TRACE
	if( !opened( ) ) return false;
	struct ifreq interface;
	memset(&interface, 0, sizeof(interface));
	strncpy( interface.ifr_ifrn.ifrn_name, device.c_str(), IFNAMSIZ-1 );
	bool ok = ( setsockopt( fd, SOL_SOCKET, SO_BINDTODEVICE, (char*)&interface, sizeof(interface) ) != -1 );
	if( !ok ) errorstr = errException( "setopt_device: SO_BINDTODEVICE '%s'", device.c_str() ).error();
	return ok;
}
//---------------------------------------------------------------------------

bool Tnet::setopt_recvbuf( int bufsize )
{FUNCTION_TRACE
	if( !opened( ) ) return false;
	bool ok = ( setsockopt( fd, SOL_SOCKET, SO_RCVBUF, (char*)&bufsize, sizeof(bufsize) ) != 1 );
	if( !ok ) errorstr = errException( "setopt_recvbuf: SO_RCVBUF '%d'", bufsize ).error();
	return ok;
}
//---------------------------------------------------------------------------




//===========================================================================
//   IMCP
//---------------------------------------------------------------------------

#define ICMP_ECHO 8        // An ICMP echo message
#define ICMP_ECHOREPLY 0   // An ICMP echo reply message
#define ICMP_HEADERSIZE 8

struct Sip
{
	BYTE ip_verlen;      // Version and header length
	BYTE ip_tos;         // Type of service
	WORD ip_len;         // Total packet length
	UINT ip_id;          // Datagram identification
	WORD ip_fragoff;     // Fragment offset
	BYTE ip_ttl;         // Time to live
	BYTE ip_proto;       // Protocol
	UINT ip_chksum;      // Checksum
	in_addr ip_src_addr; // Source address
	in_addr ip_dst_addr; // Destination address
	BYTE ip_data[1];     // Variable length data area
};

struct Sicmp
{
	BYTE  icmp_type;      // Type of message
	BYTE  icmp_code;      // Type "sub code" (zero for echos)
	WORD  icmp_cksum;     // 1's complement checksum
	WORD  icmp_id;        // Unique ID (the instance handle)
	WORD  icmp_seq;       // Tracks multiple pings
	BYTE  icmp_data[1];   // Variable length data area
};
//---------------------------------------------------------------------------

Ticmp::Ticmp( ) : Tnet( "ticmp" )
{
	timeval tv;
	gettimeofday( &tv, NULL );
	srand( tv.tv_usec * tv.tv_sec );
	icmp_id = (WORD)rand( );
}
//---------------------------------------------------------------------------

bool Ticmp::open( )
{FUNCTION_TRACE
	if( opened() ) close( );
	fd = socket( PF_INET, SOCK_RAW, IPPROTO_ICMP );
	bool ok = ( fd != -1 );
	if( !ok ) errorstr = errException( "socket" ).error();
	return ok;
}
//---------------------------------------------------------------------------

bool Ticmp::send( struct sockaddr_in& dest, WORD seq, void* data, int datasize, int timeoutms )
{FUNCTION_TRACE
	if( !opened( ) ) return false;
	try
	{
		BYTE sendbuf[ ICMP_HEADERSIZE + 64*1024 ];
		int size = ICMP_HEADERSIZE + datasize;
		if( size > (int)sizeof(sendbuf) ) throw errException( "datasize too big > %d", sizeof(sendbuf) );
        struct Sicmp* icmp = (struct Sicmp*)sendbuf;
		icmp->icmp_type  = ICMP_ECHO; // then fill in the data.
		icmp->icmp_code  = 0;         // Use the Sockman instance
		icmp->icmp_id    = icmp_id;   // handle as a unique ID.
		icmp->icmp_seq   = htons(seq);// It's important to reset  htonw
		icmp->icmp_cksum = 0;         // the checksum to zero.
		if( data ) memcpy( icmp->icmp_data, data, datasize );

		icmp->icmp_cksum = InternetChksum( (WORD*)sendbuf, size );

		if( timeoutms > 0 )
		{
			fd_set wfd; FD_ZERO( &wfd ); FD_SET( fd, &wfd );
			struct timeval tv = { timeoutms / 1000, ( timeoutms % 1000 ) * 1000 };
			int res = select( fd + 1, NULL, &wfd, NULL, &tv );
			if( res <  0 ) throw errException( "select" );
			if( res == 0 ) throw errException( "timeout" );
		}
		int res = ::sendto( fd, (BYTE*)sendbuf, size, 0, (sockaddr*)&dest, sizeof(struct sockaddr) );
		if( res == -1 ) throw errException( "sendto '%s size=%d'", sockaddrstr( dest ).c_str(), size );
		if( res != size ) throw errException( "sendto '%s size=%d' BUT sended=%d", sockaddrstr( dest ).c_str(), size, res );
	}
	catch( errException& e )
	{
		errorstr = std::string( "send: " ) + e.error();
		return false;
	}
	return true;
}
//---------------------------------------------------------------------------

bool Ticmp::send( std::string ip, WORD seq, void* data, int datasize, int timeoutms )
{
	struct sockaddr_in sockaddr = sockaddr_ip_port( ip, 0 );
	return this->send( sockaddr, seq, data, datasize, timeoutms );
}
//---------------------------------------------------------------------------

Tnet::Tresult Ticmp::recv( struct sockaddr_in& from, WORD& seg, void* data, int datasize, int timeoutms )
{FUNCTION_TRACE
	if( !opened( ) ) return netERROR;

	seg = 0;
	recved = 0;
    Ticmp::Tresult waitresult = wait( timeoutms );
	if( waitresult != netOK ) return waitresult;

	try
	{
		BYTE recvbuf[ ICMP_HEADERSIZE + 64*1024 ];
		unsigned int sockaddsize = sizeof( struct sockaddr );
		int res = recvfrom( fd, (char*)recvbuf, sizeof(recvbuf), 0, (sockaddr*)&from, &sockaddsize );
		if( res == -1 ) throw errException( netERROR, "recvfrom" );
		recved = res;

        struct Sip* pIpHeader = (struct Sip*)recvbuf;
		int iIPHeadLength = ((pIpHeader->ip_verlen)&0x0F) << 2;
		if( recved < iIPHeadLength + ICMP_HEADERSIZE ) throw errException( netERROR, "recved packet too short" );

        struct Sicmp *icmp = (struct Sicmp*)( recvbuf + iIPHeadLength );
		if( icmp->icmp_type != ICMP_ECHOREPLY ) throw errException( netSKIP, "Received packet was not an echo reply to your ping.");
		if( icmp->icmp_id != icmp_id ) throw errException( netSKIP, "Received packet was not sent by this program.");
		seg = ntohs(icmp->icmp_seq);
		if( data ) memcpy( data, icmp->icmp_data, datasize );
	}
	catch( errException& e )
	{
		errorstr = std::string( "recv: " ) + e.error();
		return (Tresult)e.code;
	}
	return netOK;
}
//---------------------------------------------------------------------------

Tnet::Tresult Ticmp::recv( std::string& from, WORD& seg, void* data, int datasize, int timeoutms )
{
	from = "";
	struct sockaddr_in sockaddr;
	Tresult res = recv( sockaddr, seg, data, datasize, timeoutms );
	from = std::string( inet_ntoa( sockaddr.sin_addr ) );
	return res;
}


//-------------------------------------------------------------------------------------------

Tnetaddr::Tnetaddr( )
{
	memset( &ip, 0, sizeof(ip) );
	memset( &mask, 0, sizeof(mask) );
	update( );
}
//-------------------------------------------------------------------------------------------

Tnetaddr::Tnetaddr( std::string str )
{
	memset( &ip, 0, sizeof(ip) );
	memset( &mask, 0, sizeof(mask) );
	update( );
	set( str );
}
//-------------------------------------------------------------------------------------------

void Tnetaddr::update( )
{
	sub = ip; sub.s_addr &= mask.s_addr;
	brd.s_addr = sub.s_addr + ( 0xFFFFFFFF^mask.s_addr );
}
//-------------------------------------------------------------------------------------------

bool Tnetaddr::set( std::string str )
{
	string::size_type n = str.find( "/" );
	string ipstr = ( n != string::npos ) ? str.substr( 0,n ) : str;
	string mstr  = ( n != string::npos ) ? str.substr( n+1 ) : "24";

	struct in_addr _ip;
	inet_aton( ipstr.c_str(), &_ip );
	int m = atoi( mstr.c_str() );

	bool result = true;
	result &= ( string( inet_ntoa(_ip) ) == ipstr );
	result &= ( ( m > 0 )&&( m < 32 ) );
	if( !result ) { Error( "incorrect ip:'%s'\n", str.c_str() ); return false; }

	ip = _ip;
	mask.s_addr = htonl( 0xFFFFFFFF << (32-m) );
	update( );
	return true;
}
//-------------------------------------------------------------------------------------------

void Tnetaddr::set( struct in_addr ip, struct in_addr mask )
{
	this->ip.s_addr = ip.s_addr;
	this->mask.s_addr = mask.s_addr;
	update( );
}
//-------------------------------------------------------------------------------------------

std::string Tnetaddr::ipstr( )   { return string( inet_ntoa( ip   ) ); }
std::string Tnetaddr::maskstr( ) { return string( inet_ntoa( mask ) ); }
std::string Tnetaddr::substr( )  { return string( inet_ntoa( sub  ) ); }
std::string Tnetaddr::brdstr( )  { return string( inet_ntoa( brd  ) ); }

//-------------------------------------------------------------------------------------------

bool Tnetaddr::incsub( )
{
	uint32_t _mask = ntohl( mask.s_addr );
	int n0 = 0;
	for( int i = 0; ( i < 32 )&&( (_mask&1)==0 ); i++, n0++, _mask >>= 1 );
	if( ( n0 < 1 )||( n0 > 31 ) ) return false;
	uint32_t _ip = ( ( ntohl( ip.s_addr ) >> n0 ) + 1 ) << n0;
	ip.s_addr = htonl( _ip );
	sub = ip; sub.s_addr &= mask.s_addr;
	return true;
}
//-------------------------------------------------------------------------------------------

void Tnetaddr::ipnum( int num )
{
	uint32_t _ip = ntohl( sub.s_addr ) + num;
	ip.s_addr = htonl( _ip );
}
//-------------------------------------------------------------------------------------------

bool Tnetaddr::incipnum( )
{
	uint32_t _ip   = ntohl( ip.s_addr ) + 1;
	uint32_t _mask = ntohl( mask.s_addr );
	uint32_t _sub  = ntohl( sub.s_addr );
	uint32_t _brd  = ntohl( brd.s_addr );
	if( (_ip & _mask) != _sub ) return false;
	if( _ip == _brd ) return false;
	ip.s_addr = htonl( _ip );
	return true;
}
//-------------------------------------------------------------------------------------------


#ifdef ifr_flags
# define IRFFLAGS ifr_flags
#else
# define IRFFLAGS ifr_flagshigh
#endif

//-------------------------------------------------------------------------------------------

TnetIF::TnetIF( )
{
	name = "";
	inited = false;
}
//-------------------------------------------------------------------------------------------

TnetIF::TnetIF( std::string name ) : name(name)
{
	update( );
}
//-------------------------------------------------------------------------------------------

bool TnetIF::ifname( std::string name )
{
	this->name = name;
	return update( );
}
//-------------------------------------------------------------------------------------------

bool TnetIF::init( )
{
	inited = false;
	memset( &ifr, 0, sizeof(ifr) );
	strncpy( ifr.ifr_name, name.c_str(), IFNAMSIZ-1 );
	ifr.ifr_addr.sa_family = AF_INET;
	sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
	if( sockfd != -1 )
	{
		if( ioctl( sockfd, SIOCGIFINDEX, &ifr ) >= 0 )
			inited = true;
		else
			close( sockfd );
	}
	errno = 0;
	return inited;
}
//-------------------------------------------------------------------------------------------

bool TnetIF::update( )
{
	if( !init( ) ) return false;

	struct sockaddr_in ip;   memset( &ip,   0, sizeof(ip) );
	struct sockaddr_in mask; memset( &mask, 0, sizeof(mask) );
	memset( mac, 0, sizeof(mac) );
	index = -1;
	flags = 0;
	mtu = 0;
	speed = 0;

	if( ioctl( sockfd, SIOCGIFADDR,    &ifr ) >= 0 ) ip = *((struct sockaddr_in*)&ifr.ifr_addr);
	if( ioctl( sockfd, SIOCGIFNETMASK, &ifr ) >= 0 ) mask = *((struct sockaddr_in*)&ifr.ifr_netmask);
	if( ioctl( sockfd, SIOCGIFHWADDR,  &ifr ) >= 0 ) memcpy( mac, ifr.ifr_hwaddr.sa_data, 6 );
	if( ioctl( sockfd, SIOCGIFINDEX,   &ifr ) >= 0 ) index = ifr.ifr_ifindex;
	if( ioctl( sockfd, SIOCGIFFLAGS,   &ifr ) >= 0 ) flags = ifr.IRFFLAGS;
	if( ioctl( sockfd, SIOCGIFMTU,     &ifr ) >= 0 ) mtu   = ifr.ifr_mtu;

	struct ethtool_cmd edata;
	edata.cmd = ETHTOOL_GSET;
	ifr.ifr_data = (__caddr_t)&edata;
	if( ioctl (sockfd, SIOCETHTOOL, &ifr) >= 0 ) speed = ethtool_cmd_speed( &edata );

	errno = 0;
	addr.set( ip.sin_addr, mask.sin_addr );
	close( sockfd );

	statistics( );

	return true;
}
//-------------------------------------------------------------------------------------------

bool TnetIF::flink( ) { return ( flags & IFF_RUNNING ); }
bool TnetIF::fup( )   { return ( flags & IFF_UP ); }

//-------------------------------------------------------------------------------------------

bool TnetIF::fmac( )
{
	BYTE macnull[6] = { 0,0,0,0,0,0 };
	return ( memcmp( mac, macnull, 6 ) == 0 );
}
//-------------------------------------------------------------------------------------------

bool TnetIF::setaddr( Tnetaddr addr )
{
	if( !init( ) ) return false;
	struct sockaddr_in ip;   memset( &ip,   0, sizeof(ip) );
	struct sockaddr_in mask; memset( &mask, 0, sizeof(mask) );
	ip.sin_family   = AF_INET; ip.sin_addr   = addr.ip;
	mask.sin_family = AF_INET; mask.sin_addr = addr.mask;

	bool result = true;
	memcpy( &ifr.ifr_addr, &ip, sizeof(struct sockaddr) );
	result &= ( ioctl( sockfd, SIOCSIFADDR, &ifr ) >= 0 );
	memcpy( &ifr.ifr_netmask, &mask, sizeof(struct sockaddr) );
	result &= ( ioctl( sockfd, SIOCSIFNETMASK, &ifr ) >= 0 );

	close( sockfd );
	update( );
	return result;
}
//-------------------------------------------------------------------------------------------

bool TnetIF::setaddr( std::string str )
{
    Tnetaddr a;
	if( !a.set( str ) ) return false;
	return setaddr( a );
}
//-------------------------------------------------------------------------------------------

string TnetIF::getaddr( )
{
	string ip_out = "";
	if( !init( ) ) return ip_out;//false;
	struct sockaddr_in ip;   memset( &ip, 0, sizeof(ip) );
	memset( &ifr.ifr_addr, 0, sizeof(struct sockaddr) );
	ioctl( sockfd, SIOCGIFADDR, &ifr );
	memcpy( &ip, &ifr.ifr_addr, sizeof(struct sockaddr_in) );
	ip_out = inet_ntoa( ip.sin_addr );
	return ip_out;
}
//-------------------------------------------------------------------------------------------

bool TnetIF::updown( bool up )
{
	if( !init( ) ) return false;
	ifr.IRFFLAGS = (up) ? flags | IFF_UP : flags & (~IFF_UP);
	bool result = ( ioctl( sockfd, SIOCSIFFLAGS, &ifr ) >= 0 );
	close( sockfd );
	update( );
	return result;
}
//-------------------------------------------------------------------------------------------

bool TnetIF::promisc( bool v )
{
	if( !init( ) ) return false;
	if( v ) ifr.ifr_flags |= IFF_PROMISC;
	else    ifr.ifr_flags &= ~IFF_PROMISC;
	bool result = ( ioctl( sockfd, SIOCSIFFLAGS, &ifr ) >= 0 );
	close( sockfd );
	update( );
	return result;
}
//-------------------------------------------------------------------------------------------

bool TnetIF::rename( std::string newname )
{
	if( !init( ) ) return false;
	if( newname.size() >= sizeof(ifr.ifr_newname) ) return false;

	strcpy( ifr.ifr_newname, newname.c_str() );
	bool result = ( ioctl( sockfd, SIOCSIFNAME, &ifr ) >= 0 );
	close( sockfd );
	this->name = newname;
	update( );
	return result;
}
//-------------------------------------------------------------------------------------------

TnetIF::Tstats TnetIF::statistics( )
{
	memset( &stats, 0, sizeof(stats) );
	string path = "/sys/class/net/" + name + "/statistics/";
	stats.rx_bytes            = readfileuint64( path + "rx_bytes" );
	stats.rx_compressed       = readfileuint64( path + "rx_compressed" );
	stats.rx_crc_errors       = readfileuint64( path + "rx_crc_errors" );
	stats.rx_dropped          = readfileuint64( path + "rx_dropped" );
	stats.rx_errors           = readfileuint64( path + "rx_errors" );
	stats.rx_fifo_errors      = readfileuint64( path + "rx_fifo_errors" );
	stats.rx_frame_errors     = readfileuint64( path + "rx_frame_errors" );
	stats.rx_length_errors    = readfileuint64( path + "rx_length_errors" );
	stats.rx_missed_errors    = readfileuint64( path + "rx_missed_errors" );
	stats.rx_over_errors      = readfileuint64( path + "rx_over_errors" );
	stats.rx_packets          = readfileuint64( path + "rx_packets" );
	stats.tx_aborted_errors   = readfileuint64( path + "tx_aborted_errors" );
	stats.tx_bytes            = readfileuint64( path + "tx_bytes" );
	stats.tx_carrier_errors   = readfileuint64( path + "tx_carrier_errors" );
	stats.tx_compressed       = readfileuint64( path + "tx_compressed" );
	stats.tx_dropped          = readfileuint64( path + "tx_dropped" );
	stats.tx_errors           = readfileuint64( path + "tx_errors" );
	stats.tx_fifo_errors      = readfileuint64( path + "tx_fifo_errors" );
	stats.tx_heartbeat_errors = readfileuint64( path + "tx_heartbeat_errors" );
	stats.tx_packets          = readfileuint64( path + "tx_packets" );
	stats.tx_window_errors    = readfileuint64( path + "tx_window_errors" );
	stats.collisions          = readfileuint64( path + "collisions" );
	stats.multicast           = readfileuint64( path + "multicast" );
	stats.time = get_uptime( );
	return stats;
}
//-------------------------------------------------------------------------------------------

std::string TnetIF::rx_speed( TnetIF::Tstats& delta )
{
	double spd = ( delta.time == 0.0 ) ? 0.0 : (double( delta.rx_bytes ))*8 / delta.time;
	return double2KMGi( spd, "b/s" );
}
//-------------------------------------------------------------------------------------------

std::string TnetIF::tx_speed( TnetIF::Tstats& delta )
{
	double spd = ( delta.time == 0.0 ) ? 0.0 : (double( delta.tx_bytes ))*8 / delta.time;
	return double2KMGi( spd, "b/s" );
}
//-------------------------------------------------------------------------------------------

#undef IRFFLAGS

//-------------------------------------------------------------------------------------------

TnetIF::Tstats stats_delta( TnetIF::Tstats& curr, TnetIF::Tstats& prev )
{
    TnetIF::Tstats s; memset( &s, 0, sizeof(s) );
	s.rx_bytes            = curr.rx_bytes            - prev.rx_bytes;
	s.rx_compressed       = curr.rx_compressed       - prev.rx_compressed;
	s.rx_crc_errors       = curr.rx_crc_errors       - prev.rx_crc_errors;
	s.rx_dropped          = curr.rx_dropped          - prev.rx_dropped;
	s.rx_errors           = curr.rx_errors           - prev.rx_errors;
	s.rx_fifo_errors      = curr.rx_fifo_errors      - prev.rx_fifo_errors;
	s.rx_frame_errors     = curr.rx_frame_errors     - prev.rx_frame_errors;
	s.rx_length_errors    = curr.rx_length_errors    - prev.rx_length_errors;
	s.rx_missed_errors    = curr.rx_missed_errors    - prev.rx_missed_errors;
	s.rx_over_errors      = curr.rx_over_errors      - prev.rx_over_errors;
	s.rx_packets          = curr.rx_packets          - prev.rx_packets;
	s.tx_aborted_errors   = curr.tx_aborted_errors   - prev.tx_aborted_errors;
	s.tx_bytes            = curr.tx_bytes            - prev.tx_bytes;
	s.tx_carrier_errors   = curr.tx_carrier_errors   - prev.tx_carrier_errors;
	s.tx_compressed       = curr.tx_compressed       - prev.tx_compressed;
	s.tx_dropped          = curr.tx_dropped          - prev.tx_dropped;
	s.tx_errors           = curr.tx_errors           - prev.tx_errors;
	s.tx_fifo_errors      = curr.tx_fifo_errors      - prev.tx_fifo_errors;
	s.tx_heartbeat_errors = curr.tx_heartbeat_errors - prev.tx_heartbeat_errors;
	s.tx_packets          = curr.tx_packets          - prev.tx_packets;
	s.tx_window_errors    = curr.tx_window_errors    - prev.tx_window_errors;
	s.collisions          = curr.collisions          - prev.collisions;
	s.multicast           = curr.multicast           - prev.multicast;
	s.time                = curr.time                - prev.time;
	return s;
}
//-------------------------------------------------------------------------------------------

uint64_t stats_errors_count( TnetIF::Tstats& d )
{
	return d.rx_crc_errors + d.rx_dropped + d.rx_errors + d.rx_fifo_errors + d.rx_frame_errors
		+ d.rx_length_errors + d.rx_missed_errors + d.rx_over_errors + d.tx_aborted_errors
		+ d.tx_carrier_errors + d.tx_dropped + d.tx_errors + d.tx_fifo_errors
		+ d.tx_heartbeat_errors + d.tx_window_errors + d.collisions;
}
//-------------------------------------------------------------------------------------------

std::string stats_errors_string( TnetIF::Tstats& d )
{
	std::string s;
	if( d.rx_crc_errors       > 0 ) s += stringformat( "rx_crc_errors:%" PRIu64 " ",       d.rx_crc_errors );
	if( d.rx_dropped          > 0 ) s += stringformat( "rx_dropped:%" PRIu64 " ",          d.rx_dropped );
	if( d.rx_errors           > 0 ) s += stringformat( "rx_errors:% " PRIu64 " ",           d.rx_errors );
	if( d.rx_fifo_errors      > 0 ) s += stringformat( "rx_fifo_errors:%" PRIu64 " ",      d.rx_fifo_errors );
	if( d.rx_frame_errors     > 0 ) s += stringformat( "rx_frame_errors:%" PRIu64 " ",     d.rx_frame_errors );
	if( d.rx_length_errors    > 0 ) s += stringformat( "rx_length_errors:%" PRIu64 " ",    d.rx_length_errors );
	if( d.rx_missed_errors    > 0 ) s += stringformat( "rx_missed_errors:%" PRIu64 " ",    d.rx_missed_errors );
	if( d.rx_over_errors      > 0 ) s += stringformat( "rx_over_errors:%" PRIu64 " ",      d.rx_over_errors );
	if( d.tx_aborted_errors   > 0 ) s += stringformat( "tx_aborted_errors:%" PRIu64 " ",   d.tx_aborted_errors );
	if( d.tx_carrier_errors   > 0 ) s += stringformat( "tx_carrier_errors:%" PRIu64 " ",   d.tx_carrier_errors );
	if( d.tx_dropped          > 0 ) s += stringformat( "tx_dropped:%" PRIu64 " ",          d.tx_dropped );
	if( d.tx_errors           > 0 ) s += stringformat( "tx_errors:%" PRIu64 " ",           d.tx_errors );
	if( d.tx_fifo_errors      > 0 ) s += stringformat( "tx_fifo_errors:%" PRIu64 " ",      d.tx_fifo_errors );
	if( d.tx_heartbeat_errors > 0 ) s += stringformat( "tx_heartbeat_errors:%" PRIu64 " ", d.tx_heartbeat_errors );
	if( d.tx_window_errors    > 0 ) s += stringformat( "tx_window_errors:%" PRIu64 " ",    d.tx_window_errors );
	if( d.collisions          > 0 ) s += stringformat( "collisions:%" PRIu64 " ",          d.collisions );
	return s;
}
//-------------------------------------------------------------------------------------------




//-------------------------------------------------------------------------------------------

bool getIFs( TnetIFs& ifs )
{FUNCTION_TRACE
	ifs.clear( );
	struct ifaddrs* ifdata = NULL;
	if( getifaddrs( &ifdata ) == -1 ) return false;

	for( struct ifaddrs* ifa = ifdata; ifa != NULL; ifa = ifa->ifa_next )
	{
		string name = string( ifa->ifa_name );
        if( findIFByName( ifs, name ) ) continue;
        ifs.push_back( TnetIF( name ) );
	}
	freeifaddrs( ifdata );
	std::sort( ifs.begin(), ifs.end() );
	return true;
}
//-------------------------------------------------------------------------------------------

TnetIF* findIFByName( TnetIFs& ifs, std::string name )
{
    FOR_EACH_ITER( TnetIFs, ifs, i )
		if( i->name == name ) return &(*i);
	return NULL;
}
//-------------------------------------------------------------------------------------------

