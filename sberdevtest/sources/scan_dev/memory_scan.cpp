#include "memory_scan.h"
#include <fstream>
#include <cmath>

MemoryScanner::MemoryScanner() : minimalSizeMb(0), testActive(false)
{
}

void MemoryScanner::setActiveTest(float minSizeMb)
{
    testActive = true;
    minimalSizeMb = minSizeMb;
}

void MemoryScanner::setDeactiveTest()
{
    testActive = false;
    minimalSizeMb = 0;
}

long long MemoryScanner::getTotalMemKb()
{
    std::ifstream memfile("/proc/meminfo");
    std::string line;

    if (memfile.is_open())
    {
        while (std::getline(memfile, line))
        {
            // Ищем строку MemTotal: 16345678 kB
            if (line.find("MemTotal") != std::string::npos)
            {
                Tstrlist parts;
                // Разбиваем по двоеточию, берем правую часть
                if (stringsplit(line, ':', parts, true) == 2)
                {
                    std::string valuePart = trim(parts[1]);
                    Tstrlist valAndUnit;
                    // Отделяем число от "kB"
                    if (stringsplit(valuePart, ' ', valAndUnit, true) >= 1)
                    {
                        return std::atoll(valAndUnit[0].c_str());
                    }
                }
                break;
            }
        }
    }
    return 0;
}

bool MemoryScanner::test()
{
    Log("\033[32mПоиск информации об ОЗУ\033[0m\n");

    long long totalKb = getTotalMemKb();
    if (totalKb <= 0) {
        Log(CLR(f_LIGHTRED) "Ошибка: Не удалось прочитать /proc/meminfo" CLR0 "\n");
        if (testActive) Log("C|CH=MEM $ERR=Данные недоступны\n");
        return false;
    }

    // Переводим в GB (используем 1024*1024 для двоичных ГБ или 1000*1000 для рыночных)
    // Большинство BIOS и системных утилит округляют 7.8 ГБ до 8 ГБ.
    double memGbActual = (double)totalKb / (1024.0 * 1024.0);
    double memMbActual = (double)totalKb / 1024.0;

    // Округляем до ближайшего целого для красивого вывода (например, 16 GB)
    double memGbRounded = std::round(memGbActual);

    bool isError = false;
    if (testActive) {
        // Сравниваем реальные МБ с минимальным порогом
        if (memMbActual < (minimalSizeMb * 0.95)) { // Даем 5% допуска на hardware reserved
            isError = true;
        }
    }

    std::string resStr = stringformat("Объем памяти: %.0f GB (физически: %.2f GB)",
                                      memGbRounded, memGbActual);

    if (isError) {
        Log(CLR(f_LIGHTRED) "%s" CLR0 "\n", resStr.c_str());
        Log("C|CH=MEM $ERR=%.0f GB < %.0f GB\n", memGbRounded, std::round(minimalSizeMb / 1024.0));
    } else {
        Log("%s\n", resStr.c_str());
        if (testActive) {
            Log("C|CH=MEM $OK=%.0f GB\n", memGbRounded);
        }
    }

    Log("\n");
    return true;
}
