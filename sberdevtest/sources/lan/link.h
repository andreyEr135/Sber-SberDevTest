#ifndef _LINK_H_
#define _LINK_H_

#include "sysutils.h"
#include <string>
#include <pthread.h>

using std::string;

// Параметры для мониторинга линка
struct SLinkMonitorParams {
    int requiredSpeed;   // Ожидаемая скорость (Мбит/с)
    int checkPeriodSec;  // Период проверки в секундах
    string interfaceName;
};

class CLinkSpeedMonitor
{
public:
    // Разовая проверка скорости
    static int VerifyCurrentSpeed(int requiredSpeed, const string& iface);

    // Статические методы для управления потоком
    static void Start(CLinkSpeedMonitor* context);
    static void Stop(CLinkSpeedMonitor* context);

    explicit CLinkSpeedMonitor(const SLinkMonitorParams& params);
    ~CLinkSpeedMonitor();

private:
    // Точка входа в поток
    static void* ThreadEntryProxy(void* arg);

    void MonitoringLoop();
    void RequestStop();

    // Потокобезопасное управление флагом завершения
    void SetFinished(bool finished);
    bool IsFinished();
    bool ExchangeFinished(bool newValue);

    SLinkMonitorParams m_params;
    pthread_t m_threadId;
    Tmutex m_stateMutex;
    bool m_isStopRequested;

    // Запрет копирования
    CLinkSpeedMonitor(const CLinkSpeedMonitor&) = delete;
    CLinkSpeedMonitor& operator=(const CLinkSpeedMonitor&) = delete;
};

#endif //_LINK_H_
