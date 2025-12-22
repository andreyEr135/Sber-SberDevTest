#ifndef devscan_pciH
#define devscan_pciH

#include <string>
#include "sysutils.h"

struct SPciDeviceUnit
{
    BYTE busAddr[4];           // [Domain, Bus, Slot, Function]
    WORD vendorDeviceId[2];    // [VendorID, DeviceID]
    int pcieGen;               // Поколение PCIe
    int pcieLanes;             // Количество линий (x1, x4, x16...)
    std::string deviceClass;   // Класс устройства
    std::string deviceName;    // Имя устройства
};

int ExecutePciBusScan();

#endif
