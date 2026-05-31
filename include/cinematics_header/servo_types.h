#ifndef SERVO_TYPES_H
#define SERVO_TYPES_H

#include <stdio.h>
#include <atomic>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <cmath>

#define SERVO_QUEUE_LEN 5

typedef struct {
    float target_rad;
    float speed;
    float acc;
    float jerk;
} ServoTaskParams;

typedef struct {
    uint32_t duty_res;
    int8_t gpio;
    uint32_t sgnl_min_duty;
    uint32_t sgnl_max_duty;
    // these values ranges from (-30.5/36+0.07)*PI rads to (+30.5/36-0.07)*PI corresponding to -139.9 degrees to +139.9 degrees,
    // with a total range of motion of 279.8 degrees, not ~309 in order to have some margin
    // because if the potentiometer barely exceeds this value, the servo will execute a +360 degrees rotation
    // in order to go back to the setted position
    float min_pos;
    float max_pos;
    std::atomic<float> current_pos; //this ensure thread safety
    std::atomic<float> current_speed;
    std::atomic<float> current_acc;
    float max_speed;
    float max_acc;
    float max_jerk;
    std::atomic<bool> moving;
} ServoData;
//modern C++ style type definition for declaring global variables in the header file without violating the one definition rule, and ensuring type safety

inline constexpr float trim= 0.07f*M_PI;
inline constexpr float backlash= 15.0f/180.0f*M_PI; //trying to compensate for backlash by n degrees
inline constexpr int servo_deadzone_ms=2;
//extern keyword means that the variable is defined in another source file
//and tells the compiler to not allocate memory for it in this file, but to look for its definition in the linked source files during the linking phase
extern ServoData servo_data;
extern float servo_deadzone;

extern QueueHandle_t xServoQueue; //queue handler
extern TaskHandle_t xTaskHandle; //task handler

#endif
