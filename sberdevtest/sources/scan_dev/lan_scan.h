#ifndef LAN_SCAN_H
#define LAN_SCAN_H

#include <string>
#include <vector>
#include "scan.h"
#include "sysutils.h"
#include "debugsystem.h"

/**
 * @brief Класс для сканирования и валидации сетевых интерфейсов (LAN).
 */
class LanScanner
{
public:
    LanScanner();
    ~LanScanner();

    void setActiveTest(int expectedCount);
    void setDeactiveTest();
    bool test(int configCount);

private:
    struct LanCriteria {
        int index;
        std::string name;
        std::vector<std::string> allowedMacs;
        int requiredSpeed = 0;
    };

    Tscan* scan;
    bool testActive;
    int expectedCount;
    int foundCount;
    std::string errorSummary;

    /**
     * @brief Ищет интерфейс и проверяет его параметры (скорость, MAC).
     */
    bool findAndCheckLan(const LanCriteria& criteria);

    /**
     * @brief Читает текущий MAC-адрес интерфейса из /sys/class/net.
     */
    std::string readMacAddress(const std::string& ifName);
};

#endif // LAN_SCAN_H
