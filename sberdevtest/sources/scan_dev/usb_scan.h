#ifndef USB_SCAN_H
#define USB_SCAN_H

#include <string>
#include <vector>
#include "scan.h"
#include "sysutils.h"

/**
 * @brief Класс для поиска и проверки USB устройств.
 */
class UsbScanner
{
public:
    UsbScanner();
    ~UsbScanner();

    void setActiveTest(int expectedCount);
    void setDeactiveTest();
    bool test(int configCount);

private:
    // Параметры для поиска конкретного порта
    struct UsbCriteria {
        std::string busport;
        int requiredType; // Например, 30 для USB 3.0
        int index;
    };

    Tscan* scan;
    bool testActive;
    int expectedCount;
    int foundCount;

    std::string errorSummary;
    Tstrlist knownVidPids;
    Tstrlist knownDrivers;

    /**
     * @brief Ищет и проверяет конкретное USB устройство.
     * @return 1 - найдено и ок, 0 - не найдено, -1 - найдено, но не тот тип.
     */
    int findAndCheckUsb(const UsbCriteria& criteria);

    /**
     * @brief Проверяет, совпадает ли устройство с белым списком VID/PID или драйверов.
     */
    bool isKnownDevice( TFindUsb& usb );
};

#endif // USB_SCAN_H
