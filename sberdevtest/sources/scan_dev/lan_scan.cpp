#include "lan_scan.h"
#include <fstream>
#include <algorithm>

LanScanner::LanScanner() : scan(nullptr), testActive(false), expectedCount(0), foundCount(0)
{
    scan = new Tscan(" net ");
}

LanScanner::~LanScanner()
{
    delete scan;
}

void LanScanner::setActiveTest(int _count)
{
    testActive = true;
    expectedCount = _count;
}

void LanScanner::setDeactiveTest()
{
    testActive = false;
    expectedCount = 0;
}

std::string LanScanner::readMacAddress(const std::string& ifName)
{
    std::string path = "/sys/class/net/" + ifName + "/address";
    std::ifstream ifs(path);
    std::string mac;
    if (ifs >> mac) {
        return trim(mac);
    }
    return "";
}

bool LanScanner::findAndCheckLan(const LanCriteria& criteria)
{
    TnetIFs netifs;
    if (!getIFs(netifs)) {
        Log(CLR(f_LIGHTRED) "Ошибка получения списка интерфейсов (getIFs)" CLR0 "\n");
        return false;
    }

    for (TtblRecord p = scan->net->first(); p; ++p)
    {
        std::string currentType = p.str(NET_TYPE);
        std::string currentName = p.str(NET_NAME);

        if (currentType == "can") continue;
        if (currentName != criteria.name) continue;

        // 1. Проверка скорости
        TnetIF* netif = findIFByName(netifs, currentName.c_str());
        std::string speedLog;
        bool speedOk = true;

        if (!netif || netif->speed < 0) {
            speedLog = "кабель не подключен";
            speedOk = false;
        } else {
            speedLog = stringformat("speed: %d Mbit/s", netif->speed);
            if (netif->speed < criteria.requiredSpeed) speedOk = false;
        }

        Log("NAME: %s [%s]\n", currentName.c_str(), speedLog.c_str());

        if (!speedOk && testActive) {
            if (!errorSummary.empty()) errorSummary += "; ";
            errorSummary += currentName + ":" + speedLog;
        }

        // 2. Проверка MAC-адреса
        std::string actualMac = readMacAddress(currentName);
        if (!criteria.allowedMacs.empty()) {
            bool macMatched = false;
            for (const auto& mac : criteria.allowedMacs) {
                if (actualMac.find(mac) != std::string::npos) {
                    macMatched = true;
                    break;
                }
            }

            if (macMatched) {
                Log("MAC (OK): %s\n", actualMac.c_str());
            } else {
                Log(CLR(f_LIGHTRED) "MAC (ERR): %s" CLR0 "\n", actualMac.c_str());
                if (testActive) {
                    if (!errorSummary.empty()) errorSummary += "; ";
                    errorSummary += currentName + ":MAC err";
                }
            }
        } else {
            Log("MAC: %s\n", actualMac.c_str());
        }

        return true;
    }

    Log(CLR(f_LIGHTRED) "LAN %s: не найден!" CLR0 "\n", criteria.name.c_str());
    return false;
}

bool LanScanner::test(int configCount)
{
    Log("\033[32mПоиск LAN устройств\033[0m\n");
    if (!scan->open()) return false;

    foundCount = 0;
    errorSummary = "";

    for (int i = 1; i <= configCount; i++)
    {
        std::string config = g_DebugSystem.conf->ReadString("NET", stringformat("lan%d", i));
        Tstrlist params;
        if (stringsplit(config, ',', params) < 1) continue;

        LanCriteria criteria;
        criteria.index = i;

        for (const auto& p : params) {
            if (p.find("name=") == 0) criteria.name = p.substr(5);
            else if (p.find("mac=") == 0) stringsplit(p.substr(4), '|', criteria.allowedMacs);
            else if (p.find("speed=") == 0) criteria.requiredSpeed = std::atoi(p.substr(6).c_str());
        }

        if (findAndCheckLan(criteria)) {
            foundCount++;
        }
    }

    Log("\n");
    scan->close();

    if (testActive) {
        if (foundCount == 0) {
            Log("C|CH=LAN $ERR=не найдено\n");
        } else {
            bool hasError = (foundCount < expectedCount) || !errorSummary.empty();
            std::string status = hasError ? "$ERR" : "$OK";
            std::string res = stringformat("C|CH=LAN %s=найдено %d/%d", status.c_str(), foundCount, expectedCount);
            if (!errorSummary.empty()) res += "; " + errorSummary;
            Log("%s\n", res.c_str());
        }
    }

    return true;
}
