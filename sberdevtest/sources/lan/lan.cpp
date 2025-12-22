#include <csignal>
#include <malloc.h>
#include "debugsystem.h"
#include "sysutils.h"
#include "netservice.h"
#include "lan.h"
#include "link.h"
#include "_version"

using namespace std;

//-------------------------------------------------------------------------------------------

string g_LanTestSummary;
TnetIFs g_InterfaceList;
TnetIF* g_ActiveInterface = nullptr;
string g_TestCaption;

// Состояние для специфических режимов (например, iperf)
int g_OperatingMode = 0; // 1 - для iperf
Ticmp g_GlobalIcmpHandle;
BYTE* g_PingDataBuffer = (BYTE*)malloc(64); // Буфер для фоновых проверок

string g_TargetIp;
WORD g_IcmpSequence = 0;
int g_PingTimeoutMs = 1000;
int g_ConsecutiveErrorCount = 0;
int g_IperfWatchdogCounter = 0;

//-------------------------------------------------------------------------------------------

TnetIF* FindInterfaceByDestinationIp(string ipAddress)
{
    FUNCTION_TRACE
    Tnetaddr targetAddr(ipAddress);
    if (!getIFs(g_InterfaceList)) throw errException("Failed to retrieve network interfaces");

    FOR_EACH_ITER(TnetIFs, g_InterfaceList, it)
    {
        // Сравнение подсетей для определения подходящего интерфейса
        if (memcmp(&targetAddr.sub, &it->addr.sub, 4) == 0)
            return &(*it);
    }
    return nullptr;
}

//-------------------------------------------------------------------------------------------

// Обработчик проверки состояния интерфейса (вызывается периодически)
void HandleInterfaceUpdate(int /*signo*/)
{
    FUNCTION_TRACE
    if (!g_ActiveInterface) return;

    if (!g_ActiveInterface->update()) throw errException("Interface update failed");
    if (!g_ActiveInterface->flink())  throw errException("Network link is down");

    // Если включен режим iperf, выполняем фоновый пинг для контроля связности
    if (g_OperatingMode == 1)
    {
        timeval startTime = TimeStart();
        if (!g_GlobalIcmpHandle.send(g_TargetIp, g_IcmpSequence, g_PingDataBuffer, 64))
            throw errException(g_GlobalIcmpHandle.error());

        WORD receivedSeq;
        string receivedIp;
        Ticmp::Tresult res = Ticmp::netTIMEOUT;

        while (TimeMilli(startTime) < g_PingTimeoutMs)
        {
            res = g_GlobalIcmpHandle.recv(receivedIp, receivedSeq, NULL, 0, g_PingTimeoutMs);
            if (res == Ticmp::netSKIP) continue;
            if (res == Ticmp::netOK && g_IcmpSequence != receivedSeq) continue;
            break;
        }

        if (res != Ticmp::netOK) g_ConsecutiveErrorCount++;
        else g_ConsecutiveErrorCount = 0;

        if (g_ConsecutiveErrorCount >= 3) THROW("Critical: 3 consecutive ping losses!");

        if (g_IperfWatchdogCounter > 0)
        {
            g_IperfWatchdogCounter++;
            if (g_IperfWatchdogCounter > 10) THROW("Iperf watchdog: no messages received!");
        }
    }
}

//-------------------------------------------------------------------------------------------

void ExecuteNetworkBenchmarks()
{
    FUNCTION_TRACE
    g_TestCaption = g_DebugSystem.checkparam("caption") ? g_DebugSystem.getparam("caption") + " " : "";

    if (g_DebugSystem.checkparam("ping"))
    {
        string ip = g_DebugSystem.getparam("ping");
        int durationSec  = time2seconds(g_DebugSystem.getparam_or_default("PING", "time"));
        int packetSize   = str2int(g_DebugSystem.getparam_or_default("PING", "size"));
        int intervalMs   = str2int(g_DebugSystem.getparam_or_default("PING", "interval"));
        int timeoutMs    = str2int(g_DebugSystem.getparam_or_default("PING", "timeout"));
        int warningLimit = str2int(g_DebugSystem.getparam("warning"));
        bool isStress    = g_DebugSystem.checkparam("stress");

        int expectedLinkSpeed = str2int(g_DebugSystem.getparam_or_default("PING", "linkSpeed"));
        int linkCheckPeriod   = time2seconds(g_DebugSystem.getparam_or_default("PING", "linkCheckPeriod"));

        g_ActiveInterface = FindInterfaceByDestinationIp(ip);

        if (ip.empty()) throw errException("Target IP is not specified");
        if (durationSec <= 0) throw errException("Invalid test duration");
        if (intervalMs <= 0)  throw errException("Invalid interval");
        if (!g_ActiveInterface) throw errException("No route to host %s (interface not found)", ip.c_str());

        Log("Target: %s\n", ip.c_str());
        Log("Interface: %s\n", g_ActiveInterface->name.c_str());
        Log("Link Speed Required: %d Mbps\n", expectedLinkSpeed);

        // 1. Исправлено: Используем новые имена SLinkMonitorParams и CLinkSpeedMonitor
        SLinkMonitorParams linkParams;
        linkParams.interfaceName = g_ActiveInterface->name;
        linkParams.requiredSpeed = expectedLinkSpeed;
        linkParams.checkPeriodSec = linkCheckPeriod;

        CLinkSpeedMonitor linkMonitor(linkParams);

        // 2. Исправлено: Безопасное выделение памяти (std::vector гарантирует очистку при throw)
        std::vector<BYTE> packetBuffer(packetSize, 0x55);

        if (expectedLinkSpeed > 0)
        {
            if (isStress) {
                // Исправлено: Метод Start вместо StartChecking
                CLinkSpeedMonitor::Start(&linkMonitor);
            } else {
                if (!g_ActiveInterface->flink()) throw errException("Link is physically down");
                if (g_ActiveInterface->speed != expectedLinkSpeed)
                    throw errException("Link speed mismatch: %d != %d", g_ActiveInterface->speed, expectedLinkSpeed);
            }
        }

        Ticmp icmp;
        if (!icmp.open()) throw errException(icmp.error());

        Log("<UI>ping %s starting...\n", ip.c_str());

        int successCount = 0;
        int warningCount = 0;
        int failureCount = 0;
        WORD currentSeq = 0;
        timeval startTime = TimeStart();

        while (true)
        {
            // Проверка интерфейса (обновление данных)
            HandleInterfaceUpdate(SIGUSR1);

            currentSeq++;
            timeval sendTime = TimeStart();

            // Используем данные из вектора
            if (!icmp.send(ip, currentSeq, packetBuffer.data(), packetSize))
                throw errException(icmp.error());

            string recvIp;
            WORD recvSeq;
            Ticmp::Tresult res = Ticmp::netTIMEOUT;

            while (TimeMilli(sendTime) < timeoutMs)
            {
                res = icmp.recv(recvIp, recvSeq, NULL, 0, timeoutMs);
                if (res == Ticmp::netSKIP) continue;
                if (res == Ticmp::netOK && currentSeq != recvSeq) continue;
                break;
            }

            double rtt = TimeMilli(sendTime);
            double elapsed = TimeSec(startTime);
            double progress = (elapsed / durationSec) * 100.0;
            string statusCaption = stringformat("%.0f%% ping %s #%d", progress, ip.c_str(), currentSeq);

            if (res == Ticmp::netOK)
            {
                successCount++;
                string rttStr = stringformat("%.3fms", rtt);
                g_LanTestSummary = stringformat("ping %s #%d %s", ip.c_str(), currentSeq, rttStr.c_str());
                Log("<UI>%s %s\n", statusCaption.c_str(), rttStr.c_str());
                Log("<INFO>%.0f%% %s\n", progress, rttStr.c_str());
            }
            else
            {
                string errorDesc = (res == Ticmp::netSKIP) ? "timeout" : icmp.error();

                if (warningCount < warningLimit)
                {
                    warningCount++;
                    if (isStress) Warning("%s %s\n", statusCaption.c_str(), errorDesc.c_str());
                    else Log("<UI>%s %s\n", statusCaption.c_str(), errorDesc.c_str());
                }
                else
                {
                    failureCount++;
                    Log("Critical failure at %s: %s\n", statusCaption.c_str(), errorDesc.c_str());
                    break;
                }
            }

            if (elapsed >= durationSec) break;
            Usleep(intervalMs * 1000);
        }

        // 3. Исправлено: Метод Stop вместо StopChecking
        CLinkSpeedMonitor::Stop(&linkMonitor);

        const char* resultTag = (failureCount > 0) ? "TEST ERR" : (warningCount > 0 ? "TEST WARNING" : "TEST OK");
        Log("%s @%d/%d/%d ping %s\n", resultTag, successCount, warningCount, failureCount, ip.c_str());

        // Завершаем процесс, если это был одиночный тест ping
        exit(0);
    }
}

//-------------------------------------------------------------------------------------------

void info( )
{FUNCTION_TRACE
}
//-------------------------------------------------------------------------------------------

void help( )
{FUNCTION_TRACE
  const char* usage[] = {
    "",
    "Использование:  lan [параметры, * обязательные]",
    "__________________________________________________________________________________",
        "ping:",
        " * ping={ip}          - ip-адрес назначения",
        "   time={10m}           - время тестирования",
        "   size={16000}         - размер данных в пакете",
        "   timeout={1000}       - ms, время ожиданя ответа",
        "   interval={1000}      - ms, интервал между проверками",
        "   warning={1}          - максимальное количество ошибок при стресc-тесте",
        "   linkSpeed={1000}     - проверка скорости линка, ожидаемая скорость",
        "   linkCheckPeriod={1s} - период проверки скорости линка 1 - 3600 сек",
        "   stress               - стресс-тест",
    "__________________________________________________________________________________",
    "",
  NULL };
  for( const char** s = usage; *s; s++ ) printf( "%s\n", *s );
}
//-------------------------------------------------------------------------------------------


int main(int argc, char* argv[])
{
    g_DebugSystem.stdbufoff();
    g_DebugSystem.init(argc, argv, &VERSION, true, LOGTIME_UPTIME);
    FUNCTION_TRACE

    try
    {
        if (g_DebugSystem.checkparam("-i")) return 0;
        if (g_DebugSystem.checkparam("-h")) { help(); return 0; }

        ExecuteNetworkBenchmarks();
        Log("TEST OK %s\n", g_LanTestSummary.c_str());
    }
    catch (errException& e)
    {
        Error("TEST ERR %s%s\n", g_TestCaption.c_str(), e.error());
    }
    CATCH
    {
        Error("TEST ERR %s%s\n", g_TestCaption.c_str(), e.what());
    }
    return 0;
}
