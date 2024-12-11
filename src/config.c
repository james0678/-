#include "config.h"
#include "logger.h"
#include <json-c/json.h>
#include <string.h>

static AppConfig app_config;

bool load_config(const char* config_file) {
    struct json_object *root;
    
    root = json_object_from_file(config_file);
    if (!root) {
        log_error("Failed to load config file: %s", config_file);
        return false;
    }

    // 네트워크 설정 로드
    struct json_object *network_obj;
    if (json_object_object_get_ex(root, "network", &network_obj)) {
        struct json_object *host_obj, *port_obj, *timeout_obj, *retries_obj;
        
        if (json_object_object_get_ex(network_obj, "host", &host_obj)) {
            const char* host = json_object_get_string(host_obj);
            app_config.network.host = strdup(host);
        }
        
        if (json_object_object_get_ex(network_obj, "port", &port_obj)) {
            app_config.network.port = json_object_get_int(port_obj);
        }
        
        if (json_object_object_get_ex(network_obj, "timeout", &timeout_obj)) {
            app_config.network.timeout_seconds = json_object_get_int(timeout_obj);
        }
        
        if (json_object_object_get_ex(network_obj, "max_retries", &retries_obj)) {
            app_config.network.max_retries = json_object_get_int(retries_obj);
        }
    }

    // 센서 보정 데이터 로드
    struct json_object *calibrations_obj;
    if (json_object_object_get_ex(root, "sensor_calibrations", &calibrations_obj)) {
        for (int i = 0; i < NUM_SENSORS && i < json_object_array_length(calibrations_obj); i++) {
            struct json_object *sensor_obj = json_object_array_get_idx(calibrations_obj, i);
            struct json_object *points_obj;
            
            if (json_object_object_get_ex(sensor_obj, "points", &points_obj)) {
                int num_points = json_object_array_length(points_obj);
                app_config.calibrations[i].points = malloc(sizeof(CalibrationPoint) * num_points);
                app_config.calibrations[i].num_points = num_points;
                
                for (int j = 0; j < num_points; j++) {
                    struct json_object *point_obj = json_object_array_get_idx(points_obj, j);
                    struct json_object *voltage_obj, *percentage_obj;
                    
                    if (json_object_object_get_ex(point_obj, "voltage", &voltage_obj)) {
                        app_config.calibrations[i].points[j].voltage = json_object_get_double(voltage_obj);
                    }
                    
                    if (json_object_object_get_ex(point_obj, "percentage", &percentage_obj)) {
                        app_config.calibrations[i].points[j].percentage = json_object_get_double(percentage_obj);
                    }
                }
            }
        }
    }

    // 로깅 설정 로드
    struct json_object *logging_obj;
    if (json_object_object_get_ex(root, "logging", &logging_obj)) {
        struct json_object *level_obj, *file_obj;
        
        if (json_object_object_get_ex(logging_obj, "level", &level_obj)) {
            app_config.log_level = json_object_get_int(level_obj);
        }
        
        if (json_object_object_get_ex(logging_obj, "file", &file_obj)) {
            strncpy(app_config.log_file, json_object_get_string(file_obj), sizeof(app_config.log_file) - 1);
        }
    }

    json_object_put(root);
    return true;
} 