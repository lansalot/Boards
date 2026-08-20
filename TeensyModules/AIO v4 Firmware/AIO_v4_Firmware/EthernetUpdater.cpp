#include "EthernetUpdater.h"

#include <NativeEthernet.h>
#include <string.h>
#include "FXUtil.h"		// read_ascii_line(), hex file support
extern "C" {
#include "FlashTxx.h"
}

EthernetUpdater::EthernetUpdater()
	: comm_(),
	destination_(),
	sender_(),
	started_(false),
	updateMode_(false),
	packetLength_(0),
	lastHeartbeatMs_(0),
	displayCount_(0),
	bufferAddr_(0),
	bufferSize_(0),
	hex_{}
{
	resetHexState();
}

void EthernetUpdater::begin()
{
	IPAddress local = Ethernet.localIP();
	destination_ = local;
	destination_[3] = 255;

	started_ = comm_.begin(ReceivePort);
	if (started_)
	{
		lastHeartbeatMs_ = millis();
		Serial.print("Ethernet Update UDP listening to port: ");
		Serial.println(ReceivePort);
	}
}

void EthernetUpdater::poll()
{
	if (!started_)
	{
		return;
	}

	if (Ethernet.linkStatus() != LinkON)
	{
		return;
	}

	uint32_t now = millis();
	if (now - lastHeartbeatMs_ >= 3000U)
	{
		sendHeartbeat();
		lastHeartbeatMs_ = now;
	}

	packetLength_ = comm_.parsePacket();
	if (packetLength_ == 0)
	{
		return;
	}
	if (packetLength_ > sizeof(receivedData_))
	{
		packetLength_ = sizeof(receivedData_);
	}

	comm_.read(receivedData_, packetLength_);
	if (updateMode_)
	{
		// Serial.print("update rx len=");
		// Serial.print(packetLength_);
		// Serial.print(" first=0x");
		// Serial.println(receivedData_[0], HEX);

		if (processHexRecord(reinterpret_cast<char*>(receivedData_), packetLength_))
		{
			Serial.print("Received update packet with len " + String(packetLength_));
			Serial.println();
			Serial.println("Update error.");
			Serial.printf("erase FLASH buffer / free RAM buffer...\n");
			delay(5000);
			firmware_buffer_free(bufferAddr_, bufferSize_);
			REBOOT;
		}
		return;
	}

	uint8_t pgnLength;
	uint16_t pgn = receivedData_[1] << 8 | receivedData_[0];

	switch (pgn)
	{
	case 32800:
		Serial.println("update packet spotted");
		pgnLength = 6;
		if (packetLength_ > pgnLength - 1)
		{
			if (goodCRC(receivedData_, pgnLength))
			{
				sender_ = comm_.remoteIP();
				if (firmware_buffer_init(&bufferAddr_, &bufferSize_))
				{
					Serial.printf("target = %s (%dK flash in %dK sectors)\n", FLASH_ID, FLASH_SIZE / 1024, FLASH_SECTOR_SIZE / 1024);
					Serial.printf("buffer = %1luK %s (%08lX - %08lX)\n", bufferSize_ / 1024, IN_FLASH(bufferAddr_) ? "FLASH" : "RAM", bufferAddr_, bufferAddr_ + bufferSize_);
					Serial.print("update sender IP: ");
					Serial.println(sender_);
					Serial.println("waiting for hex lines...\n");
					resetHexState();
					updateMode_ = true;
					sendReceiveReady();
				}
				else
				{
					Serial.println("Unable to create update buffer.");
				}
			}
		}
		break;
	}
}

bool EthernetUpdater::isUpdating() const
{
	return updateMode_;
}

int EthernetUpdater::linesReceived() const
{
	return hex_.lines;
}

void EthernetUpdater::sendLineCheck()
{
	if (!started_)
	{
		return;
	}

	if (Ethernet.linkStatus() != LinkON)
	{
		return;
	}

	uint8_t data[7];
	data[0] = 33;
	data[1] = 128;
	data[2] = hex_.lines & 255;
	data[3] = hex_.lines >> 8 & 255;
	data[4] = hex_.lines >> 16 & 255;
	data[5] = hex_.lines >> 24 & 255;
	data[6] = crc(data, 6, 0);

	comm_.beginPacket(destination_, SendPort);
	comm_.write(data, sizeof(data));
	comm_.endPacket();
}

void EthernetUpdater::sendReceiveReady()
{
	if (!started_)
	{
		return;
	}

	if (Ethernet.linkStatus() != LinkON)
	{
		return;
	}

	uint8_t data[5];
	data[0] = 34;
	data[1] = 128;
	data[2] = 0;
	data[3] = 100;
	data[4] = crc(data, 4, 0);

	Serial.print("Sending receive ready packet to broadcast ");
	Serial.print(destination_);
	Serial.print(":");
	Serial.println(SendPort);
	comm_.beginPacket(destination_, SendPort);
	comm_.write(data, sizeof(data));
	comm_.endPacket();

}

void EthernetUpdater::sendHeartbeat()
{
	if (!started_)
	{
		return;
	}

	if (Ethernet.linkStatus() != LinkON)
	{
		return;
	}

	uint8_t data[8] = { 35, 128, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	comm_.beginPacket(destination_, SendPort);
	comm_.write(data, sizeof(data));
	comm_.endPacket();
}

int EthernetUpdater::processHexRecord(char* packetBuffer, int packetSize)
{
	if (packetSize < 5)
	{
		return 1;
	}
	else if (packetBuffer[0] != 0x3a)
	{
		started_ = false; // andrew you added this
		Serial.printf("abort - invalid hex code %d\n", hex_.code);
		return 0;
	}
	else
	{
		for (int idx = 1; idx + 4 < packetSize;)
		{
			uint8_t len = static_cast<uint8_t>(packetBuffer[idx]);
			unsigned int addr = static_cast<uint8_t>(packetBuffer[idx + 1]) << 8 | static_cast<uint8_t>(packetBuffer[idx + 2]);
			uint8_t type = static_cast<uint8_t>(packetBuffer[idx + 3]);

			if (len > sizeof(hex_.data))
			{
				Serial.printf("abort - invalid hex length %d\n", len);
				return 1;
			}

			if (idx + 4 + len < packetSize)
			{
				unsigned sum = (len & 255) + ((addr >> 8) & 255) + (addr & 255) + (type & 255);

				for (uint8_t j = 0; j < len; j++)
				{
					sum += static_cast<uint8_t>(packetBuffer[idx + 4 + j]) & 255;
					hex_.data[j] = packetBuffer[idx + 4 + j];
				}

				hex_.num = len;
				hex_.code = type;
				hex_.addr = addr;

				uint8_t checksum = static_cast<uint8_t>(packetBuffer[idx + 4 + len]);
				if (((sum & 255) + (checksum & 255)) & 255)
				{
					Serial.println("abort - bad hex line");
					return 1;
				}
				else
				{
					if (hex_.code == 0)
					{
						if (hex_.base + hex_.addr + hex_.num > hex_.max)
						{
							hex_.max = hex_.base + hex_.addr + hex_.num;
						}
						if (hex_.base + hex_.addr < hex_.min)
						{
							hex_.min = hex_.base + hex_.addr;
						}

						uint32_t addrInBuffer = bufferAddr_ + hex_.base + hex_.addr - FLASH_BASE_ADDR;
						if (hex_.max > (FLASH_BASE_ADDR + bufferSize_))
						{
							Serial.printf("abort - max address %08lX too large\n", hex_.max);
							return 1;
						}
						else if (!IN_FLASH(bufferAddr_))
						{
							memcpy(reinterpret_cast<void*>(addrInBuffer), reinterpret_cast<void*>(hex_.data), hex_.num);
						}
						else if (IN_FLASH(bufferAddr_))
						{
							int error = flash_write_block(addrInBuffer, hex_.data, hex_.num);
							if (error)
							{
								Serial.println();
								Serial.printf("abort - error %02X in flash_write_block()\n", error);
								return 1;
							}
						}
					}
					else if (hex_.code == 1)
					{
						Serial.println("");
						Serial.println("EOF");
						hex_.eof = 1;
					}
					else if (hex_.code == 2)
					{
						hex_.base = ((hex_.data[0] << 8) | hex_.data[1]) << 4;
					}
					else if (hex_.code == 4)
					{
						hex_.base = ((hex_.data[0] << 8) | hex_.data[1]) << 16;
					}
					else if (hex_.code == 5)
					{
						hex_.base = (hex_.data[0] << 24) | (hex_.data[1] << 16)
							| (hex_.data[2] << 8) | (hex_.data[3] << 0);
					}
					else if (hex_.code == 6)
					{
						Serial.printf("UPDATE!!!\n");
						flash_move(FLASH_BASE_ADDR, bufferAddr_, hex_.max - hex_.min);
						REBOOT;
						return 0;
					}
					else if (hex_.code == 7)
					{
						Serial.printf("LINES DONT Match\n");
						return 1;
					}
					else
					{
						Serial.println();
						Serial.printf("abort - invalid hex code %d\n", hex_.code);
						return 1;
					}

					hex_.lines++;
					if (displayCount_++ > 100)
					{
						displayCount_ = 0;
						Serial.print(".");
					}

					if (hex_.eof)
					{
						Serial.println();
						Serial.printf("\nhex file: %1d lines %1lu bytes (%08lX - %08lX)\n", hex_.lines, hex_.max - hex_.min, hex_.min, hex_.max);
						sendLineCheck();
					}

					idx += 6 + len;
				}
			}
			else
			{
				Serial.printf("abort - invalid hex code %d\n", hex_.code);
				return 1;
			}
		}
	}

	return 0;
}

void EthernetUpdater::resetHexState()
{
	memset(hex_.data, 0, sizeof(hex_.data));
	hex_.addr = 0;
	hex_.code = 0;
	hex_.num = 0;
	hex_.base = 0;
	hex_.min = 0xFFFFFFFF;
	hex_.max = 0;
	hex_.eof = 0;
	hex_.lines = 0;
	displayCount_ = 0;
}

bool EthernetUpdater::goodCRC(const uint8_t data[], uint8_t length)
{
	uint8_t ck = crc(data, length - 1, 0);
	bool result = (ck == data[length - 1]);
	return result;
}

uint8_t EthernetUpdater::crc(const uint8_t chk[], uint8_t length, uint8_t start)
{
	uint8_t result = 0;
	int checksum = 0;
	for (int i = start; i < length; i++)
	{
		checksum += chk[i];
	}
	result = static_cast<uint8_t>(checksum);
	return result;
}

uint8_t EthernetUpdater::parseModID(uint8_t id)
{
	return id >> 4;
}
