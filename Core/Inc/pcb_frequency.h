#ifndef INC_PCB_FREQUENCY_H_
#define INC_PCB_FREQUENCY_H_

#include "main.h"

void Frequency_Init(void);
void Frequency_Reset(void);
uint32_t Frequency_Get_Pulse_Count(void);

#endif /* INC_PCB_FREQUENCY_H_ */