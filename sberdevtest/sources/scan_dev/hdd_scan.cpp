#include "hdd_scan.h"

HddScanner::HddScanner() : scanner(nullptr), testActive(false), expectedCount(0), foundCount(0)
{
    scanner = new Tscan(" pci blk ");
}

HddScanner::~HddScanner()
{
    delete scanner;
}

void HddScanner::setActiveTest(int _count)
{
    testActive = true;
    expectedCount = _count;
}

void HddScanner::setDeactiveTest()
{
    testActive = false;
    expectedCount = 0;
}

std::string HddScanner::prepareRunString(std::string templateStr, const char* devName)
{
    if (templateStr.empty()) return "";

    std::string tag = "{dev}";
    size_t pos = templateStr.find(tag);
    if (pos != std::string::npos) {
        std::string replacement = (devName && strlen(devName) > 0) ? (std::string("dev=") + devName) : "";
        templateStr.replace(pos, tag.length(), replacement);
    }
    return templateStr;
}

bool HddScanner::findDevice(const HddCriteria& criteria)
{
    for (TtblRecord p = scanner->blk->first(); p; ++p)
    {
        bool isMatch = false;
        if (criteria.use_path) {
            isMatch = (strstr(p.str(BLK_SYSPATH), criteria.path.c_str()) != nullptr);
        } else {
            isMatch = (criteria.device == p.str(BLK_DEV));
        }

        if (isMatch)
        {
            uint64_t actualSize = *((uint64_t*)p.ptr(BLK_SIZE));
            const char* devName = p.str(BLK_NAME);

            Log("dev %s [%s]: %s (%sB)\n",
                (criteria.use_path ? criteria.path.c_str() : criteria.device.c_str()),
                criteria.name.c_str(), devName, double2KMG((double)actualSize, "").c_str());

            // Вывод строки для внешних тестов
            if (!criteria.run_cmd.empty()) {
                std::string runStr = prepareRunString(criteria.run_cmd, p.str(BLK_DEV));
                Log("T|%s|hdd|%s\n", criteria.name.c_str(), runStr.c_str());
            }

            // Валидация объема
            if (testActive && criteria.min_size > 0) {
                if ((double)actualSize < criteria.min_size) {
                    if (!errorSummary.empty()) errorSummary += "; ";
                    errorSummary += stringformat("%s объем %sB < %sB",
                                                 criteria.name.c_str(),
                                                 double2KMG((double)actualSize, "").c_str(),
                                                 double2KMG(criteria.min_size).c_str());
                }
            }
            return true;
        }
    }

    Log("dev %s [%s]: не найден\n",
        (criteria.use_path ? criteria.path.c_str() : criteria.device.c_str()),
        criteria.name.c_str());
    return false;
}

bool HddScanner::test(int configCount)
{
    Log("\033[32mПоиск SATA устройств\033[0m\n");
    if (!scanner->open()) {
        Log("\033[31m не найдено (ошибка открытия сканера)\033[0m\n");
        return false;
    }

    foundCount = 0;
    errorSummary = "";

    for (int i = 1; i <= configCount; i++)
    {
        HddCriteria criteria;
        criteria.name = stringformat("HDD%d", i);

        std::string configStr = g_DebugSystem.conf->ReadString("SATA", stringformat("hdd%d", i));
        Tstrlist params;
        int paramCount = stringsplit(configStr, ',', params);

        if (paramCount < 2) continue;

        for (const auto& param : params) {
            if (param.find("path=") == 0) {
                criteria.path = param.substr(5);
                criteria.use_path = true;
            } else if (param.find("dev=") == 0) {
                criteria.device = param.substr(4);
                criteria.use_path = false;
            } else if (param.find("name=") == 0) {
                criteria.name = param.substr(5);
            } else if (param.find("min_size=") == 0) {
                criteria.min_size = KMG2double(param.substr(9).c_str());
            } else if (param.find("run=") == 0) {
                criteria.run_cmd = param.substr(4);
            }
        }

        if (findDevice(criteria)) {
            foundCount++;
        }
    }

    Log("\n");

    if (testActive)
    {
        if (foundCount == 0) {
            Log("C|CH=HDD $ERR=не найдено\n");
        } else {
            std::string resStr = "C|CH=HDD ";
            if (foundCount < expectedCount) {
                resStr += stringformat("$ERR=найдено %d/%d", foundCount, expectedCount);
                if (!errorSummary.empty()) resStr += "; " + errorSummary;
            } else {
                if (!errorSummary.empty()) {
                    resStr += stringformat("$ERR=найдено %d; %s", foundCount, errorSummary.c_str());
                } else {
                    resStr += stringformat("$OK=найдено %d", foundCount);
                }
            }
            Log("%s\n", resStr.c_str());
        }
    }

    return true;
}
