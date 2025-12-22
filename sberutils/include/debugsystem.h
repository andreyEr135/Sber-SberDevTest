#ifndef debugsystemH
#define debugsystemH
//---------------------------------------------------------------------------

#include  <stdio.h>
#include  <errno.h>
#include  <termios.h> 
#include  <pthread.h>

#include  <string>
#include  <exception>

#include "sysutils.h"
#include "config.h"
#include "netservice.h"

//---------------------------------------------------------------------------

#define  UNIT_DEFAULT    0
#define  UNIT_SYSUTILS   1
#define  UNIT_NETSERVICE 2
#define  UNIT_CONFIG     3
#define  UNIT_MEMTBL     4
#define  UNIT_SCAN       5
#define  UNIT_NEXT       6

#undef   UNIT
#define  UNIT  g_DebugSystem.units[ UNIT_DEFAULT ]
#include "log_macros.def"

#define  LOGTIME_OFF      0
#define  LOGTIME_UPTIME   1
#define  LOGTIME_DAYTIME  2

//---------------------------------------------------------------------------

class DebugSystem
{
private:
	bool okttyattr;
	struct termios ttyattr;
	
	int          logtime;
	bool         logstdout;
	FILE*        logfp;

	std::string  runfile;

public:
    DebugSystem( );
    ~DebugSystem( );
    void init( int argc, char **argv, TVERSION* V, bool loadconf, int logtime=LOGTIME_OFF );
    void confcheck( TVERSION& V );
	void stop( int signum );

    struct Tpid
	{
		pid_t pr;
		pid_t th;
        Tpid( ) { pr=getpid( ); th=getthreadpid(); }
	};
    Tpid         mainpid;
	pthread_t    mainthread;

    Tmutex       mutex;
    DWORD        log_num;
	std::string  exe;
	std::string  path;
	Tstrlist     params;
    TConf*     conf;
	bool (*OnStopSignal)( int signum );
	bool trace;
	typedef struct { const char* name; bool debug; } Tunit;
    std::vector< DebugSystem::Tunit > units;
	void addunit( const char* name, int id );

	typedef struct { const char* file; int line; const char* func; } Tdebugdata;
    typedef std::vector< DebugSystem::Tdebugdata > Tdebugstack;
    typedef struct { pthread_t id; int pid; std::string name; DebugSystem::Tdebugstack debugstack; int debugstep; } Tthread;
    typedef std::vector< DebugSystem::Tthread > Tthreads;
	Tthreads threads;
	void addthread( std::string name );
	void removethread( );
    DebugSystem::Tthread* getthread( );
	void printthreads( );

    enum TMessageType { tLOG, tINFO, tWARNING, tTRACE, tDEBUG, tERROR, tERRNO, tXXX };
	void logsetstdout( bool value );
	bool logfileopen( const char* filename );
	void logfileclose( );
    bool logmessage( DebugSystem::TMessageType type, DebugSystem::Tunit& unit, const char* file, int line, const char* func, const char* format, ... );
    bool logmessage( DebugSystem::TMessageType type, DebugSystem::Tunit& unit, const char* file, int line, const char* func, std::string format );

	void runonce( );
	void stdbufoff( );
	void fall( ) { int* p=0; *p=13; }
	bool checkparam( std::string key );
	std::string getparam( std::string key );
	std::string getparams( std::string key );
	void setparam( std::string key, std::string value );
	bool checkparam_or_default( std::string key );
	std::string getparam_or_default( std::string key );
	std::string getparam_or_default( std::string group, std::string key );
	std::string fullpath( std::string dir, std::string from );
	std::string fullpath( std::string dir );
	void getchildspid( pid_t pid, Tpidlist& list );
	void killself( );
	void killchilds( pid_t pid=0, int sig=SIGTERM );
	void kill_process_and_childs( pid_t pid, int sig=SIGTERM );
};
extern DebugSystem g_DebugSystem;

//-------------------------------------------------------------

class TDebugVar
{
protected:
    DebugSystem::Tunit& unit;
	bool newthread;
public:
    TDebugVar( DebugSystem::Tunit& unit, const char* file, int line, const char* func, const char* format, ... );
    ~TDebugVar( );
};
//---------------------------------------------------------------------------

class errException
{
private:
	char data[512];
	void errmy( int err );
	void errsys( );

public:
    errException( );
    errException( const char* format, ... );
    errException( int errmy, const char* format, ... );
    errException( std::string str );
    errException( int errmy, std::string str );
	char* error( ) { return data; }
	int   code;
};
//-------------------------------------------------------------

class errException2
{
private:
	std::string fromstr;
	std::string whatstr;
	void info( const char* file, int line, const char* func );

public:
    errException2( const char* file, int line, const char* func, int code );
    errException2( const char* file, int line, const char* func, const char* format, ... );
    errException2( const char* file, int line, const char* func, int code, const char* format, ... );
    errException2( const char* file, int line, const char* func, std::string str );
    errException2( const char* file, int line, const char* func, int code, std::string str );
	int   code;
	char* from( ) { return (char*)fromstr.c_str(); }
	char* what( ) { return (char*)whatstr.c_str(); }
};
//-------------------------------------------------------------
#define THROW(ARGS...) throw errException2(__FILE__,__LINE__,__func__,##ARGS)
#define CATCH catch(exception& e){Error("c++ %s\n",e.what());::exit(-1);}catch(errException2 &e)
#define ERROR Log( CLR(f_LIGHTRED) "ERROR: %s:" CLR0 " %s\n",e.from(),e.what())
//-------------------------------------------------------------

extern int volatile logcounter;

class Tlog
{
public:
	struct Thead { DWORD pid; DWORD dxnum; float uptime; DWORD size; };
	Thead* head;
	int fullsize;
    Tlog( Thead* head );
    Tlog( const char* format, ... );
    Tlog( std::string str );
    ~Tlog( );
};
//---------------------------------------------------------------------------

#define   f_BLACK         "30"
#define   f_RED           "31"
#define   f_GREEN         "32"
#define   f_YELLOW        "33"
#define   f_BLUE          "34"
#define   f_MAGENTA       "35"
#define   f_CYAN          "36"
#define   f_GRAY          "37"
#define   f_DARKGRAY      "01;30"
#define   f_LIGHTRED      "01;31"
#define   f_LIGHTGREEN    "01;32"
#define   f_LIGHTYELLOW   "01;33"
#define   f_LIGHTBLUE     "01;34"
#define   f_LIGHTMAGENTA  "01;35"
#define   f_LIGHTCYAN     "01;36"
#define   f_WHITE         "01;37"
#define   b_BLACK         "40"
#define   b_RED           "41"
#define   b_GREEN         "42"
#define   b_YELLOW        "43"
#define   b_BLUE          "44"
#define   b_MAGENTA       "45"
#define   b_CYAN          "46"
#define   b_GRAY          "47"
#define   b_DARKGRAY      "02;40"
#define   b_LIGHTRED      "02;41"
#define   b_LIGHTGREEN    "02;42"
#define   b_LIGHTYELLOW   "02;43"
#define   b_LIGHTBLUE     "02;44"
#define   b_LIGHTMAGENTA  "02;45"
#define   b_LIGHTCYAN     "02;46"
#define   b_WHITE         "02;47"

#define CLR(c)            "\033[" c "m"
#define CLR0              "\033[m"

#define CLRRED(a)  "" CLR(f_RED) a CLR0
#define CLRGRN(a)  "" CLR(f_GREEN) a CLR0
#define CLRYLW(a)  "" CLR(f_YELLOW) a CLR0
#define CLRCYN(a)  "" CLR(f_CYAN) a CLR0
#define CLRMGN(a)  "" CLR(f_MAGENTA) a CLR0
#define CLRBLU(a)  "" CLR(f_BLUE) a CLR0
#define CLRLRED(a) "" CLR(f_LIGHTRED) a CLR0
#define CLRLGRN(a) "" CLR(f_LIGHTGREEN) a CLR0
#define CLRLYLW(a) "" CLR(f_LIGHTYELLOW) a CLR0
#define CLRLCYN(a) "" CLR(f_LIGHTCYAN) a CLR0
#define CLRLMGN(a) "" CLR(f_LIGHTMAGENTA) a CLR0
#define CLRLBLU(a) "" CLR(b_LIGHTBLUE) a CLR0

#define LogRED(TXT,ARGS...)  Log( CLRRED( TXT ) "\n", ##ARGS)
#define LogGRN(TXT,ARGS...)  Log( CLRGRN( TXT ) "\n", ##ARGS)
#define LogYLW(TXT,ARGS...)  Log( CLRYLW( TXT ) "\n", ##ARGS)
#define LogCYN(TXT,ARGS...)  Log( CLRCYN( TXT ) "\n", ##ARGS)
#define LogMGN(TXT,ARGS...)  Log( CLRMGN( TXT ) "\n", ##ARGS)
#define LogBLU(TXT,ARGS...)  Log( CLRBLU( TXT ) "\n", ##ARGS)
#define LogLRED(TXT,ARGS...) Log( CLRLRED( TXT ) "\n", ##ARGS)
#define LogLGRN(TXT,ARGS...) Log( CLRLGRN( TXT ) "\n", ##ARGS)
#define LogLYLW(TXT,ARGS...) Log( CLRLYLW( TXT ) "\n", ##ARGS)
#define LogLCYN(TXT,ARGS...) Log( CLRLCYN( TXT ) "\n", ##ARGS)
#define LogLBLU(TXT,ARGS...) Log( CLRLBLU( TXT ) "\n", ##ARGS)

//---------------------------------------------------------------------------
#endif

