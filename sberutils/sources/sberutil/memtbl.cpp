//#define _GNU_SOURCE

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "debugsystem.h"

#include "memtbl.h"

using namespace std;

//---------------------------------------------------------------------------

#undef  UNIT
#define UNIT  g_DebugSystem.units[ UNIT_MEMTBL ]
#include "log_macros.def"

//---------------------------------------------------------------------------

TmemTable::TmemTable( const char* name )
{
	this->name = string( name );
	shm = 0;
	header = NULL;
	fields = NULL;
	data = NULL;
}
//---------------------------------------------------------------------------

TmemTable::~TmemTable( )
{
	close( );
}
//---------------------------------------------------------------------------

bool TmemTable::create( TtblField* fields, int fields_count, int alloc )
{FUNCTION_TRACE
	try
	{
		if( !fields ) throw errException( "%s: incorrect fields", name.c_str() );
		if( fields_count <= 0 ) throw errException( "%s: incorrect fields count", name.c_str() );
		if( alloc <= 0 ) throw errException( "%s: incorrect alloc", name.c_str() );
		
		TtblField f0 = { 0, "id", 4, 0 };
		int record_size = f0.size;
		fields_count += 1;
		for( TtblField* f = fields; f->id; f++ )
		{
			if( ( f->id <= 0 )||(f->id >= fields_count ) ) throw errException( "%s: field '%s' incorrect id=%d", name.c_str(), f->name, f->id );
			record_size += f->size;
		}
		int fields_size = fields_count * sizeof(TtblField);

		int memsize = sizeof( TtblHeader ) + fields_size + alloc*record_size;

		if( ( shm = shm_open( name.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0777 ) ) == -1 )
			throw errException( "%s: shm_open", name.c_str() );
			
		if( fchmod( shm, 0777 ) == -1 ) throw errException( "%s: fchmod", name.c_str() );

		if( ftruncate( shm, memsize ) == -1 )
			throw errException( "%s: ftruncate %d bytes", name.c_str(), memsize );
		
		void* addr = NULL;
		if( ( addr = mmap( 0, memsize, PROT_WRITE|PROT_READ, MAP_SHARED, shm, 0 ) ) == MAP_FAILED )
			throw errException( "%s: mmap %d bytes", name.c_str(), memsize );

		memset( addr, 0, memsize );

		int fields_offset = sizeof(TtblHeader);
		int data_offset   = fields_offset + fields_size;
		
		TtblHeader* H = (TtblHeader*)addr;
		H->memsize       = memsize;
		H->fields_count  = fields_count;
		H->record_size   = record_size;
		H->fields_offset = fields_offset;
		H->data_offset   = data_offset;
		H->alloced       = alloc;
		H->count         = 0;
		H->idnext        = 0;
		H->locks         = 0;
		H->pid           = 0;

		TtblField* F = (TtblField*)( (char*)addr + fields_offset );
		F[ 0 ] = f0;
		for( TtblField* f = fields; f->id; f++ ) F[ f->id ] = *f;
		for( int i = 0, offset = 0; i < fields_count; i++ )
		{
			F[ i ].offset = offset;
			offset += F[ i ].size;
		}

		error = "";
		Trace( "%s: create\n", name.c_str() );
	}
	catch( errException& e )
	{
		error = e.error();
		return false;
	}
	return true;
}
//---------------------------------------------------------------------------

bool TmemTable::destroy( )
{FUNCTION_TRACE
	try
	{
		if( shm_unlink( name.c_str() ) == -1 ) throw errException( "%s: shm_unlink", name.c_str() );
		error = "";                
		Trace( "%s: destroy\n", name.c_str() );
	}
	catch( errException& e )
	{
		error = e.error();
		return false;
	}
	return true;
}
//---------------------------------------------------------------------------

bool TmemTable::exist( )
{FUNCTION_TRACE
	int _shm = shm_open( name.c_str(), O_RDONLY, 0777 );
	if( _shm == -1 ) { errno=0; return false; }
	::close( _shm );
	return true;
}
//---------------------------------------------------------------------------


bool TmemTable::open( )
{FUNCTION_TRACE
	if( opened( ) ) close( );
	try
	{	
		if( ( shm = shm_open( name.c_str(), O_RDWR, 0777 ) ) == -1 )
			throw errException( "%s: shm_open", name.c_str() );

		void* addr = NULL;
		if( ( addr = mmap( 0, sizeof( TtblHeader ), PROT_WRITE|PROT_READ, MAP_SHARED, shm, 0 ) ) == MAP_FAILED )
			throw errException( "%s: mmap header", name.c_str() );

		int memsize = ((TtblHeader*)addr)->memsize;
		
		if( munmap( addr, sizeof( TtblHeader ) ) == -1 )
			throw errException( "%s: munmap header", name.c_str() );

		if( ( addr = mmap( 0, memsize, PROT_WRITE|PROT_READ, MAP_SHARED, shm, 0 ) ) == MAP_FAILED )
			throw errException( "%s: mmap %d bytes", name.c_str(), memsize );
		
		header   = (TtblHeader*)addr;
		fields   = (TtblField*)( (BYTE*)addr + header->fields_offset );
		data     = (BYTE*)addr + header->data_offset;

		//sem = sem_open( name.c_str(), 0 );
		//if( sem == SEM_FAILED ) throw errException( "%s: sem_open", name.c_str() );

		error = "";
		Debug( "%s: open\n", name.c_str() );
	}
	catch( errException& e )
	{
		error = e.error();
		return false;
	}
	return true;
}
//---------------------------------------------------------------------------

bool TmemTable::close( )
{FUNCTION_TRACE
	try
	{	
		if( opened( ) )
		{
			if( munmap( (void*)header, header->memsize ) == -1 ) throw errException( "%s munmap", name.c_str() );
			if( ::close( shm ) == -1 ) throw errException( "%s close shm", name.c_str() );
			//if( sem_close( sem ) == -1 ) throw errException( "%s close sem", name.c_str() );

			Debug( "%s: close\n", name.c_str() );
		}
		shm      = 0;
		header   = NULL;
		fields   = NULL;
		data     = NULL;
		error    = "";                
	}
	catch( errException& e )
	{
		error = e.error();
		return false;
	}
	return true;
}
//---------------------------------------------------------------------------

bool TmemTable::opened( )
{
	return ( header && fields && data );
}
//---------------------------------------------------------------------------

void TmemTable::lock( )
{FUNCTION_TRACE

}
//---------------------------------------------------------------------------

void TmemTable::unlock( )
{FUNCTION_TRACE

}
//---------------------------------------------------------------------------

bool TmemTable::clear( )
{FUNCTION_TRACE
	if( !opened() ) return false;
	LOCK( *this );
	header->count = 0;
	header->idnext = 0;
	return true;
}
//---------------------------------------------------------------------------

TtblField* TmemTable::getfield( const char* name )
{
	if( !opened() ) return NULL;
	for( int i = 0; i < header->fields_count; i++ )
		if( strcmp( fields[i].name, name ) == 0 ) return &fields[i];
	return NULL;
}
//---------------------------------------------------------------------------

TtblField* TmemTable::getfield( int fid )
{
	if( !opened() ) return NULL;
	if( ( fid < 0 )||( fid >= header->fields_count ) ) return NULL;
	return fields + fid;
}
//---------------------------------------------------------------------------

void TmemTable::print( )
{FUNCTION_TRACE
	if( !opened() ) return;
	lock( );
	printf( "--- %s ---\n", name.c_str() );
	printf( "memsize[%d]: H[%d] F[%d] D[%d]\n", header->memsize, (int)sizeof(TtblHeader), (int)sizeof(TtblField)*header->fields_count, header->record_size*header->alloced );
	printf( "recsize[%d] Alloc[%d] Count[%d] ID[%d]\n", header->record_size, header->alloced, header->count, header->idnext );

	for( int i = 0; i < header->fields_count; i++ )
		printf( "  %2d: %s: %d %d\n", i, fields[i].name, fields[i].size, fields[i].offset );
	
	for( int r = 0; r < header->count; r++ )
	{
		printf( "%2d: ", r );
		for( int b = 0; b < header->record_size; b++ )
		{
			BYTE v = *( data + r*header->record_size + b );
			printf( "%02X ", v );
		}
		printf( "|\n" );
	}
	unlock( );
}
//---------------------------------------------------------------------------

int TmemTable::count( )
{
	if( !opened() ) return -1;
	return header->count;
}
//---------------------------------------------------------------------------

TtblRecord TmemTable::record( int i )
{FUNCTION_TRACE
	if( !opened() ) return recNULL;
	if( ( i < 0 )||( i >= header->count ) ) return recNULL;
	return TtblRecord( this, data + header->record_size * i );
}
//---------------------------------------------------------------------------

TtblRecord TmemTable::operator[]( int i )
{
	return record( i );
}
//---------------------------------------------------------------------------

TtblRecord TmemTable::first( )
{FUNCTION_TRACE
	if( count( ) <= 0 ) return recNULL;
	return TtblRecord( this, data );
}
//---------------------------------------------------------------------------

TtblRecord& TmemTable::next( TtblRecord& r )
{FUNCTION_TRACE
	if( !opened() ) return r.null( );
	r.pointer = r ? (r.pointer + header->record_size) : data;
	int i = ( r.pointer - data ) / header->record_size;
	if( ( i < 0 )||( i >= header->count ) ) return r.null( );
	return r;
}
//---------------------------------------------------------------------------

TtblRecord TmemTable::add( )
{FUNCTION_TRACE
	try
	{	
		if( !opened( ) ) throw errException( "%s closed" );
		
		while( header->count >= header->alloced )
		{
			int memsize      = header->memsize;
			int newalloc     = header->alloced * 2;		
			int fields_count = header->fields_count;
			int record_size  = header->record_size;	
			Debug( "%s resize %d\n", name.c_str(), newalloc );

			if( munmap( (void*)header, memsize ) == -1 ) throw errException( "%s munmap", name.c_str() );
		
			memsize = sizeof( TtblHeader ) + fields_count*sizeof(TtblField) + newalloc*record_size;
			
			if( ftruncate( shm, memsize ) == -1 )
				throw errException( "%s ftruncate %d bytes", name.c_str(), header->memsize );

			void* addr = NULL;
			if( ( addr = mmap( 0, memsize, PROT_WRITE|PROT_READ, MAP_SHARED, shm, 0 ) ) == MAP_FAILED )
				throw errException( "%s mmap %d bytes", name.c_str(), memsize );

			header = (TtblHeader*)addr;
			header->memsize = memsize;
			header->alloced = newalloc;
			
			fields = (TtblField*)( (BYTE*)addr + header->fields_offset );
			data = ( (BYTE*)addr + header->data_offset );
		}
		
		TtblRecord r( this, data + header->record_size * header->count );
		r.clear( );
		r.set( 0, header->idnext++ );
		header->count++;
		error = "";
		return r;
	}
	catch( errException& e )
	{
		error = e.error();
	}
	return recNULL;
}
//---------------------------------------------------------------------------

TtblRecord& TmemTable::find( TtblRecord& r, TtblField* field, int v )
{FUNCTION_TRACE
	if( !opened( ) ) return r.null();
	if( !field ) return r.null();
	r.table = this; 
	LOCK( *this );
	for( ++r; r; ++r )
		if( r.get( field ) == v ) return r;
	return r.null();
}
TtblRecord& TmemTable::find( TtblRecord& r, const char* name, int v ) { return find( r, getfield( name ), v ); }
TtblRecord& TmemTable::find( TtblRecord& r, int fid, int v ) { return find( r, getfield( fid ), v ); }

//---------------------------------------------------------------------------

TtblRecord& TmemTable::find( TtblRecord& r, TtblField* field, void* data, int size )
{FUNCTION_TRACE
	if( !opened( ) ) return r.null();
	if( !field ) return r.null();
	if( size <= 0 ) size = field->size;
	r.table = this; 
	LOCK( *this ); 
	for( ++r; r; ++r )
		if( memcmp( r.ptr( field ), data, MIN( size, field->size ) ) == 0 ) return r;
	return r.null();
}
TtblRecord& TmemTable::find( TtblRecord& r, const char* name, void* data, int size ) { return find( r, getfield( name ), data, size ); }
TtblRecord& TmemTable::find( TtblRecord& r, int fid, void* data, int size ) { return find( r, getfield( fid ), data, size ); }

//---------------------------------------------------------------------------

TtblRecord& TmemTable::find( TtblRecord& r, TtblField* field, const char* str )
{FUNCTION_TRACE
	if( !opened( ) ) return r.null();
	if( !field ) return r.null();
	r.table = this; 
	LOCK( *this ); 
	for( ++r; r; ++r )
		if( strcmp( r.str( field ), str ) == 0 ) return r;
	return r.null();
}
TtblRecord& TmemTable::find( TtblRecord& r, const char* name, const char* str ) { return find( r, getfield( name ), str ); }
TtblRecord& TmemTable::find( TtblRecord& r, int fid, const char* str ) { return find( r, getfield( fid ), str ); }

//---------------------------------------------------------------------------

TtblRecord& TmemTable::find( TtblRecord& r, TtblField* field, std::string str )
{FUNCTION_TRACE
	if( !opened( ) ) return r.null();
	if( !field ) return r.null();
	r.table = this; 
	LOCK( *this ); 
	for( ++r; r; ++r )
		if( strcmp( r.str( field ), str.c_str() ) == 0 ) return r;
	return r.null();
}
TtblRecord& TmemTable::find( TtblRecord& r, const char* name, std::string str ) { return find( r, getfield( name ), str ); }
TtblRecord& TmemTable::find( TtblRecord& r, int fid, std::string str ) { return find( r, getfield( fid ), str ); }

//---------------------------------------------------------------------------

int TmemTable::findcount( TtblField* field, int v )
{FUNCTION_TRACE
	int count = 0;
	for( TtblRecord r; find( r, field, v ); count++ ) ;
	return count;
}
int TmemTable::findcount( const char* name, int v ) { return findcount( getfield( name ), v ); }
int TmemTable::findcount( int fid, int v ) { return findcount( getfield( fid ), v ); }

//---------------------------------------------------------------------------

int TmemTable::findcount( TtblField* field, void* data, int size )
{FUNCTION_TRACE
	int count = 0;
	for( TtblRecord r; find( r, field, data, size ); count++ ) ;
	return count;
}
int TmemTable::findcount( const char* name, void* data, int size ) { return findcount( getfield( name ), data, size ); }
int TmemTable::findcount( int fid, void* data, int size ) { return findcount( getfield( fid ), data, size ); }

//---------------------------------------------------------------------------

int TmemTable::findcount( TtblField* field, const char* str )
{FUNCTION_TRACE
	int count = 0;
	for( TtblRecord r; find( r, field, str ); count++ ) ;
	return count;
}
int TmemTable::findcount( const char* name, const char* str ) { return findcount( getfield( name ), str ); }
int TmemTable::findcount( int fid, const char* str ) { return findcount( getfield( fid ), str ); }

//---------------------------------------------------------------------------

TtblRecord TmemTable::findid( int id )
{
	TtblRecord r;
	find( r, 0, id );
	return r;
}
//---------------------------------------------------------------------------

void TmemTable::sort( TtblField* field )
{FUNCTION_TRACE
	if( !opened( ) ) return;
	if( !field ) return;
	LOCK( *this );
	
	int& rsize = header->record_size;
	int& fsize = field->size;
	int& foffset = field->offset;
	int& count = header->count;
	BYTE* tmp = (BYTE*)malloc( rsize );
	
	BYTE* r = data;
	for( int j = 0; j < count-1; ++j, r += rsize )
	{
		BYTE* r1 = data;
		BYTE* r2 = r1 + rsize;
		for( int i = 0; i < count-j-1; ++i, r1 += rsize, r2 += rsize )
		{
			if( memcmp( r1 + foffset, r2 +foffset, fsize ) > 0 )
			{
				memcpy( tmp, r1, rsize );
				memcpy( r1, r2, rsize );
				memcpy( r2, tmp, rsize );
			}
		}
	}
	free( tmp );
}
void TmemTable::sort( const char* name ) { sort( getfield( name ) ); }
void TmemTable::sort( int fid ) { sort( getfield( fid ) ); }

//---------------------------------------------------------------------------

int TmemTable::filtercallback( bool(*callback)( TtblRecord& r ), Treclist& reclist )
{FUNCTION_TRACE
	for( TtblRecord r = first( ); r; ++r )
	{
		bool f = true;
		if( callback ) f = (*callback)( r );
		if( f ) reclist.push_back( r );
	}
	return reclist.size();
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------

TtblField* TtblRecord::getfield( const char* name )
{
	if( !table || !pointer ) return NULL;
	return table->getfield( name ); 
}
TtblField* TtblRecord::getfield( int fid )
{
	if( !table || !pointer ) return NULL;
	return table->getfield( fid ); 
}
//---------------------------------------------------------------------------

bool TtblRecord::clear( )
{
	if( !table || !pointer ) return false;
	if( !table->header ) return false;
	memset( pointer, 0, table->header->record_size );
	return true;
}
//---------------------------------------------------------------------------

bool TtblRecord::set( TtblField* field, int v )
{
	if( !field ) return false;
	memset( pointer + field->offset, 0, field->size );
	memcpy( pointer + field->offset, &v, MIN( (int)sizeof(int), field->size ) );
	return true;
}
bool TtblRecord::set( const char* name, int v ) { return set( getfield( name ), v ); }
bool TtblRecord::set( int fid, int v )  { return set( getfield( fid ), v ); }

//---------------------------------------------------------------------------

bool TtblRecord::set( TtblField* field, void* data, int size )
{
	if( !field ) return false;
	if( size <= 0 ) size = field->size;
	memset( pointer + field->offset, 0, field->size );
	memcpy( pointer + field->offset, data, MIN( size, field->size ) );
	return true;
}
bool TtblRecord::set( const char* name, void* data, int size ) { return set( getfield( name ), data, size ); }
bool TtblRecord::set( int fid, void* data, int size )  { return set( getfield( fid ), data, size ); }

//---------------------------------------------------------------------------

bool TtblRecord::set( TtblField* field, const char* str )
{
	if( !field ) return false;
	memset( pointer + field->offset, 0, field->size );
	strncpy( (char*)( pointer + field->offset ), str, field->size-1 );
	return true;
}
bool TtblRecord::set( const char* name, const char* str ) { return set( getfield( name ), str ); }
bool TtblRecord::set( int fid, const char* str ) { return set( getfield( fid ), str ); }

//---------------------------------------------------------------------------

bool TtblRecord::set( TtblField* field, std::string str )
{
	if( !field ) return false;
	memset( pointer + field->offset, 0, field->size );
	strncpy( (char*)( pointer + field->offset ), str.c_str(), field->size-1 );
	return true;
}
bool TtblRecord::set( const char* name, std::string str ) { return set( getfield( name ), str ); }
bool TtblRecord::set( int fid, std::string str ) { return set( getfield( fid ), str ); }

//---------------------------------------------------------------------------

int  TtblRecord::get( TtblField* field )
{
	if( !field ) return -1;
	int v = 0;
	memcpy( &v, pointer + field->offset, MIN( (int)sizeof(int), field->size ) );
	return v;
}
int  TtblRecord::get( const char* name ) { return get( getfield( name ) ); }
int  TtblRecord::get( int fid ) { return get( getfield( fid ) ); }

//---------------------------------------------------------------------------

void* TtblRecord::get( TtblField* field, void* data, int size )
{
	if( !field ) return NULL;
	if( size <= 0 ) size = field->size;
	memcpy( data, pointer + field->offset, MIN( size, field->size ) );
	return data;
}
void* TtblRecord::get( const char* name, void* data, int size ) { return get( getfield( name ), data, size ); }
void* TtblRecord::get( int fid, void* data, int size ) { return get( getfield( fid ), data, size ); }

//---------------------------------------------------------------------------

void* TtblRecord::ptr( TtblField* field )
{
	if( !field ) return NULL;
	return pointer + field->offset;
}
void* TtblRecord::ptr( const char* name ) { return ptr( getfield( name ) ); }
void* TtblRecord::ptr( int fid ) { return ptr( getfield( fid ) ); }

//---------------------------------------------------------------------------

char* TtblRecord::str( TtblField* field ) { return (char*)ptr( field ); }
char* TtblRecord::str( const char* name ) { return str( getfield( name ) ); }
char* TtblRecord::str( int fid ) { return str( getfield( fid ) ); }

//---------------------------------------------------------------------------

TtblRecord& operator++( TtblRecord& r )
{FUNCTION_TRACE
	if( !r.table ) return r.null( );
	return r.table->next( r );
}
TtblRecord operator++( TtblRecord& r, int )
{FUNCTION_TRACE
	if( !r.table ) return r.null( );
	TtblRecord old = r;
	r.table->next( r );
	return old;
}
//---------------------------------------------------------------------------




