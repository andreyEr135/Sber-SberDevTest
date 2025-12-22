#ifndef firmwaredataH
#define firmwaredataH
//-------------------------------------------------------------------------------------------

#include "debugsystem.h"

//-------------------------------------------------------------------------------------------

class FirmwareData
{
private:
    std::string mac;
    std::string runCmd;
    std::string nic;

public:
    // Конструктор: инициализирует дескриптор сокета как невалидный
    FirmwareData();

    // Деструктор: закрывает сокет, если он был открыт
    ~FirmwareData();

    std::string getMac();
    bool firmware(const std::string& newSn, const std::string& newUuid, const std::string& newMac );

};
//-------------------------------------------------------------------------------------------



//-------------------------------------------------------------------------------------------
#endif
