#ifndef _CANBUS_h
#define _CANBUS_h

#include <FlexCAN_T4.h>

extern uint8_t currentState, reading, previous, remoteSwitch, steerSwitch;

void CAN_Setup();
void CANBUS_Receive();

#endif


