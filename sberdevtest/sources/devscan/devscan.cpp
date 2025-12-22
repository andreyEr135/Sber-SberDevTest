#include "debugsystem.h"
#include "sysutils.h"
#include "memtbl.h"
#include "devscan.h"

#include "devscan_pci.h"
#include "devscan_usb.h"
#include "devscan_blk.h"
#include "devscan_net.h"

#include "_version"

using namespace std;

//-------------------------------------------------------------------------------------------

bool g_IsQuietMode = false;

//-------------------------------------------------------------------------------------------

void ClearDeviceTables()
{
    FUNCTION_TRACE
    TmemTable pciMap(PCI_TBL); pciMap.destroy();
    TmemTable usbMap(USB_TBL); usbMap.destroy();
    TmemTable blkMap(BLK_TBL); blkMap.destroy();
    TmemTable netMap(NET_TBL); netMap.destroy();
}

//-------------------------------------------------------------------------------------------

struct SNodeElement { std::string busId; std::string pipeSym; };
typedef std::vector<SNodeElement> TNodeList;
typedef TNodeList::iterator TNodeListIt;


void DisplayPciTreeRecursive(TmemTable* pciTable, std::string targetBus, std::string currentIndent)
{
    FUNCTION_TRACE
    Log("%s", currentIndent.c_str());

    string nextLevelIndent = currentIndent;
    replace(nextLevelIndent, "├", "│");
    replace(nextLevelIndent, "└", " ");

    TtblRecord entry;
    pciTable->find(entry, PCI_BUSSTR, targetBus.c_str());

    if (entry)
    {
        char* rawBus = strchr(entry.str(PCI_BUSSTR), ':') + 1;
        Log(CLR(f_CYAN) " %s " CLR0, rawBus);
        Log(CLR(f_GREEN) "[%s]" CLR0 " gen%dx%d [%s] ",
            entry.str(PCI_VENDEVSTR), entry.get(PCI_GEN), entry.get(PCI_X), entry.str(PCI_DRIVER));
        Log(CLR(f_CYAN) "%s: %s" CLR0, entry.str(PCI_CLASS), entry.str(PCI_NAME));
        Log("\n");
    }
    else
    {
        Log("%s\n", targetBus.c_str());
    }

    TNodeList subDevices;
    for (TtblRecord rec = pciTable->first(); rec; ++rec)
    {
        if (getfile(rec.str(PCI_SYSPATH)) == targetBus)
        {
            SNodeElement item = { rec.str(PCI_BUSSTR), "├" };
            subDevices.push_back(item);
        }
    }

    if (!subDevices.empty())
    {
        subDevices.rbegin()->pipeSym = "└";
    }

    for (TNodeListIt it = subDevices.begin(); it != subDevices.end(); ++it)
    {
        DisplayPciTreeRecursive(pciTable, it->busId, nextLevelIndent + " " + it->pipeSym);
    }
}

//-------------------------------------------------------------------------------------------

void ExecutePciScan()
{
    FUNCTION_TRACE
    ExecutePciBusScan();

    if (g_IsQuietMode) return;

    Log("\n----- PCI SUBSYSTEM -----\n");
    TmemTable pciTbl(PCI_TBL);
    if (!pciTbl.open()) throw errException(pciTbl.error);

    {
        LOCK(pciTbl);
        Tstrlist rootBuses;
        for (TtblRecord rec = pciTbl.first(); rec; ++rec)
        {
            char* pathStr = rec.str(PCI_SYSPATH);
            char* slashPos = strchr(pathStr, '/');
            string rootId = (slashPos) ? string(pathStr, slashPos - pathStr) : string(pathStr);

            if (find(rootBuses.begin(), rootBuses.end(), rootId) == rootBuses.end())
            {
                rootBuses.push_back(rootId);
            }
        }

        for (TstrlistIt it = rootBuses.begin(); it != rootBuses.end(); ++it)
        {
            DisplayPciTreeRecursive(&pciTbl, *it, "");
        }
    }
}

//-------------------------------------------------------------------------------------------

void ExecuteUsbScan()
{
    FUNCTION_TRACE
    ExecuteUsbBusScan();

    if (g_IsQuietMode) return;

    Log("\n----- USB SUBSYSTEM -----\n");
    TmemTable usbTbl(USB_TBL);
    if (!usbTbl.open()) throw errException(usbTbl.error);

    {
        LOCK(usbTbl);
        for (TtblRecord rec = usbTbl.first(); rec; ++rec)
        {
            string detail;
            string devName = trim(string(rec.str(USB_NAME)));
            if (!devName.empty()) detail = devName;

            string serNum = trim(string(rec.str(USB_SERIAL)));
            if (!serNum.empty()) detail += " {" + serNum + "}";

            Log(CLR(f_CYAN) "%-8s" CLR0 " #%d %04X " CLR(f_GREEN) "[%s]" CLR0 " [%s] " CLR(f_CYAN) "%s" CLR0 " %s\n",
                rec.str(USB_BUSPORT), rec.get(USB_DEVNUM), rec.get(USB_BCD),
                rec.str(USB_VENDEVSTR), rec.str(USB_DRIVER), detail.c_str(), rec.str(USB_SYSPATH));
        }
    }
}

//-------------------------------------------------------------------------------------------

void ExecuteBlockDeviceScan()
{
    FUNCTION_TRACE
    ExecuteBlockDeviceEnumeration();

    if (g_IsQuietMode) return;

    Log("\n----- STORAGE DEVICES -----\n");
    TmemTable blkTbl(BLK_TBL);
    if (!blkTbl.open()) throw errException(blkTbl.error);

    {
        LOCK(blkTbl);
        for (TtblRecord rec = blkTbl.first(); rec; ++rec)
        {
            uint64_t bytesCount = *((uint64_t*)rec.ptr(BLK_SIZE));
            Log("[%2d] %s [%s] (%s) %s [%s] %s\n",
                rec.get(TBL_ID), rec.str(BLK_DEV), rec.str(BLK_NAME), rec.str(BLK_SERIAL),
                double2KMG(double(bytesCount), "B").c_str(), rec.str(BLK_DRIVER), rec.str(BLK_MOUNT));
            Log(CLR(f_CYAN) "     %s" CLR0 "\n", rec.str(BLK_SYSPATH));
        }
    }
}

//-------------------------------------------------------------------------------------------

void ExecuteNetworkScan()
{
    FUNCTION_TRACE
    ExecuteNetworkInterfaceScan();

    if (g_IsQuietMode) return;

    Log("\n----- NETWORK INTERFACES -----\n");
    TmemTable netTbl(NET_TBL);
    if (!netTbl.open()) throw errException(netTbl.error);

    {
        LOCK(netTbl);
        for (TtblRecord rec = netTbl.first(); rec; ++rec)
        {
            Log("[%2d] %s {%s} spd %d [%s]\n",
                rec.get(TBL_ID), rec.str(NET_NAME), rec.str(NET_TYPE), rec.get(NET_SPEED), rec.str(NET_DRIVER));
            Log(CLR(f_CYAN) "     %s" CLR0 "\n", rec.str(NET_SYSPATH));
        }
    }
}

//-------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    g_DebugSystem.stdbufoff();
    g_DebugSystem.init(argc, argv, &VERSION, false);
    g_DebugSystem.runonce();
    FUNCTION_TRACE

    g_DebugSystem.logfileopen(g_DebugSystem.fullpath("./" + g_DebugSystem.exe + ".log").c_str());

    bool isUsageRequested = true;
    g_IsQuietMode = g_DebugSystem.checkparam("silent");

    try
    {
        if (g_DebugSystem.checkparam("X")) ClearDeviceTables();

        bool processAll = g_DebugSystem.checkparam("all");

        if (processAll || g_DebugSystem.checkparam("pci"))  { ExecutePciScan();         isUsageRequested = false; }
        if (processAll || g_DebugSystem.checkparam("usb") || g_DebugSystem.checkparam("usbname")) { ExecuteUsbScan(); isUsageRequested = false; }
        if (processAll || g_DebugSystem.checkparam("blk"))  { ExecuteBlockDeviceScan(); isUsageRequested = false; }
        if (processAll || g_DebugSystem.checkparam("net"))  { ExecuteNetworkScan();     isUsageRequested = false; }
    }
    catch (errException& ex)
    {
        Error("%s\n", ex.error());
        return -1;
    }

    if (isUsageRequested)
    {
        Log("%s [all] [pci] [usb{name}] [blk] [net]\n", g_DebugSystem.exe.c_str());
    }

    return 0;
}
