/*
 * ds18b20.h
 *
 *	The MIT License.
 *  Created on: 20.09.2018
 *      Author: Mateusz Salamon
 *      www.msalamon.pl
 *      mateusz@msalamon.pl
 *
 */
#ifndef	_DS18B20_H
#define	_DS18B20_H

#include "onewire.h"

//
//	CONFIGURATION
//

//	Remember to configure a timer on CubeMX 1us per tick
//	example 72 MHz cpu - Prescaler=(72-1), Counter period=65000
#define _DS18B20_MAX_SENSORS		    4
#define	_DS18B20_GPIO					DS18B20_GPIO_Port
#define	_DS18B20_PIN					DS18B20_Pin

#define	_DS18B20_TIMER					htim1

//#define _DS18B20_USE_CRC

//
//	Sensor structure
//
typedef struct
{
	uint8_t 	Address[8];
	float 		Temperature;
	uint8_t		ValidDataFlag;
} Ds18b20Sensor_t;

//
//	DEFINES
//
#define DS18B20_FAMILY_CODE				0x28

#define DS18B20_CMD_ALARMSEARCH			0xEC
#define DS18B20_CMD_CONVERTTEMP			0x44

#define DS18B20_STEP_12BIT		0.0625
#define DS18B20_STEP_11BIT		0.125
#define DS18B20_STEP_10BIT		0.25
#define DS18B20_STEP_9BIT		0.5

#define DS18B20_RESOLUTION_R1	6 // Resolution bit R1
#define DS18B20_RESOLUTION_R0	5 // Resolution bit R0

#ifdef _DS18B20_USE_CRC
#define DS18B20_DATA_LEN	9
#else
#define DS18B20_DATA_LEN	5
#endif

typedef enum {
	DS18B20_Resolution_9bits = 9,
	DS18B20_Resolution_10bits = 10,
	DS18B20_Resolution_11bits = 11,
	DS18B20_Resolution_12bits = 12
} DS18B20_Resolution_t;

//
//	FUNCTIONS
//

// 	Init
void		DS18B20_Init(DS18B20_Resolution_t resolution);
//	Settings
uint8_t 	DS18B20_GetResolution(uint8_t number); // Get the sensor resolution
uint8_t 	DS18B20_SetResolution(uint8_t number, DS18B20_Resolution_t resolution);	// Set the sensor resolution
// Control
uint8_t 	DS18B20_Start(uint8_t number); // Start conversion of one sensor
void 		DS18B20_StartAll(void);	// Start conversion for all sensors
uint8_t		DS18B20_Read(uint8_t number, float* destination); // Read one sensor
void 		DS18B20_ReadAll(void);	// Read all connected sensors
uint8_t 	DS18B20_Is(uint8_t* ROM); // Check if ROM address is DS18B20 family
uint8_t 	DS18B20_AllDone(void);	// Check if all sensor's conversion is done
//	ROMs
void		DS18B20_GetROM(uint8_t number, uint8_t* ROM); // Get sensor's ROM from 'number' position
void		DS18B20_WriteROM(uint8_t number, uint8_t* ROM); // Write a ROM to 'number' position in sensors table
// Return functions
uint8_t 	DS18B20_Quantity(void);	// Returns quantity of connected sensors
uint8_t		DS18B20_GetTemperature(uint8_t number, float* destination); // Returns 0 if read data is invalid
#endif

