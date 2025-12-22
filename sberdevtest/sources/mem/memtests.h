#ifndef MEMTESTS_H
#define MEMTESTS_H

#include <linux/types.h>
#include <stddef.h>

// Определения типов для работы с памятью
typedef unsigned long volatile ulv;

// Глобальные переменные для связи с основным модулем и UI
extern char  memerrorstr[128];
extern __u64 volatile iteration_num;
extern __u64 volatile iteration_count;

/**
 * Выполняет тест оперативной памяти с использованием случайного паттерна.
 * @param bufferA Первый сегмент памяти
 * @param bufferB Второй сегмент (зеркальный)
 * @param elementCount Количество элементов типа unsigned long в каждом сегменте
 * @return 0 при успехе, -1 при обнаружении ошибки
 */
int test_random_pattern_comparison(ulv* bufferA, ulv* bufferB, size_t elementCount);

#endif // MEMTESTS_H
