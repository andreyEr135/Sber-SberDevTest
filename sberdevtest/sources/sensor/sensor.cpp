#include "debugsystem.h"
#include "sysutils.h"
#include "memtbl.h"
#include "devscan.h"
#include "_version"

using namespace std;

//-------------------------------------------------------------------------------------------

// Глобальные таблицы (базы данных в памяти)
TmemTable g_DeviceTable( SENSORDEV_TBL );
TmemTable g_MetricTable( SENSOR_TBL );

// Структура для описания запущенной задачи
struct TaskContext {
    std::string command;
    pthread_t thread_id;
    pid_t process_id;
};

typedef std::vector< TaskContext > TaskList;
TaskList g_ActiveTasks;

bool g_IsShutdownRequested = false;

//-------------------------------------------------------------------------------------------

/**
 * @brief Создание структуры таблиц датчиков
 */
int CreateSensorTables()
{
    FUNCTION_TRACE
    int status = 0;
    try
    {
        TtblField deviceFields[] =
        {
            { SENSORDEV_NAME, "name", 16 },
            { 0 }
        };
        if( !g_DeviceTable.create( deviceFields, SENSORDEV_COUNT, 16 ) )
            throw errException( g_DeviceTable.error );

        TtblField metricFields[] =
        {
            { SENSOR_DEVID, "devid",  4 },
            { SENSOR_TYPE,  "type",   1 },
            { SENSOR_NAME,  "name",  16 },
            { SENSOR_VALUE, "value", 16 },
            { SENSOR_FLAG,  "flag",   1 },
            { SENSOR_TIME,  "time",   8 },
            { SENSOR_WIDTH, "width",  1 },
            { 0 }
        };
        if( !g_MetricTable.create( metricFields, SENSOR_COUNT, 64 ) )
            throw errException( g_MetricTable.error );
    }
    catch( errException& ex )
    {
        Error( "Table Init Error: %s\n", ex.error() );
        status = -1;
    }
    return status;
}

/**
 * @brief Обновление данных о датчике в таблицах
 */
int UpdateSensorMetric( std::string deviceName, char sensorType, std::string metricName,
                        std::string value, char stateFlag, time_t timeStamp, int displayWidth )
{
    FUNCTION_TRACE
    int status = 0;
    try
    {
        TtblRecord deviceRec;
        g_DeviceTable.find( deviceRec, SENSORDEV_NAME, deviceName.c_str() );

        if( !deviceRec )
        {
            deviceRec = g_DeviceTable.add();
            deviceRec.set( SENSORDEV_NAME, deviceName );
        }

        if( !deviceRec ) throw errException( g_DeviceTable.error );

        TtblRecord metricRec;
        for( ; g_MetricTable.find( metricRec, SENSOR_DEVID, deviceRec.get(TBL_ID) ); )
        {
            if( strcmp( metricName.c_str(), metricRec.str(SENSOR_NAME) ) == 0 ) break;
        }

        if( !metricRec )
        {
            metricRec = g_MetricTable.add();
            if( !metricRec ) throw errException( g_MetricTable.error );

            metricRec.set( SENSOR_DEVID, deviceRec.get(TBL_ID) );
            metricRec.set( SENSOR_TYPE, sensorType );
            metricRec.set( SENSOR_NAME, metricName );
            metricRec.set( SENSOR_WIDTH, displayWidth );
        }

        metricRec.set( SENSOR_VALUE, value );
        metricRec.set( SENSOR_FLAG, stateFlag );
        metricRec.set( SENSOR_TIME, (unsigned long)timeStamp );
    }
    catch( errException& ex )
    {
        Error( "Update Error: %s\n", ex.error() );
        status = -1;
    }
    return status;
}

/**
 * @brief Вывод состояния датчиков в лог (формат для UI)
 */
int LogSensorStatus()
{
    FUNCTION_TRACE
    g_DeviceTable.lock();
    g_MetricTable.lock();
    int status = 0;
    try
    {
        // ВАЖНО: Сохранен формат "+%d devices"
        Log( "+%d devices\n", g_DeviceTable.count() );

        int deviceIdx = 0;
        for( TtblRecord deviceRec = g_DeviceTable.first(); deviceRec; ++deviceRec )
        {
            // Формат: "ID ИмяУстройства:"
            string rowOutput = stringformat( "%d %s:", deviceIdx++, deviceRec.str(SENSORDEV_NAME) );

            for( TtblRecord metricRec; g_MetricTable.find( metricRec, SENSOR_DEVID, deviceRec.get(TBL_ID) ); )
            {
                // Формат сегмента: TypeFlagName:Value:Width|
                rowOutput += stringformat( "%c%c%s:%s:%d|",
                                          metricRec.get(SENSOR_TYPE),
                                          metricRec.get(SENSOR_FLAG),
                                          metricRec.str(SENSOR_NAME),
                                          metricRec.str(SENSOR_VALUE),
                                          metricRec.get(SENSOR_WIDTH) );
            }
            Log( "%s\n", rowOutput.c_str() );
        }
    }
    catch( errException& ex )
    {
        Error( "Log Error: %s\n", ex.error() );
        status = -1;
    }
    g_MetricTable.unlock();
    g_DeviceTable.unlock();
    return status;
}

/**
 * @brief Поток для выполнения внешних процессов (бинарников датчиков)
 */
void *TaskExecutionThread( void* arg )
{
    DTHRD( "{task_runner}" )
    TaskContext* context = (TaskContext*)arg;

    while( !g_IsShutdownRequested )
    {
        FILE *inStream, *outStream;
        context->process_id = RunProcessIO( context->command.c_str(), inStream, outStream );
        Log( "Process started: %s [PID: %d]\n", context->command.c_str(), context->process_id );

        char buffer[ 1024 ];
        while( inStream && !feof( inStream ) )
        {
            if( !fgets( buffer, sizeof(buffer), inStream ) ) break;
            // Пересылаем сообщения об ошибках в основной лог
            if( strstr( buffer, "<BUG>" ) ) Log( "%s", buffer );
        }

        WaitProcessIO( context->process_id, inStream, outStream );
        if( !g_IsShutdownRequested ) sleep( 1 );
    }

    pthread_exit( NULL );
    return NULL;
}

/**
 * @brief Обработчик сигнала остановки
 */
bool OnShutdownSignal( int signum )
{
    FUNCTION_TRACE
    Log( "Sensor system received shutdown signal (%d)\n", signum );
    g_IsShutdownRequested = true;

    FOR_EACH_ITER( TaskList, g_ActiveTasks, task )
    {
        Log( "Terminating process [%d]: %s\n", task->process_id, task->command.c_str() );
        if( kill( task->process_id, SIGTERM ) )
            Error( "Failed to kill PID %d\n", task->process_id );
    }
    return false; // Завершение будет произведено через killself()
}

//-------------------------------------------------------------------------------------------

int main( int argc, char* argv[] )
{
    g_DebugSystem.stdbufoff();
    g_DebugSystem.init( argc, argv, &VERSION, true );
    g_DebugSystem.OnStopSignal = &OnShutdownSignal;
    FUNCTION_TRACE

    // Инициализация БД
    if( CreateSensorTables() != 0 ) return -1;
    g_DeviceTable.open();
    g_MetricTable.open();

    string intervalStr = g_DebugSystem.getparam_or_default( "interval" );
    int intervalMs = str2int( intervalStr );

    // Чтение секции RUN из конфигурации
    int taskIdx = 0;
    while( true )
    {
        string paramName = g_DebugSystem.conf->GetParamNameById( "RUN", taskIdx++ );
        if( paramName.empty() ) break;

        TaskContext newTask;
        newTask.command = g_DebugSystem.fullpath( g_DebugSystem.conf->ReadString( "RUN", paramName ) );
        replace( newTask.command, "{interval}", intervalStr );
        g_ActiveTasks.push_back( newTask );
    }

    // Запуск рабочих потоков
    FOR_EACH_ITER( TaskList, g_ActiveTasks, taskPtr )
    {
        if( pthread_create( &taskPtr->thread_id, NULL, TaskExecutionThread, &(*taskPtr) ) != 0 )
            Error( "Critical: Failed to create thread for %s\n", taskPtr->command.c_str() );
    }

    // Основной цикл: вывод данных в лог для UI
    while( !g_IsShutdownRequested )
    {
        LogSensorStatus();
        usleep( intervalMs * 1000 );
    }

    // Ожидание завершения потоков
    errno = 0;
    FOR_EACH_ITER( TaskList, g_ActiveTasks, taskPtr )
    {
        pthread_join( taskPtr->thread_id, NULL );
        Log( "Task thread finished: %s\n", taskPtr->command.c_str() );
    }

    g_DebugSystem.killself();
    sleep( 1 );
    return 0;
}
