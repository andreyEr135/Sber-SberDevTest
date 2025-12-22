#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>

#include <fstream>

#include "sysutils.h"
#include "debugsystem.h"

using namespace std;

//---------------------------------------------------------------------------

DebugSystem g_DebugSystem;

//---------------------------------------------------------------------------

static DebugSystem::Tthread* segmfaultthread;
static DebugSystem::Tthread* abortthread;
static bool stop_event = false;
static Tevent abort_event;


void TAbort( int )
{
	if( stop_event ) return;
	stop_event = true;

	bool killself = true;

	char info[ 512 ]; *info  = 0;

	if( segmfaultthread )
	{
		if( segmfaultthread && !segmfaultthread->debugstack.empty() )
		{
            DebugSystem::Tdebugdata dd = segmfaultthread->debugstack.back( );
			snprintf( info, sizeof(info), "in %s:%d:%s() %s", getfile( dd.file ).c_str(), dd.line, dd.func, ( segmfaultthread ) ? segmfaultthread->name.c_str() : "{BADTHREAD}" );
		}
        printf( CLR( f_LIGHTRED )"<BUG> Segmentation fault %s" CLR0 "\n", info );
	}
	else
	if( abortthread )
	{
		if( abortthread && !abortthread->debugstack.empty() )
		{
            DebugSystem::Tdebugdata dd = abortthread->debugstack.back( );
			snprintf( info, sizeof(info), "in %s:%d:%s() %s", getfile( dd.file ).c_str(), dd.line, dd.func, ( abortthread ) ? abortthread->name.c_str() : "{BADTHREAD}" );
		}
        printf( CLR( f_LIGHTRED ) "<BUG> Abort %s" CLR0 "\n", info );
	}
	else
	{
        if( g_DebugSystem.OnStopSignal ) killself = (*g_DebugSystem.OnStopSignal)( /*stop_signum*/ 0 );
	}

	if( killself )
	{
        g_DebugSystem.killchilds( );
        g_DebugSystem.killself( );
	}
	abort_event.broadcast();
}
//---------------------------------------------------------------------------

void StopSignal( int signum )
{
    g_DebugSystem.stop( signum );
}
//-------------------------------------------------------------

DebugSystem::DebugSystem( )
{
	okttyattr = ( tcgetattr( STDIN_FILENO, &ttyattr ) == 0 ); errno = 0;

	exe = "";
	path = "";
	conf = NULL;


    signal( SIGPROF, TAbort );

    signal( SIGSEGV, StopSignal ); // Segmentation fault
    signal( SIGABRT, StopSignal ); // (ошибка с++)
    signal( SIGPIPE, StopSignal ); // разрыв канала
    signal( SIGHUP,  StopSignal ); // обрыв линии терминала

    signal( SIGINT,  StopSignal );
    signal( SIGTERM, StopSignal );

	OnStopSignal = NULL;
	mainthread = pthread_self( );

	logstdout = true;
	
    addunit( ""            , UNIT_DEFAULT    );
    addunit( "[sysutils]"  , UNIT_SYSUTILS   );
    addunit( "[netservice]", UNIT_NETSERVICE );
    addunit( "[config]"    , UNIT_CONFIG     );
    addunit( "[memtbl]"    , UNIT_MEMTBL     );
    addunit( "[scan]"      , UNIT_SCAN       );

	addthread( "" );

	trace = false;

    log_num = 0;

	errno = 0;
}
//-------------------------------------------------------------

DebugSystem::~DebugSystem( )
{
	OnStopSignal = NULL;
	xdelete( conf );
	deletefile( runfile );
	killchilds( );
	logfileclose( );
}
//-------------------------------------------------------------

void DebugSystem::init( int argc, char **argv, TVERSION* V, bool loadconf, int logtime )
{
	string app = getlink( stringformat( "/proc/%d/exe", getpid( ) ) );
	exe  = getfile( app );
	path = getpath( app );

	string apprun = "#RUN# " + exe;
	string confname = "./" + exe + ".conf";

    char* s = NULL;
    for( int i = 1; i < argc; i++ )
	{
		apprun += string(" ") + argv[ i ];

		string key, value;
		stringkeyvalue( argv[ i ], '=', key, value );
		
		if( key == "conf"   ) { confname = value; } else
        if( key == "log"    ) { logfileopen( g_DebugSystem.fullpath( "./" + g_DebugSystem.exe + ".log" ).c_str() ); } else
		if( key == "mute"   ) { logstdout = false; } else
		if( key == "trace"  ) { trace = true; } else
		if( key == "DEBUG"  ) { trace = true; for( size_t i = 0; i < units.size(); i++ ) units[i].debug = true; }
		else
		if( ( s = strstr( argv[ i ], "debug" ) ) != NULL )
		{
			for( size_t i = 0; i < units.size(); i++ )
				if( strcmp( s+5, units[i].name ) == 0 ) { units[i].debug = true; break; }
		}
		else
			params.push_back( argv[ i ] );
	}

	if( V ) Log( "%s\n", GetVersionStr( *V ) );

	if( loadconf )
	{
        conf = new TConf( );
        if( conf->LoadFile( fullpath(confname).c_str() ) != fOK ) { Error( "load conf '%s'\n", confname.c_str() ); exit(-1); }
		if( V ) confcheck( *V );
        if( UNIT.debug ) conf->PrintAll( );
	}
	this->logtime = logtime;

	char* localeresult = setlocale( LC_ALL, "ru_RU.UTF-8" );
	if( !localeresult ) Error( "setlocale LC_ALL=ru_RU.UTF-8\n" );

	Debug( "MAIN PID [%d/%d]\n", mainpid.pr, mainpid.th );

	errno = 0;
}
//-------------------------------------------------------------

void DebugSystem::confcheck( TVERSION& V )
{
	if( !conf ) return;
    string Vconf = conf->ReadString( "", "version" ); if( Vconf.empty() ) Vconf = "<NULL>";
	string Vexec = stringformat( "%d.%d", V.v[0],V.v[1] );
	if( Vconf == Vexec ) return;

	LogRED( "\nWrong version in .conf: %s", Vconf.c_str() );
	LogYLW( "\nVersion [%s] %s", exe.c_str(), Vexec.c_str() );
	exit( -1 );
}
//-------------------------------------------------------------

void DebugSystem::runonce( )
{
    runfile = "/run/g_DebugSystem." + g_DebugSystem.exe + ".pid";
	while( 1 )
	{
		bool ok = false;
		int pid = readfileint( runfile, ok );
		if( !ok ) break;
		if( kill( pid, 0 ) != 0 ) break;
		Usleep( 10000 );
	};
	writefile( runfile, "%d", getpid() );
}
//-------------------------------------------------------------

void DebugSystem::stop( int signum )
{
	if( signum == SIGPIPE ) logsetstdout( false );
    if( signum == SIGSEGV ) segmfaultthread = g_DebugSystem.getthread( );
    if( signum == SIGABRT ) abortthread = g_DebugSystem.getthread( );

    if( pthread_kill( g_DebugSystem.mainthread, SIGPROF ) != 0 )
	{
		printf( "ERROR: send SIGPROF to mainthread\n" );
		exit( EXIT_FAILURE );
	}

	abort_event.wait( false );
}
//---------------------------------------------------------------------------

void DebugSystem::addunit( const char* name, int id )
{
    DebugSystem::Tunit unit = { name, false };
	if( id >= (int)units.size() ) units.resize( id+1 );
	units[ id ] = unit;
}
//-------------------------------------------------------------

void DebugSystem::addthread( std::string name )
{
	if( getthread( ) ) return;
    DLOCK( mutex );
    DebugSystem::Tthread thread = { pthread_self(), getthreadpid(), name, DebugSystem::Tdebugstack(), 0 };
	threads.push_back( thread );
}
//-------------------------------------------------------------

void DebugSystem::removethread( )
{
    DLOCK( mutex );
	pthread_t id = pthread_self( );
	for( Tthreads::iterator it = threads.begin(); it != threads.end(); ++it )
		if( pthread_equal( it->id, id ) != 0 ) { threads.erase( it ); break; }
}
//-------------------------------------------------------------

DebugSystem::Tthread* DebugSystem::getthread( )
{
    DLOCK( mutex );
	pthread_t id = pthread_self( );
    DebugSystem::Tthread* thread = NULL;
	for( Tthreads::iterator it = threads.begin(); it != threads.end(); ++it )
		if( pthread_equal( it->id, id ) != 0 ) thread = &(*it);
	return thread;
}
//-------------------------------------------------------------

void DebugSystem::printthreads( )
{
    DLOCK( mutex );
	for( Tthreads::iterator it = threads.begin(); it != threads.end(); ++it )
		printf( "pid[%d] name[%s]\n", it->pid, it->name.c_str() );
}
//-------------------------------------------------------------

void DebugSystem::logsetstdout( bool value )
{
	logstdout = value;
}
//---------------------------------------------------------------------------

bool DebugSystem::logfileopen( const char* filename )
{
	if( !filename ) return false;
	logfileclose( );
	logfp = fopen( filename, "w" );
	fchmod( fileno(logfp), 0666 );
	return ( logfp != NULL );
}
//---------------------------------------------------------------------------

void DebugSystem::logfileclose( )
{
	xfclose( logfp );
}
//---------------------------------------------------------------------------

bool DebugSystem::logmessage( DebugSystem::TMessageType type, DebugSystem::Tunit& unit, const char* file, int line, const char* func, const char* format, ... )
{
    DLOCK( mutex );
	int err = errno;
	bool result = true;

    if( type != tERRNO )
	{
		string msg = "";
		char str[ 1024*4 ]; FormatToStr( format, str, sizeof(str) );

        DebugSystem::Tthread* thread = getthread( );
		if( thread )
		{
			if( strncmp( str, "<-", 2 ) == 0 ) thread->debugstep--;
			for( int i = 0; i < thread->debugstep; i++ ) msg.append("  ");
			if( strncmp( str, "->", 2 ) == 0 ) thread->debugstep++;
		}
		msg += (thread) ? thread->name : string("{BADTHREAD}");

		msg += string( unit.name );

		string debugstr = "";
        if( ( type == tERROR )||( type == tXXX ) )
		{
			char* fileshort = strrchr( (char*)file, '/' );
			fileshort = ( fileshort ) ? fileshort+1 : (char*)file;
			debugstr = stringformat( "%s:%d:%s(): ", fileshort,line,func );
		}

		switch( type )
		{
            case tLOG     : msg += string(str); break;
            case tINFO    : msg += string(CLR(f_LIGHTBLUE)) + "Info: " + CLR0 + str; break;
            case tWARNING : msg += string(CLR(f_LIGHTYELLOW)) + "Warning: " + CLR0 + str; break;
            case tTRACE   : msg += string(str); break;
            case tDEBUG   : msg += string(CLR(f_MAGENTA)) + str + CLR0; break;
            case tERROR   : msg += string(CLR(f_LIGHTRED)) + "ERROR: " + debugstr + CLR0 + str; break;
            case tERRNO   : break;
            case tXXX     : msg += string(CLR(f_CYAN)) + debugstr + CLR0 + "\n";
		}

		float uptime = get_uptime( );

		//!!!memlog.push_back { uptime, msg };
		if( logtime == LOGTIME_UPTIME  ) msg = stringformat( "[%.6f] ", uptime ) + msg;
		if( logtime == LOGTIME_DAYTIME ) msg = "[" + nowtime(':') + "] " + msg;

		if( logstdout ) result &= ( fprintf( stdout, "%s", msg.c_str() ) > 0 );
		if( logfp ) { result &= ( fprintf( logfp, "%s", removecolor(msg).c_str() ) > 0 ); fflush( logfp ); }
	}

	errno = 0;
    if( err != 0 ) logmessage( tERROR, unit, file,line,func, CLRRED("#%d: %s") " DX\n", err, strerror(err) );

	return result;
}
//---------------------------------------------------------------------------

bool DebugSystem::logmessage( DebugSystem::TMessageType type, DebugSystem::Tunit& unit, const char* file, int line, const char* func, std::string format )
{
	return logmessage( type, unit, file, line, func, format.c_str() );
}
//---------------------------------------------------------------------------

void DebugSystem::stdbufoff( )
{
	setvbuf( stdin , NULL, _IONBF, 0 );
	setvbuf( stdout, NULL, _IONBF, 0 );
	setvbuf( stderr, NULL, _IONBF, 0 );
}
//---------------------------------------------------------------------------

bool DebugSystem::checkparam( std::string key )
{
    FOR_EACH_ITER( Tstrlist, params, param )
	{
		string _key, _val; stringkeyvalue( *param, '=', _key, _val );
		if( key == _key ) return true;
	}
	return false;
}
//---------------------------------------------------------------------------

std::string DebugSystem::getparam( std::string key )
{
    FOR_EACH_ITER( Tstrlist, params, param )
	{
		string _key, _val; stringkeyvalue( *param, '=', _key, _val );
		if( key == _key ) return _val;
	}
	return "";
}
//---------------------------------------------------------------------------

std::string DebugSystem::getparams( std::string key )
{
	string data;
	bool append = false;
    FOR_EACH_ITER( Tstrlist, params, param )
	{
		if( append ) data += *param + " ";
		string _key, _val; stringkeyvalue( *param, '=', _key, _val );
		if( key == _key ) { append = true; continue; }
	}
	return trim( data );
}
//---------------------------------------------------------------------------

void DebugSystem::setparam( std::string key, std::string value )
{
    FOR_EACH_ITER( Tstrlist, params, param )
	{
		string _key, _val; stringkeyvalue( *param, '=', _key, _val );
		if( key == _key ) { *param = key + "=" + value; return; }
	}
	params.push_back( key + "=" + value );
}
//---------------------------------------------------------------------------

bool DebugSystem::checkparam_or_default( std::string key )
{
	bool value = checkparam( key );
	if( !value && conf ) value = conf->ReadInt( "DEFAULT", key );
	return value;
}
//---------------------------------------------------------------------------

std::string DebugSystem::getparam_or_default( std::string key )
{
	return getparam_or_default( "DEFAULT", key );
}
//---------------------------------------------------------------------------

std::string DebugSystem::getparam_or_default( std::string group, std::string key )
{
	string value = getparam( key );
    if( ( value == "" )&& conf ) value = conf->ReadString( group, key );
	return value;
}
//---------------------------------------------------------------------------

std::string DebugSystem::fullpath( std::string dir, std::string from )
{
	if( dir[0] != '.') return dir;
	std::string fullpath = from + "/" + dir;
	while( replace( fullpath, "/./", "/" ) );
	return fullpath;
}
std::string DebugSystem::fullpath( std::string dir ) { return fullpath( dir, path ); }

//---------------------------------------------------------------------------

void DebugSystem::killself( )
{
	deletefile( runfile );
    signal( SIGTERM, SIG_DFL );
	kill( getpid(), SIGTERM );
}
//---------------------------------------------------------------------------

void DebugSystem::getchildspid( pid_t pid, Tpidlist& list )
{
	char cmd[ 32 ]; snprintf( cmd, sizeof(cmd), "pgrep -P %d", pid );
	Tstrlist data; GetProcessData( cmd, data );
    FOR_EACH_ITER( Tstrlist, data, s )
	{
		string str = trim( *s );
		if( str.find_first_not_of( "0123456789" ) != std::string::npos ) continue;
		pid_t pid = (pid_t)str2int( str );
		list.push_back( pid );
		getchildspid( pid, list );
	}
}
//---------------------------------------------------------------------------

void DebugSystem::killchilds( pid_t pid, int sig )
{
	if( pid == 0 ) { pid = getpid(); signal( SIGTERM, SIG_IGN ); } // игнорировать, если прилетит себе же (МСВС)
	Tpidlist list; getchildspid( pid, list );
    FOR_EACH_ITER( Tpidlist, list, p ) { Trace( "kill child[%d] sig[%d]\n", *p, sig ); kill( *p, sig ); errno = 0; }
}
//---------------------------------------------------------------------------

void DebugSystem::kill_process_and_childs( pid_t pid, int sig )
{
	killchilds( pid, sig );
	Trace( "kill pid[%d] sig[%d]\n", pid, sig );
	kill( pid, sig );
	errno = 0;
}
//---------------------------------------------------------------------------


TDebugVar::TDebugVar( DebugSystem::Tunit& unit, const char* file, int line, const char* func, const char* format, ... )
 : unit(unit)
{
    DLOCK( g_DebugSystem.mutex );
	newthread = ( format != NULL );
    DebugSystem::Tdebugdata dd = { file, line, func };

    if( errno != 0 ) g_DebugSystem.logmessage( DebugSystem::tERROR,unit,dd.file,dd.line,dd.func,"before\n" );

	if( newthread )
	{
		char name[32]; FormatToStr( format,name,sizeof(name) );
        g_DebugSystem.addthread( string(name) );
		Debug( "-> " CLRGRN("start") " [%d/%d]\n", getpid(), getthreadpid() );
	}
    DebugSystem::Tthread* thread = g_DebugSystem.getthread( );
	if( thread ) thread->debugstack.push_back( dd );
    if( !newthread && unit.debug ) g_DebugSystem.logmessage( DebugSystem::tDEBUG,unit,dd.file,dd.line,dd.func,"-> %s()\n", dd.func );
}
TDebugVar::~TDebugVar( )
{
    DLOCK( g_DebugSystem.mutex );
    DebugSystem::Tdebugdata dd = { NULL,0,NULL };
    DebugSystem::Tthread* thread = g_DebugSystem.getthread( );
	if( thread && !thread->debugstack.empty() ) { dd = thread->debugstack.back( ); thread->debugstack.pop_back( ); }

    if( errno != 0 ) g_DebugSystem.logmessage( DebugSystem::tERROR,unit,dd.file,dd.line,dd.func,"after\n" );

    if( !newthread && unit.debug ) g_DebugSystem.logmessage( DebugSystem::tDEBUG,unit,dd.file,dd.line,dd.func,"<- %s()\n", dd.func );
    if( newthread ) { Debug( "<- " CLRGRN("stop") " [%d/%d]\n", getpid(), getthreadpid() ); g_DebugSystem.removethread( ); }
}
//-------------------------------------------------------------

errException::errException( )
{
	data[0]=0;
}
errException::errException( const char* format, ... )
{
	data[0]=0;
	int l = strlen( data ); FormatToStr( format, data+l, sizeof(data)-l-1 );
	errsys( );
}
errException::errException( int err, const char* format, ... )
{
	data[0]=0;
	errmy( err );
	int l = strlen( data ); FormatToStr( format, data+l, sizeof(data)-l-1 );
	errsys( );
}
errException::errException( std::string str )
{
	data[0]=0;
	int l = strlen( data ); strncpy( data+l, str.c_str(), sizeof(data)-l-1 );
	errsys( );
}
errException::errException( int err, std::string str )
{
	data[0]=0;
	errmy( err );
	int l = strlen( data ); strncpy( data+l, str.c_str(), sizeof(data)-l-1 );
	errsys( );
}
void errException::errmy( int err )
{
	code = err;
}

void errException::errsys( )
{
	if( errno == 0 ) return;
	int l = strlen( data );
	snprintf( data+l, sizeof(data)-l-1, " (#%d %s)", errno, strerror(errno) );
	errno = 0;
}
//-------------------------------------------------------------

errException2::errException2( const char* file, int line, const char* func, int err )
{
	code = err;
	whatstr = "";
	info( file,line,func );
}
errException2::errException2( const char* file, int line, const char* func, const char* format, ... )
{
	code = 0;
	char buf[512]; memset( buf,0,sizeof(buf) );
	FormatToStr( format, buf, sizeof(buf)-1 );
	whatstr = std::string( buf );
	info( file,line,func );
}
errException2::errException2( const char* file, int line, const char* func, int err, const char* format, ... )
{
	code = err;
	char buf[512]; memset( buf,0,sizeof(buf) );
	FormatToStr( format, buf, sizeof(buf)-1 );
	whatstr = std::string( buf );
	info( file,line,func );
}
errException2::errException2( const char* file, int line, const char* func, std::string str )
{
	code = 0;
	whatstr = str;
	info( file,line,func );
}
errException2::errException2( const char* file, int line, const char* func, int err, std::string str )
{
	code = err;
	whatstr = str;
	info( file,line,func );
}
void errException2::info( const char* file, int line, const char* func )
{
	if( errno != 0 ) { whatstr += stringformat( " (#%d %s)", errno, strerror(errno) ); errno = 0; }
	char* fileshort = strrchr( (char*)file, '/' );
	fileshort = ( fileshort ) ? fileshort+1 : (char*)file;
	fromstr = stringformat( "%s:%d:%s()", fileshort,line,func );
}
//-------------------------------------------------------------

#undef FUNCTION_TRACE
#define FUNCTION_TRACE

int volatile logcounter = 0;

Tlog::Tlog( Thead* head )
{FUNCTION_TRACE
	if( !head ) { Error( "Tdxlog: head == NULL" ); return; }
	this->fullsize = sizeof(Thead) + head->size;
	this->head = (Thead*)malloc( this->fullsize );
	if( !this->head ) { Error( "Tdxlog: malloc" ); return; }
	memcpy( this->head, head, this->fullsize );
    logcounter++;
}
//-------------------------------------------------------------

Tlog::Tlog( const char* format, ... )
{FUNCTION_TRACE
	char buf[ 1024 ];
	FormatToStr( format, buf, sizeof(buf) );
	DWORD size = strlen( buf );
	this->fullsize = sizeof(Thead) + size;
	this->head = (Thead*)malloc( this->fullsize );
	if( !this->head ) { Error( "Tdxlog: malloc" ); return; }
	this->head->pid = getpid();
	this->head->dxnum = 0;
	this->head->uptime = get_uptime();
	this->head->size = size;
	memcpy( this->head + 1, buf, this->head->size );
    logcounter++;
}
//-------------------------------------------------------------

Tlog::Tlog( std::string str )
{FUNCTION_TRACE
	DWORD size = str.size();
	this->fullsize = sizeof(Thead) + size;
	this->head = (Thead*)malloc( this->fullsize );
	if( !this->head ) { Error( "Tdxlog: malloc" ); return; }
	this->head->pid = getpid();
	this->head->dxnum = 0;
	this->head->uptime = get_uptime();
	this->head->size = size;
	memcpy( this->head + 1, str.c_str(), this->head->size );
    logcounter++;
}
//-------------------------------------------------------------

Tlog::~Tlog( )
{FUNCTION_TRACE
	xfree( head );
    logcounter--;
}
//-------------------------------------------------------------


