#include <dirent.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>
#include <vector>

#include "debugsystem.h"
#include "sysutils.h"
#include "memtbl.h"
#include "devscan.h"
#include "devscan_usb.h"

using namespace std;

//-------------------------------------------------------------------------------------------

// Структура для хранения данных о USB устройстве из файловой системы /sys
struct SUsbSystemEntry
{
    int busNumber;
    int deviceAddress;
    string topologyId;  // Например, "1-1.2"
    string busPath;     // Путь в иерархии устройств
    string driverName;
};

typedef vector<SUsbSystemEntry> UsbSystemList;
typedef UsbSystemList::iterator UsbSystemListIt;

//-------------------------------------------------------------------------------------------

void ScanUsbSystemTopology(UsbSystemList& systemDevices)
{
    FUNCTION_TRACE
    const char* usbSysPath = "/sys/bus/usb/devices";
    DIR* sysDir = opendir(usbSysPath);
    if (sysDir == NULL) throw errException("opendir %s", usbSysPath);

    struct dirent* entry;
    while ((entry = readdir(sysDir)) != NULL)
    {
        string entryName = string(entry->d_name);
        if (entryName == "." || entryName == "..") continue;

        string deviceDir = string(usbSysPath) + "/" + entryName;

        int bNum = str2int(entryName);
        int dAddr = readfileint(deviceDir + "/devnum");

        string sysfsPath = getlink(deviceDir);
        string::size_type devPos = sysfsPath.find("devices/");
        if (devPos != string::npos) sysfsPath = sysfsPath.substr(devPos + 8);

        string boundDriver = getfile(getlink(deviceDir + "/driver"));

        SUsbSystemEntry usbEntry = { bNum, dAddr, entryName, sysfsPath, boundDriver };
        systemDevices.push_back(usbEntry);
    }
    closedir(sysDir);
}

//-------------------------------------------------------------------------------------------

// Поиск идентификатора порта по номеру шины и адресу устройства
string FindTopologyIdByAddress(UsbSystemList& sysDevices, int bus, int addr)
{
    FUNCTION_TRACE
    for (UsbSystemListIt it = sysDevices.begin(); it != sysDevices.end(); ++it)
        if (it->busNumber == bus && it->deviceAddress == addr)
            return it->topologyId;
    return "";
}

//-------------------------------------------------------------------------------------------

// Сбор всех драйверов интерфейсов для конкретного устройства (ищет префиксы типа "1-1:")
string RetrieveInterfaceDrivers(UsbSystemList& sysDevices, string baseTopologyId)
{
    FUNCTION_TRACE
    string prefix = baseTopologyId + ":";
    string drivers;
    for (UsbSystemListIt it = sysDevices.begin(); it != sysDevices.end(); ++it)
        if (it->topologyId.find(prefix) != string::npos)
        {
            if (!it->driverName.empty())
                drivers += it->driverName + " ";
        }
    return trim(drivers);
}

//-------------------------------------------------------------------------------------------

// Получение системного пути по идентификатору топологии
string ResolveUsbBusPath(UsbSystemList& sysDevices, string topologyId)
{
    FUNCTION_TRACE
    for (UsbSystemListIt it = sysDevices.begin(); it != sysDevices.end(); ++it)
        if (it->topologyId == topologyId)
            return it->busPath;
    return "";
}

//-------------------------------------------------------------------------------------------

int ExecuteUsbBusScan()
{
    FUNCTION_TRACE
    int operationStatus = 0;

    libusb_context* usbCtx = NULL;
    libusb_device** usbDeviceList = NULL;
    libusb_device_handle* usbHandle = NULL;

    try
    {
        UsbSystemList systemTopology;
        ScanUsbSystemTopology(systemTopology);

        TmemTable usbDataTable(USB_TBL);
        if (!usbDataTable.exist())
        {
            TtblField usbSchema[] =
            {
                { USB_BUSPORT,   "busport",   24, 0 },
                { USB_DEVNUM,    "devnum",     2, 0 },
                { USB_VENDEV,    "vendev",     4, 0 },
                { USB_VENDEVSTR, "vendevstr", 12, 0 },
                { USB_BCD,       "bcd",        2, 0 },
                { USB_SPEED,     "speed",      1, 0 },
                { USB_NAME,      "name",      64, 0 },
                { USB_SERIAL,    "serial",    24, 0 },
                { USB_SYSPATH,   "syspath",  256, 0 },
                { USB_DRIVER,    "driver",    32, 0 },
                { 0, "", 0, 0 }
            };
            if (!usbDataTable.create(usbSchema, USB_COUNT, 8))
                throw errException(usbDataTable.error);
        }

        if (!usbDataTable.open()) throw errException(usbDataTable.error);

        {
            LOCK(usbDataTable);
            usbDataTable.clear();

            int initResult = libusb_init(&usbCtx);
            if (initResult < 0) throw errException(initResult, "libusb_init");
            errno = 0;

            ssize_t totalDevices = libusb_get_device_list(usbCtx, &usbDeviceList);
            errno = 0;

            for (int i = 0; i < totalDevices; i++)
            {
                libusb_device* dev = usbDeviceList[i];
                struct libusb_device_descriptor desc;
                if (libusb_get_device_descriptor(dev, &desc) != LIBUSB_SUCCESS) continue;

                WORD vendorProductId[] = { desc.idVendor, desc.idProduct };
                string vidPidStr = stringformat("%04X:%04X", vendorProductId[0], vendorProductId[1]);

                int busNum = libusb_get_bus_number(dev);
                int devAddr = libusb_get_device_address(dev);
                string topologyId = FindTopologyIdByAddress(systemTopology, busNum, devAddr);
                int usbSpeed = libusb_get_device_speed(dev);

                TtblRecord record = usbDataTable.add();
                if (!record) throw errException(usbDataTable.error);

                record.set(USB_BUSPORT,   topologyId);
                record.set(USB_DEVNUM,    devAddr);
                record.set(USB_VENDEV,    vendorProductId);
                record.set(USB_VENDEVSTR, vidPidStr);
                record.set(USB_BCD,       desc.bcdUSB);
                record.set(USB_SPEED,     usbSpeed);
                record.set(USB_SYSPATH,   ResolveUsbBusPath(systemTopology, topologyId));
                record.set(USB_DRIVER,    RetrieveInterfaceDrivers(systemTopology, topologyId));

                // Попытка получить строковые дескрипторы (Имя и Серийный номер)
                if (libusb_open(dev, &usbHandle) == LIBUSB_SUCCESS)
                {
                    string fullName;
                    unsigned char buffer[256];

                    // Производитель
                    if (desc.iManufacturer > 0)
                        if (libusb_get_string_descriptor_ascii(usbHandle, desc.iManufacturer, buffer, 256) > 0)
                            fullName = trim(string((char*)buffer)) + ": ";

                    // Продукт
                    if (desc.iProduct > 0)
                        if (libusb_get_string_descriptor_ascii(usbHandle, desc.iProduct, buffer, 256) > 0)
                            fullName += trim(string((char*)buffer));

                    record.set(USB_NAME, fullName);

                    // Серийный номер
                    if (desc.iSerialNumber > 0)
                        if (libusb_get_string_descriptor_ascii(usbHandle, desc.iSerialNumber, buffer, 256) > 0)
                            record.set(USB_SERIAL, trim(string((char*)buffer)));

                    libusb_close(usbHandle);
                    usbHandle = NULL;
                }
                errno = 0;
            }
            usbDataTable.sort(USB_BUSPORT);
        }
    }
    catch (errException& ex)
    {
        Error("%s\n", ex.error());
        operationStatus = -1;
    }

    // Очистка ресурсов
    if (usbHandle) libusb_close(usbHandle);
    if (usbDeviceList) libusb_free_device_list(usbDeviceList, 1);
    if (usbCtx) libusb_exit(usbCtx);

    return operationStatus;
}

