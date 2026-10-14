#include "aim_control.h"
#include <stdlib.h>

static int Clamp(int value, int low, int high)
{
    return value < low ? low : (value > high ? high : value);
}

void AimControl_Init(AimControl *control)
{
    *control = (AimControl){0};
    control->pwm_x = 1190;
    control->pwm_y = AIM_Y_MAX;
}

void AimControl_Step(AimControl *control, const AimInput *input,
                     uint32_t now, uint32_t timeout_ticks)
{
    if (input->target_sequence != control->seen_target) {
        control->seen_target = input->target_sequence;
        control->reached = 0;
    }
    if (!input->have_target || !input->have_position || control->reached ||
        (uint32_t)(now - input->position_tick) > timeout_ticks ||
        input->position_sequence == control->seen_position) {
        return;
    }
    control->seen_position = input->position_sequence;
    int x = input->current_x;
    int y = input->current_y;
    int dx = abs((int)input->x[0] - x);
    int dy = abs((int)input->y[0] - y);
    if (dx < 3 && dy < 3) {
        control->reached = 1;
        return;
    }
    int next_x = x;
    int next_y = y;
    int e2 = 2 * (dx - dy);
    if (e2 > -dy) {
        int step = dx < 5 ? dx : 5;
        next_x += x < input->x[0] ? step : -step;
    }
    if (e2 < dx) {
        int step = dy < 5 ? dy : 5;
        next_y += y < input->y[0] ? step : -step;
    }
    if (x < next_x) { --control->pwm_x; }
    if (x > next_x) { ++control->pwm_x; }
    if (y < next_y) { ++control->pwm_y; }
    if (y > next_y) { --control->pwm_y; }
    control->pwm_x = Clamp(control->pwm_x, AIM_X_MIN, AIM_X_MAX);
    control->pwm_y = Clamp(control->pwm_y, AIM_Y_MIN, AIM_Y_MAX);
}
