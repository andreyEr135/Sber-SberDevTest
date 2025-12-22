#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sstream>

#include "debugsystem.h"

#include "_version"

using namespace std;


/**
 * @brief Проверяет, является ли строка корректным MAC-адресом формата XX:XX:XX:XX:XX:XX.
 *
 * @param macAddr Строка для проверки.
 * @return true, если строка соответствует формату MAC-адреса, false в противном случае.
 */
bool isValidMacAddress(const std::string& macAddr) {
    if (macAddr.length() != 17) {
        return false; // Неправильная длина
    }

    for (std::size_t i = 0; i < macAddr.length(); ++i) {
        if ((i + 1) % 3 == 0) { // Каждая 3-я позиция (0-индексированная: 2, 5, 8, 11, 14) должна быть двоеточием
            if (macAddr[i] != ':') {
                return false;
            }
        } else { // Остальные позиции должны быть шестнадцатеричными цифрами
            if (!std::isxdigit(static_cast<unsigned char>(macAddr[i]))) {
                return false;
            }
        }
    }
    return true; // Все проверки пройдены
}



/**
 * @brief Вспомогательная функция для парсинга вывода 'bluetoothctl devices'.
 *        Ищет устройство, соответствующее имени или MAC-адресу.
 * @param devicesOutput Полная строка вывода из 'bluetoothctl devices'.
 * @param networkName Имя Bluetooth-устройства для поиска (необязательно).
 * @param networkMac MAC-адрес Bluetooth-устройства для поиска (необязательно).
 * @return true, если найдено соответствующее устройство, false в противном случае.
 */
bool parseBluetoothDevicesOutput(const string& devicesOutput, const string& networkName, const string& networkMac) {
    istringstream iss(devicesOutput);
    string line;
    while (getline(iss, line)) {
        // Ищем строки, начинающиеся с "Device XX:XX:XX:XX:XX:XX DeviceName"
        size_t device_keyword_pos = line.find("Device ");
        if (device_keyword_pos == string::npos) {
            continue; // Это не строка с описанием устройства
        }

        // MAC-адрес начинается после "Device "
        size_t mac_start_pos = device_keyword_pos + string("Device ").length();
        const size_t MAC_LENGTH = 17; // XX:XX:XX:XX:XX:XX

        if (mac_start_pos + MAC_LENGTH > line.length()) {
            continue; // Строка слишком короткая для MAC-адреса
        }

        string current_mac = line.substr(mac_start_pos, MAC_LENGTH);

        // Сравниваем MAC-адрес (без учета регистра)
        if (!networkMac.empty()) {
            string upper_current_mac = current_mac;
            string upper_network_mac = networkMac;
            transform(upper_current_mac.begin(), upper_current_mac.end(), upper_current_mac.begin(), ::toupper);
            transform(upper_network_mac.begin(), upper_network_mac.end(), upper_network_mac.begin(), ::toupper);

            if (upper_current_mac == upper_network_mac) {
                return true;
            }
        }

        // Сравниваем имя устройства
        if (!networkName.empty()) {
            size_t name_start_pos = mac_start_pos + MAC_LENGTH;
            // Пропускаем все пробелы после MAC-адреса
            while (name_start_pos < line.length() && line[name_start_pos] == ' ') {
                name_start_pos++;
            }

            if (name_start_pos < line.length()) {
                string current_name = line.substr(name_start_pos);
                if (current_name == networkName) {
                    return true;
                }
            }
        }
    }
    return false;
}


bool findBluetooth (const string& btDevice )
{FUNCTION_TRACE
    string outData = "";
    int res;
    const int CHECK_TIMEOUT_MS = 3000; // Короткий таймаут для проверки статуса устройства

    // Команда для получения статуса Bluetooth-устройств.
    // 'hciconfig' без аргументов выводит информацию обо всех устройствах.
    string cmd = "hciconfig";

    res = GetProcessDataTimeout( cmd, outData, CHECK_TIMEOUT_MS );

    // Если команда завершилась с ошибкой или нет вывода,
    // считаем, что адаптер не найден или произошла проблема.
    if (res != 0 || outData.empty()) {
        return false;
    }

    // Ищем в выводе строку, соответствующую нашему Bluetooth-устройству.
    // Пример вывода hciconfig:
    // hci0:   Type: Primary Bus: USB
    //      BD Address: 00:11:22:33:44:55 ACL MTU: 1021:8 SCO MTU: 64:1
    //      UP RUNNING PSCAN

    // Мы ищем имя устройства, за которым следует двоеточие (например, "hci0:")
    string search_pattern = btDevice + ":";

    if (outData.find(search_pattern) != string::npos) {
        // Если найдено, это означает, что адаптер присутствует в системе.
        // Дополнительно можно проверить его состояние (UP RUNNING), но наличие строки уже достаточно.
        return true;
    }

    // Устройство с таким именем не найдено.
    return false;
}


bool findBluetoothNetwork (const string& networkName, const string& networkMac, const int& findSec )
{FUNCTION_TRACE
    // Необходимо указать либо имя, либо MAC-адрес для поиска.

    string outData = "";
    int res;
    int COMMAND_TIMEOUT_MS = findSec * 1000; // Таймаут для отдельных команд bluetoothctl

    // 1. Включаем сканирование Bluetooth-устройств.
    // Мы не парсим вывод этой команды, нам просто нужно, чтобы она запустила сканирование.
    GetProcessDataTimeout("bluetoothctl scan on", outData, COMMAND_TIMEOUT_MS);
    // На этом этапе outData может содержать информацию об "Agent registered" или "Changing scan on succeeded",
    // что для нас не критично.

    // 2. Получаем список обнаруженных Bluetooth-устройств.
    outData.clear(); // Очищаем буфер для нового вывода
    res = GetProcessDataTimeout("bluetoothctl devices", outData, COMMAND_TIMEOUT_MS);

    bool device_found = false;
    if (res == 0 && !outData.empty()) {
        Trace("res: %s\n", outData.c_str());
        // Парсим вывод для поиска устройства.
        if (parseBluetoothDevicesOutput(outData, networkName, networkMac)) {
            device_found = true;
        }
    }

    // 4. Выключаем сканирование Bluetooth-устройств.
    // Также не парсим вывод этой команды, просто останавливаем сканирование.
    GetProcessDataTimeout("bluetoothctl scan off", outData, COMMAND_TIMEOUT_MS);

    return device_found;
}

//-------------------------------------------------------------------------------------------

void help( )
{FUNCTION_TRACE
    Log( "\nТест Bluetooth:\n" );
    Log( "   %s [параметры, * обязательные]\n", g_DebugSystem.exe.c_str() );

    Log( " * network     - название устройства для поиска\n" );
    Log( " или mac       - mac-адрес устройства для поиска\n" );
    Log( " bt            - наименование адаптера\n");
    Log( " findTime      - время на поиск устройства (по умолчанию 10 сек)\n");

}
//-------------------------------------------------------------------------------------------

void Test( )
{FUNCTION_TRACE
    string networkName = g_DebugSystem.getparam_or_default( "network" );
    string networkMac  = g_DebugSystem.getparam_or_default( "mac" );
    string nameDevice = g_DebugSystem.getparam_or_default( "bt" );
    string stime = g_DebugSystem.getparam( "findTime" );
    int findSeconds = time2seconds( stime );
    if (findSeconds <= 0) findSeconds = 10;

    if ( (networkName.empty() && networkMac.empty()) || nameDevice.empty() ) throw errException( "Не указаны название bt сети/mac сетевого устройства или название bt устройства" );
    if ( (!networkMac.empty()) && (!isValidMacAddress(networkMac)) ) throw errException( "указан некорректный mac-адрес сетевого устройства" );
    if (!findBluetooth (nameDevice)) throw errException( "Адаптер %s отсутствует", nameDevice.c_str() );
    Log("<UI>Устройство bt найдено, поиск сети...\n");
    if (!findBluetoothNetwork(networkName, networkMac, findSeconds )) {
        if (!networkName.empty()) throw errException( "сеть %s не найдена", networkName.c_str() );
        if (!networkMac.empty()) throw errException( "mac %s не найден", networkMac.c_str() );
    }

}
//-------------------------------------------------------------------------------------------

int main( int argc, char* argv[] )
{
    g_DebugSystem.stdbufoff( );
    g_DebugSystem.init( argc, argv, &VERSION, true );
    FUNCTION_TRACE

	try
	{
        if( g_DebugSystem.checkparam( "-h" ) ) { help( ); return 0; }

		Test();
		Log( "TEST OK\n" );

	}
	catch( errException& e )
	{
		Error( "TEST ERR %s\n", e.error() );
	}
	return 0;
}
//-------------------------------------------------------------------------------------------

