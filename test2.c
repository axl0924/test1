#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define CHUNK_SIZE 1024
#define DATA_SIZE (CHUNK_SIZE - 6) // Start(1) + Header(2) + CRC(2) + End(1)
#define MAIN_HEADER_SIZE 16       // 워터마크(4) + 바이너리 갯수(4) + 더미(8)
#define SUB_HEADER_SIZE 64        // 워터마크(4) + 시작 위치(4) + 크기(4) + CRC(4) + 더미
#define ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))

#define START_OF_MESSAGE 0x02
#define END_OF_MESSAGE 0x03

typedef struct {
    uint32_t watermark;
    uint32_t sub_binary_count;
    uint8_t dummy[8];
} MainHeader;

typedef struct {
    uint32_t watermark;
    uint32_t start_offset;
    uint32_t size;
    uint32_t crc;
    uint8_t dummy[48];
} SubHeader;

typedef struct {
    uint8_t start;
    uint16_t header; // 상위 6bit: msg_id, 하위 10bit: message length
    uint8_t data[DATA_SIZE];
    uint16_t crc;
    uint8_t end;
} Chunk;

// CRC 계산 함수
uint32_t calculate_crc(uint8_t *data, size_t size) {
    uint32_t crc = 0;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return crc;
}

// A 기기: 바이너리와 헤더 구성
void prepare_headers(MainHeader *main_header, SubHeader *sub_headers, uint8_t **sub_binaries, uint32_t *sub_sizes, uint32_t sub_count) {
    main_header->watermark = 0x12345678; // 메인 워터마크
    main_header->sub_binary_count = sub_count;
    memset(main_header->dummy, 0, sizeof(main_header->dummy));

    uint32_t current_offset = MAIN_HEADER_SIZE + SUB_HEADER_SIZE * sub_count;

    for (uint32_t i = 0; i < sub_count; i++) {
        sub_headers[i].watermark = 0xABCDEF00 + i; // Sub 워터마크
        sub_headers[i].start_offset = current_offset;
        sub_headers[i].size = sub_sizes[i];
        sub_headers[i].crc = calculate_crc(sub_binaries[i], sub_sizes[i]);
        memset(sub_headers[i].dummy, 0, sizeof(sub_headers[i].dummy));

        current_offset += ALIGN(sub_sizes[i], 4);
    }
}

// A 기기: 바이너리를 chunk 단위로 전송
void send_binary(uint8_t **sub_binaries, uint32_t *sub_sizes, uint32_t sub_count, FILE *b_device) {
    MainHeader main_header;
    SubHeader sub_headers[sub_count];
    prepare_headers(&main_header, sub_headers, sub_binaries, sub_sizes, sub_count);

    // 메인 헤더 전송
    fwrite(&main_header, 1, sizeof(main_header), b_device);

    // Sub 헤더 전송
    fwrite(sub_headers, 1, sizeof(SubHeader) * sub_count, b_device);

    // 바이너리를 chunk로 전송
    uint16_t msg_id = 0;
    for (uint32_t i = 0; i < sub_count; i++) {
        uint32_t offset = 0;

        while (offset < sub_sizes[i]) {
            Chunk chunk = {0};
            chunk.start = START_OF_MESSAGE;

            size_t chunk_data_size = (sub_sizes[i] - offset > DATA_SIZE - 4) ? (DATA_SIZE - 4) : (sub_sizes[i] - offset);
            chunk.header = (msg_id << 10) | (chunk_data_size + 5); // Header 구성
            memcpy(chunk.data, &offset, 4);                       // Data 첫 4바이트: 누적 오프셋
            memcpy(chunk.data + 4, sub_binaries[i] + offset, chunk_data_size);

            chunk.crc = calculate_crc(chunk.data, chunk_data_size + 4);
            chunk.end = END_OF_MESSAGE;

            fwrite(&chunk, 1, CHUNK_SIZE, b_device);
            offset += chunk_data_size;
            msg_id++;
        }
    }
}

// B 기기: 수신된 chunk 검증 및 저장
int validate_and_store_chunk(Chunk *chunk, FILE *buffer, size_t *received_size) {
    if (chunk->start != START_OF_MESSAGE || chunk->end != END_OF_MESSAGE)
        return -1;

    uint16_t calculated_crc = calculate_crc(chunk->data, chunk->header & 0x03FF);
    if (chunk->crc != calculated_crc)
        return -1;

    uint32_t offset;
    memcpy(&offset, chunk->data, 4);
    fwrite(chunk->data + 4, 1, (chunk->header & 0x03FF) - 4, buffer);

    *received_size += (chunk->header & 0x03FF) - 4;
    return 0;
}

// B 기기: 전체 바이너리 CRC 확인
int validate_all_sub_binaries(FILE *buffer, SubHeader *sub_headers, uint32_t sub_count) {
    for (uint32_t i = 0; i < sub_count; i++) {
        uint8_t *binary = malloc(sub_headers[i].size);
        fseek(buffer, sub_headers[i].start_offset, SEEK_SET);
        fread(binary, 1, sub_headers[i].size, buffer);

        uint32_t calculated_crc = calculate_crc(binary, sub_headers[i].size);
        free(binary);

        if (calculated_crc != sub_headers[i].crc)
            return -1;
    }
    return 0;
}

int main() {
    // 예제 데이터
    uint8_t binary1[2048], binary2[1024];
    memset(binary1, 0xAA, sizeof(binary1));
    memset(binary2, 0xBB, sizeof(binary2));

    uint8_t *sub_binaries[] = {binary1, binary2};
    uint32_t sub_sizes[] = {sizeof(binary1), sizeof(binary2)};
    uint32_t sub_count = 2;

    // A 기기에서 B 기기로 전송
    FILE *b_device = fopen("b_device.bin", "wb+");
    send_binary(sub_binaries, sub_sizes, sub_count, b_device);
    fclose(b_device);

    // B 기기에서 수신 및 검증
    FILE *buffer = fopen("b_device.bin", "rb");
    MainHeader main_header;
    fread(&main_header, 1, sizeof(main_header), buffer);

    SubHeader sub_headers[main_header.sub_binary_count];
    fread(sub_headers, 1, sizeof(SubHeader) * main_header.sub_binary_count, buffer);

    size_t received_size = 0;
    while (received_size < sub_headers[main_header.sub_binary_count - 1].start_offset + sub_headers[main_header.sub_binary_count - 1].size) {
        Chunk chunk;
        fread(&chunk, 1, CHUNK_SIZE, buffer);
        validate_and_store_chunk(&chunk, buffer, &received_size);
    }

    if (validate_all_sub_binaries(buffer, sub_headers, main_header.sub_binary_count) == 0)
        printf("Binary transfer successful and validated.\n");
    else
        printf("Binary transfer failed.\n");

    fclose(buffer);
    return 0;
}
