#pragma once
#pragma pack(1)
#include <cstdint>
#define ROOT_ID 0
#define NEAREST_CUBE -1

enum MessageType : int {
    MSG_HELLO,
    MSG_SERVO,
    MSG_ROOT_INFO,
};

struct HelloPayload {
    int id;
};

struct ServoPayload {
    int servo_n;
    float radian;
};

struct RootInfoPayload {
    int my_id;
    int my_master_id;
    int my_slave_id;
};

struct Message {
  int dest_id;
  MessageType type;
  union {
    HelloPayload hello_payload;
    ServoPayload servo_payload;
    RootInfoPayload root_info_payload;
  } payload;
};
