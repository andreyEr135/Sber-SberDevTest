#include "usb_scan.h"
#include "debugsystem.h"
#include <sstream>

UsbScanner::UsbScanner() : scan(nullptr), testActive(false), expectedCount(0), foundCount(0)
{
    scan = new Tscan(" usb blk com ");
}

UsbScanner::~UsbScanner()
{
    delete scan;
}

void UsbScanner::setActiveTest(int _count)
{
    testActive = true;
    expectedCount = _count;
}

void UsbScanner::setDeactiveTest()
{
    testActive = false;
    expectedCount = 0;
}

bool UsbScanner::isKnownDevice( TFindUsb& usb)
{
    std::string vidpid = usb.record.str(USB_VENDEVSTR);
    std::string driver = usb.driver;

    for (const auto& kvp : knownVidPids) {
        if (kvp == vidpid) return true;
    }
    for (const auto& kd : knownDrivers) {
        if (kd == driver) return true;
    }
    return false;
}

int UsbScanner::findAndCheckUsb(const UsbCriteria& criteria)
{
    Tstrlist ports;
    stringsplit(criteria.busport, '|', ports);

    TFindUsb usb;
    for (const auto& port : ports) {
        usb = scan->fUsb(port.c_str());
        if (usb) break;
    }

    if (!usb) return 0; // Совсем не нашли

    // Проверка по белому списку (VID/PID или драйвер)
    if (!isKnownDevice(usb)) return 0;

    bool typeError = false;
    std::string speedInfo;

    // Проверка версии USB (BCD: 0x0300 -> USB 3.0)
    if (criteria.requiredType == 30) {
        char bcdStr[10];
        snprintf(bcdStr, sizeof(bcdStr), "%X%X", usb.bcd[0], usb.bcd[1]);
        int actualMode = std::atoi(bcdStr);

        if (actualMode < criteria.requiredType) {
            typeError = true;
            speedInfo = stringformat(" (тип %s не соответствует USB 3.0)", bcdStr);

            if (!errorSummary.empty()) errorSummary += "; ";
            errorSummary += stringformat("USB-%d: версия %s", criteria.index, bcdStr);
        }
    }

    std::string devName = usb.record.str(USB_NAME);
    if (devName.empty()) devName = usb.driver;

    if (typeError) {
        Log("USB-%d: %s : %s" CLR(f_LIGHTRED) "%s" CLR0 "\n",
            criteria.index, usb.record.str(USB_BUSPORT), devName.c_str(), speedInfo.c_str());
        return -1;
    } else {
        Log("USB-%d: %s : %s\n", criteria.index, usb.record.str(USB_BUSPORT), devName.c_str());
        return 1;
    }
}

bool UsbScanner::test(int configCount)
{
    Log("\033[32mПоиск USB устройств\033[0m\n");
    if (!scan->open()) return false;

    foundCount = 0;
    errorSummary = "";

    // Загружаем белые списки
    std::string vids = g_DebugSystem.conf->ReadString("USB", "known_vid_pid");
    stringsplit(vids, ',', knownVidPids);

    std::string drivers = g_DebugSystem.conf->ReadString("USB", "known_drivers");
    stringsplit(drivers, ',', knownDrivers);

    for (int i = 1; i <= configCount; i++)
    {
        std::string config = g_DebugSystem.conf->ReadString("USB", stringformat("usb%d", i));
        Tstrlist params;
        stringsplit(config, ',', params);

        UsbCriteria criteria;
        criteria.index = i;
        criteria.requiredType = 0;

        for (auto& p : params) {
            if (p.find("busport=") == 0) {
                // Извлекаем значение между 'busport=' и возможным символом в конце (как в оригинале)
                criteria.busport = p.substr(8);
                criteria.busport.erase(
                    std::remove(criteria.busport.begin(), criteria.busport.end(), '\"'),
                    criteria.busport.end()
                );
                if (!criteria.busport.empty() && (criteria.busport.back() == '/' || criteria.busport.back() == ' '))
                    criteria.busport.pop_back();
            } else if (p.find("type=") == 0) {
                criteria.requiredType = std::atoi(p.substr(5).c_str());
            }
        }

        int res = findAndCheckUsb(criteria);
        if (res == 1) foundCount++;
        else if (res == -1) foundCount++; // Нашли, но с ошибкой типа (по логике оригинала)
        else Log("USB-%d: " CLR(f_LIGHTRED) "не найдено!" CLR0 "\n", i);
    }

    Log("\n");
    scan->close();

    if (testActive) {
        if (foundCount == 0) {
            Log("C|CH=USB $ERR=не найдено\n");
        } else {
            std::string status = (foundCount < expectedCount || !errorSummary.empty()) ? "$ERR" : "$OK";
            std::string resStr = stringformat("C|CH=USB %s=найдено %d/%d", status.c_str(), foundCount, expectedCount);
            if (!errorSummary.empty()) resStr += "; " + errorSummary;
            Log("%s\n", resStr.c_str());
        }
    }

    return true;
}
