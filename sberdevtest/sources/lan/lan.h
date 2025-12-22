#ifndef lanH
#define lanH

#include <string>
#include "netservice.h"

// Глобальная строка с итоговой информацией
extern std::string g_LanTestSummary;

// Поиск интерфейса по IP-адресу назначения
TnetIF* FindInterfaceByDestinationIp(std::string ipAddress);

// Основная функция запуска тестов
void ExecuteNetworkBenchmarks();

#endif
