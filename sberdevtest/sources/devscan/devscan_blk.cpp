#include <dirent.h>
#include <sys/stat.h>
#include <linux/hdreg.h>
#include <sys/sysmacros.h>

#include "debugsystem.h"
#include "sysutils.h"
#include "memtbl.h"
#include "devscan.h"
#include "devscan_blk.h"

using namespace std;

//-------------------------------------------------------------------------------------------

struct SMountInfo
{
    int majorId;
    int minorId;
    string mountPoint;
};
typedef std::vector<SMountInfo> TMountList;
typedef TMountList::iterator TMountListIt;

//-------------------------------------------------------------------------------------------

void CollectSystemMountPoints(TMountList& activeMounts)
{
    FUNCTION_TRACE

    // Получаем информацию о корневом устройстве [/]
    struct stat rootNodeStats;
    stat("/", &rootNodeStats);
    SMountInfo rootEntry = { (int)major(rootNodeStats.st_dev), (int)minor(rootNodeStats.st_dev), "/" };
    activeMounts.push_back(rootEntry);

    // Читаем список смонтированных файловых систем
    FILE* procMountsStream = fopen("/proc/mounts", "r");
    if (!procMountsStream) throw errException("не удалось открыть /proc/mounts");

    char lineBuffer[256];
    while (!feof(procMountsStream))
    {
        if (!fgets(lineBuffer, sizeof(lineBuffer), procMountsStream)) break;

        char* parserPtr = lineBuffer;
        char* devNodePath = parserPtr;
        parserPtr = strchr(parserPtr, ' ');
        if (parserPtr) { *parserPtr = '\0'; parserPtr++; }

        char* pathTarget = parserPtr;
        parserPtr = strchr(parserPtr, ' ');
        if (parserPtr) { *parserPtr = '\0'; parserPtr++; }

        struct stat nodeInfo;
        if (stat(devNodePath, &nodeInfo) != 0) { errno = 0; continue; }

        string decodedPath(pathTarget);
        while (replace(decodedPath, "\\040", " "));

        SMountInfo currentMount = { (int)major(nodeInfo.st_rdev), (int)minor(nodeInfo.st_rdev), decodedPath };
        activeMounts.push_back(currentMount);
    }
    fclose(procMountsStream);
}

//-------------------------------------------------------------------------------------------

int ExecuteBlockDeviceEnumeration()
{
    FUNCTION_TRACE
    int statusResult = 0;

    try
    {
        TMountList systemMounts;
        CollectSystemMountPoints(systemMounts);

        for (TMountListIt it = systemMounts.begin(); it != systemMounts.end(); ++it)
            Trace("%d:%d %s\n", it->majorId, it->minorId, it->mountPoint.c_str());

        TmemTable blockTable(BLK_TBL);
        if (!blockTable.exist())
        {
            TtblField schema[] =
            {
                { BLK_DEV,     "dev",      16, 0 },
                { BLK_NAME,    "name",     32, 0 },
                { BLK_SERIAL,  "serial",   32, 0 },
                { BLK_SIZE,    "size",      8, 0 },
                { BLK_MOUNT,   "mount",    64, 0 },
                { BLK_SYSPATH, "syspath", 256, 0 },
                { BLK_DRIVER,  "driver",   32, 0 },
                { 0, "", 0, 0 }
            };
            if (!blockTable.create(schema, BLK_COUNT, 8)) throw errException(blockTable.error);
        }

        if (!blockTable.open()) throw errException(blockTable.error);

        {
            LOCK(blockTable);
            blockTable.clear();

            const char* sysBlockBase = "/sys/block";
            DIR* blockBaseDir = opendir(sysBlockBase);
            if (blockBaseDir == NULL) throw errException("opendir %s", sysBlockBase);

            struct dirent* blockEntry;
            while ((blockEntry = readdir(blockBaseDir)) != NULL)
            {
                string devFolderPath = stringformat("%s/%s", sysBlockBase, blockEntry->d_name);
                string deviceLinkPath = devFolderPath + "/device";

                if (!fileexist(deviceLinkPath)) continue;

                string kernelName = string(blockEntry->d_name);
                string modelTitle;

                if (fileexist(deviceLinkPath + "/model")) modelTitle = readfilestr(deviceLinkPath + "/model");
                if (fileexist(deviceLinkPath + "/name"))  modelTitle = readfilestr(deviceLinkPath + "/name");
                modelTitle = trim(modelTitle);

                uint64_t capacityBytes = readfileuint64(devFolderPath + "/size") * 512;
                string kernelDriver = getfile(getlink(deviceLinkPath + "/driver"));

                string hardwareAddress = getlink(devFolderPath);
                if (hardwareAddress == "") hardwareAddress = getlink(deviceLinkPath);

                string::size_type devSegPos = hardwareAddress.find("devices/");
                if (devSegPos != string::npos) hardwareAddress = hardwareAddress.substr(devSegPos + 8);

                Trace("%s - %s\n", kernelName.c_str(), modelTitle.c_str());

                string partitionMap;
                DIR* devSubDir = opendir(devFolderPath.c_str());
                if (devSubDir != NULL)
                {
                    struct dirent* subItem;
                    while ((subItem = readdir(devSubDir)) != NULL)
                    {
                        if (subItem->d_type != DT_DIR) continue;

                        string partName = string(subItem->d_name);
                        string partDevFile = devFolderPath + "/" + partName + "/dev";

                        if (!fileexist(partDevFile)) continue;

                        string majMinRaw = readfilestr(partDevFile);
                        int majNum = 0, minNum = 0;
                        if (sscanf(majMinRaw.c_str(), "%d:%d", &majNum, &minNum) != 2)
                            Error("ошибка парсинга %s\n", partDevFile.c_str());

                        if (partName == ".") partName = kernelName;

                        Trace("- %d:%d %s\n", majNum, minNum, partName.c_str());

                        for (TMountListIt mntIt = systemMounts.begin(); mntIt != systemMounts.end(); ++mntIt)
                        {
                            if ((majNum == mntIt->majorId) && (minNum == mntIt->minorId))
                            {
                                partitionMap += "{" + partName + ":" + mntIt->mountPoint + "}";
                                break;
                            }
                        }
                    }
                    closedir(devSubDir);
                }

                string diskSerial = "";
                string linuxDevNode = "/dev/" + kernelName;
                int devFd = open(linuxDevNode.c_str(), O_RDONLY | O_NONBLOCK);

                if (devFd >= 0)
                {
                    struct hd_driveid driveId;
                    if (ioctl(devFd, HDIO_GET_IDENTITY, &driveId) == 0)
                    {
                        modelTitle = trim(string((char*)driveId.model, 40));
                        diskSerial = trim(string((char*)driveId.serial_no, 20));
                    }
                    close(devFd);
                }
                errno = 0;

                TtblRecord record = blockTable.add();
                if (!record) throw errException(blockTable.error);

                record.set(BLK_DEV,     kernelName);
                record.set(BLK_NAME,    modelTitle);
                record.set(BLK_SERIAL,  diskSerial);
                record.set(BLK_SIZE,    &capacityBytes);
                record.set(BLK_MOUNT,   partitionMap);
                record.set(BLK_SYSPATH, hardwareAddress);
                record.set(BLK_DRIVER,  kernelDriver);
            }
            closedir(blockBaseDir);
            blockTable.sort(BLK_DEV);
        }
    }
    catch (errException& ex)
    {
        Error("%s\n", ex.error());
        statusResult = -1;
    }
    return statusResult;
}
