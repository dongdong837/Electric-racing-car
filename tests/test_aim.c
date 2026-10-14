#include "aim_protocol.h"
#include "aim_control.h"
#include <string.h>

volatile unsigned g_checks;
#define CHECK(condition) do { ++g_checks; if (!(condition)) return __LINE__; } while (0)

static void Finish(uint8_t *frame, unsigned length)
{
    unsigned sum = 0;
    frame[0] = 0x24;
    for (unsigned i = 1; i < length - 2; ++i) { sum += frame[i]; }
    frame[length - 2] = (uint8_t)sum;
    frame[length - 1] = 0x23;
}

static unsigned Feed(AimParser *parser, AimInput *input, const uint8_t *bytes, unsigned size)
{
    unsigned frames = 0;
    for (unsigned i = 0; i < size; ++i) { frames += AimParser_Push(parser, bytes[i], input) != 0; }
    return frames;
}

int RunTests(void)
{
    struct { uint32_t before; AimParser parser; uint32_t after; } guarded = {
        .before = 0x12345678, .parser = {{0}, 0}, .after = 0x87654321
    };
    AimParser *parser = &guarded.parser;
    AimInput input = {0};
    uint8_t target[22] = {0x24, 1, 2, 3, 0x34, 0x12, 0x78, 0x56,
                         0xFF, 0xFF, 0, 0, 1, 0, 2, 0, 3, 0, 4, 0};
    uint8_t position[11] = {0x24, 1, 2, 3, 4, 0xBC, 0x9A, 0xF0, 0xDE};
    Finish(target, 22);
    Finish(position, 11);
    CHECK(Feed(parser, &input, target, 7) == 0);
    CHECK(Feed(parser, &input, target + 7, 15) == 1);
    CHECK(input.x[0] == 0x1234 && input.y[0] == 0x5678 && input.x[1] == 65535);
    CHECK(input.x[3] == 3 && input.y[3] == 4 && input.shape == 3);
    CHECK(input.have_target && input.target_sequence == 1 && !input.have_position);
    CHECK(Feed(parser, &input, position, 11) == 1);
    CHECK(input.current_x == 0x9ABC && input.current_y == 0xDEF0 && input.position_sequence == 1);
    position[9] ^= 1;
    CHECK(Feed(parser, &input, position, 11) == 0);
    CHECK(input.position_sequence == 1);
    position[9] ^= 1;
    CHECK(Feed(parser, &input, position, 11) == 1);
    CHECK(input.position_sequence == 2);
    uint8_t noise[] = {0, 0x23, 0xFF, 0x24, 0, 0};
    Feed(parser, &input, noise, sizeof(noise));
    CHECK(Feed(parser, &input, target, 22) == 1);
    CHECK(Feed(parser, &input, position, 11) == 1);
    AimParser_Reset(parser);
    Feed(parser, &input, target, 5);
    AimParser_Reset(parser);
    CHECK(Feed(parser, &input, target, 22) == 1);
    uint32_t random = 12345;
    for (unsigned i = 0; i < 20000; ++i) {
        random = random * 1664525U + 1013904223U;
        AimParser_Push(parser, (uint8_t)(random >> 24), &input);
        CHECK(parser->used < AIM_FRAME_MAX);
        CHECK(guarded.before == 0x12345678 && guarded.after == 0x87654321);
    }

    AimControl control;
    AimControl_Init(&control);
    CHECK(control.pwm_x == 1190 && control.pwm_y == 1210);
    input = (AimInput){0};
    AimControl_Step(&control, &input, 0, 500);
    CHECK(control.pwm_x == 1190 && control.pwm_y == 1210);
    input.have_target = input.have_position = 1;
    input.target_sequence = input.position_sequence = 1;
    input.x[0] = 100; input.y[0] = 100;
    input.current_x = 50; input.current_y = 150;
    input.position_tick = 100;
    AimControl_Step(&control, &input, 100, 500);
    CHECK(control.pwm_x == 1189 && control.pwm_y == 1209);
    AimControl_Step(&control, &input, 110, 500);
    CHECK(control.pwm_x == 1189 && control.pwm_y == 1209);
    ++input.position_sequence;
    AimControl_Step(&control, &input, 601, 500);
    CHECK(control.pwm_x == 1189 && control.pwm_y == 1209);
    input.position_tick = 0xFFFFFFF0;
    AimControl_Step(&control, &input, 0x10, 500);
    CHECK(control.pwm_x == 1188 && control.pwm_y == 1208);
    input.position_tick = 0;
    for (unsigned i = 0; i < 2000; ++i) {
        ++input.position_sequence;
        AimControl_Step(&control, &input, 0, 500);
        CHECK(control.pwm_x >= AIM_X_MIN && control.pwm_y >= AIM_Y_MIN);
    }
    CHECK(control.pwm_x == AIM_X_MIN && control.pwm_y == AIM_Y_MIN);
    input.current_x = 150; input.current_y = 50;
    for (unsigned i = 0; i < 2000; ++i) {
        ++input.position_sequence;
        AimControl_Step(&control, &input, 0, 500);
        CHECK(control.pwm_x <= AIM_X_MAX && control.pwm_y <= AIM_Y_MAX);
    }
    CHECK(control.pwm_x == AIM_X_MAX && control.pwm_y == AIM_Y_MAX);
    input.current_x = input.x[0]; input.current_y = input.y[0];
    ++input.position_sequence;
    AimControl_Step(&control, &input, 0, 500);
    CHECK(control.reached);
    input.current_x = 50;
    ++input.position_sequence;
    AimControl_Step(&control, &input, 0, 500);
    CHECK(control.reached && control.pwm_x == AIM_X_MAX);
    ++input.target_sequence;
    AimControl_Step(&control, &input, 0, 500);
    CHECK(!control.reached && control.pwm_x == AIM_X_MAX - 1);
    input.current_x = 0; input.x[0] = 65535;
    input.current_y = 65535; input.y[0] = 0;
    ++input.position_sequence;
    AimControl_Step(&control, &input, 0, 500);
    CHECK(control.pwm_x == AIM_X_MAX - 2 && control.pwm_y == AIM_Y_MAX - 1);
    return 0;
}
