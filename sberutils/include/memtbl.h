#ifndef memtblH
#define memtblH
//---------------------------------------------------------------------------

//#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>

#include <string>
#include <vector>
#include <iterator>

//---------------------------------------------------------------------------

#define recNULL TtblRecord( )

//---------------------------------------------------------------------------

struct TtblHeader
{
	int memsize;
	int fields_offset;
	int data_offset;
	int fields_count;
	int record_size;
	int alloced;
	int count;
	int idnext;
	//sem_t sem;
	int locks;
	pid_t pid;
};

struct TtblField
{
	int   id;
	char  name[16];
	int   size;
	int   offset;
};

class TtblRecord;

typedef std::vector< TtblRecord > Treclist;
typedef Treclist::iterator TreclistIt;

//---------------------------------------------------------------------------

class TmemTable
{
private:
	friend class TtblRecord;	

	std::string     name;
	int             shm;
	TtblHeader*     header;
	TtblField*      fields;
	BYTE*           data;
	sem_t*          sem;

public:
    TmemTable( const char* name );
    virtual ~TmemTable( );

	std::string error;

	bool  create( TtblField* fields, int fields_count, int alloc );
	bool  destroy( );
	bool  exist( );
	bool  open( );
	bool  close( );
	bool  opened( );
	void  lock( );
	void  unlock( );
	bool  clear( );
	void  print( );
	int   count( );
	TtblField*  getfield( const char* name );
	TtblField*  getfield( int fid );
	TtblRecord  record( int i );
	TtblRecord  operator[]( int i );
	TtblRecord  first( );
	TtblRecord& next( TtblRecord& r );
	TtblRecord  add( );

	TtblRecord& find( TtblRecord& r, TtblField* field, int v );
	TtblRecord& find( TtblRecord& r, const char* name, int v );
	TtblRecord& find( TtblRecord& r, int fid, int v );
	TtblRecord& find( TtblRecord& r, TtblField* field, void* data, int size=0 );
	TtblRecord& find( TtblRecord& r, const char* name, void* data, int size=0 );
	TtblRecord& find( TtblRecord& r, int fid, void* data, int size=0 );
	TtblRecord& find( TtblRecord& r, TtblField* field, const char* str );
	TtblRecord& find( TtblRecord& r, const char* name, const char* str );
	TtblRecord& find( TtblRecord& r, int fid, const char* str );
	TtblRecord& find( TtblRecord& r, TtblField* field, std::string str );
	TtblRecord& find( TtblRecord& r, const char* name, std::string str );
	TtblRecord& find( TtblRecord& r, int fid, std::string str );

	int findcount( TtblField* field, int v );
	int findcount( const char* name, int v );
	int findcount( int fid, int v );
	int findcount( TtblField* field, void* data, int size=0 );
	int findcount( const char* name, void* data, int size=0 );
	int findcount( int fid, void* data, int size=0 );
	int findcount( TtblField* field, const char* str );
	int findcount( const char* name, const char* str );
	int findcount( int fid, const char* str );

	TtblRecord  findid( int id );
	
	void sort( TtblField* field );
	void sort( const char* name );
	void sort( int fid );
	
	int filtercallback( bool(*callback)( TtblRecord& r ), Treclist& reclist );
};
//---------------------------------------------------------------------------

#define LOCK(X) TtblLock __dxtbllock(&X)

class TtblLock
{
private:
    TmemTable* tbl;
public:
    TtblLock( TmemTable* tbl ):tbl(tbl) { tbl->lock( ); }
	~TtblLock( ) { tbl->unlock( ); }
};
//---------------------------------------------------------------------------

class TtblRecord
{
private:
    friend class TmemTable;
	
    TmemTable* table;
	BYTE*        pointer;

	TtblField*   getfield( const char* name );
	TtblField*   getfield( int fid );

public:
	TtblRecord( ): table(NULL),pointer(NULL) { }
    TtblRecord( TmemTable* table, BYTE* pointer ): table(table),pointer(pointer) { }
	~TtblRecord( ) { }
	bool  clear( );

	bool  set( TtblField* field, int v );
	bool  set( const char* name, int v );
	bool  set( int fid, int v );

	bool  set( TtblField* field, void* data, int size=0 );
	bool  set( const char* name, void* data, int size=0 );
	bool  set( int fid, void* data, int size=0 );

	bool  set( TtblField* field, const char* str );
	bool  set( const char* name, const char* str );
	bool  set( int fid, const char* str );

	bool  set( TtblField* field, std::string str );
	bool  set( const char* name, std::string str );
	bool  set( int fid, std::string str );

	int   get( TtblField* field );
	int   get( const char* name );
	int   get( int fid );

	void* get( TtblField* field, void* data, int size=0 );
	void* get( const char* name, void* data, int size=0 );
	void* get( int fid, void* data, int size=0 );

	void* ptr( TtblField* field );
	void* ptr( const char* name );
	void* ptr( int fid );

	char* str( TtblField* field );
	char* str( const char* name );
	char* str( int fid );

	TtblRecord& null( ) { table = NULL; pointer = NULL; return *this; }
	operator bool( ) { return ( pointer != NULL ); }
	friend TtblRecord& operator++( TtblRecord& r );      // prefix increment
	friend TtblRecord  operator++( TtblRecord& r, int ); // postfix increment
};

//---------------------------------------------------------------------------
#endif

