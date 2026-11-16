#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

typedef enum
{
    ENCODER_BUTTON_NONE = 0,
    ENCODER_BUTTON_SHORT,
    ENCODER_BUTTON_LONG
} EncoderButtonEvent;

void Encoder_Init(void);
int8_t Encoder_GetDelta(void);
EncoderButtonEvent Encoder_GetButtonEvent(void);

#endif
