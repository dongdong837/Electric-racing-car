#ifndef AIM_CONTROL_H
#define AIM_CONTROL_H
#include "aim_protocol.h"

#define AIM_X_MIN 1100
#define AIM_X_MAX 1280
#define AIM_Y_MIN 1050
#define AIM_Y_MAX 1210
typedef struct {
    int pwm_x, pwm_y;
    uint32_t seen_target, seen_position;
    uint8_t reached;
} AimControl;

void AimControl_Init(AimControl *control);
void AimControl_Step(AimControl *control, const AimInput *input,
                     uint32_t now, uint32_t timeout_ticks);
#endif
