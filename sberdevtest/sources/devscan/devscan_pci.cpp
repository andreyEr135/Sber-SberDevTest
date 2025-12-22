#include <unistd.h>
#include <sys/types.h>

#include "debugsystem.h"
#include "memtbl.h"
#include "devscan.h"
#include "devscan_pci.h"
#include "libpci.h"

using namespace std;

//-------------------------------------------------------------------------------------------

std::string ResolvePciSystemPath(SPciDeviceUnit &device)
{
    FUNCTION_TRACE
    string symlinkPath = getlink(stringformat("/sys/bus/pci/devices/%04x:%02x:%02x.%d",
                                 device.busAddr[0], device.busAddr[1], device.busAddr[2], device.busAddr[3]));

    if (symlinkPath.empty()) { errno = 0; return ""; }

    string::size_type markerPos = symlinkPath.find("devices/");
    if (markerPos == string::npos) return "";

    return getpath(symlinkPath.substr(markerPos + 8));
}

//-------------------------------------------------------------------------------------------

std::string IdentifyPciDriver(SPciDeviceUnit &device)
{
    FUNCTION_TRACE
    string driverLink = getlink(stringformat("/sys/bus/pci/devices/%04x:%02x:%02x.%d/driver",
                                device.busAddr[0], device.busAddr[1], device.busAddr[2], device.busAddr[3]));

    if (driverLink.empty()) { errno = 0; return ""; }

    return getfile(driverLink);
}

//-------------------------------------------------------------------------------------------

bool FetchHardwareIdsFromSysfs(string pciAddrStr, DWORD *outVendor, DWORD *outDevice, string *outFullName)
{
    string baseSysPath = "/sys/bus/pci/devices/" + pciAddrStr;
    *outFullName = "Device ";

    // Чтение Vendor ID
    string hexString = readfilestr(baseSysPath + "/vendor");
    hexString = trim(hexString.substr(2));
    *outFullName += hexString;
    sscanf(hexString.c_str(), "%X", outVendor);

    // Чтение Device ID
    hexString = readfilestr(baseSysPath + "/device");
    hexString = trim(hexString.substr(2));
    *outFullName = *outFullName + ":" + hexString;
    sscanf(hexString.c_str(), "%X", outDevice);

    return true;
}

//-------------------------------------------------------------------------------------------

int ExecutePciBusScan()
{
    FUNCTION_TRACE
    int operationStatus = 0;

    InitializePciAccess();

    try
    {
        TmemTable pciDataTable(PCI_TBL);
        if (!pciDataTable.exist())
        {
            TtblField pciSchema[] =
            {
                { PCI_BUS,       "bus",        4 },
                { PCI_BUSSTR,    "busstr",    16 },
                { PCI_VENDEV,    "vendev",     4 },
                { PCI_VENDEVSTR, "vendevstr", 12 },
                { PCI_GEN,       "gen",        1 },
                { PCI_X,         "x",          1 },
                { PCI_CLASS,     "class",     64 },
                { PCI_NAME,      "name",     128 },
                { PCI_SYSPATH,   "syspath",  256 },
                { PCI_PARENT,    "parent",     4 },
                { PCI_DRIVER,    "driver",    32 },
                { 0 }
            };
            if (!pciDataTable.create(pciSchema, PCI_COUNT, 8))
                throw errException(pciDataTable.error);
        }

        if (!pciDataTable.open()) throw errException(pciDataTable.error);

        {
            LOCK(pciDataTable);
            pciDataTable.clear();

            SPciDeviceUnit devUnit;
            while (FetchNextPciDevice(devUnit))
            {
                TtblRecord record = pciDataTable.add();
                if (!record) throw errException(pciDataTable.error);

                string resolvedPath = ResolvePciSystemPath(devUnit);
                string parentFolderName = getfile(resolvedPath);

                BYTE parentBusAddr[4];
                memset(parentBusAddr, 0xFF, sizeof(parentBusAddr));

                int tempBusParts[4];
                if (sscanf(parentFolderName.c_str(), "%04x:%02x:%02x.%d",
                    &tempBusParts[0], &tempBusParts[1], &tempBusParts[2], &tempBusParts[3]) == 4)
                {
                    parentBusAddr[0] = (BYTE)tempBusParts[0];
                    parentBusAddr[1] = (BYTE)tempBusParts[1];
                    parentBusAddr[2] = (BYTE)tempBusParts[2];
                    parentBusAddr[3] = (BYTE)tempBusParts[3];
                }

                // Исправление для пустых ID (если libpci не вернула данные)
                if ((devUnit.vendorDeviceId[0] == 0x0000) && (devUnit.vendorDeviceId[1] == 0x0000))
                {
                    DWORD vId, dId;
                    string fullTitle;
                    FetchHardwareIdsFromSysfs(pcibusstr(devUnit.busAddr), &vId, &dId, &fullTitle);
                    devUnit.vendorDeviceId[0] = (WORD)vId;
                    devUnit.vendorDeviceId[1] = (WORD)dId;
                    devUnit.deviceName = fullTitle;
                }

                record.set(PCI_BUS,       devUnit.busAddr);
                record.set(PCI_BUSSTR,    pcibusstr(devUnit.busAddr));
                record.set(PCI_VENDEV,    devUnit.vendorDeviceId);
                record.set(PCI_VENDEVSTR, vendevstr(devUnit.vendorDeviceId));
                record.set(PCI_GEN,       devUnit.pcieGen);
                record.set(PCI_X,         devUnit.pcieLanes);
                record.set(PCI_CLASS,     devUnit.deviceClass);
                record.set(PCI_NAME,      devUnit.deviceName);
                record.set(PCI_SYSPATH,   resolvedPath);
                record.set(PCI_PARENT,    parentBusAddr);
                record.set(PCI_DRIVER,    IdentifyPciDriver(devUnit));
            }
            pciDataTable.sort(PCI_BUS);
        }
    }
    catch (errException& ex)
    {
        Error("%s\n", ex.error());
        operationStatus = -1;
    }

    FinalizePciAccess();

    return operationStatus;
}
