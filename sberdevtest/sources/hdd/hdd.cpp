#define _LARGEFILE64_SOURCE // для lseek64
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/fs.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <cmath>

#include "math.h"
#include "debugsystem.h"
#include "sysutils.h"
#include "memtbl.h"
#include "devscan.h"
#include "_version"

using namespace std;

//-------------------------------------------------------------------------------------------

// Глобальные переменные состояния
string g_ReadSpeedSummary;
string g_WriteSpeedSummary;
string g_LastTemperatureSummary;
pid_t g_ActiveChildPid = 0;

// Параметры временных файлов для очистки
int g_TestFileIndex = -1;
string g_TestFileName = "";
float g_TotalTestSize = 0;

//-------------------------------------------------------------------------------------------

// Обработчик таймера обновления интерфейса (отправляет SIGUSR1 в dd для вывода статуса)
void HandleUiRefreshTimer(sigval sv)
{
    DTHRD("{UI_Timer}")
    if (g_ActiveChildPid == 0) return;

    // В Linux сигнал SIGUSR1 заставляет dd вывести текущую статистику в stderr
    if (kill(g_ActiveChildPid, SIGUSR1) != 0)
        Error("Failed to send SIGUSR1 to PID %d\n", g_ActiveChildPid);
}

//-------------------------------------------------------------------------------------------

// Обработчик таймера зависания (Watchdog)
void HandleWatchdogTimeout(sigval sv)
{
    DTHRD("{Watchdog}")
    Log("STORAGE TEST ERROR: Operation timed out\n");
    g_DebugSystem.killchilds();
    exit(0);
}

//-------------------------------------------------------------------------------------------

// Обработка сигнала остановки (удаление тестовых файлов)
bool HandleTerminationSignal(int signum)
{
    FUNCTION_TRACE
    Log("%s: Termination signal received, cleaning up...\n", g_DebugSystem.exe.c_str());

    char pathBuffer[1024];
    if (g_TotalTestSize < 1900000000.0f) // Если файл один (меньше ~2ГБ)
    {
        if (g_TotalTestSize > 1024 * 1024)
        {
            if (remove(g_TestFileName.c_str()) != 0)
                Error("Cleanup failed: could not remove %s\n", g_TestFileName.c_str());
        }
    }
    else // Если файлов несколько
    {
        for (int i = 0; i <= g_TestFileIndex; i++)
        {
            sprintf(pathBuffer, "%s%d", g_TestFileName.c_str(), i);
            remove(pathBuffer);
        }
    }
    return true;
}

//-------------------------------------------------------------------------------------------

// Получение времени в микросекундах
unsigned long int GetMicroseconds()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000 + tv.tv_usec);
}

//-------------------------------------------------------------------------------------------

// Основная функция запуска dd и парсинга результатов
void RunPerformanceBenchmark(string deviceName, string mode, string speedLimitStr)
{
    FUNCTION_TRACE

    // Загрузка настроек размера и блока
    string sizeConfig = g_DebugSystem.getparam("size");
    if (sizeConfig.empty()) sizeConfig = g_DebugSystem.conf->ReadString("DD", "size");

    string bsConfig = g_DebugSystem.getparam("bs");
    if (bsConfig.empty()) bsConfig = g_DebugSystem.conf->ReadString("DD", "bs");

    bool monitorTemp = g_DebugSystem.checkparam("temp");

    double targetSize = KMG2double(sizeConfig);
    double blockSize = KMG2double(bsConfig);
    double blockCount = ceil(targetSize / blockSize);
    string countStr = stringformat("%.0f", blockCount);

    // Подготовка команды dd из конфига
    string commandTemplate = g_DebugSystem.conf->ReadString("DD", mode);
    replace(commandTemplate, "{dev}", deviceName);
    replace(commandTemplate, "{bs}", bsConfig);
    replace(commandTemplate, "{count}", countStr);

    LogCYN("Executing: %s", commandTemplate.c_str());
    Log("<UI> %s %s starting...\n", deviceName.c_str(), mode.c_str());

    FILE *pipeIn, *pipeOut;
    int attempt = 0;
    bool isSuccessful = false;

    while (attempt < 3) // До 3-х попыток в случае сбоя
    {
        isSuccessful = true;
        g_ActiveChildPid = RunProcessIO(commandTemplate.c_str(), pipeIn, pipeOut);

        if (!pipeIn) {
            if (attempt == 2) throw errException("Failed to initiate process: %s", commandTemplate.c_str());
            isSuccessful = false;
        }

        // Повышаем приоритет процесса для точности замеров
        setpriority(PRIO_PROCESS, 0, -19);
        setpriority(PRIO_PROCESS, g_ActiveChildPid, -19);

        timer_t uiTimer, watchdogTimer;
        StartTimerNewThread(&uiTimer, 1, 0, &HandleUiRefreshTimer);
        StartTimerNewThread(&watchdogTimer, 30, 0, &HandleWatchdogTimeout);

        int recordsProcessed = 0;
        string bytesCopiedStr;
        string lastErrorMessage = "";
        double currentSpeedBps = 0;
        string formattedSpeed;
        string tempInfo = "";

        char buffer[512];
        while (pipeIn && !feof(pipeIn))
        {
            if (!fgets(buffer, sizeof(buffer), pipeIn)) break;
            Log("%s", buffer);

            // Сброс watchdog при получении данных
            StopTimer(watchdogTimer);
            StartTimerNewThread(&watchdogTimer, 30, 0, &HandleWatchdogTimeout);

            // Парсинг количества записей
            if (strstr(buffer, "records in") || strstr(buffer, "записей считано") || strstr(buffer, "записей получено"))
            {
                sscanf(buffer, "%d", &recordsProcessed);
            }

            // Парсинг строки скорости: "1055761408 bytes copied, 18.8 s, 53.5 MB/s"
            if (strstr(buffer, "copied") || strstr(buffer, "скопирован"))
            {
                char* openParen = strchr(buffer, '(');
                char* closeParen = strchr(buffer, ')');
                if (openParen && closeParen)
                    bytesCopiedStr = string(openParen + 1, closeParen - openParen - 1);

                char* lastSpace = strrchr(buffer, ' ');
                if (lastSpace) {
                    char* ptr = lastSpace;
                    while (ptr != buffer) {
                        ptr--;
                        if (*ptr == ',' && *(ptr + 1) == ' ') break;
                    }

                    currentSpeedBps = KMG2double(ptr + 1);
                    formattedSpeed = double2KMG(currentSpeedBps, "B/s");
                }

                if (monitorTemp) {
                    int t = str2int(GetLinesProcess(("hddtemp -n /dev/" + deviceName).c_str()));
                    tempInfo = stringformat("temp: %d°C", t);
                }

                double progress = (blockCount > 0) ? (recordsProcessed * 100.0 / blockCount) : 0;
                Log("<UI> %s %s %.0f%%: %s, %s %s\n",
                    deviceName.c_str(), mode.c_str(), progress, bytesCopiedStr.c_str(), formattedSpeed.c_str(), tempInfo.c_str());
                Log("<INFO> %.0f%%: %c %s\n", progress, mode[0], formattedSpeed.c_str());
            }

            if (strncmp(buffer, "dd:", 3) == 0) lastErrorMessage = trim(buffer + 3);
        }

        int exitStatus = WaitProcessIO(g_ActiveChildPid, pipeIn, pipeOut);
        g_ActiveChildPid = 0;

        StopTimer(uiTimer);
        StopTimer(watchdogTimer);

        if (exitStatus != 0) {
            isSuccessful = false;
            if (attempt == 2) throw errException(exitStatus, "dd failed: %s", lastErrorMessage.c_str());
        }

        // Сохранение итоговых данных
        if (mode == "read")  g_ReadSpeedSummary = "read: " + formattedSpeed;
        if (mode == "write") g_WriteSpeedSummary = "write: " + formattedSpeed;
        if (monitorTemp)     g_LastTemperatureSummary = tempInfo;

        // Проверка лимита скорости
        double minRequiredSpeed = KMG2double(speedLimitStr);
        if (!g_DebugSystem.checkparam("stress") && currentSpeedBps < minRequiredSpeed) {
            isSuccessful = false;
            if (attempt == 2)
                throw errException("%s speed too low: %s < %s (%s)",
                                   mode.c_str(), formattedSpeed.c_str(),
                                   double2KMG(minRequiredSpeed, "B/s").c_str(), tempInfo.c_str());
        }

        if (isSuccessful) break;
        attempt++;
        Log("Retrying %s benchmark (attempt %d/3)...\n", mode.c_str(), attempt + 1);
    }
}

//-------------------------------------------------------------------------------------------

void ExecuteStorageBenchmarks()
{
    FUNCTION_TRACE
    g_DebugSystem.OnStopSignal = &HandleTerminationSignal;

    bool doRead = g_DebugSystem.checkparam("read");
    bool doWrite = g_DebugSystem.checkparam("write");
    if (!doRead && !doWrite) throw errException("No mode specified (use 'read' and/or 'write')");

    string devName = g_DebugSystem.getparam("dev");
    if (devName.empty()) throw errException("Device parameter 'dev=' is missing");

    // Проверка устройства в таблице блочных устройств
    TmemTable blkTable(BLK_TBL);
    if (!blkTable.open()) throw errException(blkTable.error);
    TtblRecord rec;
    blkTable.find(rec, BLK_DEV, devName.c_str());
    if (!rec) throw errException("Device %s not found in block table", devName.c_str());

    string mountPoint = rec.str(BLK_MOUNT);
    string durationStr = g_DebugSystem.getparam("time");
    timeval startTime = TimeStart();
    int testSeconds = time2seconds(durationStr);

    do
    {
        if (doRead)
        {
            RunPerformanceBenchmark(devName, "read", g_DebugSystem.getparam("readlimit"));
        }

        if (doWrite)
        {
            // Запрет на запись, если раздел примонтирован (во избежание порчи данных)
            if (mountPoint.empty())
                RunPerformanceBenchmark(devName, "write", g_DebugSystem.getparam("writelimit"));
            else
                Warning("Skipping write test: %s is mounted on %s\n", devName.c_str(), mountPoint.c_str());
        }
    }
    while (TimeSec(startTime) < testSeconds);
}

//-------------------------------------------------------------------------------------------

void DisplayUsageHelp()
{
    FUNCTION_TRACE
    Log("Usage: %s dev={dev} [read] [write] [size={1G}] [bs={4M}] [readlimit={200Mb/s}] [writelimit={100Mb/s}] [temp] [stress] [time={10m}]\n",
        g_DebugSystem.exe.c_str());
}

//-------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    g_DebugSystem.stdbufoff();
    g_DebugSystem.init(argc, argv, &VERSION, true);
    FUNCTION_TRACE

    try
    {
        if (g_DebugSystem.checkparam("-h") || g_DebugSystem.checkparam("--help")) {
            DisplayUsageHelp();
            return 0;
        }

        // Проверка версии USB порта (если переданы параметры Vers/VersP)
        int requiredUsbVers = str2int(g_DebugSystem.getparam("Vers"));
        int actualUsbVers = str2int(g_DebugSystem.getparam("VersP"));

        if (actualUsbVers > 0 && actualUsbVers < requiredUsbVers)
        {
            float v = requiredUsbVers / 10.0f;
            Log("<WAR> USB version mismatch: actual version is lower than expected %.1f\n", v);
        }

        ExecuteStorageBenchmarks();

        Log("TEST OK %s %s %s\n",
            g_ReadSpeedSummary.c_str(),
            g_WriteSpeedSummary.c_str(),
            g_LastTemperatureSummary.c_str());
    }
    catch (errException& ex)
    {
        Error("TEST ERR: %s\n", ex.error());
        return -1;
    }

    return 0;
}
