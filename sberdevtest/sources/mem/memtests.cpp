#include "memtests.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

// Глобальные данные для отчетов об ошибках и прогрессе
char memerrorstr[128];
__u64 volatile iteration_num;
__u64 volatile iteration_count;

// Вспомогательные макросы для генерации случайных чисел и работы с разрядностью
#define GET_RAND_32() ((unsigned int)rand() | ((unsigned int)rand() << 16))

#if (ULONG_MAX == 4294967295UL)
    #define GET_RAND_NATIVE() GET_RAND_32()
    #define WORD_ALL_ONES      0xffffffffUL
#elif (ULONG_MAX == 18446744073709551615ULL)
    #define GET_RAND_64()     (((unsigned long)GET_RAND_32()) << 32 | ((unsigned long)GET_RAND_32()))
    #define GET_RAND_NATIVE() GET_RAND_64()
    #define WORD_ALL_ONES      0xffffffffffffffffUL
#else
    #error "Unsupported platform: long is neither 32 nor 64 bits"
#endif

/**
 * Сравнивает два региона памяти поэлементно.
 */
static int VerifyMemoryMatching(ulv* bufferA, ulv* bufferB, size_t count)
{
    ulv* ptrA = bufferA;
    ulv* ptrB = bufferB;

    for (size_t i = 0; i < count; ++i)
    {
        if (*ptrA != *ptrB)
        {
            snprintf(memerrorstr, sizeof(memerrorstr),
                     "Mismatch at offset 0x%zx (ValA: 0x%lx, ValB: 0x%lx)",
                     i * sizeof(unsigned long), (unsigned long)*ptrA, (unsigned long)*ptrB);
            return -1;
        }
        ptrA++;
        ptrB++;
    }
    return 0;
}

/**
 * Реализация теста: Random Pattern Comparison
 */
int test_random_pattern_comparison(ulv* bufferA, ulv* bufferB, size_t elementCount)
{
    iteration_num = 0;
    iteration_count = elementCount * 2; // Два прохода: прямой и инвертированный

    unsigned long pattern = GET_RAND_NATIVE();
    ulv* ptrA = bufferA;
    ulv* ptrB = bufferB;

    // Шаг 1: Заполнение и проверка оригинального паттерна
    for (size_t i = 0; i < elementCount; ++i)
    {
        *ptrA++ = *ptrB++ = pattern;
        iteration_num++;
    }

    if (VerifyMemoryMatching(bufferA, bufferB, elementCount) < 0) return -1;

    // Шаг 2: Заполнение и проверка инвертированного паттерна
    ptrA = bufferA;
    ptrB = bufferB;
    unsigned long invPattern = ~pattern;

    for (size_t i = 0; i < elementCount; ++i)
    {
        *ptrA++ = *ptrB++ = invPattern;
        iteration_num++;
    }

    return VerifyMemoryMatching(bufferA, bufferB, elementCount);
}
