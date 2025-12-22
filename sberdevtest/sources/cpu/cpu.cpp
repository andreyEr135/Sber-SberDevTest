#include "debugsystem.h"
#include "cpu.h"
#include "cputests.h"
#include "_version"
#include <fstream>
#include <algorithm>

using std::string;
using std::vector;
using std::pair;
using std::fstream;
using std::ios;

// --- Глобальные объекты ---
SBenchmarkTask AvailableTests[] = {
    { "Pi", run_pi_benchmark },
    { NULL, NULL }
};

SCpuContext g_CpuCtx;

int   g_ActiveTestIdx;
int   g_TotalTestCount;
const char* g_ActiveTestTitle;
char  g_GlobalErrorBuffer[ 128 ];
unsigned long long volatile g_CurrentStep;
unsigned long long volatile g_TotalSteps;

// --- Функции ---
float parseClockFromProc()
{
    float mhzValue = 0;
    string row;
    fstream cpuinfo_stream;

    cpuinfo_stream.open("/proc/cpuinfo", ios::in);
    while (cpuinfo_stream.good() && getline(cpuinfo_stream, row))
    {
        if (!row.empty())
        {
            string attr, val;
            bool isParsed = stringkeyvalue(row, ':', attr, val);
            if (isParsed)
            {
                if (attr == "cpu MHz")
                {
                    mhzValue = str2float(val);
                    break;
                }
            }
        }
    }
    return mhzValue;
}

int acquireCurrentFrequency()
{
    int finalMhz = 0;
    if (g_CpuCtx.freq_sys_path.empty()) return (int)parseClockFromProc();

    bool readOk;
    const float rawFreq = readfilefloat(g_CpuCtx.freq_sys_path, readOk);

    if (!readOk) return 0;

    finalMhz = (int)rawFreq / 1000;

    return finalMhz;
}

void refreshInterfaceStatus(int signalId)
{
    FUNCTION_TRACE
    const int currentMhz = acquireCurrentFrequency();
    if (g_ActiveTestTitle == AvailableTests[0].label) update_pi_bench_progress();

    Log("<UI> %d/%d %2llu%% %s частота %d MHz\n",
        g_ActiveTestIdx, g_TotalTestCount, g_CurrentStep * 100 / g_TotalSteps, g_ActiveTestTitle, currentMhz);
    Log("<INFO>%2llu%%:%d MHz \n", g_CurrentStep * 100 / g_TotalSteps, currentMhz);
}

void collectSystemSpecs()
{
    FILE* procFile = NULL;
    try
    {
        if ((procFile = fopen("/proc/cpuinfo", "r")) == NULL)
            throw errException("не удалось открыть /proc/cpuinfo");

        char lineBuf[256];
        bool isModelLine, isProcLine;

        while (!feof(procFile))
        {
            if (!fgets(lineBuf, sizeof(lineBuf), procFile)) break;
            isModelLine = isProcLine = false;

            isModelLine |= (strncmp(lineBuf, "vendor_id", 9) == 0);
            isModelLine |= (strncmp(lineBuf, "model name", 10) == 0);
            isModelLine |= (strncmp(lineBuf, "Processor", 9) == 0);
            isModelLine |= (strncmp(lineBuf, "cpu model", 9) == 0);
            isProcLine  |= (strncmp(lineBuf, "processor", 9) == 0);

            char* separator = strchr(lineBuf, ':');
            if (!separator) continue;

            if (isModelLine && (g_CpuCtx.modelName == ""))
                g_CpuCtx.modelName = trim(string(separator + 1));
            else if (isProcLine)
                g_CpuCtx.coreCount++;
        }
        fclose(procFile);
        procFile = NULL;

        if (g_CpuCtx.modelName == "")
        {
            FILE* outPipe = NULL;
            FILE* inPipe = NULL;
            RunProcessIOTimeout("lscpu", outPipe, inPipe, 1000);
            while (!feof(outPipe))
            {
                if (!fgets(lineBuf, sizeof(lineBuf), outPipe)) break;
                isModelLine = (strncmp(lineBuf, "Vendor ID", 9) == 0);
                char* separator = strchr(lineBuf, ':');
                if (!separator) continue;
                if (isModelLine && (g_CpuCtx.modelName == ""))
                    g_CpuCtx.modelName = trim(string(separator + 1));
            }
            fclose(outPipe);
        }

        if (g_CpuCtx.coreCount == 0) g_CpuCtx.coreCount = 1;
    }
    catch (errException& error)
    {
        if (procFile) fclose(procFile);
        throw error;
    }

    string brand = g_CpuCtx.modelName;
    std::transform(brand.begin(), brand.end(), brand.begin(), ::toupper);

    if (brand.find("INTEL") != string::npos) g_CpuCtx.platform = "INTEL";
    else if (brand.find("AMD") != string::npos) g_CpuCtx.platform = "AMD";
    else if (brand.find("ARM") != string::npos) g_CpuCtx.platform = "ARM";
    else if (brand.find("MIPS") != string::npos) g_CpuCtx.platform = "MIPS";
    else if (brand.find("MBE2S-PC") != string::npos) g_CpuCtx.platform = "INTEL";
    else throw errException("неизвестный процессор: %s", g_CpuCtx.modelName.c_str());

    int iterMultiplier = str2int(g_DebugSystem.getparam_or_default("iter"));
    g_CpuCtx.pi_depth = (unsigned long long)iterMultiplier * 1000000ULL;

    string durationStr = g_DebugSystem.getparam("time");
    g_CpuCtx.duration_sec = time2seconds(durationStr);

    if (g_CpuCtx.duration_sec > 0) Log("Stress duration: %s\n", durationStr.c_str());

    g_CpuCtx.freq_sys_path = g_DebugSystem.getparam_or_default("cpu_freq");
}

void buildTestQueue()
{
    FUNCTION_TRACE
    g_CpuCtx.tasks.clear();

    Debug("[TEST]\n");
    vector<pair<string, string>> configMap;
    int counter = 1;
    while (true)
    {
        string idStr = stringformat("%d", counter++);
        string taskTitle = g_DebugSystem.conf->ReadString("TEST", idStr);
        if (taskTitle == "") break;
        configMap.push_back(std::make_pair(idStr, taskTitle));
    }

    for (const auto& entry : configMap)
        Debug(" #%2s: %s\n", entry.first.c_str(), entry.second.c_str());

    string enabledTests = g_DebugSystem.conf->ReadString(g_CpuCtx.platform, "tests");
    Debug("[%s] %s\n", g_CpuCtx.platform.c_str(), enabledTests.c_str());

    char* token = strtok((char*)enabledTests.c_str(), ",. ");
    while (token)
    {
        Debug("%2s", token);
        string currentId = string(token);
        string targetName = "";

        for (const auto& entry : configMap)
        {
            if (entry.first == currentId)
            {
                targetName = entry.second;
                break;
            }
        }

        bool isMatched = false;
        for (int i = 0; AvailableTests[i].label; i++)
        {
            if (targetName == string(AvailableTests[i].label))
            {
                isMatched = true;
                g_CpuCtx.tasks.push_back(&AvailableTests[i]);
                break;
            }
        }

        Debug(" %c %s\n", isMatched ? '+' : '-', targetName.c_str());
        token = strtok(NULL, ",. ");
    }

    Debug("Final test queue:\n");
    for (const auto* task : g_CpuCtx.tasks)
        Debug("  %s\n", task->label);
}

void runDiagnosticSuite()
{
    FUNCTION_TRACE
    for (uint i = 0; i < g_CpuCtx.tasks.size(); i++)
    {
        Log("<UI> Test %s\n", g_CpuCtx.tasks[i]->label);

        g_ActiveTestIdx = i + 1;
        g_TotalTestCount = g_CpuCtx.tasks.size();
        g_ActiveTestTitle = g_CpuCtx.tasks[i]->label;

        timer_t guiTimerId;
        if (StartTimerOwnThread(&guiTimerId, 0, 500000, &refreshInterfaceStatus, SIGUSR1) != 0)
            Error("Ошибка запуска таймера\n");

        int opResult = (*g_CpuCtx.tasks[i]->execute)();

        StopTimer(guiTimerId);
        if (opResult == 0) Log("OK\n");
        else
        {
            Error("%s\n", g_GlobalErrorBuffer);
            throw errException(g_GlobalErrorBuffer);
        }
    }
}

void displayCpuSummary()
{
    FUNCTION_TRACE
    Log("%s, cores(%d)\n", g_CpuCtx.modelName.c_str(), g_CpuCtx.coreCount);
#if defined(__x86_64__) || defined(_M_X64)
    Log("Архитектура: 64bit\n");
#elif defined(__i386) || defined(_M_IX86)
    Log("Архитектура: 32bit\n");
#elif defined(__arm__)
    Log("Архитектура: ARM\n");
#elif defined(__mips__)
    Log("Архитектура: MIPS\n");
#else
    Log("Архитектура: Unknown\n");
#endif
}

// --- Main ---

int main(int argc, char* argv[])
{
    g_DebugSystem.stdbufoff();
    g_DebugSystem.init(argc, argv, &VERSION, true);
    FUNCTION_TRACE

    try
    {
        collectSystemSpecs();
        buildTestQueue();

        if (g_DebugSystem.checkparam("-i")) { displayCpuSummary(); return 0; }
        if (g_DebugSystem.checkparam("-h"))
        {
            Log("%s iter={500} time={10m}\n", g_DebugSystem.exe.c_str());
            return 0;
        }

        runDiagnosticSuite();

        const int finalFreq = acquireCurrentFrequency();
        Log("TEST OK частота %d MHz\n", finalFreq);
    }
    catch (errException& ex)
    {
        Error("TEST ERR %s\n", ex.error());
    }
    return 0;
}
