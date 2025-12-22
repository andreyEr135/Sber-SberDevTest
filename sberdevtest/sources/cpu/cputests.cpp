//#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <math.h>

#include "debugsystem.h"
#include "cpu.h"
#include "cputests.h"

// --- Структуры данных для потоков ---

struct SWorkerStats
{
    int coreId;
    pthread_t threadHandle;
    unsigned long long volatile current_iter;
    double error_margin;
    double run_duration;
};

// Глобальные переменные модуля (переименованы)
SWorkerStats* g_WorkerPool = nullptr;
timeval g_BenchStartTime;

// --- Вспомогательные функции потоков ---

void* compute_pi_task(void *context)
{
    SWorkerStats* stats = (SWorkerStats*)context;
    DTHRD("{pi_core_%d}", stats->coreId)

    Trace("Поток #%d: pid=%d thread_pid=%d\n", stats->coreId, getpid(), getthreadpid());

    // Привязка потока к конкретному ядру процессора
    cpu_set_t affinity_mask;
    CPU_ZERO(&affinity_mask);
    CPU_SET(stats->coreId, &affinity_mask);

    if (sched_setaffinity(0, sizeof(cpu_set_t), &affinity_mask) != 0)
    {
        Error("Ошибка: не удалось установить affinity для ядра %d\n", stats->coreId);
    }

    double pi_estimate, divisor, sign;

    do
    {
        pi_estimate = 1.0;
        divisor = 3.0;
        sign = -1.0;

        // Основной цикл вычисления числа Пи (ряд Лейбница)
        for (stats->current_iter = 0; stats->current_iter < g_CpuCtx.pi_depth; stats->current_iter++)
        {
            pi_estimate += sign / divisor;
            divisor += 2.0;
            sign *= -1.0;
        }
    }
    while (TimeSec(g_BenchStartTime) < g_CpuCtx.duration_sec);

    pi_estimate *= 4.0;
    stats->error_margin = fabs(M_PI - pi_estimate);
    stats->run_duration = TimeSec(g_BenchStartTime);

    pthread_exit(NULL);
    return NULL;
}

// --- Основные тестовые функции ---
int run_pi_benchmark()
{
    FUNCTION_TRACE
    Trace("Запуск Pi теста: ядер=%d, итераций=%llu\n", g_CpuCtx.coreCount, g_CpuCtx.pi_depth);
    Trace("Main: pid=%d thread_pid=%d\n", getpid(), getthreadpid());

    // Выделение памяти под статистику воркеров
    g_WorkerPool = (SWorkerStats*)calloc(g_CpuCtx.coreCount, sizeof(SWorkerStats));

    g_BenchStartTime = TimeStart();

    // Запуск потоков по количеству ядер
    for (int idx = 0; idx < g_CpuCtx.coreCount; idx++)
    {
        g_WorkerPool[idx].coreId = idx;
        g_WorkerPool[idx].error_margin = 0.0;

        if (pthread_create(&g_WorkerPool[idx].threadHandle, NULL, compute_pi_task, (void*)&g_WorkerPool[idx]) != 0)
        {
            Error("Не удалось создать поток для ядра %d\n", idx);
            return -1;
        }
    }

    int status_code = 0;

    // Ожидание завершения всех потоков
    for (int idx = 0; idx < g_CpuCtx.coreCount; idx++)
    {
        pthread_join(g_WorkerPool[idx].threadHandle, NULL);
        Trace("Ядро %d: время %.3f сек, погрешность=%.16f\n", idx, g_WorkerPool[idx].run_duration, g_WorkerPool[idx].error_margin);

        if (status_code == 0)
        {
            status_code = (g_WorkerPool[idx].error_margin < 0.0001) ? 0 : -1;
        }

        if (status_code == -1)
        {
            snprintf(g_GlobalErrorBuffer, sizeof(g_GlobalErrorBuffer),
                     "Pi погрешность %.6f превышает лимит 0.0001", g_WorkerPool[idx].error_margin);
        }
    }

    free(g_WorkerPool);
    g_WorkerPool = nullptr;

    return status_code;
}

void update_pi_bench_progress()
{
    FUNCTION_TRACE

    if (g_CpuCtx.duration_sec > 0)
    {
        // Если запущен стресс-тест по времени
        g_TotalSteps = (unsigned long long)g_CpuCtx.duration_sec * 1000;
        g_CurrentStep = TimeMilli(g_BenchStartTime);
    }
    else
    {
        // Если расчет идет по количеству итераций
        g_CurrentStep = 0;
        g_TotalSteps = g_CpuCtx.pi_depth * g_CpuCtx.coreCount;

        if (g_WorkerPool)
        {
            for (int idx = 0; idx < g_CpuCtx.coreCount; idx++)
            {
                g_CurrentStep += g_WorkerPool[idx].current_iter;
            }
        }
    }
}
