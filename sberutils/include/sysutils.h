#ifndef sysutilsH
#define sysutilsH
//-------------------------------------------------------------

#define __STDC_FORMAT_MACROS 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
#include <dirent.h>
#include <stdlib.h>

#include <string>
#include <vector>
#include <algorithm> 

#include <filesystem>

#include <version.h>

#include <pthread.h>

//-------------------------------------------------------------

#define FormatToStr(fmt,str,size) do{ va_list va;va_start(va,fmt);vsnprintf(str,size,fmt,va);va_end(va); }while(0)

#define FOR_EACH_ITER(type, container, it) \
for (type::iterator it = (container).begin(); it != (container).end(); ++it)

#define FOR_EACH_RITER(type, container, it) \
for (type::reverse_iterator it = (container).rbegin(); it != (container).rend(); ++it)

#define FOR_EACH_TOKEN(str, delim, list, it) \
Tstrlist list; stringsplit(str, delim, list); FOR_EACH_ITER(Tstrlist, list, it)

#define MIN(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
#define MAX(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })

#define xdelete(a) do{ if(a)delete a; a=NULL; }while(0)
#define xfree(a) do{ if(a) free(a); a=NULL; }while(0)
#define xfclose(a) do{ if(a) fclose(a); a=NULL; }while(0)
#define xclose(a) do{ if(a) close(a); a=0; }while(0)

//-------------------------------------------------------------

typedef uint8_t       BYTE;
typedef uint16_t      WORD;
typedef uint32_t      DWORD;
typedef unsigned int  UINT;

typedef std::vector< std::string > Tstrlist;
typedef Tstrlist::iterator TstrlistIt;

typedef std::vector< int > Tintlist;
typedef std::vector< pid_t > Tpidlist;

extern const double pi;

//-------------------------------------------------------------

class Tdir
{
private:
	DIR *dir;
public:
    Tdir( std::string path ) { dir = opendir( path.c_str() ); }
    ~Tdir( ) { if( dir ) closedir( dir ); }
	struct dirent*	next( ) { return ( dir ? readdir( dir ) : NULL ); }
};
//-------------------------------------------------------------

#define dxfordir(d,p,e) Tdir d(p);struct dirent* e;while((e=d.next())!=NULL)

//-------------------------------------------------------------

class Tlock
{
private:
	pthread_mutex_t*  mutex;
public:
	Tlock( pthread_mutex_t* mutex ) : mutex(mutex) { pthread_mutex_lock( this->mutex ); }
	~Tlock( ) { pthread_mutex_unlock( this->mutex ); }
};
//-------------------------------------------------------------

class Tmutex
{
public:
	pthread_mutex_t  mutex;
	pthread_t lockedthread;
    Tmutex( );
    ~Tmutex( );
	bool lock( int id=0 );
	void unlock( int id=0 );
};
class Tdlock
{
private:
    Tmutex* g_mutex;
	bool locked;
	int id;
public:
    Tdlock( Tmutex* mutex, int id=0 );
    ~Tdlock( );
};
//---------------------------------------------------------------------------

#define DLOCK(m) Tdlock __lock(&m)

//---------------------------------------------------------------------------

class Tevent
{
private:
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	volatile bool flag;

public:
    Tevent( );
    ~Tevent( );
	void set( );
	void broadcast( );
	void reset( );
	void wait( bool reset=true );
};
//---------------------------------------------------------------------------

char* GetVersionStr( const TVERSION& V, bool color=true );

extern std::string trim ( const std::string &str );
extern std::string ltrim( const std::string &str );
extern std::string rtrim( const std::string &str );
extern std::string ctrim( const std::string &str, char c );
static inline int  utf8length(std::string &s) { return mbstowcs(NULL,s.c_str(),0); }
extern int         utf8indexsymbols( std::string& s, int n );
extern std::string stringformat( const char* format, ... );
extern void        stringsplit2( const std::string &s, char delim1, char delim2, Tstrlist& list, bool skipempty=false );
extern int         stringsplit( const std::string &s, char delim, Tstrlist& list, bool skipempty=false );
extern void        stringsplit( const std::string &s, char delim1, char delim2, Tstrlist& list );
extern bool        strlistcheck( Tstrlist& list, std::string str );
extern bool        stringkeyvalue( const std::string &s, char delim, std::string& key, std::string& value );
extern std::string stringupper( std::string str );
extern std::string stringlower( std::string str );
extern char*       removecolor( char* str );
extern std::string removecolor( std::string str );
extern bool        maskmatch( std::string str, std::string mask );
extern std::string specsymbols(std::string str );
extern std::string hexstr( void* buf, int size );

extern void   Usleep ( const int time );
extern char   Sign   ( const double a );
extern double Round  ( const double x );

extern bool   RunProcess( pid_t& pid, const char* command, int* inpipe = NULL, int* outpipe = NULL );
extern int    WaitProcess( const pid_t pid );
extern bool   CheckProcess( const pid_t pid, int& status );
extern int    ExecuteProcess( const char *command );

extern pid_t  RunProcessIO( const char* command, FILE* &instream, FILE* &outstream );
extern int    WaitProcessIO( pid_t pid, FILE* &instream, FILE* &outstream );
extern int    ExecuteProcessSilent( const char* command );
extern int    ExecuteProcessIO( pid_t& childpid, const char* command, FILE* &OutStream, bool(*OnInStream)( char* str, void* param ), void* param, char* resultstr=NULL );
std::string   GetLinesProcess( const char* command, int* status=NULL );
extern int    GetProcessData( std::string command, Tstrlist &data, int timeout=0 );

typedef struct { pid_t pid; timer_t timer; int sig; } Tpidtimer;
extern Tpidtimer* RunProcessIOTimeout( std::string command, FILE* &instream, FILE* &outstream, int timeoutms, int sig=SIGTERM );
extern int    WaitProcessIOTimeout( Tpidtimer* pidtimer, FILE* &instream, FILE* &outstream );
extern int    GetProcessDataTimeout( std::string command, std::string& data, int timeoutms, int sig=SIGTERM );


extern bool   WritePipe( int *pipe, const void *buf, int size );
extern int    WaitPipe ( int *pipe, int timeout );
extern int    ReadPipe ( int *pipe, void *buf, int bufsize, int timeout ); // >0-datasize, =0-timeout, <0-error

// создает отдельный поток и вызывает function, c параметром sigval; return ok=0, err=- --> errno
extern int    StartTimerNewThread( timer_t* tt, uint sec, uint micro, void(*function)( sigval sv ) );
extern int    StartTimerNewThread( timer_t* tt, uint sec, uint micro, void(*function)( sigval sv ), int svint );
extern int    StartTimerNewThread( timer_t* tt, uint sec, uint micro, void(*function)( sigval sv ), void* svptr );
extern int    StartTimerNewThread( timer_t* tt, uint sec, uint micro, void(*function)( sigval sv ), sigval sv );
// посылает сигнал signo в этот же поток, перехватывает этот сигнал function с параметром signo; return ok=0, err=- --> errno
extern int    StartTimerOwnThread( timer_t* tt, uint sec, uint micro, void(*function)( int signo ), int signo );
// остановка и удаление таймера
extern void   StopTimer( timer_t& tt );

extern timeval TimeStart( );
extern double  TimeMicro( timeval& t );
extern double  TimeMilli( timeval& t );
extern double  TimeSec  ( timeval& t );

bool fileexist( const char* filename );
bool fileexist( std::string filename );
bool direxist( const char* dirname );
bool direxist( std::string dirname );

std::string getpath( const char* filename );
std::string getpath( std::string );
std::string getfile( const char* filename );
std::string getfile( std::string );

std::string getFilenameFromPath( const char* filepath );
std::string getFilenameFromPath( std::string filename );

bool copyFile( const char* oldFilePath, const char* newFilePath );
bool copyFile( std::string oldFilePath, std::string newFilePath );

std::string readfilestr( const char* filename, bool& ok );
std::string readfilestr( std::string filename, bool& ok );
std::string readfilestr( const char* filename );
std::string readfilestr( std::string filename );

int readfileint( const char* filename, bool& ok );
int readfileint( std::string filename, bool& ok );
int readfileint( const char* filename );
int readfileint( std::string filename );

uint64_t readfileuint64( const char* filename, bool& ok );
uint64_t readfileuint64( std::string filename, bool& ok );
uint64_t readfileuint64( const char* filename );
uint64_t readfileuint64( std::string filename );

int readfilehex( const char* filename, bool& ok );
int readfilehex( std::string filename, bool& ok );
int readfilehex( const char* filename );
int readfilehex( std::string filename );

float readfilefloat( const char* filename, bool& ok );
float readfilefloat( std::string filename, bool& ok );
float readfilefloat( const char* filename );
float readfilefloat( std::string filename );

bool readfiletext( std::string filename, Tstrlist& text );

bool writefile( std::string filename, const char* format, ... );
bool appendfile( std::string filename, const char* format, ... );

bool deletefile( const char* filename );
bool deletefile( std::string filename );

bool replace( std::string& source, std::string find, std::string str );
int time2seconds( std::string time );

int str2int( const char* str, bool& ok );
int str2int( std::string str, bool& ok );
int str2int( const char* str );
int str2int( std::string str );

float str2float( const char* str, bool& ok );
float str2float( std::string str, bool& ok );
float str2float( const char* str );
float str2float( std::string str );

int str2hex( const char* str, bool& ok );
int str2hex( std::string str, bool& ok );
int str2hex( const char* str );
int str2hex( std::string str );

double KMG2double( std::string str );
double KMG2double( const char* str );
std::string double2KMG( double value, std::string suf="" );
double KMGi2double( std::string str );
double KMGi2double( const char* str );
std::string double2KMGi( double value, std::string suf="" );
std::string double2KMGr( double value, std::string suf="" );

std::string nowtime( char separator = ':' );
std::string nowdate( char separator = '-' );
std::string nowrdate( char separator = '-' );

float get_uptime( );
float delta_uptime( float& t );

pid_t getthreadpid( );

std::string getlink( std::string linkname );

std::string macstr( BYTE* mac );
std::string pcibusstr( BYTE* pcibus );
std::string vendevstr( WORD* vendev );
std::string ipstr( struct in_addr& ip );
bool pcibusscan( std::string str, BYTE* pcibus );
bool vendevscan( std::string str, WORD* vendev );
bool macscan( std::string str, BYTE* mac );

std::string getline( char* &b );
std::string getparamstr( char* data, std::string param );
std::string getparamstr( std::string data, std::string param );
int         getparamint( char* data, std::string param );

Tstrlist getstringvars( std::string str, std::string b, std::string e );

//-------------------------------------------------------------
#endif

