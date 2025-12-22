#include "func.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "scan.h"
#include "memory_scan.h"
#include "cpu_scan.h"
#include "hdd_scan.h"
#include "pci_scan.h"
#include "usb_scan.h"
#include "lan_scan.h"

using namespace std;

// Глобальный объект сканера для общей инициализации библиотек
Tscan g_mainScanner(" usb pci blk net ");

// Определение указателей
MemoryScanner *check_mem = nullptr;
CpuScanner    *check_cpu = nullptr;
HddScanner    *check_hdd = nullptr;
PciScanner    *check_pci = nullptr;
UsbScanner    *check_usb = nullptr;
LanScanner    *check_lan = nullptr;

int read_conf(DeviceConfig &config_dev)
{
    // Открываем системный сканер
    if (!g_mainScanner.open()) {
        THROW(g_mainScanner.error());
    }

    string str;
    int count = 0;

    // 1. Процессор
    str = g_DebugSystem.conf->ReadString("CPU", "cpu");
    if (str == "check" && check_cpu) {
        check_cpu->test();
    }

    // 2. Память
    str = g_DebugSystem.conf->ReadString("MEM", "mem");
    if (str == "check" && check_mem) {
        check_mem->test();
    }

    // 3. USB устройства
    count = std::atoi(g_DebugSystem.conf->ReadString("USB", "cnt").c_str());
    if (count > 0 && check_usb) {
        check_usb->test(count);
    }

    // 4. Сетевые интерфейсы (LAN)
    count = std::atoi(g_DebugSystem.conf->ReadString("NET", "cnt").c_str());
    if (count > 0 && check_lan) {
        check_lan->test(count);
    }

    // 5. Дисковые накопители (SATA/NVME)
    count = std::atoi(g_DebugSystem.conf->ReadString("SATA", "cnt").c_str());
    if (count > 0 && check_hdd) {
        check_hdd->test(count);
    }

    // 6. PCI устройства
    count = std::atoi(g_DebugSystem.conf->ReadString("PCI", "cnt").c_str());
    if (count > 0 && check_pci) {
        check_pci->test(count);
    }

    return 1;
}


//Функция вывода помощи на экран
int help()
{
	printf("Помощь \n");
	printf("Программа для поиска всех устройств \n");
	printf(" \n");
	printf("Запуск без ключей: поиск всех устройств\n");
	printf(" \n");
    printf("Возможно сканирование и поиск следующих устройств: \n");
    printf(" - CPU с выводом названия процессора, его частоты, количества ядер; \n");
    printf(" - MEM с выводом объема памяти; \n");
    printf(" - USB2.0 / USB3.0 (); \n");
    printf(" - LAN; \n");
	printf(" - SATA; \n");
    printf(" - PCI; \n");
	printf(" \n");
	printf("Возможен тест следующих устройств: \n");
    printf(" - Проверка максимальной частоты процессора и количества ядер; \n");
    printf(" - Проверка минимального объема ОЗУ; \n");
    printf(" - Подсчет количества USB устройств и проверка его типа (2.0/3.0); \n");
    printf(" - Подсчет количества LAN устройств и проверка скорости линка; \n");
	printf(" - Подсчет количества SATA устройств; \n");
    printf(" - Подсчет количества PCI устройств и проверка скорости линка; \n");
	printf(" \n");
	printf("Формат .conf файла:\n");
	printf("\n");
	printf("[USB]                                           #Поиск USB устройств \n");
    printf("cnt=2                                           # количество устройств usb для поиска\n");
    printf("usb1=busport=\"1-5.2\",type=20                  # поиск первого устройства на шине 1-5.2 с типом 2.0\n");
    printf("usb2=busport=\"1-1|2-4\",type=30                # поиск второго устройства на шине 1-1 или 2-4 с типом 3.0\n");
    printf("known_vid_pid=2357:0138,067B:23A3               # список известных VID:PID через запятую\n");
    printf("known_drivers=usbhid,usb-storage                # список известных драйверов через запятую\n");
	printf("\n");

	printf("[NET]                                          #Поиск Ethernet устройств \n");
    printf("cnt=1                                          # количество устройств Ethernet для поиска\n");
    printf("lan1=name=enp1s0,speed=1000                    # поиск по названию сетевого контроллера и проверкой линка на 1гбит\n");
	printf("[SATA]                                         #Поиск SATA устройств \n");
    printf("cnt=1                                          # количество устройств HDD для поиска\n");
    printf("hdd1=path=0000:00:1c.4,name=NVME,run={dev} read write size=100M   \n  #" \
           "поиск диска на контроллере 0000:00:1c.4, добавление теста с названием NVME \n"
           "со строкой запуска run=...\n");
    printf("[PCI]                                           #Поиск PCI устройств \n");
    printf("cnt=1                                           # количество устройств PCI для поиска\n");
    printf("pci1=path=0000:00:1c.3,vidpid=8086:2725,gen=1x1 # поиск устройства на пути 0000:00:1c.3 с \n" \
           "vidpid=8086:2725 и линком 1x1\n");
	printf("\n");
    printf("Формат ini файла для проверок:\n");
    printf("[check]                                        # ключевое слово проверок\n");
    printf("TESTCPU = min_freq|5.6G count_cores|6          # проверка процессора на минимальную частоту и количество ядер\n");
    printf("TESTMEM = min_size|32G                         # проверка ОЗУ на минимальное количество памяти\n");
    printf("TESTHDD = count|1                              # проверка количества дисков\n");
    printf("TESTUSB = count|8                              # проверка количества USB устройств\n");
    printf("TESTLAN = count|1                              # проверка количества сетевых контроллеров\n");
    printf("LANCHECK =                                     # проверка линка соединения\n");
    printf("TESTPCISLOT = count|1                          # проверка количества PCI устройств\n");

	return 0;
}

 
