/*
 * onewire.c
 *
 *	The MIT License.
 *  Created on: 20.09.2018
 *      Author: Mateusz Salamon
 *      www.msalamon.pl
 *      mateusz@msalamon.pl
 *
 */
#include "tim.h"
#include "onewire.h"
#include "ds18b20.h"

//
//	Delay function for constant 1-Wire timings
//
void OneWire_Delay(uint16_t us)
{
	_DS18B20_TIMER.Instance->CNT = 0;
	while(_DS18B20_TIMER.Instance->CNT <= us);
}

//
//	Bus direction control
//
void OneWire_BusInputDirection(OneWire_t *onewire)
{
	GPIO_InitTypeDef	GPIO_InitStruct;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT; // Set as input
	GPIO_InitStruct.Pull = GPIO_NOPULL; // No pullup - the pullup resistor is external
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM; // Medium GPIO frequency
	GPIO_InitStruct.Pin = onewire->GPIO_Pin; // Pin for 1-Wire bus
	HAL_GPIO_Init(onewire->GPIOx, &GPIO_InitStruct); // Reinitialize
}	

void OneWire_BusOutputDirection(OneWire_t *onewire)
{
	GPIO_InitTypeDef	GPIO_InitStruct;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; // Set as open-drain output
	GPIO_InitStruct.Pull = GPIO_NOPULL; // No pullup - the pullup resistor is external
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM; // Medium GPIO frequency
	GPIO_InitStruct.Pin = onewire->GPIO_Pin; // Pin for 1-Wire bus
	HAL_GPIO_Init(onewire->GPIOx, &GPIO_InitStruct); // Reinitialize
}

//
//	Bus pin output state control
//
void OneWire_OutputLow(OneWire_t *onewire)
{
	onewire->GPIOx->BSRR = onewire->GPIO_Pin<<16; // Reset the 1-Wire pin
}	

void OneWire_OutputHigh(OneWire_t *onewire)
{
	onewire->GPIOx->BSRR = onewire->GPIO_Pin; // Set the 1-Wire pin
}

//
//	1-Wire bus reset signal
//
//	Returns:
//	0 - Reset ok
//	1 - Error
//
uint8_t OneWire_Reset(OneWire_t* onewire)
{
	uint8_t i;
	
	OneWire_OutputLow(onewire);  // Write bus output low
	OneWire_BusOutputDirection(onewire);
	OneWire_Delay(480); // Wait 480 us for reset

	OneWire_BusInputDirection(onewire); // Release the bus by switching to input
	OneWire_Delay(70);
	
	i = HAL_GPIO_ReadPin(onewire->GPIOx, onewire->GPIO_Pin); // Check if bus is low
															 // if it's high - no device is presence on the bus
	OneWire_Delay(410);

	return i;
}

//
//	Writing/Reading operations
//
void OneWire_WriteBit(OneWire_t* onewire, uint8_t bit)
{
	if (bit) // Send '1',
	{
		OneWire_OutputLow(onewire);	// Set the bus low
		OneWire_BusOutputDirection(onewire);
		OneWire_Delay(6);
		
		OneWire_BusInputDirection(onewire); // Release bus - bit high by pullup
		OneWire_Delay(64);
	} 
	else // Send '0'
	{
		OneWire_OutputLow(onewire); // Set the bus low
		OneWire_BusOutputDirection(onewire);
		OneWire_Delay(60);
		
		OneWire_BusInputDirection(onewire); // Release bus - bit high by pullup
		OneWire_Delay(10);
	}
}

uint8_t OneWire_ReadBit(OneWire_t* onewire)
{
	uint8_t bit = 0; // Default read bit state is low
	
	OneWire_OutputLow(onewire); // Set low to initiate reading
	OneWire_BusOutputDirection(onewire);
	OneWire_Delay(2);
	
	OneWire_BusInputDirection(onewire); // Release bus for Slave response
	OneWire_Delay(10);
	
	if (HAL_GPIO_ReadPin(onewire->GPIOx, onewire->GPIO_Pin)) // Read the bus state
		bit = 1;
	
	OneWire_Delay(50); // Wait for end of read cycle

	return bit;
}

void OneWire_WriteByte(OneWire_t* onewire, uint8_t byte)
{
	uint8_t i = 8;

	do
	{
		OneWire_WriteBit(onewire, byte & 1); // LSB first
		byte >>= 1;
	} while(--i);
}

uint8_t OneWire_ReadByte(OneWire_t* onewire)
{
	uint8_t i = 8, byte = 0;

	do{
		byte >>= 1;
		byte |= (OneWire_ReadBit(onewire) << 7); // LSB first
	} while(--i);
	
	return byte;
}

//
// 1-Wire search operations
//
void OneWire_ResetSearch(OneWire_t* onewire)
{
	// Clear the search results
	onewire->LastDiscrepancy = 0;
	onewire->LastDeviceFlag = 0;
	onewire->LastFamilyDiscrepancy = 0;
}

uint8_t OneWire_Search(OneWire_t* onewire, uint8_t command)
{
	uint8_t id_bit_number;
	uint8_t last_zero, rom_byte_number, search_result;
	uint8_t id_bit, cmp_id_bit;
	uint8_t rom_byte_mask, search_direction;

	id_bit_number = 1;
	last_zero = 0;
	rom_byte_number = 0;
	rom_byte_mask = 1;
	search_result = 0;

	if (!onewire->LastDeviceFlag) // If last device flag is not set
	{
		if (OneWire_Reset(onewire)) // Reset bus
		{
			// If error while reset - reset search results
			onewire->LastDiscrepancy = 0;
			onewire->LastDeviceFlag = 0;
			onewire->LastFamilyDiscrepancy = 0;
			return 0;
		}

		OneWire_WriteByte(onewire, command); // Send searching command

		// Searching loop, Maxim APPLICATION NOTE 187
		do
		{
			id_bit = OneWire_ReadBit(onewire); // Read a bit 1
			cmp_id_bit = OneWire_ReadBit(onewire); // Read the complement of bit 1

			if ((id_bit == 1) && (cmp_id_bit == 1)) // 11 - data error
			{
				break;
			}
			else
			{
				if (id_bit != cmp_id_bit)
				{
					search_direction = id_bit;  // Bit write value for search
				}
				else // 00 - 2 devices
				{
					// Table 3. Search Path Direction
					if (id_bit_number < onewire->LastDiscrepancy)
					{
						search_direction = ((onewire->ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
					}
					else
					{
						// If bit is equal to last - pick 1
						// If not - then pick 0
						search_direction = (id_bit_number == onewire->LastDiscrepancy);
					}

					if (search_direction == 0) // If 0 was picked, write it to LastZero
					{
						last_zero = id_bit_number;

						if (last_zero < 9) // Check for last discrepancy in family
						{
							onewire->LastFamilyDiscrepancy = last_zero;
						}
					}
				}

				if (search_direction == 1)
				{
					onewire->ROM_NO[rom_byte_number] |= rom_byte_mask; // Set the bit in the ROM byte rom_byte_number
				}
				else
				{
					onewire->ROM_NO[rom_byte_number] &= ~rom_byte_mask; // Clear the bit in the ROM byte rom_byte_number
				}
				
				OneWire_WriteBit(onewire, search_direction); // Search direction write bit

				id_bit_number++; // Next bit search - increase the id
				rom_byte_mask <<= 1; // Shoft the mask for next bit

				if (rom_byte_mask == 0) // If the mask is 0, it says the whole byte is read
				{
					rom_byte_number++; // Next byte number
					rom_byte_mask = 1; // Reset the mask - first bit
				}
			}
		} while(rom_byte_number < 8);  // Read 8 bytes

		if (!(id_bit_number < 65)) // If all read bits number is below 65 (8 bytes)
		{
			onewire->LastDiscrepancy = last_zero;

			if (onewire->LastDiscrepancy == 0) // If last discrepancy is 0 - last device found
			{
				onewire->LastDeviceFlag = 1; // Set the flag
			}

			search_result = 1; // Searching successful
		}
	}

	// If no device is found - reset search data and return 0
	if (!search_result || !onewire->ROM_NO[0])
	{
		onewire->LastDiscrepancy = 0;
		onewire->LastDeviceFlag = 0;
		onewire->LastFamilyDiscrepancy = 0;
		search_result = 0;
	}

	return search_result;
}

//
//	Return first device on 1-Wire bus
//
uint8_t OneWire_First(OneWire_t* onewire)
{
	OneWire_ResetSearch(onewire);

	return OneWire_Search(onewire, ONEWIRE_CMD_SEARCHROM);
}

//
//	Return next device on 1-Wire bus
//
uint8_t OneWire_Next(OneWire_t* onewire)
{
   /* Leave the search state alone */
   return OneWire_Search(onewire, ONEWIRE_CMD_SEARCHROM);
}

//
//	Select a device on bus by address
//
void OneWire_Select(OneWire_t* onewire, uint8_t* addr)
{
	uint8_t i;
	OneWire_WriteByte(onewire, ONEWIRE_CMD_MATCHROM); // Match ROM command
	
	for (i = 0; i < 8; i++)
	{
		OneWire_WriteByte(onewire, *(addr + i));
	}
}

//
//	Select a device on bus by pointer to ROM address
//
void OneWire_SelectWithPointer(OneWire_t* onewire, uint8_t *ROM)
{
	uint8_t i;
	OneWire_WriteByte(onewire, ONEWIRE_CMD_MATCHROM); // Match ROM command
	
	for (i = 0; i < 8; i++)
	{
		OneWire_WriteByte(onewire, *(ROM + i));
	}	
}

//
//	Get the ROM of found device
//
void OneWire_GetFullROM(OneWire_t* onewire, uint8_t *firstIndex)
{
	uint8_t i;
	for (i = 0; i < 8; i++) {
		*(firstIndex + i) = onewire->ROM_NO[i];
	}
}

//
//	Calculate CRC
//
uint8_t OneWire_CRC8(uint8_t *addr, uint8_t len) {
	uint8_t crc = 0, inbyte, i, mix;
	
	while (len--)
	{
		inbyte = *addr++;
		for (i = 8; i; i--)
		{
			mix = (crc ^ inbyte) & 0x01;
			crc >>= 1;
			if (mix)
			{
				crc ^= 0x8C;
			}
			inbyte >>= 1;
		}
	}
	
	return crc;
}

//
//	1-Wire initialization
//
void OneWire_Init(OneWire_t* onewire, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
	HAL_TIM_Base_Start(&_DS18B20_TIMER); // Start the delay timer

	onewire->GPIOx = GPIOx; // Save 1-wire bus pin
	onewire->GPIO_Pin = GPIO_Pin;

	// 1-Wire bit bang initialization
	OneWire_BusOutputDirection(onewire);
	OneWire_OutputHigh(onewire);
	HAL_Delay(100);
	OneWire_OutputLow(onewire);
	HAL_Delay(100);
	OneWire_OutputHigh(onewire);
	HAL_Delay(200);
}

