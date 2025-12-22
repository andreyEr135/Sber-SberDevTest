#include <sys/sysinfo.h>
#include <dirent.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

#include "debugsystem.h"
#include "sysutils.h"
#include "_version"

#include "sensor_monitor.h"

using namespace std;

// Структуры данных
struct CpuTicks {
    unsigned long total_time;
    unsigned long work_time;
};

struct ProcessorCore {
    CpuTicks snapshots[2];
    float load_percentage;
};

struct MathModifier {
    char operation_type;
    float operand_value;
};

struct SensorDefinition {
    string category;
    string label;
    string display_format;
    string sysfs_path;
    float warning_threshold;
    float critical_threshold;
    float last_raw_value;
    vector<MathModifier> math_pipeline;
};

// Глобальные настройки и состояние
string G_MONITOR_NAME;
bool G_ENABLE_CPU_SUMMARY = false;
bool G_ENABLE_CORE_DETAIL = false;
bool G_ENABLE_MEM_MONITOR = false;
bool G_ENABLE_HDD_MONITOR = false;

vector<ProcessorCore> g_cpu_cores;
vector<SensorDefinition> g_active_sensors;

/**
 * @brief Читает статистику CPU из /proc/stat
 */
bool fetch_cpu_ticks()
{
    FUNCTION_TRACE
    FILE* stat_file = fopen("/proc/stat", "r");
    if (!stat_file) {
        Error("Failed to open /proc/stat\n");
        return false;
    }

    char line_buffer[256];
    unsigned long metrics[7];

    while (fgets(line_buffer, sizeof(line_buffer), stat_file)) {
        if (strncmp(line_buffer, "cpu", 3) != 0) continue;

        // Извлекаем значения: user, nice, system, idle, iowait, irq, softirq
        sscanf(line_buffer + 5, "%lu %lu %lu %lu %lu %lu %lu",
               &metrics[0], &metrics[1], &metrics[2], &metrics[3], &metrics[4], &metrics[5], &metrics[6]);

        CpuTicks current_ticks = {0, 0};
        for (int i = 0; i < 7; i++) current_ticks.total_time += metrics[i];
        current_ticks.work_time = current_ticks.total_time - metrics[3]; // Total - Idle

        unsigned int core_idx;
        if (line_buffer[3] == ' ') core_idx = 0; // Общая статистика
        else { sscanf(line_buffer + 3, "%u", &core_idx); core_idx++; }

        if (core_idx >= g_cpu_cores.size()) g_cpu_cores.resize(core_idx + 1);
        g_cpu_cores[core_idx].snapshots[0] = current_ticks;
    }

    fclose(stat_file);
    return true;
}

/**
 * @brief Разрешает пути для hwmon устройств (обработка масок и имен драйверов)
 */
string resolve_system_path(string raw_path, int lookup_mode)
{
    if (lookup_mode == 0) { // Обычный путь или маска hwmon*
        if (raw_path.find("hwmon*") != string::npos) {
            size_t pos = raw_path.find("hwmon*");
            string base_dir = raw_path.substr(0, pos);
            string sub_path = raw_path.substr(pos + 6);

            DIR* dir = opendir(base_dir.c_str());
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir))) {
                    if (strstr(entry->d_name, "hwmon")) {
                        string found_path = base_dir + entry->d_name + sub_path;
                        closedir(dir);
                        return found_path;
                    }
                }
                closedir(dir);
            }
        }
        return raw_path;
    }
    else if (lookup_mode == 1) { // Поиск по имени драйвера
        Tstrlist path_tokens;
        if (stringsplit(raw_path, '/', path_tokens) < 2) return "";

        string target_driver = path_tokens[0];
        string target_file = path_tokens.back();
        int instance_index = (path_tokens.size() == 3) ? atoi(path_tokens[1].c_str()) : 0;

        const string hwmon_base = "/sys/class/hwmon/";
        DIR* dir = opendir(hwmon_base.c_str());
        if (!dir) return "";

        struct dirent* entry;
        int current_match = 0;
        string final_path = "";

        while ((entry = readdir(dir))) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            string driver_name_file = hwmon_base + entry->d_name + "/name";
            if (!fileexist(driver_name_file)) {
                driver_name_file = hwmon_base + entry->d_name + "/device/name";
            }

            if (fileexist(driver_name_file)) {
                string driver_name = trim(readfilestr(driver_name_file));
                if (driver_name == target_driver) {
                    if (current_match == instance_index) {
                        final_path = hwmon_base + entry->d_name + "/" + target_file;
                        // Проверка на вложенность device
                        if (!fileexist(final_path)) final_path = hwmon_base + entry->d_name + "/device/" + target_file;
                        break;
                    }
                    current_match++;
                }
            }
        }
        closedir(dir);
        return final_path;
    }
    return "";
}

/**
 * @brief Первичная конфигурация монитора
 */
void initialize_monitor()
{
    FUNCTION_TRACE
    G_ENABLE_CPU_SUMMARY = (g_DebugSystem.conf->ReadInt("MAIN", "cpu") == 1);
    G_ENABLE_CORE_DETAIL = (g_DebugSystem.conf->ReadInt("MAIN", "cores") == 1);
    G_ENABLE_MEM_MONITOR = (g_DebugSystem.conf->ReadInt("MAIN", "mem") == 1);
    G_ENABLE_HDD_MONITOR = (g_DebugSystem.conf->ReadInt("MAIN", "hdd") == 1);

    if (G_ENABLE_CPU_SUMMARY || G_ENABLE_CORE_DETAIL) fetch_cpu_ticks();

    time_t now = time(nullptr);
    if (G_ENABLE_CPU_SUMMARY) update_sensor_metric(G_MONITOR_NAME, 'U', "CPU", "", ' ', now, 0);
    if (G_ENABLE_CORE_DETAIL) update_sensor_metric(G_MONITOR_NAME, 'C', "Cores", "", ' ', now, 0);
    if (G_ENABLE_MEM_MONITOR) update_sensor_metric(G_MONITOR_NAME, 'M', "FreeMem", "", ' ', now, 0);

    int sensor_idx = 1;
    while (true) {
        string section = stringformat("%d", sensor_idx++);
        SensorDefinition snz;
        snz.category = g_DebugSystem.conf->ReadString(section, "type");
        if (snz.category.empty()) break;

        snz.label = g_DebugSystem.conf->ReadString(section, "name");
        snz.display_format = g_DebugSystem.conf->ReadString(section, "format");

        string cfg_file = g_DebugSystem.conf->ReadString(section, "file");
        string cfg_driver = g_DebugSystem.conf->ReadString(section, "drv_name");

        if (!cfg_file.empty()) snz.sysfs_path = resolve_system_path(g_DebugSystem.fullpath(cfg_file), 0);
        else if (!cfg_driver.empty()) snz.sysfs_path = resolve_system_path(cfg_driver, 1);

        errno = 0;
        snz.warning_threshold = g_DebugSystem.conf->ReadFloat(section, "warn");
        snz.critical_threshold = g_DebugSystem.conf->ReadFloat(section, "crit");

        stringstream ss(g_DebugSystem.conf->ReadString(section, "opers"));
        string op_token;
        while (ss >> op_token) {
            MathModifier mod;
            mod.operation_type = op_token[0];
            sscanf(op_token.c_str() + 1, "%f", &mod.operand_value);
            snz.math_pipeline.push_back(mod);
        }
        update_sensor_metric(G_MONITOR_NAME, snz.category[0], snz.label, "", ' ', now, 0);
        g_active_sensors.push_back(snz);
    }
}

/**
 * @brief Основной цикл обработки данных
 */
void run_monitoring_cycle()
{
    FUNCTION_TRACE

    // 1. Расчет загрузки CPU
    if (G_ENABLE_CPU_SUMMARY || G_ENABLE_CORE_DETAIL) {
        for (auto &core : g_cpu_cores) core.snapshots[1] = core.snapshots[0];
        fetch_cpu_ticks();

        for (auto &core : g_cpu_cores) {
            unsigned long diff_total = core.snapshots[0].total_time - core.snapshots[1].total_time;
            unsigned long diff_work  = core.snapshots[0].work_time - core.snapshots[1].work_time;
            core.load_percentage = (diff_total == 0) ? 0.0f : (100.0f * diff_work / diff_total);
        }
    }

    time_t now = time(nullptr);
    string value_str;

    // 2. Обновление CPU Summary
    if (G_ENABLE_CPU_SUMMARY) {
        char status_flag = ' ';
        if (g_cpu_cores[0].load_percentage >= 60) status_flag = '!';
        if (g_cpu_cores[0].load_percentage >= 90) status_flag = '#';
        value_str = stringformat("%3.0f%%", g_cpu_cores[0].load_percentage);
        update_sensor_metric(G_MONITOR_NAME, 'U', "CPU", value_str, status_flag, now, 4);
    }

    // 3. Обновление детальной загрузки ядер
    if (G_ENABLE_CORE_DETAIL) {
        string core_map = "";
        for (size_t i = 1; i < g_cpu_cores.size(); i++) {
            int intensity = (int)(g_cpu_cores[i].load_percentage / 10.0f);
            if (intensity == 0) core_map += "_";
            else if (intensity >= 10) core_map += "X";
            else core_map += to_string(intensity);
        }
        update_sensor_metric(G_MONITOR_NAME, 'C', "Cores", core_map, ' ', now, core_map.length());
    }

    // 4. Память
    if (G_ENABLE_MEM_MONITOR) {
        struct sysinfo si;
        sysinfo(&si);
        unsigned int free_mb = ((si.freeram >> 10) * si.mem_unit) >> 10;
        value_str = stringformat("%u MB", free_mb);
        update_sensor_metric(G_MONITOR_NAME, 'M', "FreeMem", value_str, ' ', now, 6);
    }

    // 5. HDD Температура (через hddtemp daemon)
    if (G_ENABLE_HDD_MONITOR) {
        string hdd_raw = GetLinesProcess("telnet 127.0.0.1 7634");
        char *ptr = (char*)hdd_raw.c_str();
        while ((ptr = strstr(ptr, "|/dev"))) {
            ptr++;
            char* end_ptr = strchr(ptr, '|'); if (!end_ptr) break;
            *end_ptr = 0;
            string dev_node = getfile(ptr);

            ptr = end_ptr + 1; end_ptr = strchr(ptr, '|'); if (!end_ptr) break;
            ptr = end_ptr + 1; end_ptr = strchr(ptr, '|'); if (!end_ptr) break;
            *end_ptr = 0;

            float temp = -999.0f;
            sscanf(ptr, "%f", &temp);
            ptr = end_ptr + 1;

            char status_flag = (temp >= 60) ? '!' : (temp >= 90 ? '#' : ' ');
            string temp_label = (temp == -999.0f) ? "ERR" : stringformat("%3.0f°", temp);
            update_sensor_metric(G_MONITOR_NAME, 'T', "HDD-" + dev_node, temp_label, status_flag, now, 6);
        }
    }

    // 6. Пользовательские сенсоры (HWMON/SysFS)
    for (auto &snz : g_active_sensors) {
        char status_flag = ' ';
        string display_val = "N/A";
        bool read_ok;
        float current_val = readfilefloat(snz.sysfs_path, read_ok);

        if (!read_ok) {
            status_flag = '!';
        } else {
            if (current_val == -2.0f) current_val = snz.last_raw_value;
            snz.last_raw_value = current_val;

            for (const auto &op : snz.math_pipeline) {
                switch (op.operation_type) {
                    case '+': current_val += op.operand_value; break;
                    case '-': current_val -= op.operand_value; break;
                    case '*': current_val *= op.operand_value; break;
                    case '/': if(op.operand_value != 0) current_val /= op.operand_value; break;
                }
            }

            if (current_val >= snz.warning_threshold) status_flag = '!';
            if (current_val >= snz.critical_threshold) status_flag = '#';
            display_val = stringformat(snz.display_format.c_str(), current_val);
        }

        char type_code = snz.category.empty() ? ' ' : snz.category[0];
        update_sensor_metric(G_MONITOR_NAME, type_code, snz.label, display_val, status_flag, now, 0);
    }

    log_device_metrics(G_MONITOR_NAME);
}

int main(int argc, char* argv[])
{
    g_DebugSystem.stdbufoff();
    g_DebugSystem.init(argc, argv, &VERSION, true);
    FUNCTION_TRACE

    try {
        if (!device_registry.open()) throw errException(device_registry.error);
        if (!metrics_registry.open()) throw errException(metrics_registry.error);

        G_MONITOR_NAME = g_DebugSystem.getparam_or_default("name");
        int interval_ms = str2int(g_DebugSystem.getparam_or_default("interval"));

        initialize_monitor();

        while (true) {
            run_monitoring_cycle();
            usleep(interval_ms * 1000);
        }
    }
    catch (errException& ex) {
        Error("Critical Failure: %s\n", ex.error());
        return -1;
    }

    return 0;
}
