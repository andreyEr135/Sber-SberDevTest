#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "debugsystem.h"

#include "_version"

using namespace std;

bool findWifi (const string& wifiDevice )
{FUNCTION_TRACE
    string outData = "";
    int res;
    const int CHECK_TIMEOUT_MS = 3000; // Короткий таймаут для проверки статуса устройства

    // Команда для получения статуса всех сетевых устройств.
    // '-t' для табличного вывода, '-f DEVICE,TYPE' для вывода только имени и типа устройства.
    string cmd = "nmcli -t -f DEVICE,TYPE dev status";

    res = GetProcessDataTimeout( cmd, outData, CHECK_TIMEOUT_MS );

    // Если команда завершилась с ошибкой или нет вывода,
    // считаем, что устройство не найдено или произошла проблема.
    if (res != 0 || outData.empty()) {
        return false;
    }

    // Ищем в выводе строку, соответствующую нашему устройству и типу 'wifi'.
    // Пример вывода: "wlp4s0:wifi:connected" (но нам нужен только DEVICE:TYPE)
    string search_pattern = wifiDevice + ":wifi";

    if (outData.find(search_pattern) != string::npos) {
        // Устройство найдено и является Wi-Fi адаптером.
        return true;
    }

    // Устройство с таким именем и типом 'wifi' не найдено.
    return false;
}

bool connectToWifi (const string& ssid, const string& passwd )
{FUNCTION_TRACE
    string outData = "";
    string cmd = stringformat("nmcli dev wifi connect \"%s\" password \"%s\"", ssid.c_str(), passwd.c_str() );
    int res = GetProcessDataTimeout( cmd, outData, 30000 );
    if ( (res != 0) || (outData.empty()) ) return false;
    if ( (outData.find("успешно активировано") != string::npos) ||
         (outData.find("successfully activated") != string::npos) ) {
        // Подключение успешно установлено
        return true;
    }
    return false;
}
//-------------------------------------------------------------------------------------------

bool checkConnectionToWifi (const string& ssid, const string& wifiDevice )
{FUNCTION_TRACE
    string outData = "";
    int res;
    const int CHECK_TIMEOUT_MS = 5000;

    // 1. Проверяем статус соединения Wi-Fi через 'iw dev <device> link'
    string cmd_iw_link = stringformat("/usr/sbin/iw dev %s link", wifiDevice.c_str());
    res = GetProcessDataTimeout( cmd_iw_link, outData, CHECK_TIMEOUT_MS );
    Trace("%s  res:%s", cmd_iw_link.c_str(), outData.c_str());

    // Если команда iw завершилась с ошибкой, нет вывода, или "Not connected."
    if (res != 0 || outData.empty() || outData.find("Not connected.") != string::npos) {
        return false;
    }

    // Ищем SSID в выводе iw dev link
    string ssid_pattern = "SSID: ";
    size_t ssid_pos = outData.find(ssid_pattern);

    if (ssid_pos == string::npos) {
        return false; // Поле SSID не найдено
    }

    // Извлекаем значение SSID из вывода
    size_t ssid_val_start = ssid_pos + ssid_pattern.length();
    size_t ssid_val_end = outData.find('\n', ssid_val_start);
    if (ssid_val_end == string::npos) ssid_val_end = outData.length();

    string current_ssid = outData.substr(ssid_val_start, ssid_val_end - ssid_val_start);

    // Удаляем возможные пробелы в начале или конце SSID
    size_t first = current_ssid.find_first_not_of(' ');
    if (string::npos == first) return false;
    size_t last = current_ssid.find_last_not_of(' ');
    current_ssid = current_ssid.substr(first, (last - first + 1));

    if (current_ssid != ssid) {
        return false; // Подключено, но не к нужному SSID
    }

    // 2. Проверяем наличие IP-адреса через 'ip a show <device>'
    string cmd_ip_address = stringformat("/usr/bin/ip a show %s", wifiDevice.c_str());
    outData.clear(); // Очищаем буфер для нового вывода
    res = GetProcessDataTimeout( cmd_ip_address, outData, CHECK_TIMEOUT_MS );

    if (res != 0 || outData.empty()) {
        return false;
    }

    // Ищем IPv4 адрес
    // Пример: "    inet 192.168.31.69/24 brd 192.168.31.255 scope global dynamic wlp4s0"
    string inet_pattern = "inet ";
    size_t inet_pos = outData.find(inet_pattern);

    if (inet_pos != string::npos) {
        // Проверяем, что найденный "inet" относится к IPv4 (не IPv6, т.е. не "inet6")
        // и что за ним следует что-то похожее на IP-адрес (содержит '.')
        size_t next_space = outData.find(' ', inet_pos + inet_pattern.length());
        if (next_space != string::npos) {
            string potential_ip_segment = outData.substr(inet_pos + inet_pattern.length(), next_space - (inet_pos + inet_pattern.length()));
            if (potential_ip_segment.find('.') != string::npos) {
                // Все условия выполнены: подключено к нужному SSID и есть IPv4 адрес.
                return true;
            }
        }
    }

    // Если IP-адрес не найден
    return false;
}

//-------------------------------------------------------------------------------------------

void help( )
{FUNCTION_TRACE
    Log( "\nТест WiFi:\n" );
	Log( "   %s [параметры, * обязательные]\n", g_DebugSystem.exe.c_str() );

    Log( " * ssid     - название сети для подключения\n" );
    Log( " * password - пароль для сети\n" );
    Log( "   wifi     - наименование сетевого адаптера\n");
    Log( "   wait     - время на подключение к сети, по умолчанию 1 сек\n");
}
//-------------------------------------------------------------------------------------------

void Test( )
{FUNCTION_TRACE
    string ssid              = g_DebugSystem.getparam_or_default( "ssid" );
    string passwd            = g_DebugSystem.getparam_or_default( "password" );
    string nameDevice        = g_DebugSystem.getparam_or_default( "wifi" );
    string waitConnectionStr = g_DebugSystem.getparam_or_default( "wait" );
    int waitConnection = 1;
    if (!waitConnectionStr.empty()) {
        waitConnection = str2int(waitConnectionStr);
        if (waitConnection <= 0) waitConnection = 10;
    }

    if ( (ssid.empty()) || (passwd.empty()) || (nameDevice.empty()) ) throw errException( "Не указаны ssid/пароль сети или название устройства" );
    Log("<UI> Проверка наличия сетевого адаптера %s...\n", nameDevice.c_str());
    if (!findWifi (nameDevice)) throw errException( "Адаптер %s отсутствует", nameDevice.c_str() );
    Log("<UI> Проверка подключения к сети %s...\n", ssid.c_str());
    if (!checkConnectionToWifi (ssid, nameDevice))
    {
        Log("<UI> Подключение к сети %s...\n", ssid.c_str());
        if (!connectToWifi (ssid, passwd)) throw errException( "Не удалось подключиться к сети %s", ssid.c_str() );
        Log("<UI> Проверка подключения к сети %s...\n", ssid.c_str());
        sleep(waitConnection);
        if (!checkConnectionToWifi (ssid, nameDevice)) throw errException( "Не удалось подключиться к сети %s", ssid.c_str() );
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

