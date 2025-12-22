#include "pci_scan.h"

PciScanner::PciScanner() : scan(nullptr), testActive(false), expectedCount(0), foundCount(0)
{
    scan = new Tscan(" pci ");
}

PciScanner::~PciScanner()
{
    delete scan;
}

void PciScanner::setActiveTest(int _count)
{
    testActive = true;
    expectedCount = _count;
}

void PciScanner::setDeactiveTest()
{
    testActive = false;
    expectedCount = 0;
}

void PciScanner::parseGenString(PciCriteria& criteria)
{
    if (criteria.genStr.empty()) return;

    size_t xPos = criteria.genStr.find_first_of("xX");
    if (xPos != std::string::npos) {
        criteria.expectedGen = std::atoi(criteria.genStr.substr(0, xPos).c_str());
        criteria.expectedX = std::atoi(criteria.genStr.substr(xPos + 1).c_str());
    }
}

bool PciScanner::findAndCheckDevice(PciCriteria& criteria)
{
    parseGenString(criteria);

    for (TtblRecord p = scan->pci->first(); p; ++p)
    {
        const char* currentPath = p.str(PCI_SYSPATH);
        const char* currentVidPid = p.str(PCI_VENDEVSTR);

        if (strstr(currentPath, criteria.path.c_str()) && strstr(currentVidPid, criteria.vidpid.c_str()))
        {
            std::string info = stringformat("PCI%d: %s: %s", criteria.index, p.str(PCI_BUSSTR), p.str(PCI_NAME));

            int actualGen = p.get(PCI_GEN);
            int actualX = p.get(PCI_X);
            bool speedError = false;

            if (actualGen > 0 && actualX > 0) {
                info += stringformat(" [gen%dx%d]", actualGen, actualX);

                if (criteria.expectedGen >= 0 && criteria.expectedX >= 0) {
                    if (actualGen == criteria.expectedGen && actualX == criteria.expectedX) {
                        info += " (OK)";
                    } else {
                        speedError = true;
                        std::string err = stringformat("PCI%d: gen%dx%d != %s",
                            criteria.index, actualGen, actualX, criteria.genStr.c_str());

                        if (!errorSummary.empty()) errorSummary += "; ";
                        errorSummary += err;
                        info += stringformat(" (ошибка, ожидается gen%s)", criteria.genStr.c_str());
                    }
                }
            }

            if (speedError) {
                Log(CLR(f_LIGHTRED) "%s" CLR0 "\n", info.c_str());
            } else {
                Log("%s\n", info.c_str());
            }
            return true;
        }
    }

    Log(CLR(f_LIGHTRED) "PCI%d: %s [%s] не найдено!" CLR0 "\n",
        criteria.index, criteria.path.c_str(), criteria.vidpid.c_str());
    return false;
}

bool PciScanner::test(int configCount)
{
    Log("\033[32mПоиск информации о PCI портах\033[0m\n");
    if (!scan->open()) return false;

    foundCount = 0;
    errorSummary = "";

    for (int i = 1; i <= configCount; i++)
    {
        std::string config = g_DebugSystem.conf->ReadString("PCI", stringformat("pci%d", i));
        Tstrlist params;
        int paramCount = stringsplit(config, ',', params);

        if (paramCount < 2) continue;

        PciCriteria criteria;
        criteria.index = i;

        for (const auto& param : params) {
            if (param.find("path=") == 0) criteria.path = param.substr(5);
            else if (param.find("vidpid=") == 0) criteria.vidpid = param.substr(7);
            else if (param.find("gen=") == 0) criteria.genStr = param.substr(4);
        }

        if (findAndCheckDevice(criteria)) {
            foundCount++;
        }
    }

    Log("\n");
    scan->close();

    if (testActive) {
        std::string res;
        bool hasError = false;

        if (foundCount < expectedCount) {
            if (foundCount == 0) res = "не найдены";
            else res = stringformat("кол-во %d/%d", foundCount, expectedCount);
            hasError = true;
        } else {
            res = stringformat("кол-во %d", foundCount);
        }

        if (!errorSummary.empty()) {
            res += "; " + errorSummary;
            hasError = true;
        }

        if (hasError) {
            Log("C|CH=PCI $ERR=%s\n", res.c_str());
        } else {
            Log("C|CH=PCI $OK=%s\n", res.c_str());
        }
    }

    return true;
}
