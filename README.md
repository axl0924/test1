#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define START_OF_MESSAGE 0xAA
#define END_OF_MESSAGE 0x55

// CRC 계산 함수 (예제용, 실제로는 더 정교한 알고리즘 사용 가능)
uint16_t calculate_crc(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// 데이터 전송 함수
void send_message(uint8_t msg_id, uint8_t *message, uint16_t msg_length) {
    uint8_t buffer[256];
    uint16_t index = 0;

    // Start of Message
    buffer[index++] = START_OF_MESSAGE;

    // Header (2 bytes: 6 bits msg_id, 10 bits msg_length)
    uint16_t header = ((msg_id & 0x3F) << 10) | (msg_length & 0x03FF);
    buffer[index++] = (header >> 8) & 0xFF;  // High byte
    buffer[index++] = header & 0xFF;         // Low byte

    // Message
    memcpy(&buffer[index], message, msg_length);
    index += msg_length;

    // CRC (2 bytes)
    uint16_t crc = calculate_crc(message, msg_length);
    buffer[index++] = (crc >> 8) & 0xFF;  // High byte
    buffer[index++] = crc & 0xFF;         // Low byte

    // End of Message
    buffer[index++] = END_OF_MESSAGE;

    // UART 전송 함수 (예제용 출력)
    for (uint16_t i = 0; i < index; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n");
}



#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define START_OF_MESSAGE 0xAA
#define END_OF_MESSAGE 0x55

// CRC 계산 함수 (A 기기와 동일)
uint16_t calculate_crc(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// 메시지 처리 함수
void process_message(const uint8_t *buffer, uint16_t length) {
    if (buffer[0] != START_OF_MESSAGE || buffer[length - 1] != END_OF_MESSAGE) {
        printf("Invalid message format\n");
        return;
    }

    // Header 읽기
    uint16_t header = (buffer[1] << 8) | buffer[2];
    uint8_t msg_id = (header >> 10) & 0x3F;
    uint16_t msg_length = header & 0x03FF;

    // 메시지 유효성 검증
    if (msg_length + 5 > length) {  // Header(2) + CRC(2) + End(1)
        printf("Invalid message length\n");
        return;
    }

    // 메시지 추출
    uint8_t message[msg_length];
    memcpy(message, &buffer[3], msg_length);

    // CRC 검증
    uint16_t received_crc = (buffer[3 + msg_length] << 8) | buffer[3 + msg_length + 1];
    uint16_t calculated_crc = calculate_crc(message, msg_length);
    if (received_crc != calculated_crc) {
        printf("CRC mismatch\n");
        return;
    }

    // 메시지 출력
    printf("Message ID: %d\n", msg_id);
    printf("Message: ");
    for (uint16_t i = 0; i < msg_length; i++) {
        printf("%02X ", message[i]);
    }
    printf("\n");
}

int main() {
    uint8_t received_data[] = {0xAA, 0x6A, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05, 0x58, 0x9B, 0x55};  // 예제 데이터
    process_message(received_data, sizeof(received_data));
    return 0;
}
int main() {
    uint8_t message[] = {0x01, 0x02, 0x03, 0x04, 0x05};  // 예제 메시지
    send_message(0x1A, message, sizeof(message));
    return 0;
}



