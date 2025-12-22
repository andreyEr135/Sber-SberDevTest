#include <dirent.h>

#include "debugsystem.h"
#include "memtbl.h"
#include "sysutils.h"
#include "devscan.h"
#include "devscan_net.h"

using namespace std;

//---------------------------------------------------------------------------

int ExecuteNetworkInterfaceScan()
{
    FUNCTION_TRACE
    int operationStatus = 0;

    try
    {
        TmemTable netInterfaceTable(NET_TBL);
        if (!netInterfaceTable.exist())
        {
            TtblField netSchema[] =
            {
                { NET_NAME,    "name",     16 },
                { NET_TYPE,    "type",     16 },
                { NET_SPEED,   "speed",     2 },
                { NET_SYSPATH, "syspath", 256 },
                { NET_DRIVER,  "driver",   32 },
                { 0 }
            };
            if (!netInterfaceTable.create(netSchema, NET_COUNT, 8))
                throw errException(netInterfaceTable.error);
        }

        if (!netInterfaceTable.open()) throw errException(netInterfaceTable.error);

        {
            LOCK(netInterfaceTable);
            netInterfaceTable.clear();

            //----- Сканирование системного класса сетевых устройств
            const char* netClassPath = "/sys/class/net";
            DIR* interfaceDir = opendir(netClassPath);
            if (interfaceDir == NULL) throw errException("opendir %s", netClassPath);

            struct dirent* dirEntry;
            while ((dirEntry = readdir(interfaceDir)) != NULL)
            {
                string ifacePath = stringformat("%s/%s", netClassPath, dirEntry->d_name);
                string deviceFolderPath = ifacePath + "/device";

                // Пропускаем виртуальные интерфейсы без привязки к физическому устройству
                if (!fileexist(deviceFolderPath)) continue;

                string ifaceName = string(dirEntry->d_name);

                // Определение типа интерфейса
                int rawInterfaceType = readfileint(ifacePath + "/type");
                string interfaceCategory;
                switch (rawInterfaceType)
                {
                    case   1: interfaceCategory = "ether"; break;
                    case 280: interfaceCategory = "can";   break;
                    default : interfaceCategory = "unknown";
                }

                int linkSpeed = readfileint(ifacePath + "/speed");

                // Формирование системного пути (bus path)
                string busIdentity = getlink(ifacePath);
                if (busIdentity.empty()) busIdentity = getlink(ifacePath + "/device");

                string::size_type devicesPos = busIdentity.find("devices/");
                if (devicesPos != string::npos)
                    busIdentity = busIdentity.substr(devicesPos + 8);

                string moduleDriver = getfile(getlink(deviceFolderPath + "/driver"));

                TtblRecord targetRecord = netInterfaceTable.add();
                if (!targetRecord) throw errException(netInterfaceTable.error);

                targetRecord.set(NET_NAME,    ifaceName);
                targetRecord.set(NET_TYPE,    interfaceCategory);
                targetRecord.set(NET_SPEED,   linkSpeed);
                targetRecord.set(NET_SYSPATH, busIdentity);
                targetRecord.set(NET_DRIVER,  moduleDriver);
            }
            closedir(interfaceDir);

            netInterfaceTable.sort(NET_NAME);
        }
    }
    catch (errException& ex)
    {
        Error("%s\n", ex.error());
        operationStatus = -1;
    }

    return operationStatus;
}

//---------------------------------------------------------------------------
