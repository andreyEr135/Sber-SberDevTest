#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/stat.h>

#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>

#include "debugsystem.h"
#include "sysutils.h"


using namespace std;

//-------------------------------------------------------------

#undef  UNIT
#define UNIT  g_DebugSystem.units[ UNIT_SYSUTILS ]
#include "log_macros.def"

const double pi = 3.141592653589793;

//-------------------------------------------------------------

char* GetVersionStr( const TVERSION& V, bool color )
{FUNCTION_TRACE
	struct tm *ts; ts = localtime( &V.time );
	string datestr = stringformat( " (%02d.%02d.%04d)", ts->tm_mday, ts->tm_mon+1, ts->tm_year+1900 );
	string versionstr = "";
	if( V.v[0]  >= 0 ) versionstr += stringformat( " v%d", V.v[0] );
	if( V.v[1]  >= 0 ) versionstr += stringformat( ".%d",  V.v[1] );
	if( V.build >= 0 ) versionstr += stringformat( " build %d", V.build );

	static char str[ 128 ];
	if( color )
		if( V.build == -1 ) snprintf( str,sizeof(str), CLRGRN("%s%s") "%s", V.name, versionstr.c_str(), datestr.c_str() );
		else                snprintf( str,sizeof(str), CLRYLW("%s%s") "%s", V.name, versionstr.c_str(), datestr.c_str() );
	else
		snprintf( str,sizeof(str), "%s%s%s", V.name,versionstr.c_str(), datestr.c_str() );
	return str;
}
//-------------------------------------------------------------

Tmutex::Tmutex( )
{
	pthread_mutex_init( &mutex, NULL );
	lockedthread = 0;
}
Tmutex::~Tmutex( )
{
	pthread_mutex_destroy( &mutex );
}
bool Tmutex::lock( int id )
{
	bool lock = !pthread_equal( pthread_self( ), lockedthread );
	if( lock )
	{
		pthread_mutex_lock( &mutex );
		lockedthread = pthread_self( );
	}
	return lock;
}
void Tmutex::unlock( int id )
{
	lockedthread = 0;
	pthread_mutex_unlock( &mutex );
}
//-------------------------------------------------------------

Tdlock::Tdlock( Tmutex* mutex, int id ) : g_mutex(mutex),id(id)
{
    if( this->g_mutex ) locked = this->g_mutex->lock( id );
}
Tdlock::~Tdlock( )
{
    if( this->g_mutex && locked ) this->g_mutex->unlock( id );
}
//-------------------------------------------------------------

Tevent::Tevent( )
{
	pthread_cond_init( &cond, NULL );
	pthread_mutex_init( &mutex, NULL );
	flag = false;
}
Tevent::~Tevent( )
{
	pthread_mutex_destroy( &mutex );
	pthread_cond_destroy( &cond );
}
void Tevent::set( )
{
	pthread_mutex_lock( &mutex );
	flag = true;
	pthread_cond_signal( &cond );
	pthread_mutex_unlock( &mutex );
}
void Tevent::broadcast( )
{
	pthread_mutex_lock( &mutex );
	flag = true;
	pthread_cond_broadcast( &cond );
	pthread_mutex_unlock( &mutex );
}
void Tevent::reset( )
{
	pthread_mutex_lock( &mutex );
	flag = false;
	pthread_mutex_unlock( &mutex );
}
void Tevent::wait( bool reset )
{
	pthread_mutex_lock( &mutex );
	while( !flag )
		pthread_cond_wait( &cond, &mutex );
	if( reset ) flag = false;
	pthread_mutex_unlock( &mutex );
}
//-------------------------------------------------------------------------------------------

std::string trim( const std::string &str )
{
	std::string::const_iterator it = str.begin();
	while( it != str.end() && isspace(*it) ) it++;
	std::string::const_reverse_iterator rit = str.rbegin();
	while( rit.base() != it && isspace(*rit) ) rit++;
	return std::string( it, rit.base() );
}
std::string ltrim( const std::string &str )
{
	std::string::const_iterator it = str.begin();
	while( it != str.end() && isspace(*it) ) it++;
	return std::string( it, str.end() );
}
std::string rtrim( const std::string &str )
{
	std::string::const_reverse_iterator rit = str.rbegin();
	while( rit.base() != str.begin() && isspace(*rit) ) rit++;
	return std::string( str.begin(), rit.base() );
}
std::string ctrim( const std::string &str, char c )
{
	std::string::const_iterator it = str.begin();
	while( it != str.end() && ( isspace(*it) || ((*it) == c) ) ) it++;
	std::string::const_reverse_iterator rit = str.rbegin();
	while( rit.base() != it && ( isspace(*it) || ((*it) == c) ) ) rit++;
	return std::string( it, rit.base() );
}
//-------------------------------------------------------------


int utf8indexsymbols( std::string& s, int n )
{
	int index = 0;
	for( int i = 0; i < n; i++ )
	{
		BYTE c = s[index];
		if( (c&0xFF80) == 0x00 ) index += 1; // (1 байт)  0aaa aaaa
		if( (c&0xFFE0) == 0xC0 ) index += 2; // (2 байта) 110x xxxx 10xx xxxx
		if( (c&0xFFF0) == 0xE0 ) index += 3; // (3 байта) 1110 xxxx 10xx xxxx 10xx xxxx
		if( index >= (int)s.length() ) break;
	}
	return index;
}
//-------------------------------------------------------------

std::string stringformat( const char* format, ... )
{
	char str[ 1024 ];
	FormatToStr( format, str, sizeof(str) );
	return std::string( str );
}

void removeChar(char *str, char garbage) {

	char *src, *dst;
	for (src = dst = str; *src != '\0'; src++) {
		*dst = *src;
		if (*dst != garbage) dst++;
	}
	*dst = '\0';
}
//-------------------------------------------------------------

void stringsplit2( const std::string &s, char delim1, char delim2, Tstrlist& list, bool skipempty )
{
	list.clear( );
	std::string buf = "";
	int ok=0;
	for (unsigned int i = 0; i<s.length(); i++)
	{
		if ((s[i]!=delim1)&&(s[i]!=delim2)) buf = buf + s[i];
		else if ((s[i]==delim1)&&(s[i+1]==delim2))
		{
			ok=1;
			list.push_back(buf);
			buf = "";
		}
		if (i + 1 == s.length())
		{
			if (ok==1) list.push_back(buf);
			buf = "";
		}
	}
}

//------------------------------------------------------------

int stringsplit( const std::string &s, char delim, Tstrlist& list, bool skipempty )
{
	list.clear( );
	std::stringstream ss(s);
	std::string item;
	while( std::getline(ss, item, delim ) )
	{
		if( skipempty && item.empty( ) ) continue;
		list.push_back(item);
	}
	return list.size();
}
//-------------------------------------------------------------

void stringsplit( const std::string &s, char delim1, char delim2, Tstrlist& list )
{
	list.clear( );
	std::stringstream ss(s);
	std::string item;
	while( std::getline(ss, item, delim2 ) )
	{
		std::string::size_type pos = item.rfind( delim1 );
		if( pos == std::string::npos ) continue;
		list.push_back( item.substr(pos+1) );
	}
}
//-------------------------------------------------------------

bool strlistcheck( Tstrlist& list, std::string str )
{
    FOR_EACH_ITER( Tstrlist, list, s ) if( *s == str ) return true;
	return false;
}
//-------------------------------------------------------------

bool stringkeyvalue( const std::string &s, char delim, std::string& key, std::string& value )
{
	std::string::size_type pos = s.find( delim );
	if( pos == std::string::npos )
	{
		key = s;
		value = "";
		return false;
	}
	key = trim( s.substr( 0, pos ) );
	value = trim( s.substr( pos+1 ) );
	return true;
}
//-------------------------------------------------------------

std::string stringupper( std::string str )
{
	std::transform( str.begin(), str.end(), str.begin(), ::toupper );
	return str;
}
std::string stringlower( std::string str )
{
	std::transform( str.begin(), str.end(), str.begin(), ::tolower );
	return str;
}
//-------------------------------------------------------------

std::string removecolor( std::string str )
{
	while( 1 )
	{
		string::size_type b = str.find( "\033[" ); if( b == string::npos ) break;
		string::size_type e = str.find( "m", b );  if( e == string::npos ) break;
		string v = str.substr( b,e-b+1 );
		replace( str, v, "" );
	}
	return str;
}
//-------------------------------------------------------------

char* removecolor( char* str )
{
	char tmp[ strlen(str) + 1 ]; *tmp = 0;
	char* s = str;
	while( 1 )
	{
		char* b = strstr( s, "\033[" ); if( !b ) break;
		char* e = strchr( b, 'm' );     if( !e ) break;
		*b = 0;
		strcat( tmp, s );
		s = e + 1;
	}
	strcat( tmp, s );
	strcpy( str, tmp );
	return str;
}
//-------------------------------------------------------------

//https://www.codeproject.com/Articles/188256/A-Simple-Wildcard-Matching-Function?msg=3874388
bool maskmatch( std::string str, std::string mask  )
{FUNCTION_TRACE
	char* s = (char*)str.c_str();
	char* m = (char*)mask.c_str();
	char* sp = NULL;
	char* mp = NULL;
	while( *s )
	{
		if( *m == '*' ) { if( !*++m ) return true; mp = m; sp = s + 1; }
		else if( *m == '?' || *m == *s ) { m++; s++; }
		else if( !sp ) return false;
		else { m = mp; s = sp++; }
	}
	while( *m == '*' ) m++;
	return !*m;
}
//-------------------------------------------------------------

std::string specsymbols( std::string str )
{
	while( replace( str, "\\n", "\n" ) );
	while( replace( str, "\\r", "\r" ) );

	size_t pos;
	while( ( pos = str.find( "\\x" ) ) != string::npos )
	{
		string code = str.substr( pos, 4 );
		unsigned int b;
		if( sscanf( code.c_str()+2, "%x", &b ) != 1 )
		{
			str.replace( pos, 2, "" );
			continue;
		}
		char c[2] = { char(b),0 };
		replace( str, code, string(c) );
	}
	return str;
}
//-------------------------------------------------------------

std::string hexstr( void* buf, int size )
{
	string str;
	for( int i = 0; i < size; i++ ) str += stringformat( "%02X ", ((BYTE*)buf)[i] );
	return str;
}
//-------------------------------------------------------------

void Usleep( const int time )
{
	struct timeval timeout;
	timeout.tv_sec = time / 1000000;
	timeout.tv_usec = time % 1000000;
	do
	{
		select ( 0, NULL, NULL, NULL, &timeout );
	}
	while( ( timeout.tv_sec > 0 )&&( timeout.tv_usec > 0 ) );
}
//-------------------------------------------------------------

char Sign( const double a )
{
	if( a > 0 ) return +1;
	if( a < 0 ) return -1;
	return 0;
}
//-------------------------------------------------------------

double Round( const double x )
{
	double ax = fabs( x );
	if ( ax-long(ax) < 0.5 ) return long(ax)*Sign( x );
	else                     return (long(ax)+1)*Sign( x );
}
//-------------------------------------------------------------

bool RunProcess( pid_t& pid, const char* command, int* inpipe, int* outpipe )
{
	bool result = false;

	int checkpipe[2];

	if( pipe( checkpipe ) != 0 ) printf( "Error: create checkpipe\n" );
	int rc;
	if( ( rc = fcntl( checkpipe[1], F_GETFD, 0 ) ) < 0 || fcntl( checkpipe[1], F_SETFD, rc | FD_CLOEXEC ) == -1
	 || ( rc = fcntl( checkpipe[0], F_GETFD, 0 ) ) < 0 || fcntl( checkpipe[0], F_SETFD, rc | FD_CLOEXEC ) == -1 ) printf( "Error: checkpipe FD_CLOEXEC\n" );


	pid = fork( );
	if( pid == -1 )
	{
		printf( "Error: fork\n" );
		result = false;
	}
	else
	if ( pid == 0 ) // для дочернего процесса
	{
		close( checkpipe[0] ); // close checkpipe read

		if( inpipe )
		{
			if( dup2( inpipe[1], STDOUT_FILENO ) == -1 ) printf( "Error: dup2 STDOUT\n" ); // child stdout --> parent inpipe
			if( dup2( inpipe[1], STDERR_FILENO ) == -1 ) printf( "Error: dup2 STDERR\n" ); // child stderr --> parent inpipe
			if( close( inpipe[0] ) != 0 ) printf( "Error: close child inpipe[0]\n" );
			if( close( inpipe[1] ) != 0 ) printf( "Error: close child inpipe[1]\n" );
		}
		if( outpipe )
		{
			if( dup2( outpipe[0], STDIN_FILENO ) == -1 ) printf( "Error: dup2 STDIN\n" ); // parent outpipe --> child stdin
			if( close( outpipe[0] ) != 0 ) printf( "Error: close child outpipe[0]\n" );
			if( close( outpipe[1] ) != 0 ) printf( "Error: close child outpipe[1]\n" );
		}
		// close all fds, кроме 0 1 2 and checkpipe[1]
		long max = sysconf( _SC_OPEN_MAX );
		while( --max > 2 ) if( max != checkpipe[1] ) close( max );
		errno = 0;

		//разблокировка всех сигналов
		sigset_t sig_mask;
		sigfillset( &sig_mask );
		sigprocmask( SIG_UNBLOCK, &sig_mask, NULL );

		//сигнал, приходящий процессу при завершении работы родительского процесса
		prctl( PR_SET_PDEATHSIG, SIGTERM );

		int size = strlen(command) + 1;
		char buf[ size ]; strcpy( buf, command );
		char* arg[64]; memset( arg, 0, sizeof(arg) );
		int n = 0; bool vl = false, st = false;
		for( int i = 0; i < size; i++ )
		{

			if( isspace( buf[i] ) && !st ) { buf[i] = 0; vl = false; continue; }
			if( ( buf[i] == '\'' )||( buf[i] == '\"' ) ) { st = !st; continue; }
			if( !vl ) { arg[ n++ ] = buf + i; vl = true; }
		}

		for (int i = 0; i < n; i++)
		{
			while ( (strchr (arg[i],'\'') != NULL) || (strchr (arg[i],'\"') != NULL) )
			{
				if (strchr (arg[i],'\'') != NULL) removeChar(arg[i], '\'');
				else  if (strchr (arg[i],'\"') != NULL) removeChar(arg[i], '\"');
			}
		}

		execvp( arg[0], arg );

		printf( "Error: execvp '%s'\n", arg[0] );
		if( write( checkpipe[1], "X", 1 ) == -1 ) printf( "Error: write checkpipe\n" );
		close( checkpipe[1] );

		_exit( EXIT_FAILURE );
	}
	else  // родительский процесс
	{
		close( checkpipe[1] );  // close checkpipe write

		char out; int n = read( checkpipe[0], &out, 1 );
		close( checkpipe[0] );
		result = ( n == 0 );
	}

	return result;
}
//-------------------------------------------------------------

int WaitProcess( const pid_t pid )
{FUNCTION_TRACE
	int status = 0;
	waitpid( pid, &status, 0 );
	return status;
}
//-------------------------------------------------------------

bool CheckProcess( const pid_t pid, int& status )
{FUNCTION_TRACE
	status = 0;
	pid_t p = waitpid( pid, &status, WNOHANG );
	return ( p == 0 );
}
//-------------------------------------------------------------

int ExecuteProcess( const char *command )
{FUNCTION_TRACE
	pid_t pid;
	RunProcess( pid, command );
	int status = WaitProcess( pid );
	return status;
}
//-------------------------------------------------------------

pid_t RunProcessIO( const char* command, FILE* &instream, FILE* &outstream )
{FUNCTION_TRACE
	int inpipe[2];   if( pipe( inpipe  ) != 0 ) Error( "create inpipe\n" );
	int outpipe[2];  if( pipe( outpipe ) != 0 ) Error( "create outpipe\n" );

	pid_t childpid = 0;
	if( RunProcess( childpid, command, inpipe, outpipe ) )
	{
		int flags = 0;
		if( ( flags = fcntl( inpipe [0], F_GETFD, 0 ) ) < 0 || fcntl( inpipe [0], F_SETFD, flags | FD_CLOEXEC ) == -1 ) Error( "inpipe[0] FD_CLOEXEC" );
		if( ( flags = fcntl( outpipe[1], F_GETFD, 0 ) ) < 0 || fcntl( outpipe[1], F_SETFD, flags | FD_CLOEXEC ) == -1 ) Error( "outpipe[1] FD_CLOEXEC" );

		if( !( instream  = fdopen( inpipe [0], "r" ) ) ) Error( "create instream\n" );  // чтение из inpipe
		if( !( outstream = fdopen( outpipe[1], "w" ) ) ) Error( "create outstream\n" ); // запись в outpipe

		errno = 0;
		if( instream )  setvbuf( instream,  NULL, _IONBF, 0 );
		if( outstream ) setvbuf( outstream, NULL, _IONBF, 0 );
	}
	else
	{
		instream = NULL;
		outstream = NULL;
		close( inpipe [0] );
		close( outpipe[1] );
	}
	close( inpipe [1] );  // закрываем запись в inpipe
	close( outpipe[0] );  // закрываем чтение из outpipe
	//pthread_mutex_unlock( &processiomutex );
	return childpid;
}
//-------------------------------------------------------------

int WaitProcessIO( pid_t pid, FILE* &instream, FILE* &outstream )
{FUNCTION_TRACE
	int status = WaitProcess( pid );
	xfclose( instream );
	xfclose( outstream );
	return status;
}
//-------------------------------------------------------------

int ExecuteProcessSilent( const char* command )
{FUNCTION_TRACE
	FILE *instream, *outstream;
	pid_t childpid = RunProcessIO( command, instream, outstream );
	return WaitProcessIO( childpid, instream, outstream );
}
//-------------------------------------------------------------

int ExecuteProcessIO( pid_t& childpid, const char* command, FILE* &OutStream, bool(*OnInStream)( char* str, void* param ), void* param, char* resultstr )
{FUNCTION_TRACE
	FILE *instream, *outstream;

	childpid = RunProcessIO( command, instream, outstream );
	OutStream = outstream;

	if( instream && OnInStream )
	{
		char buf[ 1024*4 ];
		while( !feof( instream ) )
		{
			if( !fgets( buf, sizeof(buf), instream ) ) break;
			if( !(*OnInStream)( buf, param ) && resultstr ) strcpy( resultstr, buf );
		}
	}

	int status = WaitProcessIO( childpid, instream, outstream );
	childpid = 0;
	OutStream = NULL;
	return status;
}
//-------------------------------------------------------------

std::string GetLinesProcess( const char* command, int* status )
{FUNCTION_TRACE
	FILE *instream, *outstream;
	pid_t childpid = RunProcessIO( command, instream, outstream );

	char buf[ 1024 ];
	std::string data;
	while( instream && !feof( instream ) )
	{
		if( !fgets( buf, sizeof(buf), instream ) ) break;
		data += string( buf );
	}

	int result = WaitProcessIO( childpid, instream, outstream );
	if( status ) *status = result;
	return data;
}
//-------------------------------------------------------------

int GetProcessData( std::string command, Tstrlist &data, int timeout )
{FUNCTION_TRACE
	FILE *instream, *outstream;
	pid_t childpid = RunProcessIO( command.c_str(), instream, outstream );
	if( !instream ) return WaitProcessIO( childpid, instream, outstream );

	if( timeout != 0 )
	{
		fd_set readfds;
		FD_ZERO( &readfds );
		FD_SET( fileno(instream), &readfds );
		struct timeval to;
		timeout *= 1000;
		to.tv_sec = timeout / 1000000;
		to.tv_usec = timeout % 1000000;
		int selectresult = select( fileno(instream) + 1, &readfds, 0, 0, &to );
		if( selectresult < 0  ) Error( "process '%s' select error, killing process...\n", command.c_str() );
		if( selectresult == 0 ) Error( "process '%s' TIMEOUT, killing process...\n", command.c_str() );
		if( selectresult <= 0 )
			if( kill( childpid, SIGTERM ) !=0 ) Error( "kill %d SIGTERM\n", childpid );
	}

	char buf[ 1024 ];
	while( !feof( instream ) )
	{
		if( !fgets( buf, sizeof(buf), instream ) ) break;
		data.push_back( string( buf ) );
	}

	int result = WaitProcessIO( childpid, instream, outstream );

	return result;
}
//-------------------------------------------------------------------------------------------

void OnTimeoutKill( sigval sv )
{
	Tpidtimer* pidtimer = (Tpidtimer*)sv.sival_ptr;
	if( !pidtimer || pidtimer->timer == timer_t(-1) ) return;
	StopTimer( pidtimer->timer );
	kill( pidtimer->pid, pidtimer->sig );
}
//-------------------------------------------------------------------------------------------

Tpidtimer* RunProcessIOTimeout( std::string command, FILE* &instream, FILE* &outstream, int timeoutms, int sig )
{FUNCTION_TRACE
	Tpidtimer* pidtimer = new Tpidtimer;
	pidtimer->sig = sig;

	pidtimer->pid = RunProcessIO( command.c_str(), instream, outstream );
	if( !instream )
	{
		WaitProcessIO( pidtimer->pid, instream, outstream );
		xdelete( pidtimer );
		return NULL;
	}

	if( StartTimerNewThread( &pidtimer->timer, timeoutms/1000,(timeoutms % 1000)*1000, &OnTimeoutKill, pidtimer ) != 0 )
	{
		kill( pidtimer->pid, sig );
		StopTimer( pidtimer->timer );
		WaitProcessIO( pidtimer->pid, instream, outstream );
		xdelete( pidtimer );
		return NULL;
	}
	return pidtimer;
}
//-------------------------------------------------------------------------------------------

int WaitProcessIOTimeout( Tpidtimer* pidtimer, FILE* &instream, FILE* &outstream )
{FUNCTION_TRACE
	int status = WaitProcessIO( pidtimer->pid, instream, outstream );
	StopTimer( pidtimer->timer );
	xdelete( pidtimer );
	return status;
}
//-------------------------------------------------------------

int GetProcessDataTimeout( std::string command, std::string& data, int timeoutms, int sig )
{FUNCTION_TRACE
	data = "";
	int result = -1;

	FILE *instream, *outstream;
	Tpidtimer* pidtimer = RunProcessIOTimeout( command, instream, outstream, timeoutms, sig );
	if( !pidtimer ) { data = "run " + command; return -1; }

	char buf[ 1024 ];
	while( !feof( instream ) )
	{
		if( !fgets( buf, sizeof(buf), instream ) ) break;
		data += string( buf );
	}
	result = WaitProcessIOTimeout( pidtimer, instream, outstream );
	if( WIFSIGNALED(result) && ( WTERMSIG(result) == SIGTERM )) data += string(" timeout");
	return result;
}
//-------------------------------------------------------------------------------------------




bool WritePipe( int *pipe, const void *buf, int size )
{FUNCTION_TRACE
	WORD head[2] = { 0xFFFF, (WORD)size };
	bool result = ( write( pipe[1], head, sizeof(head) ) != -1 );
	result &= ( write( pipe[1], buf, size ) != -1 );
	if( !result ) Error( "write to pipe\n" );
	return result;
}
//-------------------------------------------------------------

int WaitPipe( int *pipe, int timeout )
{FUNCTION_TRACE
	if( !pipe ) return -2;
	fd_set readfds;
	FD_ZERO( &readfds );
	FD_SET( pipe[0], &readfds );
	struct timeval to;
	to.tv_sec = timeout / 1000000;
	to.tv_usec = timeout % 1000000;
	return select( pipe[0] + 1, &readfds, 0, 0, &to );
}
//-------------------------------------------------------------

int ReadPipe ( int *pipe, void *buf, int bufsize, int timeout )
{FUNCTION_TRACE
	if( !pipe ) { Error( "pipe == NULL\n"); return -1; }
	if( !buf  ) { Error( "buf == NULL\n"); return -1; }

	int result = WaitPipe( pipe, timeout );
	if( result == 0 ) return  0;  // timeout
	if( result <  0 ) return -1;  // select error

	WORD head[2];
	if( read( pipe[0], head, sizeof(head) ) != sizeof(head) ) { Error( "pipe read head error\n"); return -2; }
	if( head[0] != 0xFFFF ) { Error( "pipe read 0xFFFF error\n"); return -3; }
	int datasize = head[1];
	if( datasize > bufsize ) { Error( "pipe read error datasize > bufsize\n"); return -4; }
	if( read( pipe[0], buf, datasize ) != datasize ) { Error( "pipe read data error\n"); return -5; }

	return datasize;
}
//-------------------------------------------------------------

int StartTimerNewThread( timer_t* tt, uint sec, uint micro, void(*function)( sigval sv ), sigval sv )
{FUNCTION_TRACE
	sigevent se;
	memset( &se, 0, sizeof(se) );
	se.sigev_notify = SIGEV_THREAD;
	se.sigev_notify_function = function;
    se.sigev_value = sv;

	itimerspec tv;
	memset( &tv, 0, sizeof(tv) );
	tv.it_interval.tv_sec  = sec;
	tv.it_interval.tv_nsec = micro * 1000;
	tv.it_value.tv_sec     = sec;
	tv.it_value.tv_nsec    = micro * 1000;

	memset( tt, 0, sizeof(timer_t) );
	if( timer_create( CLOCK_REALTIME, &se, tt ) != 0 ) return -1;
	if( timer_settime( *tt, 0, &tv, NULL ) != 0 ) return -2;
	return 0;

}
int StartTimerNewThread( timer_t* tt, uint sec, uint micro, void(*function)( sigval sv ) )
{
	sigval sv; return StartTimerNewThread( tt, sec, micro, function, sv );
}
int StartTimerNewThread( timer_t* tt, uint sec, uint micro, void(*function)( sigval sv ), int svint )
{
	sigval sv; sv.sival_int = svint; return StartTimerNewThread( tt, sec, micro, function, sv );
}
int StartTimerNewThread( timer_t* tt, uint sec, uint micro, void(*function)( sigval sv ), void* svptr )
{
	sigval sv; sv.sival_ptr = svptr; return StartTimerNewThread( tt, sec, micro, function, sv );
}
//-------------------------------------------------------------

int StartTimerOwnThread( timer_t* tt, uint sec, uint micro, void(*function)( int signo ), int signo )
{FUNCTION_TRACE
	sigevent se;
	signal( signo, (sighandler_t)function );

	memset( &se, 0, sizeof(se) );
	se.sigev_notify = SIGEV_THREAD_ID;
	se._sigev_un._tid = getthreadpid( );
	se.sigev_signo = signo;

	memset( tt, 0, sizeof(timer_t) );
	if( timer_create( CLOCK_REALTIME, &se, tt ) != 0 )
	{
		errno = 0;
		memset( &se, 0, sizeof(se) );
		se.sigev_notify = SIGEV_SIGNAL;
		se.sigev_signo = signo;
		memset( tt, 0, sizeof(timer_t) );
		if( timer_create( CLOCK_REALTIME, &se, tt ) != 0 ) return -1;
	}

	itimerspec tv;
	memset( &tv, 0, sizeof(tv) );
	tv.it_interval.tv_sec  = sec;
	tv.it_interval.tv_nsec = micro * 1000;
	tv.it_value.tv_sec     = sec;
	tv.it_value.tv_nsec    = micro * 1000;
	if( timer_settime( *tt, 0, &tv, NULL ) != 0 ) return -2;

	return 0;
}
//-------------------------------------------------------------

void StopTimer( timer_t& tt )
{FUNCTION_TRACE
	if( tt == timer_t(-1) ) return;
	if( timer_delete( tt ) != 0 ) Error( "timer_delete\n" );
	tt = timer_t(-1);
}
//-------------------------------------------------------------

timeval TimeStart( ) { timeval t; gettimeofday( &t, NULL ); return t; }
double TimeMicro( timeval& t ) { timeval e = TimeStart(); return ((e.tv_sec*1000000.0 )+e.tv_usec) - ((t.tv_sec*1000000.0 )+t.tv_usec); }
double TimeMilli( timeval& t ) { return TimeMicro(t) / 1000.0; }
double TimeSec  ( timeval& t ) { return TimeMicro(t) / 1000000.0; }

//-------------------------------------------------------------

bool fileexist( const char* filename )
{FUNCTION_TRACE
	struct stat sb;

	if( stat(filename, &sb) == -1 ) {
		errno = 0;
		return false;
	}
	errno = 0;
	return true;
}
bool fileexist( std::string filename )
{ return fileexist( filename.c_str() ); }

//-------------------------------------------------------------

bool direxist( const char* dirname )
{FUNCTION_TRACE
	struct stat st;
	if( stat( dirname, &st ) != 0 ) { errno = 0; return false; }
	return S_ISDIR( st.st_mode );
}
bool direxist( std::string dirname )
{ return direxist( dirname.c_str() ); }

//-------------------------------------------------------------

std::string getpath( const char* filename )
{
	char* c = strrchr( (char*)filename, '/' );
	if( !c ) return "";
	return string( (char*)filename, c );
}
std::string getpath( std::string filename )
{ return getpath( filename.c_str() ); }

//-------------------------------------------------------------

std::string getfile( const char* filename )
{
	char* c = strrchr( (char*)filename, '/' );
	if( !c ) return string( (char*)filename );
	return string( c + 1 );
}
std::string getfile( std::string filename )
{ return getfile( filename.c_str() ); }

//-------------------------------------------------------------

std::string getFilenameFromPath( const char* filepath )
{
    if (filepath == nullptr) {
        return ""; // Возвращаем пустую строку, если путь нулевой
    }

    std::string path_str(filepath); // Преобразуем const char* в std::string

    // Находим позицию последнего разделителя '/'
    size_t last_slash_pos = path_str.find_last_of('/');

    // Если разделитель найден, имя файла начинается сразу после него
    if (last_slash_pos != std::string::npos) {
        return path_str.substr(last_slash_pos + 1);
    } else {
        // Если разделителей нет, вся строка - это имя файла
        return path_str;
    }
}
std::string getFilenameFromPath( std::string filename )
{ return getFilenameFromPath( filename.c_str() ); }

//-------------------------------------------------------------

bool copyFile( const char* oldFilePath, const char* newFilePath )
{
    try {
        // fs::copy(oldFilePath, newFilePath, copy_options);
        // copy_options позволяют настроить поведение:
        // - fs::copy_options::none (по умолчанию) - копирует файл.
        // - fs::copy_options::skip_existing - пропустить, если целевой файл существует.
        // - fs::copy_options::overwrite_existing - перезаписать, если существует.
        // - fs::copy_options::update_existing - перезаписать, если исходный файл новее.
        // - fs::copy_options::recursive - для каталогов, копировать рекурсивно.
        // и другие.
        std::filesystem::copy(oldFilePath, newFilePath, std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        return false;
    }
}
bool copyFile( std::string oldFilePath, std::string newFilePath )
{
    return copyFile( oldFilePath.c_str(), newFilePath.c_str() );
}

//-------------------------------------------------------------

std::string readfilestr( const char* filename, bool& ok )
{FUNCTION_TRACE
	ok = false;
	if( !filename ) return "";
	string content = "";
	char buf[256]; *buf = 0;
	FILE* fp = fopen( filename, "r" );
	if( !fp ) { errno = 0; return ""; }
	while( fgets( buf, sizeof(buf), fp ) ) content += buf;
	fclose( fp );
	errno = 0;
	ok = true;
	return content;
}
std::string readfilestr( std::string filename, bool& ok ) { return readfilestr( filename.c_str(), ok ); }
std::string readfilestr( const char* filename ) { bool ok; return readfilestr( filename, ok ); }
std::string readfilestr( std::string filename ) { bool ok; return readfilestr( filename, ok ); }

//-------------------------------------------------------------

int readfileint( const char* filename, bool& ok )
{FUNCTION_TRACE
	std::string content = readfilestr( filename, ok );
	if( !ok ) return 0;
	int result = 0;
	if( sscanf( content.c_str(), "%d", &result ) != 1 ) ok = false;
	return result;
}
int readfileint( std::string filename, bool& ok ) { return readfileint( filename.c_str(), ok ); }
int readfileint( const char* filename ) { bool ok; return readfileint( filename, ok ); }
int readfileint( std::string filename ) { bool ok; return readfileint( filename, ok ); }

//-------------------------------------------------------------

uint64_t readfileuint64( const char* filename, bool& ok )
{FUNCTION_TRACE
	std::string content = readfilestr( filename, ok );
	if( !ok ) return 0;
	uint64_t result = 0;
	if( sscanf( content.c_str(), "%" SCNu64, &result ) != 1 ) ok = false;
	return result;
}
uint64_t readfileuint64( std::string filename, bool& ok ) { return readfileuint64( filename.c_str(), ok ); }
uint64_t readfileuint64( const char* filename ) { bool ok; return readfileuint64( filename, ok ); }
uint64_t readfileuint64( std::string filename ) { bool ok; return readfileuint64( filename, ok ); }

//-------------------------------------------------------------

int readfilehex( const char* filename, bool& ok )
{FUNCTION_TRACE
	std::string content = readfilestr( filename, ok );
	if( !ok ) return 0;
	int result = 0;
	if( sscanf( content.c_str(), "%x", &result ) != 1 ) ok = false;
	return result;
}
int readfilehex( std::string filename, bool& ok ) { return readfilehex( filename.c_str(), ok ); }
int readfilehex( const char* filename ) { bool ok; return readfilehex( filename, ok ); }
int readfilehex( std::string filename ) { bool ok; return readfilehex( filename, ok ); }

//-------------------------------------------------------------

float readfilefloat( const char* filename, bool& ok )
{FUNCTION_TRACE
	std::string content = readfilestr( filename, ok ); if( !ok ) return -1;
	float result = str2float( content, ok );           if( !ok ) return -2;
	return result;
}
float readfilefloat( std::string filename, bool& ok ) { return readfilefloat( filename.c_str(), ok ); }
float readfilefloat( const char* filename ) { bool ok; return readfilefloat( filename, ok ); }
float readfilefloat( std::string filename ) { bool ok; return readfilefloat( filename, ok ); }

//-------------------------------------------------------------

bool readfiletext( std::string filename, Tstrlist& text )
{FUNCTION_TRACE
	text.clear();
	if( filename.empty() ) return false;
	char buf[1024]; *buf = 0;
	FILE* fp = fopen( filename.c_str(), "r" );
	if( !fp ) { errno = 0; return false; }
	while( fgets( buf, sizeof(buf), fp ) ) text.push_back( buf );
	xfclose( fp );
	errno = 0;
	return true;
}
//-------------------------------------------------------------

bool writefile( std::string filename, const char* format, ... )
{FUNCTION_TRACE
	bool result = false;
	FILE* fp = fopen( filename.c_str(), "w" );
	if( !fp )  { errno = 0; return false; }
	char str[ 1024 ]; FormatToStr( format, str, sizeof(str) );
	if( fputs( str, fp ) != EOF ) result = true;
	fflush(fp);
	sync();
	fclose( fp );
	errno = 0;
	return result;
}
//-------------------------------------------------------------

bool appendfile( std::string filename, const char* format, ... )
{FUNCTION_TRACE
	bool result = false;
	FILE* fp = fopen( filename.c_str(), "a" );
	if( !fp )  { errno = 0; return false; }
	char str[ 1024 ]; FormatToStr( format, str, sizeof(str) );
	if( fputs( str, fp ) != EOF ) result = true;
	fflush(fp);
	sync();
	fclose( fp );
	errno = 0;
	return result;
}
//-------------------------------------------------------------

bool deletefile( const char* filename )
{
	if( strlen( filename ) <= 0 ) return false;
	bool result = ( unlink( filename ) == 0 );
	errno = 0;
	return result;
}
bool deletefile( std::string filename ) { return deletefile( filename.c_str() ); }

//-------------------------------------------------------------


bool replace( std::string& source, std::string find, std::string str )
{
	size_t pos = source.find( find );
	if( pos == string::npos ) return false;
	source.replace( pos, find.length(), str );
	return true;
}
//-------------------------------------------------------------

int time2seconds( std::string time )
{
	char* s = (char*)time.c_str();
	float value = 0;
	int seconds = -1;
	if( sscanf( s, "%f", &value ) != 1 ) return -1;
	if( strchr( s, 's' ) ) seconds = int(value); else
	if( strchr( s, 'm' ) ) seconds = int(value*60); else
	if( strchr( s, 'h' ) ) seconds = int(value*60*60); else
	if( strchr( s, 'd' ) ) seconds = int(value*60*60*24); else
		return -2;
	return seconds;
}
//-------------------------------------------------------------

int str2int( const char* str, bool& ok )
{
	char* s = (char*)str;
	for( ; ( *s != 0 )&&( !isdigit(*s) ); s++ ) ;
	if( ( s > (char*)str )&&( (*(s-1)=='+')||(*(s-1)=='-') ) ) s--;
	int value = 0;
	ok = ( sscanf( s, "%d", &value ) == 1 );
	return value;
}
int str2int( std::string str, bool& ok ) { return str2int( str.c_str(), ok ); }
int str2int( const char* str ) { bool ok; return str2int( str, ok ); }
int str2int( std::string str ) { bool ok; return str2int( str, ok ); }

//-------------------------------------------------------------

float str2float( const char* str, bool& ok )
{
	char* s = (char*)str;
	for( ; ( *s != 0 )&&( !isdigit(*s) ); s++ ) ;
	if( ( s > (char*)str )&&( (*(s-1)=='+')||(*(s-1)=='-') ) ) s--;
	string ss = string( s );
	struct lconv* lc = localeconv( );
	if( strcmp( lc->decimal_point, "." ) == 0 ) replace( ss, ",", "." );
	if( strcmp( lc->decimal_point, "," ) == 0 ) replace( ss, ".", "," );
	float value = 0;
	ok = ( sscanf( ss.c_str(), "%f", &value ) == 1 );
	return value;
}
float str2float( std::string str, bool& ok ) { return str2float( str.c_str(), ok ); }
float str2float( const char* str ) { bool ok; return str2float( str, ok ); }
float str2float( std::string str ) { bool ok; return str2float( str, ok ); }

//-------------------------------------------------------------

int str2hex( const char* str, bool& ok )
{
	char* s = (char*)str;
	if( *s == 0 && *(s+1)=='x' ) s += 2;
	int value = 0;
	ok = ( sscanf( s, "%x", &value ) == 1 );
	return value;
}
int str2hex( std::string str, bool& ok ) { return str2hex( str.c_str(), ok ); }
int str2hex( const char* str ) { bool ok; return str2hex( str, ok ); }
int str2hex( std::string str ) { bool ok; return str2hex( str, ok ); }

//-------------------------------------------------------------

double KMG2double( std::string str )
{
	str = stringupper( str );
	char* s = (char*)str.c_str();
	bool ok; double value = str2float( s, ok ); if( !ok ) return -1.0;
	if( ( strstr( s, "K" ) ) || ( strstr( s, "кб" ) ) ) value *= 1000; else
	if( ( strstr( s, "M" ) ) || ( strstr( s, "мб" ) ) ) value *= 1000*1000; else
	if( ( strstr( s, "G" ) ) || ( strstr( s, "гб" ) ) ) value *= 1000*1000*1000;
	return value;
}
double KMG2double( const char* str ) { return KMG2double( string(str) ); }

//-------------------------------------------------------------
std::string double2KMG( double value, std::string suf )
{
	if( value > 1000*1000*1000 ) { suf = "G"+suf; value /= 1000*1000*1000; } else
	if( value > 1000*1000      ) { suf = "M"+suf; value /= 1000*1000; } else
	if( value > 1000           ) { suf = "K"+suf; value /= 1000; }
	return stringformat( "%.2f %s", value, suf.c_str() );
}
//-------------------------------------------------------------
double KMGi2double( std::string str )
{
	str = stringupper( str );
	char* s = (char*)str.c_str();
	bool ok; double value = str2float( s, ok ); if( !ok ) return -1.0;
	if( ( strstr( s, "K" ) ) || ( strstr( s, "K" ) ) )  value *= 1024; else
	if( ( strstr( s, "M" ) ) || ( strstr( s, "M" ) ) ) value *= 1024*1024; else
	if( ( strstr( s, "G" ) ) || ( strstr( s, "G" ) ) ) value *= 1024*1024*1024;
	return value;
}
double KMGi2double( const char* str ) { return KMGi2double( string(str) ); }
//-------------------------------------------------------------

std::string double2KMGi( double value, std::string suf )
{
	if( value > 1024*1024*1024 ) { suf = "G"+suf; value /= 1024*1024*1024; } else
	if( value > 1024*1024      ) { suf = "M"+suf; value /= 1024*1024; } else
	if( value > 1024           ) { suf = "K"+suf; value /= 1024; }
	return stringformat( "%.2f %s", value, suf.c_str() );
}
//-------------------------------------------------------------

std::string double2KMGr( double value, std::string suf )
{
	if( value > 1000*1000*1000 ) { suf = "G"+suf; value /= 1000*1000*1000; } else
	if( value > 1000*1000      ) { suf = "M"+suf; value /= 1000*1000; } else
	if( value > 1000           ) { suf = "K"+suf; value /= 1000; }
    value = round(value);
	return stringformat( "%.0f %s", value, suf.c_str() );
}

//-------------------------------------------------------------


// witout FUNCTION_TRACE !!!
std::string nowtime( char separator )
{
	time_t nowtime = time( 0 ); struct tm* ts = localtime( &nowtime );
	return stringformat( "%02d%c%02d%c%02d", ts->tm_hour, separator, ts->tm_min, separator, ts->tm_sec );
}
std::string nowdate( char separator )
{
	time_t nowtime = time( 0 ); struct tm* ts = localtime( &nowtime );
	return stringformat( "%02d%c%02d%c%04d", ts->tm_mday, separator, ts->tm_mon+1, separator, ts->tm_year+1900 );
}
std::string nowrdate( char separator )
{
	time_t nowtime = time( 0 ); struct tm* ts = localtime( &nowtime );
	return stringformat( "%04d%c%02d%c%02d", ts->tm_year+1900, separator, ts->tm_mon+1, separator, ts->tm_mday );
}
//-------------------------------------------------------------

// witout FUNCTION_TRACE !!!
float get_uptime( )
{
	struct timespec t;
	clock_gettime( CLOCK_MONOTONIC, &t );
	return (double)t.tv_nsec / (double)1000000000.0 + t.tv_sec;
}
//---------------------------------------------------------------------------

float delta_uptime( float& t )
{
	return get_uptime( ) - t;
}
//---------------------------------------------------------------------------


pid_t getthreadpid( )
{
	return (pid_t)syscall( SYS_gettid );
}
//-------------------------------------------------------------

std::string getlink( std::string linkname )
{
	char link[512]; *link = 0; ssize_t len = 0;
	if( ( len = readlink( linkname.c_str(), link, sizeof(link)-1 ) ) == -1 ) { errno=0; return ""; }
	link[len] = '\0';
	return string( link );
}
//-------------------------------------------------------------

std::string macstr( BYTE* mac ) { return mac ? stringformat( "%02x:%02x:%02x:%02x:%02x:%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5] ) : "NULL"; }
std::string pcibusstr( BYTE* pcibus ) { return pcibus ? stringformat( "%04x:%02x:%02x.%d", pcibus[0],pcibus[1],pcibus[2],pcibus[3] ) : "NULL"; }
std::string vendevstr( WORD* vendev ) { return vendev ? stringformat( "%04x:%04x", vendev[0],vendev[1] ) : "NULL"; }
std::string ipstr( struct in_addr& ip ) { return string( inet_ntoa( ip ) ); }

//-------------------------------------------------------------

bool pcibusscan( std::string str, BYTE* pcibus )
{
	memset( pcibus, 0, 4 );
	int b[4] = { 0,0,0,0 };
	bool ok = ( sscanf( (char*)str.c_str(), "%04x:%02x:%02x.%d", &b[0],&b[1],&b[2],&b[3] ) == 4 );
	if( !ok ) { b[0] = 0; ok = ( sscanf( (char*)str.c_str(), "%02x:%02x.%d", &b[1],&b[2],&b[3] ) == 3 ); }
	if( ok ) for( int i = 0; i < 4; i++ ) pcibus[i] = (BYTE)b[i];
	return ok;
}
//-------------------------------------------------------------

bool vendevscan( std::string str, WORD* vendev )
{
	memset( vendev, 0, 4 );
	int b[2] = { 0,0 };
	bool ok = ( sscanf( (char*)str.c_str(), "%04x:%04x", &b[0],&b[1] ) == 2 );
	if( ok ) for( int i = 0; i < 2; i++ ) vendev[i] = (WORD)b[i];
	return ok;
}
//-------------------------------------------------------------

bool macscan( std::string str, BYTE* mac )
{
	memset( mac, 0, 6 );
	int b[6] = { 0,0,0,0,0,0 };
	bool ok = ( sscanf( (char*)str.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", &b[0],&b[1],&b[2],&b[3],&b[4],&b[5] ) == 6 );
	if( ok ) for( int i = 0; i < 6; i++ ) mac[i] = (BYTE)b[i];
	return ok;
}
//-------------------------------------------------------------

std::string getline( char* &b )
{
	if( !b ) return "";
	char* e = strchr( b, '\n' );
	std::string line = e ? std::string( b, e-b ) : std::string( b );
	b = e ? e + 1 : NULL;
	return trim( line );
}
//-------------------------------------------------------------

std::string getparamstr( char* data, std::string param )
{
	do
	{
		std::string line = getline( data );
		if( line.find( param ) != 0 ) continue;
		return trim( line.substr( param.size() ) );
	}
	while( data );
	return "";
}
std::string getparamstr( std::string data, std::string param )
{
	return getparamstr( (char*)data.c_str(), param );
}
//-------------------------------------------------------------


int getparamint( char* data, std::string param )
{
	return str2int( getparamstr( data, param ) );
}
//-------------------------------------------------------------

Tstrlist getstringvars( std::string str, std::string b, std::string e )
{
	Tstrlist list;
	std::size_t pos = 0;
	while( 1 )
	{
		std::size_t posb = str.find( b, pos );  if( posb == std::string::npos ) break;
		std::size_t pose = str.find( e, posb ); if( pose == std::string::npos ) break;
		list.push_back( str.substr( posb+b.size(), pose-posb-b.size() ) );
		pos = pose + e.size();
	}
	return list;
}
//-------------------------------------------------------------
