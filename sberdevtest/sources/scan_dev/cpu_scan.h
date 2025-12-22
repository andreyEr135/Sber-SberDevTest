#ifndef CPU_SCAN_H
#define CPU_SCAN_H

#include <string>

// Предполагается, что эти заголовки предоставляют необходимые утилиты
#include "sysutils.h"   // для Log, stringformat, stringsplit, trim и т.д.
#include "debugsystem.h" // для CLR, макросов цветов

/**
 * @brief Класс для сбора информации о процессоре и проверки его характеристик.
 */
class CpuScanner
{
public:
    CpuScanner();

    /**
     * @brief Активирует режим тестирования с заданными пороговыми значениями.
     * @param minFreq Минимально допустимая частота (в МГц).
     * @param coreCount Минимально допустимое количество ядер.
     */
    void setActiveTest(float minFreq, int coreCount);

    /**
     * @brief Деактивирует режим тестирования.
     */
    void setDeactiveTest();

    /**
     * @brief Основной метод, выполняющий сканирование и проверку.
     * @return Всегда возвращает true, указывая на то, что сканирование было выполнено.
     */
    bool test();

private:
    // --- Параметры теста ---
    bool testActive;
    float minimalFreq;   // Ожидаемая минимальная частота
    int   countCores;    // Ожидаемое количество ядер

    // --- Внутренняя структура для частот ---
    typedef struct {
        float scaling_min_freq;
        float scaling_cur_freq;
        float scaling_max_freq;
    } CpuFreqs;

    // --- Вспомогательные методы ---
    /**
     * @brief Получает модель и количество ядер CPU с помощью команды 'lscpu'.
     * @param model Ссылка на строку для записи модели процессора.
     * @param cores Ссылка на строку для записи количества ядер.
     * @return true, если информация была успешно получена.
     */
    bool getCpuInfo(std::string &model, std::string &cores);

    /**
     * @brief Получает частоты из файловой системы sysfs.
     * @param freqs Ссылка на структуру для записи частот.
     * @return true, если частоты были успешно прочитаны.
     */
    bool getFreq(CpuFreqs &freqs);

    /**
     * @brief Получает текущую частоту из /proc/cpuinfo (метод резерва).
     * @return Текущая частота в МГц или 0 в случае ошибки.
     */
    float getFreqFromCpuinfo();
};

#endif // CPU_SCAN_H
