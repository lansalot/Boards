#pragma once

#include <Arduino.h>
#include <NativeEthernetUdp.h>

class EthernetUpdater
{
public:
    EthernetUpdater();

    void begin();
    void poll();

    bool isUpdating() const;
    int linesReceived() const;

private:
    struct HexInfo
    {
        char data[16];
        unsigned int addr;
        unsigned int code;
        unsigned int num;

        uint32_t base;
        uint32_t min;
        uint32_t max;

        int eof;
        int lines;
    };

    EthernetUDP comm_;
    IPAddress destination_;
    IPAddress sender_;
    bool started_;

    bool updateMode_;
    uint16_t packetLength_;
    uint8_t receivedData_[500];
    uint32_t lastHeartbeatMs_;
    int displayCount_;
    uint32_t bufferAddr_;
    uint32_t bufferSize_;
    HexInfo hex_;

    static constexpr uint16_t ReceivePort = 29100;
    static constexpr uint16_t SendPort = 29000;

    void resetHexState();
    void sendLineCheck();
    void sendReceiveReady();
    void sendHeartbeat();

    int processHexRecord(char* packetBuffer, int packetSize);

    static bool goodCRC(const uint8_t data[], uint8_t length);
    static uint8_t crc(const uint8_t chk[], uint8_t length, uint8_t start);
    static uint8_t parseModID(uint8_t id);
};
