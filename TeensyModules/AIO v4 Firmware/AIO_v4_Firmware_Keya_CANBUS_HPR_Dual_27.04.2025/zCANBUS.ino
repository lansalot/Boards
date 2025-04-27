
// templates for commands with matching responses, only need first 4 bytes
uint8_t keyaDisableCommand[] = { 0x23, 0x0C, 0x20, 0x01 };
uint8_t keyaDisableResponse[] = { 0x60, 0x0C, 0x20, 0x00 };

uint8_t keyaEnableCommand[] = { 0x23, 0x0D, 0x20, 0x01 };
uint8_t keyaEnableResponse[] = { 0x60, 0x0D, 0x20, 0x00 };

uint8_t keyaSpeedCommand[] = { 0x23, 0x00, 0x20, 0x01 };
uint8_t keyaSpeedResponse[] = { 0x60, 0x00, 0x20, 0x00 };

uint8_t keyaCurrentQuery[] = { 0x40, 0x00, 0x21, 0x01 };
uint8_t keyaCurrentResponse[] = { 0x60, 0x00, 0x21, 0x01 };

uint8_t keyaFaultQuery[] = { 0x40, 0x12, 0x21, 0x01 };
uint8_t keyaFaultResponse[] = { 0x60, 0x12, 0x21, 0x01 };

uint8_t keyaVoltageQuery[] = { 0x40, 0x0D, 0x21, 0x02 };
uint8_t keyaVoltageResponse[] = { 0x60, 0x0D, 0x21, 0x02 };

uint8_t keyaTemperatureQuery[] = { 0x40, 0x0F, 0x21, 0x01 };
uint8_t keyaTemperatureResponse[] = { 0x60, 0x0F, 0x21, 0x01 };

uint8_t keyaVersionQuery[] = { 0x40, 0x01, 0x11, 0x11 };
uint8_t keyaVersionResponse[] = { 0x60, 0x01, 0x11, 0x11 };

uint8_t keyaEncoderResponse[] = { 0x60, 0x04, 0x21, 0x01 };

uint8_t keyaEncoderSpeedResponse[] = { 0x60, 0x03, 0x21, 0x01 };

uint64_t KeyaID = 0x06000001; // 0x01 is default ID


void CAN_Setup()
{
	Keya_Bus.begin();
	Keya_Bus.setBaudRate(250000);
}

void KeyaBus_Receive()
{
    CAN_message_t KeyaBusReceiveData;
    if (Keya_Bus.read(KeyaBusReceiveData))
    {
        // Heatbeat
        if (KeyaBusReceiveData.id == 0x07000001)
        {
            lastKeyaHeatbeat = 0;

            if (!keyaDetected)
            {
                Serial.println("Keya heartbeat detected! Enabling Keya CANBUS");
                keyaDetected = true;
            }
            // 0-1 - Cumulative value of angle (360 def / circle)
            // 2-3 - Motor speed, signed int eg -500 or 500
            // 4-5 - Motor current
            // 6-7 - Control_Close (error code)
            // TODO Yeah, if we ever see something here, fire off a disable, refuse to engage autosteer or..?

            uint32_t time = millis();

            keyaSteeringPosition = (int16_t)((int16_t)KeyaBusReceiveData.buf[0] << 8 | (int16_t)KeyaBusReceiveData.buf[1]) * -1;
            keyaCurrentActualSpeed = (int16_t)((int16_t)KeyaBusReceiveData.buf[2] << 8 | (int16_t)KeyaBusReceiveData.buf[3]);
            int16_t current = (int16_t)((int16_t)KeyaBusReceiveData.buf[4] << 8 | (int16_t)KeyaBusReceiveData.buf[5]);
            
            int16_t error = abs(keyaCurrentActualSpeed - keyaCurrentSetSpeed);
            static int16_t counter = 0;
            
            if (error > abs(keyaCurrentSetSpeed) + 10)
            {
                if (counter++ < 8)
                {
                    //Serial.print("Counter\t");
                }
                else
                {
                    //Serial.print("Stop\t");
                    sensorReading = abs(abs(keyaCurrentSetSpeed) - error);
                }
            }
            else
            {
                //Serial.print("Run\t");
                sensorReading = 0;
                counter = 0;
            }
            /*
            Serial.print(keyaSteeringPosition);
            Serial.print("\t");
            Serial.print(keyaCurrentActualSpeed);
            Serial.print("\t");
            Serial.print(current);
            Serial.print("\t");
            */
            
            //Serial.print(keyaCurrentSetSpeed);
            //Serial.print("\t");
            //Serial.print(keyaCurrentActualSpeed);
            //Serial.print("\t");
            //Serial.print(error);
            //Serial.print("\t");
            
            
            //if (bitRead(KeyaBusReceiveData.buf[7], 0)) Serial.print("Disabled\t");
            //else Serial.print("Enabled \t");
            
            // check if there's any motor diag/error data and parse it
            if (KeyaBusReceiveData.buf[7] > 1 || KeyaBusReceiveData.buf[6] > 0)
            {
                if (bitRead(KeyaBusReceiveData.buf[7], 1)) Serial.print("Over voltage\t");
                if (bitRead(KeyaBusReceiveData.buf[7], 2)) Serial.print("Hardware protection\t");
                if (bitRead(KeyaBusReceiveData.buf[7], 3)) Serial.print("E2PROM\t");
                if (bitRead(KeyaBusReceiveData.buf[7], 4)) Serial.print("Under voltage\t");
                if (bitRead(KeyaBusReceiveData.buf[7], 5)) Serial.print("N/A\t");
                if (bitRead(KeyaBusReceiveData.buf[7], 6)) Serial.print("Over current\t");
                if (bitRead(KeyaBusReceiveData.buf[7], 7)) Serial.print("Mode failure\t");

                if (bitRead(KeyaBusReceiveData.buf[6], 0)) Serial.print("Less phase\t");
                if (bitRead(KeyaBusReceiveData.buf[6], 1)) Serial.print("Motor stall\t");
                if (bitRead(KeyaBusReceiveData.buf[6], 2)) Serial.print("Reserved\t");
                if (bitRead(KeyaBusReceiveData.buf[6], 3)) Serial.print("Hall failure\t");
                if (bitRead(KeyaBusReceiveData.buf[6], 4)) Serial.print("Current sensing\t");
                if (bitRead(KeyaBusReceiveData.buf[6], 5)) Serial.print("No RS232 Steer Command\t");
                if (bitRead(KeyaBusReceiveData.buf[6], 6)) Serial.print("No CAN Steer Command\t");
                if (bitRead(KeyaBusReceiveData.buf[6], 7)) Serial.print("Motor stalled\t");
                
                Serial.println("Kill Autosteer");
                steerSwitch = 1;
                currentState = 1;
                previous = 0;
            }

            //Serial.println();
        }
    }
}

void SteerKeya(int steerSpeed, bool intendToSteer)
{
    if (keyaDetected)
    {
        int16_t actualSpeed;

        if (intendToSteer)
        {
            actualSpeed = steerSpeed * 3.9;
        }
        else
        {
            keyaCommand(keyaDisableCommand);
            actualSpeed = 0;
        }

        keyaCurrentSetSpeed = actualSpeed * 0.1;

        CAN_message_t KeyaBusSendData;
        KeyaBusSendData.id = KeyaID;
        KeyaBusSendData.flags.extended = true;
        KeyaBusSendData.len = 8;
        memcpy(KeyaBusSendData.buf, keyaSpeedCommand, 4);
        if (steerSpeed < 0)
        {
            KeyaBusSendData.buf[4] = highByte(actualSpeed);
            KeyaBusSendData.buf[5] = lowByte(actualSpeed);
            KeyaBusSendData.buf[6] = 0xff;
            KeyaBusSendData.buf[7] = 0xff;
        }
        else
        {
            KeyaBusSendData.buf[4] = highByte(actualSpeed);
            KeyaBusSendData.buf[5] = lowByte(actualSpeed);
            KeyaBusSendData.buf[6] = 0x00;
            KeyaBusSendData.buf[7] = 0x00;
        }
        Keya_Bus.write(KeyaBusSendData);

        if(intendToSteer) keyaCommand(keyaEnableCommand);
    }
}

// only issue one query at a time, wait for respone
void keyaCommand(uint8_t command[])
{
    if (keyaDetected)
    {
        CAN_message_t KeyaBusSendData;
        KeyaBusSendData.id = KeyaID;
        KeyaBusSendData.flags.extended = true;
        KeyaBusSendData.len = 8;
        memcpy(KeyaBusSendData.buf, command, 4);
        Keya_Bus.write(KeyaBusSendData);
    }
}
