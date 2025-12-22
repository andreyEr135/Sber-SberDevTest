#ifndef MEMORY_SCAN_H
#define MEMORY_SCAN_H

#include <string>
#include "sysutils.h"
#include "debugsystem.h"

/**
 * @brief Класс для сбора информации об оперативной памяти и проверки её объема.
 */
class MemoryScanner
{
public:
    MemoryScanner();

    /**
     * @brief Активирует режим тестирования.
     * @param minSizeMb Минимально допустимый объем памяти в МБ.
     */
    void setActiveTest(float minSizeMb);

    void setDeactiveTest();

    /**
     * @brief Выполняет проверку памяти.
     * @return true, если сканирование завершено.
     */
    bool test();

private:
    float minimalSizeMb; // Ожидаемый объем в МБ
    bool testActive;

    /**
     * @brief Извлекает общий объем памяти из системы в КБ.
     */
    long long getTotalMemKb();
};

#endif // MEMORY_H
