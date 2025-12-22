#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>

#include <string>
#include <vector>
#include <iterator>
#include <utility>

#include "debugsystem.h"
#include "memtests.h"
#include "_version"

using namespace std;

//-------------------------------------------------------------------------------------------

struct STestAlgorithm
{
    const char* description;
    int (*execute)(ulv* bufferA, ulv* bufferB, size_t elementCount);
};

// Оставляем только Random Pattern Comparison
STestAlgorithm g_SelectedTest = { "Random Pattern Comparison", test_random_pattern_comparison };

struct SMemoryTestContext
{
    void volatile* rawAllocation;   // Указатель на выделенную память
    void volatile* alignedBuffer;   // Выровненный указатель для тестов
    size_t         bufferSize;      // Общий размер буфера в байтах
    int            deviceHandle;    // Дескриптор /dev/mem (если используется)
};

SMemoryTestContext g_MemContext;
bool g_ManageSwap = false;
bool g_DropCaches = false;
timer_t g_UpdateTimer;

// Внешние переменные из memtests.h для отслеживания прогресса
extern char memerrorstr[];

//-------------------------------------------------------------------------------------------

void ReleaseResources()
{
    FUNCTION_TRACE
    if (g_MemContext.rawAllocation)
    {
        free((void*)g_MemContext.rawAllocation);
        g_MemContext.rawAllocation = nullptr;
    }

    if (g_MemContext.deviceHandle)
    {
        close(g_MemContext.deviceHandle);
        g_MemContext.deviceHandle = 0;
    }

    if (g_ManageSwap)
    {
        Log("<UI> Enabling swap space...\n");
        if (system("/sbin/swapon -a") != 0) Error("Failed to enable swap\n");
    }
}

//-------------------------------------------------------------------------------------------

void InitializeMemoryBuffer()
{
    FUNCTION_TRACE
    Log("<UI> Initializing Memory Buffer\n");

    g_MemContext.rawAllocation = nullptr;
    g_MemContext.alignedBuffer = nullptr;
    g_MemContext.bufferSize = 0;

    int sizePercent = g_DebugSystem.conf->ReadInt("MEM", "sizepercent");
    if (sizePercent <= 0) sizePercent = 90;

    struct sysinfo systemStats;
    sysinfo(&systemStats);

    uint64_t totalFreeRam = (uint64_t)systemStats.freeram * systemStats.mem_unit;
    uint64_t targetTestRam = (totalFreeRam / 100) * sizePercent;

    // Определение размера страницы
    long pageSizeValue = sysconf(_SC_PAGE_SIZE);
    if (pageSizeValue == -1) pageSizeValue = 4096;

    size_t pageSize = (size_t)pageSizeValue;
    size_t pageMask = ~(pageSize - 1);

    size_t currentRequestSize = (size_t)targetTestRam;
    void volatile* allocatedPtr = nullptr;

    // Пытаемся выделить память, уменьшая запрос при неудаче
    while (!allocatedPtr && (currentRequestSize > 0))
    {
        allocatedPtr = (void volatile*)malloc(currentRequestSize);
        if (!allocatedPtr) currentRequestSize -= pageSize;
    }

    if (!allocatedPtr) throw errException("Out of memory: cannot allocate test buffer");

    // Выравнивание по границе страницы
    void volatile* alignedPtr = nullptr;
    if ((size_t)allocatedPtr % pageSize)
    {
        alignedPtr = (void volatile*)(((size_t)allocatedPtr & pageMask) + pageSize);
        currentRequestSize -= ((size_t)alignedPtr - (size_t)allocatedPtr);
    }
    else
    {
        alignedPtr = allocatedPtr;
    }

    g_MemContext.rawAllocation = allocatedPtr;
    g_MemContext.alignedBuffer = alignedPtr;
    g_MemContext.bufferSize = currentRequestSize;

    Log("Available RAM: %llu Mb\n", totalFreeRam >> 20);
    Log("Allocated for Test: %zu Mb (%llu%%) at 0x%zx\n",
        g_MemContext.bufferSize >> 20,
        (uint64_t)g_MemContext.bufferSize * 100 / totalFreeRam,
        (size_t)g_MemContext.alignedBuffer);
}

//-------------------------------------------------------------------------------------------

void OnProgressUpdate(int /*signo*/)
{
    // Расчет прогресса текущей итерации
    uint64_t progress = (iteration_count > 0) ? (iteration_num * 100 / iteration_count) : 0;

    Log("<UI> Running: %s [%llu%%]\n", g_SelectedTest.description, progress);
    Log("<INFO> 1/1 %llu%%\n", progress);
}

//-------------------------------------------------------------------------------------------

void ExecuteMemoryValidation()
{
    FUNCTION_TRACE
    bool hasFailed = false;
    errException savedException;

    try
    {
        Log("<UI> Locking address space into RAM...\n");
        if (mlock((void*)g_MemContext.alignedBuffer, g_MemContext.bufferSize) < 0)
            throw errException("mlock failed - check privileges");

        size_t halfBufferBytes = g_MemContext.bufferSize / 2;
        size_t elementsPerHalf = halfBufferBytes / sizeof(size_t);

        ulv* bufferA = (ulv*)g_MemContext.alignedBuffer;
        ulv* bufferB = (ulv*)((size_t)g_MemContext.alignedBuffer + halfBufferBytes);

        int testDurationSec = time2seconds(g_DebugSystem.getparam_or_default("MEM", "time"));
        timeval startTime = TimeStart();

        Log("Starting stress test: %s\n", g_SelectedTest.description);

        do
        {
            // Запуск таймера обновления UI (раз в 500мс)
            if (StartTimerOwnThread(&g_UpdateTimer, 0, 500000, &OnProgressUpdate, SIGUSR1) != 0)
                Error("Failed to start UI timer\n");

            int status = g_SelectedTest.execute(bufferA, bufferB, elementsPerHalf);

            StopTimer(g_UpdateTimer);

            if (status != 0)
            {
                Error("Memory verification failed: %s\n", memerrorstr);
                throw errException(memerrorstr);
            }

            Log("Iteration completed successfully\n");

        } while (TimeSec(startTime) < testDurationSec);
    }
    catch (errException& e)
    {
        savedException = e;
        hasFailed = true;
    }

    if (g_MemContext.alignedBuffer)
        munlock((void*)g_MemContext.alignedBuffer, g_MemContext.bufferSize);

    if (hasFailed) throw savedException;
}

//-------------------------------------------------------------------------------------------

bool HandleShutdown(int /*signum*/)
{
    FUNCTION_TRACE
    Log("Memory test interrupted by user\n");
    StopTimer(g_UpdateTimer);
    ReleaseResources();
    return true;
}

//-------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    g_DebugSystem.stdbufoff();
    g_DebugSystem.init(argc, argv, &VERSION, true);
    g_DebugSystem.OnStopSignal = &HandleShutdown;
    FUNCTION_TRACE

    g_ManageSwap = g_DebugSystem.conf->ReadInt("MEM", "swapoff");
    g_DropCaches = g_DebugSystem.conf->ReadInt("MEM", "clearcaches");

    try
    {
        if (g_DebugSystem.checkparam("-h"))
        {
            Log("Usage: %s [time=10m]\n", g_DebugSystem.exe.c_str());
            return 0;
        }
        if (g_ManageSwap)
        {
            Log("<UI> Disabling swap...\n");
            if (system("/sbin/swapoff -a") != 0) Error("Failed to disable swap\n");
        }

        if (g_DropCaches)
        {
            Log("<UI> Dropping system caches...\n");
            if (system("sync") == -1) {
                Error("Failed to run sync\n");
            }
            if (system("echo 3 > /proc/sys/vm/drop_caches") != 0) Error("Failed to drop caches\n");
        }

        InitializeMemoryBuffer();
        ExecuteMemoryValidation();

        Log("TEST OK %s passed\n", g_SelectedTest.description);
    }
    catch (errException& e)
    {
        Error("TEST ERR: %s\n", e.error());
    }

    ReleaseResources();
    return 0;
}
