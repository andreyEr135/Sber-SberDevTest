#ifndef PCI_SCAN_H
#define PCI_SCAN_H

#include <string>
#include <vector>
#include "sysutils.h"
#include "debugsystem.h"
#include "scan.h"

/**
 * @brief Класс для сканирования и валидации PCI устройств.
 */
class PciScanner
{
public:
    PciScanner();
    ~PciScanner();

    void setActiveTest(int expectedCount);
    void setDeactiveTest();
    bool test(int configCount);

private:
    struct PciCriteria {
        int index;
        std::string path;
        std::string vidpid;
        std::string genStr; // Ожидаемое значение, например "3x4"
        int expectedGen = -1;
        int expectedX = -1;
    };

    Tscan* scan;
    bool testActive;
    int expectedCount;
    int foundCount;
    std::string errorSummary;

    /**
     * @brief Ищет устройство по пути и VID/PID, проверяет версию PCI-Gen.
     */
    bool findAndCheckDevice(PciCriteria& criteria);

    /**
     * @brief Парсит строку вида "3x16" в отдельные числа gen и x.
     */
    void parseGenString(PciCriteria& criteria);
};

#endif // PCI_SCAN_H
