#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <string>
#include <vector>

#include "debugsystem.h"
#include "sysutils.h"
#include "scan.h"
#include "func.h"
#include "errno.h"
#include "_version"

// Заголовочные файлы сканеров
#include "memory_scan.h"
#include "cpu_scan.h"
#include "hdd_scan.h"
#include "pci_scan.h"
#include "usb_scan.h"
#include "lan_scan.h"

using namespace std;

/**
 * @brief Обработчик сигнала остановки.
 */
bool OnStopSignal(int signum) {
    printf("Завершение программы...\n");
    sleep(1);
    return true;
}

/**
 * @brief Разбирает строку параметров тестирования (флаг --check).
 * Формат: $TESTUSB=count|8$TESTLAN=count|1$LANCHECK$TESTCPU=min_freq|2.5G
 */
void configureActiveTests(const string& checkStr) {
    if (checkStr.empty()) return;

    Tstrlist tokens;
    // Разбиваем строку по символу '$'
    stringsplit(checkStr, '$', tokens);

    for (const auto& token : tokens) {
        if (token.empty()) continue;

        string key, value;
        // Пробуем разделить на КЛЮЧ=ЗНАЧЕНИЕ
        if (!stringkeyvalue(token, '=', key, value)) {
            key = token; // Если '=' нет, считаем весь токен ключом (напр. LANCHECK)
        }

        if (key == "TESTUSB") {
            // Ожидаем формат count|N
            string k, v;
            if (stringkeyvalue(value, '|', k, v)) check_usb->setActiveTest(std::atoi(v.c_str()));
        }
        else if (key == "TESTLAN") {
            string k, v;
            if (stringkeyvalue(value, '|', k, v)) check_lan->setActiveTest(std::atoi(v.c_str()));
        }
        else if (key == "LANCHECK") {
            // Включаем расширенную проверку скоростей в LanScanner
            // (Логика внутри LanScanner::test при наличии установленного setActiveTest)
        }
        else if (key == "TESTHDD") {
            string k, v;
            if (stringkeyvalue(value, '|', k, v)) check_hdd->setActiveTest(std::atoi(v.c_str()));
        }
        else if (key == "TESTPCISLOT") {
            string k, v;
            if (stringkeyvalue(value, '|', k, v)) check_pci->setActiveTest(std::atoi(v.c_str()));
        }
        else if (key == "TESTCPU") {
            Tstrlist params;
            stringsplit(value, ' ', params);
            double minFreq = 0;
            int cores = 0;
            for (const auto& p : params) {
                string pk, pv;
                if (stringkeyvalue(p, '|', pk, pv)) {
                    if (pk == "min_freq") minFreq = KMG2double(pv) / (1000 * 1000);
                    else if (pk == "count_cores") cores = std::atoi(pv.c_str());
                }
            }
            if (minFreq > 0 || cores > 0) check_cpu->setActiveTest(minFreq, cores);
        }
        else if (key == "TESTMEM") {
            string k, v;
            if (stringkeyvalue(value, '|', k, v) && k == "min_size") {
                check_mem->setActiveTest(KMG2double(v) / (1000 * 1000));
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // 1. Инициализация системы отладки и логов
    g_DebugSystem.stdbufoff();
    g_DebugSystem.init(argc, argv, NULL, true);
    g_DebugSystem.confcheck(VERSION);
    g_DebugSystem.OnStopSignal = &OnStopSignal;

    Log("Поиск всех устройств и интерфейсов\n", VERSION);

    DeviceConfig config;

    try {
        if (g_DebugSystem.checkparam("-h")) {
            help();
            return 0;
        }

        // 2. Создание объектов сканеров
        check_mem = new MemoryScanner();
        check_cpu = new CpuScanner();
        check_hdd = new HddScanner();
        check_usb = new UsbScanner();
        check_pci = new PciScanner();
        check_lan = new LanScanner();

        // 3. Настройка активных тестов на основе параметров командной строки
        configureActiveTests(g_DebugSystem.getparam("check"));

        // 4. Запуск основного процесса сканирования (читает .conf и выполняет тесты)
        if (read_conf(config) == 1) {
            errno = 0;
        }
    }
    CATCH {
        ERROR; // Вывод системной ошибки в лог
    }

    // 5. Очистка ресурсов
    delete check_mem;
    delete check_cpu;
    delete check_hdd;
    delete check_usb;
    delete check_pci;
    delete check_lan;

    return 0;
}

