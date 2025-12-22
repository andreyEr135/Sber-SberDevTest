#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <fstream>

#include <stdio.h>

#include "debugsystem.h"
#include "memtbl.h"

#include "_version"

using namespace std;

//-------------------------------------------------------------------------------------------

bool findVideoPort( const string& port )
{
    string path = stringformat("/sys/class/drm/%s", port.c_str());
    if (!direxist( path )) throw errException("Порт %s не найден в системе", port.c_str());
    if (!fileexist(path + "/enabled")) throw errException("Не удалось найти статусный файл порта %s", port.c_str());
    string status = readfilestr( path + "/enabled" );
    Trace("%s\n", status.c_str());
    if ((status.empty()) || (trim(status) == "disabled")) throw errException("Статус порта %s неизвестен или выключен", port.c_str());
    return true;
}

//-------------------------------------------------------------------------------------------

void help( )
{FUNCTION_TRACE
    Log( "\nТест Video:\n" );
    Log( "   %s [параметры, * обязательные]\n", g_DebugSystem.exe.c_str() );
    Log( " * port     - наименование порта видео-выхода\n" );
}

//-------------------------------------------------------------------------------------------

void Test( )
{FUNCTION_TRACE
    string port       = g_DebugSystem.getparam_or_default( "port" );

    if (port.empty()) throw errException( "Не указан порт видео-выхода" );

    findVideoPort( port );

    char buf[ 256 ];
    std::string answer;
    bool ok = false;
    while( 1 )
    {
        Log( "<DIALOG>60,9{}{Изображение на дисплее %s имеется?}{Да|Нет}{Нет}\n", port.c_str() );

        if( !fgets( buf, sizeof(buf), stdin ) ) throw errException( "fgets answer" );
        answer = trim( buf );
        Trace( "answer '%s'\n", answer.c_str() );

        if( answer == "Да"  ) { ok = true;  break; } else
        if( answer == "Нет" ) { ok = false; break; } else
        continue;
    }

    if( !ok ) throw errException( "изображение отсутствует" );
}
//-------------------------------------------------------------------------------------------

int main( int argc, char* argv[] )
{
	g_DebugSystem.stdbufoff( );
	g_DebugSystem.init( argc, argv, &VERSION, true );
    FUNCTION_TRACE

	try
	{
        if( g_DebugSystem.checkparam( "-h" ) ) { help( ); return 0; }

        Test();
        Log( "TEST OK\n" );
	}
	catch( errException& e )
	{
        Log( "TEST ERR %s\n", e.error() );
	}

	return 0;
}
//-------------------------------------------------------------------------------------------

