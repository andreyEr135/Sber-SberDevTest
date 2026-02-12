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

/*bool findVideoPort( const string& port )
{
    string path = stringformat("/sys/class/drm/%s", port.c_str());
    if (!direxist( path )) throw errException("Порт %s не найден в системе", port.c_str());
    if (!fileexist(path + "/enabled")) throw errException("Не удалось найти статусный файл порта %s", port.c_str());
    string status = readfilestr( path + "/enabled" );
    Trace("%s\n", status.c_str());
    if ((status.empty()) || (trim(status) == "disabled")) throw errException("Статус порта %s неизвестен или выключен", port.c_str());
    return true;
}*/

bool findVideoPort(const std::string& prefix)
{
    std::string drmPath = "/sys/class/drm";

    // Проверяем существование базовой директории DRM
    if (!direxist(drmPath)) {
        throw errException("Директория DRM (%s) не найдена в системе.", drmPath.c_str());
    }

    DIR* dir = opendir(drmPath.c_str());
    if (!dir) {
        throw errException("Не удалось открыть директорию %s для сканирования.", drmPath.c_str());
    }

    struct dirent* entry;
    std::vector<std::string> matchingPorts;

    // Шаг 1: Сканируем директорию /sys/class/drm и ищем подходящие имена
    while ((entry = readdir(dir)) != nullptr) {
        std::string portName = entry->d_name;
        // Проверяем, начинается ли имя порта с заданного префикса
        if (portName.rfind(prefix, 0) == 0) { // rfind(prefix, 0) проверяет начало строки
            matchingPorts.push_back(portName);
        }
    }
    closedir(dir);

    // Если ни одного порта, соответствующего префиксу, не найдено
    if (matchingPorts.empty()) {
        throw errException("Видеопорты, соответствующие маске '%s*', не найдены в системе.", prefix.c_str());
    }

    // Шаг 2: Проверяем статус каждого найденного порта
    for (const std::string& portName : matchingPorts) {
        std::string portPath = drmPath + "/" + portName;
        std::string enabledFilePath = portPath + "/enabled";

        // Проверяем существование файла статуса
        if (!fileexist(enabledFilePath)) {
            Trace("Предупреждение: для порта %s не найден статусный файл '%s'. Пропускаем.\n",
                  portName.c_str(), enabledFilePath.c_str());
            continue; // Пропускаем этот порт, если нет файла "enabled"
        }

        std::string status = readfilestr(enabledFilePath);
        Trace("Порт %s, статус: '%s'\n", portName.c_str(), status.c_str());

        // Проверяем статус: если не пустой и не "disabled" (после обрезки пробелов)
        if (!status.empty() && trim(status) != "disabled") {
            Trace("Сигнал обнаружен на порту: %s\n", portName.c_str());
            return true; // Нашли активный порт, возвращаем true
        }
    }

    // Если дошли сюда, значит, ни один из подходящих портов не активен
    throw errException("Статус порта %s неизвестен или выключен", prefix.c_str());
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

