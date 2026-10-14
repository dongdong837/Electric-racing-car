#include "aim_protocol.h"
#include <string.h>

static uint16_t ReadLE16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint8_t ValidFrame(const uint8_t *p, uint8_t length)
{
    uint8_t sum = 0;
    if (p[0] != 0x24U || p[length - 1U] != 0x23U) {
        return 0;
    }
    for (uint8_t i = 1; i < length - 2U; ++i) {
        sum = (uint8_t)(sum + p[i]);
    }
    return (uint8_t)(sum == p[length - 2U]);
}

void AimParser_Reset(AimParser *parser)
{
    parser->used = 0;
}

uint8_t AimParser_Push(AimParser *parser, uint8_t byte, AimInput *input)
{
    uint8_t kind = 0;
    parser->bytes[parser->used++] = byte;
    while (parser->used != 0U) {
        const uint8_t *p = parser->bytes;
        if (p[0] == 0x24U) {
            if (parser->used >= 11U && ValidFrame(p, 11U)) {
                input->current_x = ReadLE16(&p[5]);
                input->current_y = ReadLE16(&p[7]);
                input->have_position = 1;
                ++input->position_sequence;
                kind = 11U;
            } else if (parser->used == 22U && ValidFrame(p, 22U)) {
                input->shape = p[3];
                for (uint8_t i = 0; i < 4U; ++i) {
                    input->x[i] = ReadLE16(&p[4U + 4U * i]);
                    input->y[i] = ReadLE16(&p[6U + 4U * i]);
                }
                input->have_target = 1;
                ++input->target_sequence;
                kind = 22U;
            } else if (parser->used < AIM_FRAME_MAX) {
                break;
            }
        }
        uint8_t consumed = (kind != 0U) ? kind : 1U;
        parser->used = (uint8_t)(parser->used - consumed);
        memmove(parser->bytes, &parser->bytes[consumed], parser->used);
        if (kind != 0U) {
            break;
        }
    }
    return kind;
}
