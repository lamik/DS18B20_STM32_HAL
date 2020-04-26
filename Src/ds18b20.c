/*
 * ds18b20.c
 *
 *	The MIT License.
 *  Created on: 20.09.2018
 *      Author: Mateusz Salamon
 *      www.msalamon.pl
 *      mateusz@msalamon.pl
 *
 */
#include "ds18b20.h"

//
//	VARIABLES
//
Ds18b20Sensor_t	ds18b20[_DS18B20_MAX_SENSORS];

OneWire_t OneWire;
uint8_t	OneWireDevices;
uint8_t TempSensorCount=0;

//
//	FUNCTIONS
//

//
//	Start conversion of @number sensor
//
uint8_t DS18B20_Start(uint8_t number)
{
	if( number >= TempSensorCount) // If read sensor is not availible
		return 0;

	if (!DS18B20_Is((uint8_t*)&ds18b20[number].Address)) // Check if sensor is DS18B20 family
		return 0;

	OneWire_Reset(&OneWire); // Reset the bus
	OneWire_SelectWithPointer(&OneWire, (uint8_t*)ds18b20[number].Address); // Select the sensor by ROM
	OneWire_WriteByte(&OneWire, DS18B20_CMD_CONVERTTEMP); // Convert command
	
	return 1;
}

//
//	Start conversion on all sensors
//
void DS18B20_StartAll()
{
	OneWire_Reset(&OneWire); // Reset the bus
	OneWire_WriteByte(&OneWire, ONEWIRE_CMD_SKIPROM); // Skip ROM command
	OneWire_WriteByte(&OneWire, DS18B20_CMD_CONVERTTEMP); // Start conversion on all sensors
}

//
//	Read one sensor
//
uint8_t DS18B20_Read(uint8_t number, float *destination)
{
	if( number >= TempSensorCount) // If read sensor is not availible
		return 0;

	uint16_t temperature;
	uint8_t resolution;
	float result;
	uint8_t i = 0;
	uint8_t data[DS18B20_DATA_LEN];
#ifdef _DS18B20_USE_CRC
	uint8_t crc;

#endif

	
	if (!DS18B20_Is((uint8_t*)&ds18b20[number].Address)) // Check if sensor is DS18B20 family
		return 0;

	if (!OneWire_ReadBit(&OneWire)) // Check if the bus is released
		return 0; // Busy bus - conversion is not finished

	OneWire_Reset(&OneWire); // Reset the bus
	OneWire_SelectWithPointer(&OneWire, (uint8_t*)&ds18b20[number].Address); // Select the sensor by ROM
	OneWire_WriteByte(&OneWire, ONEWIRE_CMD_RSCRATCHPAD); // Read scratchpad command
	
	for (i = 0; i < DS18B20_DATA_LEN; i++) // Read scratchpad
		data[i] = OneWire_ReadByte(&OneWire);
	
#ifdef _DS18B20_USE_CRC
	crc = OneWire_CRC8(data, 8); // CRC calculation

	if (crc != data[8])
		return 0; // CRC invalid
#endif
	temperature = data[0] | (data[1] << 8); // Temperature is 16-bit length

	OneWire_Reset(&OneWire); // Reset the bus
	
	resolution = ((data[4] & 0x60) >> 5) + 9; // Sensor's resolution from scratchpad's byte 4

	switch (resolution) // Chceck the correct value dur to resolution
	{
		case DS18B20_Resolution_9bits:
			result = temperature*(float)DS18B20_STEP_9BIT;
		break;
		case DS18B20_Resolution_10bits:
			result = temperature*(float)DS18B20_STEP_10BIT;
		 break;
		case DS18B20_Resolution_11bits:
			result = temperature*(float)DS18B20_STEP_11BIT;
		break;
		case DS18B20_Resolution_12bits:
			result = temperature*(float)DS18B20_STEP_12BIT;
		 break;
		default: 
			result = 0xFF;
	}
	
	*destination = result;
	
	return 1; //temperature valid
}

uint8_t DS18B20_GetResolution(uint8_t number)
{
	if( number >= TempSensorCount)
		return 0;

	uint8_t conf;
	
	if (!DS18B20_Is((uint8_t*)&ds18b20[number].Address))
		return 0;
	
	OneWire_Reset(&OneWire); // Reset the bus
	OneWire_SelectWithPointer(&OneWire, (uint8_t*)&ds18b20[number].Address); // Select the sensor by ROM
	OneWire_WriteByte(&OneWire, ONEWIRE_CMD_RSCRATCHPAD); // Read scratchpad command

	OneWire_ReadByte(&OneWire);
	OneWire_ReadByte(&OneWire);
	OneWire_ReadByte(&OneWire);
	OneWire_ReadByte(&OneWire);
	
	conf = OneWire_ReadByte(&OneWire); // Register 5 is the configuration register with resolution
	conf &= 0x60; // Mask two resolution bits
	conf >>= 5; // Shift to left
	conf += 9; // Get the result in number of resolution bits
	
	return conf;
}

uint8_t DS18B20_SetResolution(uint8_t number, DS18B20_Resolution_t resolution)
{
	if( number >= TempSensorCount)
		return 0;

	uint8_t th, tl, conf;
	if (!DS18B20_Is((uint8_t*)&ds18b20[number].Address))
		return 0;
	
	OneWire_Reset(&OneWire); // Reset the bus
	OneWire_SelectWithPointer(&OneWire, (uint8_t*)&ds18b20[number].Address); // Select the sensor by ROM
	OneWire_WriteByte(&OneWire, ONEWIRE_CMD_RSCRATCHPAD); // Read scratchpad command
	
	OneWire_ReadByte(&OneWire);
	OneWire_ReadByte(&OneWire);
	
	th = OneWire_ReadByte(&OneWire); 	// Writing to scratchpad begins from the temperature alarms bytes
	tl = OneWire_ReadByte(&OneWire); 	// 	so i have to store them.
	conf = OneWire_ReadByte(&OneWire);	// Config byte
	
	if (resolution == DS18B20_Resolution_9bits) // Bits setting
	{
		conf &= ~(1 << DS18B20_RESOLUTION_R1);
		conf &= ~(1 << DS18B20_RESOLUTION_R0);
	}
	else if (resolution == DS18B20_Resolution_10bits) 
	{
		conf &= ~(1 << DS18B20_RESOLUTION_R1);
		conf |= 1 << DS18B20_RESOLUTION_R0;
	}
	else if (resolution == DS18B20_Resolution_11bits)
	{
		conf |= 1 << DS18B20_RESOLUTION_R1;
		conf &= ~(1 << DS18B20_RESOLUTION_R0);
	}
	else if (resolution == DS18B20_Resolution_12bits)
	{
		conf |= 1 << DS18B20_RESOLUTION_R1;
		conf |= 1 << DS18B20_RESOLUTION_R0;
	}
	
	OneWire_Reset(&OneWire); // Reset the bus
	OneWire_SelectWithPointer(&OneWire, (uint8_t*)&ds18b20[number].Address); // Select the sensor by ROM
	OneWire_WriteByte(&OneWire, ONEWIRE_CMD_WSCRATCHPAD); // Write scratchpad command
	
	OneWire_WriteByte(&OneWire, th); // Write 3 bytes to scratchpad
	OneWire_WriteByte(&OneWire, tl);
	OneWire_WriteByte(&OneWire, conf);
	
	OneWire_Reset(&OneWire); // Reset the bus
	OneWire_SelectWithPointer(&OneWire, (uint8_t*)&ds18b20[number].Address); // Select the sensor by ROM
	OneWire_WriteByte(&OneWire, ONEWIRE_CMD_CPYSCRATCHPAD); // Copy scratchpad to EEPROM
	
	return 1;
}

uint8_t DS18B20_Is(uint8_t* ROM)
{
	if (*ROM == DS18B20_FAMILY_CODE) // Check family code
		return 1;
	return 0;
}

uint8_t DS18B20_AllDone(void)
{
	return OneWire_ReadBit(&OneWire); // Bus is down - busy
}

void DS18B20_ReadAll(void)
{
	uint8_t i;

	if (DS18B20_AllDone())
	{
		for(i = 0; i < TempSensorCount; i++) // All detected sensors loop
		{
			ds18b20[i].ValidDataFlag = 0;

			if (DS18B20_Is((uint8_t*)&ds18b20[i].Address))
			{
				ds18b20[i].ValidDataFlag = DS18B20_Read(i, &ds18b20[i].Temperature); // Read single sensor
			}
		}
	}
}

void DS18B20_GetROM(uint8_t number, uint8_t* ROM)
{
	if( number >= TempSensorCount)
		number = TempSensorCount;

	uint8_t i;

	for(i = 0; i < 8; i++)
		ROM[i] = ds18b20[number].Address[i];
}

void DS18B20_WriteROM(uint8_t number, uint8_t* ROM)
{
	if( number >= TempSensorCount)
		return;

	uint8_t i;

	for(i = 0; i < 8; i++)
		ds18b20[number].Address[i] = ROM[i]; // Write ROM into sensor's structure
}

uint8_t DS18B20_Quantity(void)
{
	return TempSensorCount;
}

uint8_t DS18B20_GetTemperature(uint8_t number, float* destination)
{
	if(!ds18b20[number].ValidDataFlag)
		return 0;

	*destination = ds18b20[number].Temperature;
	return 1;

}

void DS18B20_Init(DS18B20_Resolution_t resolution)
{
	uint8_t next = 0, i = 0, j;
	OneWire_Init(&OneWire, DS18B20_GPIO_Port, DS18B20_Pin); // Init OneWire bus

	next = OneWire_First(&OneWire); // Search first OneWire device
	while(next)
	{
		TempSensorCount++;
		OneWire_GetFullROM(&OneWire, (uint8_t*)&ds18b20[i++].Address); // Get the ROM of next sensor
		next = OneWire_Next(&OneWire);
		if(TempSensorCount >= _DS18B20_MAX_SENSORS) // More sensors than set maximum is not allowed
			break;
	}

	for(j = 0; j < i; j++)
	{
		DS18B20_SetResolution(j, resolution); // Set the initial resolution to sensor

		DS18B20_StartAll(); // Start conversion on all sensors
	}
}


