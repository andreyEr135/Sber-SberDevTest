#ifndef HDD_SCAN_H
#define HDD_SCAN_H

#include <string>
#include <vector>
#include "sysutils.h"
#include "debugsystem.h"
#include "scan.h"

/**
 * @brief Класс для поиска и тестирования HDD/SATA устройств.
 */
class HddScanner
{
public:
    HddScanner();
    ~HddScanner();

    void setActiveTest(int expectedCount);
    void setDeactiveTest();
    bool test(int configCount);

private:
    // Структура для хранения критериев поиска конкретного диска
    struct HddCriteria {
        std::string path;
        std::string device;
        std::string name;
        std::string run_cmd;
        double min_size = 0.0;
        bool use_path = true; // true - поиск по syspath, false - по dev name
    };

    Tscan *scanner;
    bool testActive;
    int  expectedCount;
    int  foundCount;
    std::string errorSummary;

    /**
     * @brief Универсальный метод поиска устройства по заданным критериям.
     */
    bool findDevice(const HddCriteria& criteria);

    /**
     * @brief Заменяет теги в строке запуска (например, {dev} на dev=sda).
     */
    std::string prepareRunString(std::string templateStr, const char* devName);
};

#endif // HDD_SCAN_H
