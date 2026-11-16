#ifndef SCOPE_H
#define SCOPE_H

#include <stdint.h>

void Scope_Init(void);
void Scope_Update(void);
uint8_t Scope_IsAcquiring(void);
void Scope_TogglePause(void);
void Scope_AutoSetup(void);
void Scope_AdjustValue(int8_t direction);
void Scope_ChangeTimebase(int8_t direction);
void Scope_ChangeVoltageScale(int8_t direction);

#endif
