#include "debugsystem.h"
#include "sysutils.h"
#include "sensor_monitor.h"

using namespace std;

TmemTable device_registry(SENSORDEV_TBL);
TmemTable metrics_registry(SENSOR_TBL);

int update_sensor_metric(string device_name, char type_code, string metric_label,
                         string current_value, char state_flag, time_t timestamp, int field_width)
{
    FUNCTION_TRACE
    int status = 0;
    device_registry.lock();
    metrics_registry.lock();

    try {
        TtblRecord dev_record;
        device_registry.find(dev_record, SENSORDEV_NAME, device_name.c_str());

        if (!dev_record) {
            dev_record = device_registry.add();
            dev_record.set(SENSORDEV_NAME, device_name);
        }

        if (!dev_record) throw errException(device_registry.error);

        TtblRecord metric_record;
        // Поиск существующей метрики для данного устройства
        for (; metrics_registry.find(metric_record, SENSOR_DEVID, dev_record.get(TBL_ID)); ) {
            if (strcmp(metric_label.c_str(), metric_record.str(SENSOR_NAME)) == 0) break;
        }

        if (!metric_record) {
            metric_record = metrics_registry.add();
            if (!metric_record) throw errException(metrics_registry.error);

            metric_record.set(SENSOR_DEVID, dev_record.get(TBL_ID));
            metric_record.set(SENSOR_TYPE, type_code);
            metric_record.set(SENSOR_NAME, metric_label);
            metric_record.set(SENSOR_WIDTH, field_width);
        }

        metric_record.set(SENSOR_VALUE, current_value);
        metric_record.set(SENSOR_FLAG, state_flag);
        metric_record.set(SENSOR_TIME, (unsigned long)timestamp);
    }
    catch (errException& ex) {
        Error("DB Update Error: %s\n", ex.error());
        status = -1;
    }

    metrics_registry.unlock();
    device_registry.unlock();
    return status;
}

int log_device_metrics(string device_name)
{
    FUNCTION_TRACE
    metrics_registry.lock();
    device_registry.lock();
    int status = 0;

    try {
        TtblRecord dev_record;
        device_registry.find(dev_record, SENSORDEV_NAME, device_name.c_str());
        if (!dev_record) throw errException("Device not found in registry");

        string output_buffer = stringformat("+%s:", dev_record.str(SENSORDEV_NAME));

        for (TtblRecord met_record; metrics_registry.find(met_record, SENSOR_DEVID, dev_record.get(TBL_ID)); ) {
            output_buffer += stringformat("%c%c%s:%s|",
                                          met_record.get(SENSOR_TYPE),
                                          met_record.get(SENSOR_FLAG),
                                          met_record.str(SENSOR_NAME),
                                          met_record.str(SENSOR_VALUE));
        }
        Log("%s\n", output_buffer.c_str());
    }
    catch (errException& ex) {
        Error("DB Print Error: %s\n", ex.error());
        status = -1;
    }

    device_registry.unlock();
    metrics_registry.unlock();
    return status;
}

