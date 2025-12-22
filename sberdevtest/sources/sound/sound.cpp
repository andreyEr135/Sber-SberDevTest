#include <stdio.h>
#include <pthread.h>
#include <string>
#include <vector>

#include "debugsystem.h"
#include "_version"

using namespace std;

//-------------------------------------------------------------------------------------------

// Глобальные идентификаторы процессов и потоков
pid_t     g_playback_pid;
pthread_t g_playback_thread;
bool      g_playback_should_stop;
pid_t     g_recording_pid;

//-------------------------------------------------------------------------------------------

/**
 * @brief Поток для циклического воспроизведения аудиофайла
 */
void* playback_worker_thread( void* arg )
{
    DTHRD("{audio_output}")

    std::string player_bin = g_DebugSystem.conf->ReadString( "SOUND", "player" );
    std::string audio_file = g_DebugSystem.fullpath( g_DebugSystem.conf->ReadString( "SOUND", "playfile" ) );
    std::string playback_cmd = player_bin + " " + audio_file;

    Trace( "playback_cmd: %s\n", playback_cmd.c_str() );
    Log( "<UI> Воспроизведение запущено\n" );

    while( !g_playback_should_stop )
    {
        if( !RunProcess( g_playback_pid, playback_cmd.c_str() ) )
            throw errException( "Failed to run playback: %s", playback_cmd.c_str() );

        WaitProcess( g_playback_pid );
    }

    Log( "<UI> Воспроизведение остановлено\n" );

    pthread_exit( NULL );
    return NULL;
}

//-------------------------------------------------------------------------------------------

/**
 * @brief Тестирование динамиков (вывод звука)
 */
void perform_speaker_test()
{
    FUNCTION_TRACE
    g_playback_should_stop = false;

    // 1. Настройка громкости, если указана в конфиге
    std::string vol_cfg_cmd = g_DebugSystem.conf->ReadString( "SOUND", "set_vol" );
    std::string vol_level = "";

    if ( !vol_cfg_cmd.empty() )
    {
        vol_level = g_DebugSystem.conf->ReadString( "SOUND", "volume" );
        if ( !vol_level.empty() )
        {
            size_t placeholder_pos = vol_cfg_cmd.find("{vol}");
            if ( placeholder_pos != std::string::npos )
            {
                vol_cfg_cmd.replace( placeholder_pos, 5, vol_level );
            }
            else vol_cfg_cmd = "";
        }
        else vol_cfg_cmd = "";
    }

    Trace( "volume_set_cmd: %s\n", vol_cfg_cmd.c_str() );

    if ( !vol_cfg_cmd.empty() )
    {
        Log( "<UI> Установка громкости: %s\n", vol_level.c_str() );
        if( !RunProcess( g_playback_pid, vol_cfg_cmd.c_str() ) )
            throw errException( "Volume set failed: %s", vol_cfg_cmd.c_str() );

        WaitProcess( g_playback_pid );
    }

    // 2. Запуск потока воспроизведения
    if( pthread_create( &g_playback_thread, NULL, playback_worker_thread, NULL ) != 0 )
        throw errException( "Could not create playback thread" );

    // 3. Интерактивный диалог с пользователем
    char input_buffer[ 256 ];
    std::string user_answer;
    bool is_audio_ok = false;

    while( true )
    {
        Log( "<DIALOG>40,9{}{Слышны звуки из динамика?}{Да|Нет}{Нет}\n" );

        if( !fgets( input_buffer, sizeof(input_buffer), stdin ) )
            throw errException( "Input read error" );

        user_answer = trim( input_buffer );
        Trace( "user response: '%s'\n", user_answer.c_str() );

        if( user_answer == "Да" ) {
            is_audio_ok = true;
            break;
        }
        else if( user_answer == "Нет" ) {
            is_audio_ok = false;
            break;
        }
    }

    // 4. Остановка и очистка
    g_playback_should_stop = true;
    if( kill( g_playback_pid, SIGTERM ) != 0 )
        Error( "Failed to terminate playback process %d\n", g_playback_pid );

    pthread_join( g_playback_thread, NULL );

    if( !is_audio_ok ) throw errException( "Звук не обнаружен пользователем" );
}

//-------------------------------------------------------------------------------------------

/**
 * @brief Тестирование микрофона (запись и последующее воспроизведение)
 */
void perform_microphone_test()
{
    FUNCTION_TRACE

    // Подготовка команд из конфига
    std::string player_bin = g_DebugSystem.conf->ReadString( "MIC", "player" );
    std::string recorder_bin = g_DebugSystem.conf->ReadString( "MIC", "recorder" );
    std::string tmp_dir = g_DebugSystem.conf->ReadString( "MIC", "tmpdir" );
    std::string rec_filename = g_DebugSystem.conf->ReadString( "MIC", "recfile" );
    std::string ref_audio = g_DebugSystem.fullpath( g_DebugSystem.conf->ReadString( "MIC", "playfile" ) );

    std::string record_path = tmp_dir + rec_filename;
    std::string playback_ref_cmd = player_bin + " " + ref_audio;
    std::string record_cmd = recorder_bin + " " + record_path;

    Trace( "record_cmd: %s\n", record_cmd.c_str() );

    // 1. Запуск записи в фоновом режиме
    Log( "<UI> Подготовка записи...\n" );
    if( !RunProcess( g_recording_pid, record_cmd.c_str() ) )
        throw errException( "Record start failed: %s", record_cmd.c_str() );

    // 2. Воспроизведение эталонного звука (чтобы было что записывать)
    Log( "<UI> Запись пошла (воспроизведение эталона)...\n" );
    if( !RunProcess( g_playback_pid, playback_ref_cmd.c_str() ) )
        throw errException( "Reference playback failed: %s", playback_ref_cmd.c_str() );

    WaitProcess( g_playback_pid );

    Log( "<UI> Запись завершена\n" );
    kill( g_recording_pid, SIGTERM );

    // 3. Воспроизведение того, что записали
    Log( "<UI> Проверка записи...\n" );
    std::string playback_recorded_cmd = player_bin + " " + record_path;

    if( !RunProcess( g_playback_pid, playback_recorded_cmd.c_str() ) )
        throw errException( "Playback of recording failed" );

    Log( "<DIALOG>40,9{}{Слышны записанные звуки?}{Да|Нет}{Нет}\n" );

    char input_buffer[ 256 ];
    if( !fgets( input_buffer, sizeof(input_buffer), stdin ) )
        throw errException( "Input read error" );

    std::string user_answer = trim( input_buffer );
    bool is_mic_ok = (user_answer == "Да");

    // Очистка
    kill( g_playback_pid, SIGTERM );
    WaitProcess( g_playback_pid );

    if( !is_mic_ok ) throw errException( "Микрофон не записал звук" );
}

//-------------------------------------------------------------------------------------------

/**
 * @brief Диспетчер тестов
 */
void execute_selected_test()
{
    FUNCTION_TRACE
    std::string test_mode = g_DebugSystem.getparam( "mode" );

    if( test_mode == "sound" ) {
        perform_speaker_test();
    }
    else if( test_mode == "mic" ) {
        perform_microphone_test();
    }
    else {
        throw errException( "Неизвестный режим теста: %s", test_mode.c_str() );
    }
}

//-------------------------------------------------------------------------------------------

int main( int argc, char* argv[] )
{
    g_DebugSystem.stdbufoff();
    g_DebugSystem.init( argc, argv, &VERSION, true );
    FUNCTION_TRACE

    // Определение типа запуска (test или вывод справки)
    std::string action_type = ( g_DebugSystem.params.size() > 0 ) ? g_DebugSystem.params[ 0 ] : "";

    try
    {
        if( action_type == "test" )
        {
            execute_selected_test();
            Log( "TEST OK\n" );
        }
        else
        {
            Log( "Использование: %s test mode={sound|mic}\n", g_DebugSystem.exe.c_str() );
        }
    }
    catch( errException& ex )
    {
        if( action_type == "test" ) {
            Log( "TEST ERR %s\n", ex.error() );
        } else {
            Error( "Critical error: %s\n", ex.error() );
        }
        return -1;
    }

    return 0;
}
