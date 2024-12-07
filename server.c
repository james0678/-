#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <json-c/json.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void initialize_database();
void save_to_database(const char *json_data);
void start_server();

int main() {
    initialize_database();
    start_server();
    return 0;
}

// SQLite 데이터베이스 초기화
void initialize_database() {
    sqlite3 *db;
    char *err_msg = NULL;

    int rc = sqlite3_open("sensor_data.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }

    const char *sql = 
        "CREATE TABLE IF NOT EXISTS tb_ph ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "timestamp TEXT, "
        "sensor_id TEXT, "
        "location TEXT, "
        "pH_value REAL, "
        "voltage REAL);";

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(1);
    }

    sqlite3_close(db);
}

// 서버 소켓 설정 및 실행
void start_server() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(1);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(1);
    }

    printf("Server is listening on port %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        int bytes_read = read(new_socket, buffer, BUFFER_SIZE);
        if (bytes_read > 0) {
            printf("Received data: %s\n", buffer);
            save_to_database(buffer);
        }

        close(new_socket);
        memset(buffer, 0, BUFFER_SIZE);
    }
}

// 데이터베이스에 JSON 데이터 저장
void save_to_database(const char *json_data) {
    sqlite3 *db;
    char *err_msg = NULL;

    int rc = sqlite3_open("sensor_data.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    // JSON 데이터 파싱
    struct json_object *parsed_json;
    struct json_object *timestamp, *sensor_id, *location, *pH_value, *voltage;

    parsed_json = json_tokener_parse(json_data);
    json_object_object_get_ex(parsed_json, "timestamp", &timestamp);
    json_object_object_get_ex(parsed_json, "sensor_id", &sensor_id);
    json_object_object_get_ex(parsed_json, "location", &location);
    json_object_object_get_ex(parsed_json, "pH_value", &pH_value);
    json_object_object_get_ex(parsed_json, "voltage", &voltage);

    // SQL INSERT 명령 실행
    const char *sql_template = 
        "INSERT INTO tb_ph (timestamp, sensor_id, location, pH_value, voltage) "
        "VALUES ('%s', '%s', '%s', %.1f, %.2f);";
    char sql[512];
    snprintf(sql, sizeof(sql), sql_template, 
             json_object_get_string(timestamp), 
             json_object_get_string(sensor_id), 
             json_object_get_string(location), 
             json_object_get_double(pH_value), 
             json_object_get_double(voltage));

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to insert data: %s\n", err_msg);
        sqlite3_free(err_msg);
    } else {
        printf("Data inserted successfully: %s\n", json_data);
    }

    sqlite3_close(db);
}