#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <elf.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>

#include <string>

#include <debugsystem.h>
#include <version.h>

#include "crc32.h"

using namespace std;

//-------------------------------------------------------------

TVERSION Get_version( const char* sourcepath )
{FUNCTION_TRACE
	string name = string( sourcepath ) + "/_version";
	const char* filename = name.c_str();
	Trace( "_version: %s\n", filename );

    TVERSION V;
	FILE* fp = fopen( filename, "r" );
	if( fp == NULL ) throw errException( "open %s", filename );
	char buf[ 128 ];
	while( !feof(fp) )
	{
		if( !fgets( buf, sizeof(buf), fp ) ) break;
		
		if( buf[0] != '{' ) continue;
		
		char* c1 = strchr( buf, '\"' ); if( c1 ) { c1++;        } else throw errException( "error recognize %s", filename );
		char* c2 = strchr( c1,  '\"' ); if( c2 ) { *c2=0; c2++; } else throw errException( "error recognize %s", filename );
		
		strncpy( V.name, c1, sizeof(V.name)-1 );
		sscanf( c2, ",{%d,%d},%d,%u,%lu", &V.v[0], &V.v[1], &V.build, &V.crc32, &V.time );
	}
	fclose( fp );
	return V;
}
//-------------------------------------------------------------

void ScanSources( const char* sourcepath, TVERSION& V )
{FUNCTION_TRACE
	string name = string( sourcepath ) + "/_sourcelist";
	const char* filename = name.c_str();
	Trace( "_sourcelist: %s\n", filename );
	
	FILE* fp = fopen( filename, "r" );
	if( fp == NULL ) throw errException( "open %s", filename );
	
	uint32_t sourcescrc = 0;
	time_t   sourcestime = 0;
	char     buf[ 64 ];
	string   cpp;
	
	while( !feof( fp ) )
	{
		if( !fgets( buf, sizeof(buf), fp ) ) break;
		string cpp = trim( string( buf ) );
		if( cpp.empty() ) continue;
		
		cpp = string( sourcepath ) + "/" + cpp;
		Trace( "  cpp: %s\n", cpp.c_str() );

        if( fcrc32file( sourcescrc, cpp.c_str() ) < 0 ) errException( "dxcrc32file %s", cpp.c_str() );
		Trace( "  crc: %d\n", sourcescrc );

		struct stat st;
		if( stat( cpp.c_str(), &st ) == -1 ) errException( "stat %s", cpp.c_str() );

		if( st.st_mtime > sourcestime ) sourcestime = st.st_mtime;
	}
	fclose( fp );
	V.crc32 = sourcescrc;
	V.time = sourcestime;
	return ;
}
//-------------------------------------------------------------

int GetVersionOffset( FILE* fd )
{FUNCTION_TRACE
	Elf32_Ehdr elf32hdr;
	Elf32_Shdr elf32shdr;
	Elf64_Ehdr elf64hdr;
	Elf64_Shdr elf64shdr;

	char* sectnames = NULL;
	int offset = -1;
	
	fseek( fd, 0, SEEK_SET );
	if( fread( &elf32hdr, sizeof(Elf32_Ehdr), 1, fd ) != 1 ) throw errException( "error read elf32hdr\n" );

	fseek( fd, 0, SEEK_SET );
	if( fread( &elf64hdr, sizeof(Elf64_Ehdr), 1, fd ) != 1 ) throw errException( "error read elf64hdr\n" );

	if( memcmp( elf32hdr.e_ident, ELFMAG, SELFMAG ) != 0 ) throw errException( "error ELF magic\n" );

	if( elf32hdr.e_ident[ EI_CLASS ] == ELFCLASS32 )
	{
		Trace( "ELF32 " );

		if( fseek( fd, elf32hdr.e_shoff + elf32hdr.e_shstrndx * sizeof(Elf32_Shdr) , SEEK_SET ) == -1 ) throw errException( "error seek\n" );
		if( fread( &elf32shdr, sizeof(Elf32_Shdr), 1, fd ) != 1 ) throw errException( "error read elf32shdr\n" );

		sectnames = (char*)malloc( elf32shdr.sh_size );
		if( !sectnames ) throw errException( "error malloc sectnames\n" );

		if( fseek( fd, elf32shdr.sh_offset, SEEK_SET ) == -1 ) throw errException( "error seek\n" );
		if( fread( sectnames, elf32shdr.sh_size, 1, fd ) != 1 ) throw errException( "error read sectnames\n" );
		
		for( int idx = 0; idx < elf32hdr.e_shnum; idx++ )
		{
			if( fseek( fd, elf32hdr.e_shoff + idx * sizeof(Elf32_Shdr), SEEK_SET ) == -1 ) throw errException( "error seek\n" );
			if( fread( &elf32shdr, sizeof(Elf32_Shdr), 1, fd ) != 1 ) throw errException( "error read elf32shdr\n" );
			
			char* name = sectnames + elf32shdr.sh_name;

            if( strcmp( name, "tVERSION" ) == 0 ) { offset = elf32shdr.sh_offset; break; }
		}
		free( sectnames );
	}
	
	if( elf32hdr.e_ident[ EI_CLASS ] == ELFCLASS64 )
	{
		Trace( "ELF64 " );

		if( fseek( fd, elf64hdr.e_shoff + elf64hdr.e_shstrndx * sizeof(Elf64_Shdr) , SEEK_SET ) == -1 ) throw errException( "error seek\n" );
		if( fread( &elf64shdr, sizeof(Elf64_Shdr), 1, fd ) != 1 ) throw errException( "error read elf64shdr\n" );

		sectnames = (char*)malloc( elf64shdr.sh_size );
		if( !sectnames ) throw errException( "error malloc sectnames\n" );

		if( fseek( fd, elf64shdr.sh_offset, SEEK_SET ) == -1 ) throw errException( "error seek\n" );
		if( fread( sectnames, elf64shdr.sh_size, 1, fd ) != 1 ) throw errException( "error read sectnames\n" );
		
		for( int idx = 0; idx < elf64hdr.e_shnum; idx++ )
		{
			if( fseek( fd, elf64hdr.e_shoff + idx * sizeof(Elf64_Shdr), SEEK_SET ) == -1 ) throw errException( "error seek\n" );
			if( fread( &elf64shdr, sizeof(Elf64_Shdr), 1, fd ) != 1 ) throw errException( "error read elf32shdr\n" );
			
			char* name = sectnames + elf64shdr.sh_name;

            if( strcmp( name, "tVERSION" ) == 0 ) { offset = elf64shdr.sh_offset; break; }
		}
		free( sectnames );
	}
	Trace( "offset[0x%X]\n", offset );
	return offset;
}
//-------------------------------------------------------------


int main( int argc, char **argv )
{
	g_DebugSystem.init( argc, argv, NULL, false );
	FUNCTION_TRACE

	try
	{
		if( g_DebugSystem.params.empty() ) throw errException( "incorrect using" );

		char* sourcepath = (char*)g_DebugSystem.params[ 0 ].c_str();
		bool release = g_DebugSystem.checkparam( "release" );
		
		TraceS( sourcepath );
		TraceD( release );
		
        TVERSION V, newV;
		struct tm *ts;

		newV = V = Get_version( sourcepath );
		Trace( "readed %s\n", GetVersionStr( V ) );
		ts = localtime( &V.time );
		Trace( "crc32 %u, %02d.%02d.%02d %02d:%02d:%02d\n", V.crc32, ts->tm_mday,ts->tm_mon+1,ts->tm_year+1900,ts->tm_hour,ts->tm_min,ts->tm_sec );
	
		ScanSources( sourcepath, newV );
		ts = localtime( &newV.time );
		Trace( "new crc32 %u, %02d.%02d.%02d %02d:%02d:%02d\n", newV.crc32, ts->tm_mday,ts->tm_mon+1,ts->tm_year+1900,ts->tm_hour,ts->tm_min,ts->tm_sec );

		if( release )
		{
			Log( "set release\n" );
			newV.build = -1;
		}
		else
		{
			if( V.build == -1 )
			{
				Log( "was released\n" );
				if( newV.crc32 != V.crc32 )
				{
					Warning( "sources was changed!!!, increase v, build=1\n" );
					newV.v[ 1 ]++;
					newV.build = 1;
				}
			}
			else
			{
				if( newV.crc32 != V.crc32 )
				{
					Log( "increase build\n" );
					newV.build++;
				}
				else
					Log( "sources unchanged\n" );
			}
		}
		V = newV;		
		
		//---------------------------------- write to binary
		string name = g_DebugSystem.fullpath( trim(readfilestr( string(sourcepath)+"/_bin" )) );
		const char* filename = name.c_str();
		Trace( "bin: %s\n", filename );

		FILE* binfd = fopen( filename, "r+" );
		if( binfd == NULL ) throw errException( "open %s", filename );
		int offset = GetVersionOffset( binfd );
		if( offset == -1 ) throw errException( "offset == -1" );
		if( fseek( binfd, offset, SEEK_SET ) == -1 ) throw errException( "seek %s", filename );
        if( fwrite( &V, sizeof(TVERSION), 1, binfd ) != 1 ) throw errException( "write TVERSION %s", filename );
		fclose( binfd );
	
		//---------------------------------- write _version
		name = string( sourcepath ) + "/_version";
		filename = name.c_str();
		Trace( "_version: %s\n", filename );

		FILE* vfd = fopen( filename, "w" );
		if( vfd == NULL ) throw errException( "open %s", filename );
		fprintf( vfd, "#ifndef _versionH\n" );
		fprintf( vfd, "#define _versionH\n" );
        fprintf( vfd, "#include \"version.h\"\n" );
        fprintf( vfd, "TVERSION VERSION __attribute__((section(\"tVERSION\"))) =\n" );
		fprintf( vfd, "{\"%s\",{%d,%d},%d,%uU,%luUL};\n", V.name, V.v[0], V.v[1], V.build, V.crc32, (unsigned long)V.time );
		fprintf( vfd, "#endif\n\n" );
		fclose( vfd );

		//---------------------------------- set sourses time to _version.h
		struct utimbuf zerotime = { V.time, V.time };
		if( utime( filename, &zerotime ) != 0 ) throw errException( "utime %s", filename );

		Log( "%s\n", GetVersionStr( V ) );
	}
	catch( errException& e )
	{
		Error( "%s\n", e.error() );
		return -1;
	}

	return 0;
}
//-------------------------------------------------------------------------------------------

