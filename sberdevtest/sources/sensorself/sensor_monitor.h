#ifndef SENSOR_MONITOR_H
#define SENSOR_MONITOR_H

#include "memtbl.h"
#include "devscan.h"
#include <string>

// Экспорт таблиц для использования в других модулях
extern TmemTable device_registry;
extern TmemTable metrics_registry;

/**
 * @brief Обновляет запись о конкретной метрике в таблице
 */
int update_sensor_metric(std::string device_name, char type_code, std::string metric_label,
                         std::string current_value, char state_flag, time_t timestamp, int field_width);

/**
 * @brief Выводит текущее состояние устройства в лог
 */
int log_device_metrics(std::string device_name);

#endif
