#ifndef _MACHINE_H
#define _MACHINE_H

#include <Arduino.h>

extern uint8_t aogSections[8];
extern uint8_t machineSections[8];

void printByteInBinary(byte b);
void printArrayInBinary(byte arr[], int size);

void pollMachineForSections();

void SendMachineSectionCommand();

#endif
