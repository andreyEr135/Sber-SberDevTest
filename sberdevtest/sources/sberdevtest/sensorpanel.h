#ifndef sensorpanelH
#define sensorpanelH

#include <vector>
#include <string>
#include <pthread.h>
#include "ui.h"

//-------------------------------------------------------------------------------------------

/**
 * @brief Панель мониторинга датчиков (температура, напряжение и т.д.)
 * Работает в отдельном потоке, опрашивая внешний процесс-обработчик.
 */
class TSensorPanel : public TUIPanel
{
  protected:
    virtual void resize( );
    virtual void draw( );

    bool m_shouldExit;              ///< Флаг завершения потока
    pid_t m_sensorPid;              ///< PID процесса, отдающего данные датчиков
    pthread_t m_monitorThread;      ///< Дескриптор потока мониторинга

    std::vector<std::string> m_sensorRecords; ///< Массив строк с данными датчиков

    static void* MonitorThread( void* arg );

  public:
    TSensorPanel( const TUI* ui, const char* name, int x, int y, int w, int h, int color );
    virtual ~TSensorPanel( );

    void Refresh( );
    void LogSensorsByMask( std::string masks );
};

//-------------------------------------------------------------------------------------------

extern TSensorPanel* SensorPanel;

#endif
