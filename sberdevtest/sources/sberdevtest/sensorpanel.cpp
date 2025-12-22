#include <stdio.h>
#include <sys/sysinfo.h>
#include <string>
#include <signal.h>
#include <unistd.h>

#include "debugsystem.h"
#include "main.h"
#include "ui.h"
#include "logpanel.h"
#include "sensorpanel.h"

using namespace std;

//-------------------------------------------------------------------------------------------

TSensorPanel* SensorPanel;

#undef  UNIT
#define UNIT  g_DebugSystem.units[ SENSORPANEL_UNIT ]
#include "log_macros.def"

//-------------------------------------------------------------------------------------------

TSensorPanel::TSensorPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color )
    : TUIPanel( ui, name, x, y, w, h, color )
{
    FUNCTION_TRACE
    keyable = false;
    m_shouldExit = false;
    m_sensorPid = -1;

    if( pthread_create( &m_monitorThread, NULL, TSensorPanel::MonitorThread, this ) != 0 )
    {
        Error( "Не удалось создать поток TSensorPanel::MonitorThread\n" );
    }
}

TSensorPanel::~TSensorPanel( )
{
    FUNCTION_TRACE
    m_shouldExit = true;

    if( m_sensorPid != -1 )
    {
        Log( "Завершение процесса сенсоров: %d\n", m_sensorPid );
        if( kill( m_sensorPid, SIGTERM ) != 0 )
        {
            Error( "Ошибка при отправке SIGTERM процессу %d\n", m_sensorPid );
        }
        pthread_join( m_monitorThread, NULL );
        Log( "Поток мониторинга завершен\n" );
    }
    m_sensorRecords.clear( );
}

//-------------------------------------------------------------------------------------------

void* TSensorPanel::MonitorThread( void* arg )
{
    DTHRD("{sensor}")
    TSensorPanel* panel = (TSensorPanel*)arg;

    string sensorCmd = g_DebugSystem.conf->ReadString( "Sensor", "0" );
    if( sensorCmd.empty() ) return NULL;

    string fullPath = g_DebugSystem.fullpath( sensorCmd );

    do
    {
        FILE *instream = NULL, *outstream = NULL;
        panel->m_sensorPid = RunProcessIO( fullPath.c_str(), instream, outstream );

        char buf[ 1024 ];
        while( instream && !feof( instream ) )
        {
            if( !fgets( buf, sizeof(buf), instream ) ) break;

            // Если в потоке обнаружен маркер ошибки
            if( strstr( buf, "<BUG>" ) )
            {
                toLog( CLR(f_RED) "%s {sensor} %s" CLR0, nowtime().c_str(), buf );
            }

            // Данные начинаются с префикса '+'
            if( buf[0] != '+' ) continue;

            {
                UILOCK; // Защищаем обращение к вектору данных
                int count = str2int( buf + 1 );
                if( count > (int)panel->m_sensorRecords.size() )
                {
                    panel->m_sensorRecords.resize( count );
                }

                for( int i = 0; i < count; ++i )
                {
                    if( !fgets( buf, sizeof(buf), instream ) ) break;

                    int id = str2int( buf );
                    char* separator = strchr( buf, ' ' );
                    if( separator && ( id < count ) )
                    {
                        panel->m_sensorRecords[ id ] = string( separator + 1 );
                    }
                }
            }
            // Уведомляем UI о необходимости перерисовки
            UI->DoLock( SENSOR_UPDATE );
        }

        int status = WaitProcessIO( panel->m_sensorPid, instream, outstream );

        if( panel->m_shouldExit ) break;

        sleep( 3 ); // Пауза перед перезапуском в случае падения процесса
        toLog( CLR(f_RED) "Перезапуск процесса сенсоров! Статус: [ %d ]" CLR0 "\n", status );

    } while( !panel->m_shouldExit );

    pthread_exit( NULL );
    return NULL;
}

//-------------------------------------------------------------------------------------------

void TSensorPanel::resize( )
{
    FUNCTION_TRACE
    // Панель датчиков обычно прижата к правому краю
    doresize( COLS - w, 1, w, LINES - 2 );
}

void TSensorPanel::draw( )
{
    FUNCTION_TRACE
    clear( 0, 0, w, h );
    setcolor( window, color );

    // Вывод текущего времени и даты в шапке панели
    mvwprintw( window, 1, 4, "%s", nowtime(':').c_str() );
    mvwprintw( window, 2, 3, "%s", nowdate('.').c_str() );

    int currentY = 3;
    for( size_t i = 0; i < m_sensorRecords.size(); i++ )
    {
        char* dataPtr = (char*)m_sensorRecords[i].c_str();
        char* colonPos = strchr( dataPtr, ':' );

        if( !colonPos ) continue;
        if( !strchr( colonPos + 1, '|' ) ) continue; // Нет активных датчиков в этой группе

        string deviceName( dataPtr, colonPos );

        setcolor( window, color );
        mvwhline( window, currentY, 0, ACS_HLINE, w );

        // Рисуем имя устройства (если не self)
        if( !deviceName.empty() && deviceName != "self" )
        {
            setcolor( window, UIGREEN );
            int len = utf8length( deviceName );
            mvwprintw( window, currentY, w - len - 1, " %s", deviceName.c_str() );
        }

        currentY++;
        char* sensorPtr = colonPos + 1;
        char* pipePos;

        // Парсим значения датчиков, разделенные '|'
        while( ( pipePos = strchr( sensorPtr, '|' ) ) != NULL )
        {
            // Обработка цветовой индикации критических значений
            if( sensorPtr[1] == '!' ) setcolor( window, UIYELLOW );
            else if( sensorPtr[1] == '#' ) setcolor( window, UIRED );
            else setcolor( window, color );

            char* nameStart = sensorPtr + 2;
            char* valStart = strchr( nameStart, ':' ); if( !valStart ) break; valStart++;
            char* unitStart = strchr( valStart, ':' ); if( !unitStart ) break; unitStart++;

            string displayStr( nameStart, unitStart - 1 );
            if( nameStart[0] == ':' ) displayStr = displayStr.substr(1);

            displayStr = trim(displayStr);
            int visualLen = utf8length( displayStr );
            if( visualLen > w ) visualLen = w;

            // Центрируем текст датчика
            mvwprintw( window, currentY++, (w - visualLen) / 2, "%s", displayStr.c_str() );
            sensorPtr = pipePos + 1;
        }

        setcolor( window, color );
        mvwhline( window, currentY, 0, ACS_HLINE, w );
    }
}

//-------------------------------------------------------------------------------------------

void TSensorPanel::Refresh( )
{
    FUNCTION_TRACE
    if( !visible ) return;
    draw( );
    uirefresh( );
}

/**
 * @brief Вывод значений датчиков в лог по маске (например, "cpu|temp")
 */
void TSensorPanel::LogSensorsByMask( std::string masks )
{
    FUNCTION_TRACE
    toLog( "\n" );

    FOR_EACH_TOKEN( masks, ' ', maskTokens, it )
    {
        Tstrlist parts;
        if( stringsplit( *it, '|', parts, false ) != 2 ) continue;

        for( size_t i = 0; i < m_sensorRecords.size(); i++ )
        {
            char* s = (char*)m_sensorRecords[i].c_str();
            char* e = strchr( s, ':' ); if( !e ) continue;

            string device( s, e );
            if( device == "self" ) device = "";
            device = trim(device);

            s = e + 1;
            char* p;
            while( ( p = strchr( s, '|' ) ) != NULL )
            {
                char* n = s + 2;
                char* v = strchr( n, ':' ); if( !v ) break; v++;
                char* w_ptr = strchr( v, ':' ); if( !w_ptr ) break; w_ptr++;

                string sensorInfo( n, w_ptr - 1 );
                if( n[0] == ':' ) sensorInfo = sensorInfo.substr(1);
                sensorInfo = trim(sensorInfo);

                // Проверка соответствия маске устройства и маске датчика
                if( maskmatch( device, parts[0] ) && maskmatch( sensorInfo, parts[1] ) )
                {
                    string out = device.empty() ? sensorInfo : (device + ": " + sensorInfo);
                    toLog( "%s\n", out.c_str() );
                }
                s = p + 1;
            }
        }
    }
}
