#include "Machine.h"


uint8_t aogSections[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
uint8_t machineSections[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

void printByteInBinary(byte b) {
    for (int i = 7; i >= 0; i--) {
        if (b & (1 << i)) {
            Serial.print("1");
        }
        else {
            Serial.print("0");
        }
    }
    Serial.print(" ");
}

void printArrayInBinary(byte arr[], int size) {
    for (int i = 0; i < size; i++) {
        printByteInBinary(arr[i]);
    }
    Serial.println();
}



void pollMachineForSections()
{
}

void SendMachineSectionCommand()
{
}

