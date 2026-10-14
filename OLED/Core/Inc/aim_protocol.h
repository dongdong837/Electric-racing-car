#ifndef AIM_PROTOCOL_H
#define AIM_PROTOCOL_H
#include <stdint.h>

#define AIM_FRAME_MAX 22U
typedef struct {
    uint16_t x[4], y[4];
    uint16_t current_x, current_y;
    uint8_t shape;
    uint8_t have_target, have_position;
    uint32_t target_sequence, position_sequence;
    uint32_t position_tick;
} AimInput;

typedef struct {
    uint8_t bytes[AIM_FRAME_MAX];
    uint8_t used;
} AimParser;

void AimParser_Reset(AimParser *parser);
uint8_t AimParser_Push(AimParser *parser, uint8_t byte, AimInput *input);
#endif
