#ifndef FUNC_H
#define FUNC_H

#include <string>
#include "sysutils.h"
#include "debugsystem.h"

// Предварительное объявление классов сканеров
class MemoryScanner;
class CpuScanner;
class HddScanner;
class PciScanner;
class UsbScanner;
class LanScanner;

// Указатели на экземпляры сканеров (инициализируются в main)
extern MemoryScanner *check_mem;
extern CpuScanner    *check_cpu;
extern HddScanner    *check_hdd;
extern PciScanner    *check_pci;
extern UsbScanner    *check_usb;
extern LanScanner    *check_lan;

struct DeviceConfig
{
    // Параметры конфигурации (если требуются для специфичных нужд)
    char rundb[255];
    char n_conf_file[100];
};

/**
 * @brief Читает конфигурацию и запускает соответствующие тесты.
 */
int read_conf(DeviceConfig &config);

/**
 * @brief Вывод справки по использованию программы.
 */
int help();

#endif // FUNC_H
