#include <string>
#include <cstring>      // Для memset
#include <unistd.h>     // Для close
#include <errno.h>      // Для errno, strerror

#include "debugsystem.h"

#include "main.h"

#include "firmwaredata.h"

using namespace std;

//-------------------------------------------------------------------------------------------
#undef  UNIT
#define UNIT  g_DebugSystem.units[ FWMAC_UNIT ]
#include "log_macros.def"

//-------------------------------------------------------------------------------------------

FirmwareData::FirmwareData()
{FUNCTION_TRACE
    runCmd = g_DebugSystem.conf->ReadString("Fw", "bin");
    nic = g_DebugSystem.conf->ReadString("Fw", "nic");
}

//-------------------------------------------------------------------------------------------

FirmwareData::~FirmwareData() {
FUNCTION_TRACE

}

//-------------------------------------------------------------------------------------------

std::string FirmwareData::getMac()
{
    std::string pathMac = stringformat("/sys/class/net/%s/address", nic.c_str());
    mac = readfilestr(pathMac);
    return mac;
}

//-------------------------------------------------------------------------------------------

bool FirmwareData::firmware(const std::string& newSn, const std::string& newUuid, const std::string& newMac )
{
    if (newSn.empty()) return false;
    if (newUuid.empty()) return false;
    if (newMac.empty()) return false;
    if (runCmd.empty()) return false;
    if (!fileexist(g_DebugSystem.fullpath( runCmd ) )) return false;
    std::string cmd = stringformat("sudo %s %s %s %s", g_DebugSystem.fullpath( runCmd ).c_str(), newSn.c_str(), newUuid.c_str(), newMac.c_str());
    string outData = "";
    int res = GetProcessDataTimeout(cmd.c_str(), outData, 10000);
    if (res != 0) return false;
    return true;
}

//-------------------------------------------------------------------------------------------

