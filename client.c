#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <json-c/json.h>

#define SERVER_IP "192.168.14.5"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

void send_data(const char *json_data);

int main() {
    while (1) {
        struct json_object *json_data = json_object_new_object();

        json_object_object_add(json_data, "timestamp", json_object_new_string("2024-11-26T10:30:00Z"));
        json_object_object_add(json_data, "sensor_id", json_object_new_string("pH_sensor_01"));
        json_object_object_add(json_data, "location", json_object_new_string("tank_1"));
        json_object_object_add(json_data, "pH_value", json_object_new_double(7.1));
        json_object_object_add(json_data, "voltage", json_object_new_double(2.95));
        json_object_object_add(json_data, "table", json_object_new_string("tb_ph"));

        const char *json_str = json_object_to_json_string(json_data);
        send_data(json_str);

        json_object_put(json_data);
        sleep(5); // 5초마다 전송
    }
    return 0;
}

void send_data(const char *json_data) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address or Address not supported");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    send(sock, json_data, strlen(json_data), 0);
    printf("Data sent: %s\n", json_data);
    close(sock);
}