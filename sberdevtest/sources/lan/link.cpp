#include "link.h"
#include "debugsystem.h"
#include "sysutils.h"

CLinkSpeedMonitor::CLinkSpeedMonitor(const SLinkMonitorParams& params)
    : m_params(params), m_threadId(0), m_isStopRequested(true)
{
}

CLinkSpeedMonitor::~CLinkSpeedMonitor()
{
    RequestStop();
}

void CLinkSpeedMonitor::SetFinished(bool finished)
{
    m_stateMutex.lock(0);
    m_isStopRequested = finished;
    m_stateMutex.unlock(0);
}

bool CLinkSpeedMonitor::IsFinished()
{
    bool status;
    m_stateMutex.lock(0);
    status = m_isStopRequested;
    m_stateMutex.unlock(0);
    return status;
}

// Атомарно получает текущее значение и устанавливает новое
bool CLinkSpeedMonitor::ExchangeFinished(bool newValue)
{
    bool oldValue;
    m_stateMutex.lock(0);
    oldValue = m_isStopRequested;
    m_isStopRequested = newValue;
    m_stateMutex.unlock(0);
    return oldValue;
}

int CLinkSpeedMonitor::VerifyCurrentSpeed(int requiredSpeed, const string& iface)
{
    const string speedFilePath = "/sys/class/net/" + iface + "/speed";

    // Считываем значение из sysfs
    int currentSpeed = readfileint(speedFilePath);

    if (currentSpeed < requiredSpeed) {
        Warning("Link degradation detected on %s: current %d Mbps, expected %d Mbps\n",
                iface.c_str(), currentSpeed, requiredSpeed);
    }

    return currentSpeed;
}

void* CLinkSpeedMonitor::ThreadEntryProxy(void* arg)
{
    if (arg) {
        static_cast<CLinkSpeedMonitor*>(arg)->MonitoringLoop();
    }
    return nullptr;
}

void CLinkSpeedMonitor::MonitoringLoop()
{
    int interval = m_params.checkPeriodSec;
    if (interval < 1) interval = 1;
    if (interval > 3600) interval = 3600;

    int iterationCounter = 0;

    while (!IsFinished())
    {
        if (iterationCounter >= interval) {
            iterationCounter = 0;
            VerifyCurrentSpeed(m_params.requiredSpeed, m_params.interfaceName);
        }

        sleep(1);
        iterationCounter++;
    }
}

void CLinkSpeedMonitor::RequestStop()
{
    // Если уже остановлено (true), ничего не делаем
    if (ExchangeFinished(true) == true) return;

    if (m_threadId != 0) {
        pthread_join(m_threadId, nullptr);
        m_threadId = 0;
    }
}

void CLinkSpeedMonitor::Stop(CLinkSpeedMonitor* context)
{
    if (context) context->RequestStop();
}

void CLinkSpeedMonitor::Start(CLinkSpeedMonitor* context)
{
    if (!context) return;

    // Если поток уже запущен (m_isStopRequested == false), выходим
    if (context->ExchangeFinished(false) == false) return;

    if (pthread_create(&(context->m_threadId), nullptr, &CLinkSpeedMonitor::ThreadEntryProxy, context) != 0) {
        throw errException("Failed to create link speed monitor thread");
    }
}
