#include "cpu_scan.h"
#include <fstream>
#include <sstream> // Для разбора вывода lscpu

//---------------------------------------------------------------------------

CpuScanner::CpuScanner()
{
    testActive = false;
    minimalFreq = 0;
    countCores = 0;
}

void CpuScanner::setActiveTest(float _minimalFreq, int _countCores)
{
    testActive = true;
    minimalFreq = _minimalFreq;
    countCores = _countCores;
}

void CpuScanner::setDeactiveTest()
{
    testActive = false;
    minimalFreq = 0;
    countCores = 0;
}

bool CpuScanner::getFreq(CpuFreqs &freqs)
{
    // Примечание: policy0 может быть недоступна на всех системах.
    // В системах с несколькими кластерами CPU могут быть policy0, policy4 и т.д.
    const std::string freq_dir = "/sys/devices/system/cpu/cpufreq/policy0/";
    const std::string freq_files[3] = {
        "scaling_min_freq",
        "scaling_cur_freq",
        "scaling_max_freq"
    };

    bool result = true;
    float* freq_ptrs[3] = { &freqs.scaling_min_freq, &freqs.scaling_cur_freq, &freqs.scaling_max_freq };

    for (unsigned int i = 0; i < 3; ++i)
    {
        bool status = false;
        const float freq = readfilefloat((freq_dir + freq_files[i]).c_str(), status);
        if (status) {
            *freq_ptrs[i] = freq;
        } else {
            result = false; // Если хотя бы один файл не прочитан, считаем операцию неуспешной
        }
    }
    return result;
}

float CpuScanner::getFreqFromCpuinfo()
{
    std::ifstream fs("/proc/cpuinfo");
    std::string line;

    while (std::getline(fs, line))
    {
        std::string key, val;
        if (stringkeyvalue(line, ':', key, val) && key == "cpu MHz")
        {
            return str2float(val);
        }
    }
    return 0.0;
}

bool CpuScanner::getCpuInfo(std::string &model, std::string &cores)
{
    model = "";
    cores = "";

    // 1. Пытаемся получить данные из lscpu
    int status = 0;
    std::string res = GetLinesProcess("LC_ALL=C lscpu", &status);

    if (status == 0 && !res.empty()) {
        std::stringstream ss(res);
        std::string line;
        while (std::getline(ss, line)) {
            size_t colonPos = line.find(':');
            if (colonPos == std::string::npos) continue;

            // Извлекаем ключ и значение вручную без сложных функций
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);

            // Очищаем пробелы и табуляцию через trim (из sysutils)
            key = trim(key);
            value = trim(value);

            if (key == "Model name") {
                model = value;
            } else if (key == "CPU(s)") {
                cores = value;
            }
        }
    }

    // 2. Если данные не найдены (или lscpu пуст), берем из /proc/cpuinfo
    if (model.empty() || cores.empty()) {
        std::ifstream fs("/proc/cpuinfo");
        std::string line;
        int logical_cores = 0;

        while (std::getline(fs, line)) {
            size_t colonPos = line.find(':');
            if (colonPos == std::string::npos) continue;

            std::string key = trim(line.substr(0, colonPos));
            std::string value = trim(line.substr(colonPos + 1));

            if (key == "model name" && model.empty()) {
                model = value;
            }
            if (key == "processor") {
                logical_cores++;
            }
        }

        if (cores.empty() && logical_cores > 0) {
            cores = std::to_string(logical_cores);
        }
    }

    return !model.empty();
}

bool CpuScanner::test()
{
    Log("\033[32mПоиск информации о CPU\033[0m\n");

    // --- 1. Сбор информации с использованием вспомогательных методов ---
    std::string model, cores_str;
    getCpuInfo(model, cores_str); // Пытаемся получить через lscpu

    int cpu_cores = 0;
    if (!cores_str.empty()) {
        cpu_cores = std::atoi(cores_str.c_str());
    }

    CpuFreqs freqs = {0, 0, 0};
    const bool get_freq_status = getFreq(freqs);

    float freq_cur = 0.0;
    if (get_freq_status) {
        freq_cur = freqs.scaling_cur_freq / 1000.0;
    } else {
        freq_cur = getFreqFromCpuinfo(); // Используем /proc/cpuinfo как запасной вариант
    }

    float freq_max = 0.0;
    if (get_freq_status) {
        freq_max = freqs.scaling_max_freq / 1000.0;
    }

    // --- 2. Логирование результатов и проверка ---
    Log("Модель: %s\n", model.c_str());

    // Проверка и логирование ядер
    bool err_cores = false;
    std::string cores_log = stringformat("Количество ядер: %d", cpu_cores);
    if (testActive && cpu_cores < countCores) {
        err_cores = true;
        cores_log += stringformat(" (ожидается: %d)", countCores);
    }
    Log("%s\n", err_cores ? stringformat(CLR(f_LIGHTRED) "%s" CLR0, cores_log.c_str()).c_str() : cores_log.c_str());

    // Логирование частот
    std::string str_min{"мин: неопределена"}, str_cur{"текущая: неопределена"}, str_max{"макс: неопределена"};
    if (get_freq_status) {
        str_min = stringformat("мин: %.2f", freqs.scaling_min_freq / 1000.0);
        str_max = stringformat("макс: %.2f", freq_max);
    }
    if (freq_cur > 0) {
        str_cur = stringformat("текущая: %.2f", freq_cur);
    }
    std::string freq_log = "Частота MHz: " + str_min + ", " + str_cur + ", " + str_max;

    // Проверка и логирование частоты
    bool err_freq = false;
    int freq_for_check = int(freq_max > 0 ? freq_max : freq_cur);
    if (testActive && freq_for_check < int(minimalFreq)) {
        err_freq = true;
    }
    Log("%s\n", err_freq ? stringformat(CLR(f_LIGHTRED) "%s" CLR0, freq_log.c_str()).c_str() : freq_log.c_str());

    // --- 3. Финальный отчет для парсинга ---
    if (testActive)
    {
        if (!err_cores && !err_freq)
        {
            Log("C|CH=CPU $OK=%d MHz, ядер: %d\n", freq_for_check, cpu_cores);
        }
        else
        {
            std::string err_report = stringformat("C|CH=CPU $ERR=%d MHz", freq_for_check);
            if (err_freq) err_report += stringformat(" (ожидается: %d MHz)", int(minimalFreq));
            err_report += stringformat(", ядер: %d", cpu_cores);
            if (err_cores) err_report += stringformat(" (ожидается: %d)", countCores);
            Log("%s\n", err_report.c_str());
        }
    }
    Log("\n");
    return true;
}
