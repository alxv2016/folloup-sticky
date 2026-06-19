/******************************************************************************
lsm6ds3.cpp
LSM6DS3 driver implementation

ESP-IDF-native port with direct I2C/SPI device handle support.
******************************************************************************/

#include "lsm6ds3.h"

#include <vector>

#include "esp_rom_sys.h"
#include "lsm6ds3_reg.h"

namespace {

constexpr int kI2cTimeoutMs = 50;

bool is_supported_who_am_i(uint8_t value)
{
	return value == lsm6ds3::kWhoAmIValueLsm6ds3 || value == lsm6ds3::kWhoAmIValueLsm6ds3trc;
}

status_t i2c_read_registers(i2c_master_dev_handle_t i2c_dev, uint8_t offset, uint8_t* output, uint8_t length)
{
	if( i2c_dev == nullptr || output == nullptr || length == 0 )
	{
		return IMU_HW_ERROR;
	}

	return i2c_master_transmit_receive(i2c_dev, &offset, sizeof(offset), output, length, kI2cTimeoutMs) == ESP_OK
		       ? IMU_SUCCESS
		       : IMU_HW_ERROR;
}

status_t i2c_write_register(i2c_master_dev_handle_t i2c_dev, uint8_t offset, uint8_t data)
{
	if( i2c_dev == nullptr )
	{
		return IMU_HW_ERROR;
	}

	const uint8_t payload[] = {offset, data};
	return i2c_master_transmit(i2c_dev, payload, sizeof(payload), kI2cTimeoutMs) == ESP_OK ? IMU_SUCCESS
	                                                                                         : IMU_HW_ERROR;
}

status_t spi_read_registers(spi_device_handle_t spi_dev, uint8_t offset, uint8_t* output, uint8_t length)
{
	if( spi_dev == nullptr || output == nullptr || length == 0 )
	{
		return IMU_HW_ERROR;
	}

	std::vector<uint8_t> tx_buffer(static_cast<size_t>(length) + 1U, 0x00);
	std::vector<uint8_t> rx_buffer(static_cast<size_t>(length) + 1U, 0x00);
	tx_buffer[0] = static_cast<uint8_t>(offset | 0x80U);

	spi_transaction_t transaction = {};
	transaction.length = static_cast<size_t>(tx_buffer.size()) * 8U;
	transaction.tx_buffer = tx_buffer.data();
	transaction.rx_buffer = rx_buffer.data();

	if( spi_device_polling_transmit(spi_dev, &transaction) != ESP_OK )
	{
		return IMU_HW_ERROR;
	}

	uint8_t ff_count = 0;
	for( uint8_t i = 0; i < length; ++i )
	{
		output[i] = rx_buffer[static_cast<size_t>(i) + 1U];
		if( output[i] == 0xFFU )
		{
			ff_count++;
		}
	}

	return ff_count == length ? IMU_ALL_ONES_WARNING : IMU_SUCCESS;
}

status_t spi_write_register(spi_device_handle_t spi_dev, uint8_t offset, uint8_t data)
{
	if( spi_dev == nullptr )
	{
		return IMU_HW_ERROR;
	}

	const uint8_t payload[] = {offset, data};
	spi_transaction_t transaction = {};
	transaction.length = sizeof(payload) * 8U;
	transaction.tx_buffer = payload;

	return spi_device_polling_transmit(spi_dev, &transaction) == ESP_OK ? IMU_SUCCESS : IMU_HW_ERROR;
}

} // namespace

//****************************************************************************//
//
//  LSM6DS3Core functions.
//
//  ESP-IDF constructors accept initialized I2C or SPI device handles.
//
//****************************************************************************//
LSM6DS3Core::LSM6DS3Core( i2c_master_dev_handle_t i2c_device ) : commInterface(I2C_MODE), i2c_dev(i2c_device), spi_dev(nullptr)
{
}

LSM6DS3Core::LSM6DS3Core( spi_device_handle_t spi_device ) : commInterface(SPI_MODE), i2c_dev(nullptr), spi_dev(spi_device)
{
}

status_t LSM6DS3Core::beginCore(void)
{
	status_t returnError = IMU_SUCCESS;

	switch (commInterface) {

	case I2C_MODE:
		if( i2c_dev == nullptr )
		{
			return IMU_HW_ERROR;
		}
		break;

	case SPI_MODE:
		if( spi_dev == nullptr )
		{
			return IMU_HW_ERROR;
		}
		break;
	default:
		return IMU_HW_ERROR;
	}

	esp_rom_delay_us(1000);

	//Check the ID register to determine if the operation was a success.
	uint8_t readCheck = 0;
	returnError = readRegister(&readCheck, lsm6ds3::kWhoAmIRegister);
	if( returnError != IMU_SUCCESS )
	{
		return returnError;
	}

	if( !is_supported_who_am_i(readCheck) )
	{
		returnError = IMU_HW_ERROR;
	}

	return returnError;

}

//****************************************************************************//
//
//  ReadRegisterRegion
//
//  Parameters:
//    *outputPointer -- Pass &variable (base address of) to save read data to
//    offset -- register to read
//    length -- number of bytes to read
//
//  Note:  Does not know if the target memory space is an array or not, or
//    if there is the array is big enough.  if the variable passed is only
//    two bytes long and 3 bytes are requested, this will over-write some
//    other memory!
//
//****************************************************************************//
status_t LSM6DS3Core::readRegisterRegion(uint8_t *outputPointer , uint8_t offset, uint8_t length)
{
	status_t returnError = IMU_SUCCESS;

	switch (commInterface) {

	case I2C_MODE:
		returnError = i2c_read_registers(i2c_dev, offset, outputPointer, length);
		break;

	case SPI_MODE:
		returnError = spi_read_registers(spi_dev, offset, outputPointer, length);
		break;

	default:
		break;
	}

	return returnError;
}

//****************************************************************************//
//
//  ReadRegister
//
//  Parameters:
//    *outputPointer -- Pass &variable (address of) to save read data to
//    offset -- register to read
//
//****************************************************************************//
status_t LSM6DS3Core::readRegister(uint8_t* outputPointer, uint8_t offset) {
	//Return value
	return readRegisterRegion(outputPointer, offset, 1);
}

//****************************************************************************//
//
//  readRegisterInt16
//
//  Parameters:
//    *outputPointer -- Pass &variable (base address of) to save read data to
//    offset -- register to read
//
//****************************************************************************//
status_t LSM6DS3Core::readRegisterInt16( int16_t* outputPointer, uint8_t offset )
{
	uint8_t myBuffer[2];
	status_t returnError = readRegisterRegion(myBuffer, offset, 2);  //Does memory transfer
	int16_t output = (int16_t)myBuffer[0] | int16_t(myBuffer[1] << 8);

	*outputPointer = output;
	return returnError;
}

//****************************************************************************//
//
//  writeRegister
//
//  Parameters:
//    offset -- register to write
//    dataToWrite -- 8 bit data to write to register
//
//****************************************************************************//
status_t LSM6DS3Core::writeRegister(uint8_t offset, uint8_t dataToWrite) {
	status_t returnError = IMU_SUCCESS;
	switch (commInterface) {
	case I2C_MODE:
		returnError = i2c_write_register(i2c_dev, offset, dataToWrite);
		break;

	case SPI_MODE:
		returnError = spi_write_register(spi_dev, offset, dataToWrite);
		break;

		//No way to check error on this write (Except to read back but that's not reliable)

	default:
		break;
	}

	return returnError;
}

status_t LSM6DS3Core::embeddedPage( void )
{
	status_t returnError = writeRegister( LSM6DS3_ACC_GYRO_RAM_ACCESS, 0x80 );

	return returnError;
}

status_t LSM6DS3Core::basePage( void )
{
	status_t returnError = writeRegister( LSM6DS3_ACC_GYRO_RAM_ACCESS, 0x00 );

	return returnError;
}


//****************************************************************************//
//
//  Main user class -- wrapper for the core class + maths
//
//  Construct with same rules as the core ( uint8_t busType, uint8_t inputArg )
//
//****************************************************************************//
LSM6DS3::LSM6DS3( i2c_master_dev_handle_t i2c_device ) : LSM6DS3Core( i2c_device )
{
	//Construct with these default settings

	settings.gyroEnabled = 1;  //Can be 0 or 1
	settings.gyroRange = 2000;   //Max deg/s.  Can be: 125, 245, 500, 1000, 2000
	settings.gyroSampleRate = 416;   //Hz.  Can be: 13, 26, 52, 104, 208, 416, 833, 1666
	settings.gyroBandWidth = 400;  //Hz.  Can be: 50, 100, 200, 400;
	settings.gyroFifoEnabled = 1;  //Set to include gyro in FIFO
	settings.gyroFifoDecimation = 1;  //set 1 for on /1

	settings.accelEnabled = 1;
	settings.accelODROff = 1;
	settings.accelRange = 16;      //Max G force readable.  Can be: 2, 4, 8, 16
	settings.accelSampleRate = 416;  //Hz.  Can be: 13, 26, 52, 104, 208, 416, 833, 1666, 3332, 6664, 13330
	settings.accelBandWidth = 100;  //Hz.  Can be: 50, 100, 200, 400;
	settings.accelFifoEnabled = 1;  //Set to include accelerometer in the FIFO
	settings.accelFifoDecimation = 1;  //set 1 for on /1

	settings.tempEnabled = 1;

	//Select interface mode
	settings.commMode = 1;  //Can be modes 1, 2 or 3

	//FIFO control data
	settings.fifoThreshold = 3000;  //Can be 0 to 4095 (16 bit bytes)
	settings.fifoSampleRate = 10;  //default 10Hz
	settings.fifoModeWord = 0;  //Default off

	allOnesCounter = 0;
	nonSuccessCounter = 0;

}

LSM6DS3::LSM6DS3( spi_device_handle_t spi_device ) : LSM6DS3Core( spi_device )
{
	//Construct with these default settings

	settings.gyroEnabled = 1;  //Can be 0 or 1
	settings.gyroRange = 2000;   //Max deg/s.  Can be: 125, 245, 500, 1000, 2000
	settings.gyroSampleRate = 416;   //Hz.  Can be: 13, 26, 52, 104, 208, 416, 833, 1666
	settings.gyroBandWidth = 400;  //Hz.  Can be: 50, 100, 200, 400;
	settings.gyroFifoEnabled = 1;  //Set to include gyro in FIFO
	settings.gyroFifoDecimation = 1;  //set 1 for on /1

	settings.accelEnabled = 1;
	settings.accelODROff = 1;
	settings.accelRange = 16;      //Max G force readable.  Can be: 2, 4, 8, 16
	settings.accelSampleRate = 416;  //Hz.  Can be: 13, 26, 52, 104, 208, 416, 833, 1666, 3332, 6664, 13330
	settings.accelBandWidth = 100;  //Hz.  Can be: 50, 100, 200, 400;
	settings.accelFifoEnabled = 1;  //Set to include accelerometer in the FIFO
	settings.accelFifoDecimation = 1;  //set 1 for on /1

	settings.tempEnabled = 1;

	//Select interface mode
	settings.commMode = 1;  //Can be modes 1, 2 or 3

	//FIFO control data
	settings.fifoThreshold = 3000;  //Can be 0 to 4095 (16 bit bytes)
	settings.fifoSampleRate = 10;  //default 10Hz
	settings.fifoModeWord = 0;  //Default off

	allOnesCounter = 0;
	nonSuccessCounter = 0;

}

//****************************************************************************//
//
//  Configuration section
//
//  This uses the stored SensorSettings to start the IMU
//  Use statements such as "myIMU.settings.commInterface = SPI_MODE;" or
//  "myIMU.settings.accelEnabled = 1;" to configure before calling .begin();
//
//****************************************************************************//
status_t LSM6DS3::begin(SensorSettings* pSettingsYouWanted)
{
	//Check the settings structure values to determine how to setup the device
	uint8_t dataToWrite = 0;  //Temporary variable

	//Begin the inherited core.  This gets the physical wires connected
	status_t returnError = beginCore();
	if( returnError != IMU_SUCCESS )
	{
		return returnError;
	}

	// Copy the values from the user's settings into the output 'pSettingsYouWanted'
	// compare settings with 'pSettingsYouWanted' after 'begin' to see if anything changed
	if(pSettingsYouWanted != NULL){
		pSettingsYouWanted->gyroEnabled = settings.gyroEnabled;
		pSettingsYouWanted->gyroRange = settings.gyroRange;
		pSettingsYouWanted->gyroSampleRate = settings.gyroSampleRate;
		pSettingsYouWanted->gyroBandWidth = settings.gyroBandWidth;
		pSettingsYouWanted->gyroFifoEnabled = settings.gyroFifoEnabled;
		pSettingsYouWanted->gyroFifoDecimation = settings.gyroFifoDecimation;
		pSettingsYouWanted->accelEnabled = settings.accelEnabled;
		pSettingsYouWanted->accelODROff = settings.accelODROff;
		pSettingsYouWanted->accelRange = settings.accelRange;
		pSettingsYouWanted->accelSampleRate = settings.accelSampleRate;
		pSettingsYouWanted->accelBandWidth = settings.accelBandWidth;
		pSettingsYouWanted->accelFifoEnabled = settings.accelFifoEnabled;
		pSettingsYouWanted->accelFifoDecimation = settings.accelFifoDecimation;
		pSettingsYouWanted->tempEnabled = settings.tempEnabled;
		pSettingsYouWanted->commMode = settings.commMode;
		pSettingsYouWanted->fifoThreshold = settings.fifoThreshold;
		pSettingsYouWanted->fifoSampleRate = settings.fifoSampleRate;
		pSettingsYouWanted->fifoModeWord = settings.fifoModeWord;
	}

	//Setup the accelerometer******************************
	dataToWrite = 0; //Start Fresh!
	if ( settings.accelEnabled == 1) {
		//Build config reg
		//First patch in filter bandwidth
		switch (settings.accelBandWidth) {
		case 50:
			dataToWrite |= LSM6DS3_ACC_GYRO_BW_XL_50Hz;
			break;
		case 100:
			dataToWrite |= LSM6DS3_ACC_GYRO_BW_XL_100Hz;
			break;
		case 200:
			dataToWrite |= LSM6DS3_ACC_GYRO_BW_XL_200Hz;
			break;
		default:  //set default case to max passthrough
			settings.accelBandWidth = 400;
			[[fallthrough]];
		case 400:
			dataToWrite |= LSM6DS3_ACC_GYRO_BW_XL_400Hz;
			break;
		}
		//Next, patch in full scale
		switch (settings.accelRange) {
		case 2:
			dataToWrite |= LSM6DS3_ACC_GYRO_FS_XL_2g;
			break;
		case 4:
			dataToWrite |= LSM6DS3_ACC_GYRO_FS_XL_4g;
			break;
		case 8:
			dataToWrite |= LSM6DS3_ACC_GYRO_FS_XL_8g;
			break;
		default:  //set default case to 16(max)
			settings.accelRange = 16;
			[[fallthrough]];
		case 16:
			dataToWrite |= LSM6DS3_ACC_GYRO_FS_XL_16g;
			break;
		}
		//Lastly, patch in accelerometer ODR
		switch (settings.accelSampleRate) {
		case 13:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_XL_13Hz;
			break;
		case 26:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_XL_26Hz;
			break;
		case 52:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_XL_52Hz;
			break;
		default:  //Set default to 104
			settings.accelSampleRate = 104;
			[[fallthrough]];
		case 104:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_XL_104Hz;
			break;
		case 208:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_XL_208Hz;
			break;
		case 416:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_XL_416Hz;
			break;
		case 833:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_XL_833Hz;
			break;
		case 1660:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_XL_1660Hz;
			break;
		case 3330:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_XL_3330Hz;
			break;
		case 6660:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_XL_6660Hz;
			break;
		case 13330:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_XL_13330Hz;
			break;
		}
	}
	else
	{
		//dataToWrite already = 0 (powerdown);
	}

	//Now, write the patched together data
	writeRegister(LSM6DS3_ACC_GYRO_CTRL1_XL, dataToWrite);

	//Set the ODR bit
	readRegister(&dataToWrite, LSM6DS3_ACC_GYRO_CTRL4_C);
	dataToWrite &= ~((uint8_t)LSM6DS3_ACC_GYRO_BW_SCAL_ODR_ENABLED);
	if ( settings.accelODROff == 1) {
		dataToWrite |= LSM6DS3_ACC_GYRO_BW_SCAL_ODR_ENABLED;
	}
	writeRegister(LSM6DS3_ACC_GYRO_CTRL4_C, dataToWrite);

	//Setup the gyroscope**********************************************
	dataToWrite = 0; //Start Fresh!
	if ( settings.gyroEnabled == 1) {
		//Build config reg
		//First, patch in full scale
		switch (settings.gyroRange) {
		case 125:
			dataToWrite |= LSM6DS3_ACC_GYRO_FS_125_ENABLED;
			break;
		case 245:
			dataToWrite |= LSM6DS3_ACC_GYRO_FS_G_245dps;
			break;
		case 500:
			dataToWrite |= LSM6DS3_ACC_GYRO_FS_G_500dps;
			break;
		case 1000:
			dataToWrite |= LSM6DS3_ACC_GYRO_FS_G_1000dps;
			break;
		default:  //Default to full 2000DPS range
			settings.gyroRange = 2000;
			[[fallthrough]];
		case 2000:
			dataToWrite |= LSM6DS3_ACC_GYRO_FS_G_2000dps;
			break;
		}
		//Lastly, patch in gyro ODR
		switch (settings.gyroSampleRate) {
		case 13:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_G_13Hz;
			break;
		case 26:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_G_26Hz;
			break;
		case 52:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_G_52Hz;
			break;
		default:  //Set default to 104
			settings.gyroSampleRate = 104;
			[[fallthrough]];
		case 104:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_G_104Hz;
			break;
		case 208:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_G_208Hz;
			break;
		case 416:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_G_416Hz;
			break;
		case 833:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_G_833Hz;
			break;
		case 1660:
			dataToWrite |= LSM6DS3_ACC_GYRO_ODR_G_1660Hz;
			break;
		}
	}
	else
	{
		//dataToWrite already = 0 (powerdown);
	}
	//Write the byte
	writeRegister(LSM6DS3_ACC_GYRO_CTRL2_G, dataToWrite);

	//Setup the internal temperature sensor
	if ( settings.tempEnabled == 1) {
	}

	//Return WHO AM I reg  //Not no mo!
	uint8_t result;
	readRegister(&result, LSM6DS3_ACC_GYRO_WHO_AM_I_REG);

	return returnError;
}

//****************************************************************************//
//
//  Accelerometer section
//
//****************************************************************************//
int16_t LSM6DS3::readRawAccelX( void )
{
	int16_t output;
	status_t errorLevel = readRegisterInt16( &output, LSM6DS3_ACC_GYRO_OUTX_L_XL );
	if( errorLevel != IMU_SUCCESS )
	{
		if( errorLevel == IMU_ALL_ONES_WARNING )
		{
			allOnesCounter++;
		}
		else
		{
			nonSuccessCounter++;
		}
	}
	return output;
}
float LSM6DS3::readFloatAccelX( void )
{
	float output = calcAccel(readRawAccelX());
	return output;
}

int16_t LSM6DS3::readRawAccelY( void )
{
	int16_t output;
	status_t errorLevel = readRegisterInt16( &output, LSM6DS3_ACC_GYRO_OUTY_L_XL );
	if( errorLevel != IMU_SUCCESS )
	{
		if( errorLevel == IMU_ALL_ONES_WARNING )
		{
			allOnesCounter++;
		}
		else
		{
			nonSuccessCounter++;
		}
	}
	return output;
}
float LSM6DS3::readFloatAccelY( void )
{
	float output = calcAccel(readRawAccelY());
	return output;
}

int16_t LSM6DS3::readRawAccelZ( void )
{
	int16_t output;
	status_t errorLevel = readRegisterInt16( &output, LSM6DS3_ACC_GYRO_OUTZ_L_XL );
	if( errorLevel != IMU_SUCCESS )
	{
		if( errorLevel == IMU_ALL_ONES_WARNING )
		{
			allOnesCounter++;
		}
		else
		{
			nonSuccessCounter++;
		}
	}
	return output;
}
float LSM6DS3::readFloatAccelZ( void )
{
	float output = calcAccel(readRawAccelZ());
	return output;
}

float LSM6DS3::calcAccel( int16_t input )
{
	float output = (float)input * 0.061 * (settings.accelRange >> 1) / 1000;
	return output;
}

//****************************************************************************//
//
//  Gyroscope section
//
//****************************************************************************//
int16_t LSM6DS3::readRawGyroX( void )
{
	int16_t output;
	status_t errorLevel = readRegisterInt16( &output, LSM6DS3_ACC_GYRO_OUTX_L_G );
	if( errorLevel != IMU_SUCCESS )
	{
		if( errorLevel == IMU_ALL_ONES_WARNING )
		{
			allOnesCounter++;
		}
		else
		{
			nonSuccessCounter++;
		}
	}
	return output;
}
float LSM6DS3::readFloatGyroX( void )
{
	float output = calcGyro(readRawGyroX());
	return output;
}

int16_t LSM6DS3::readRawGyroY( void )
{
	int16_t output;
	status_t errorLevel = readRegisterInt16( &output, LSM6DS3_ACC_GYRO_OUTY_L_G );
	if( errorLevel != IMU_SUCCESS )
	{
		if( errorLevel == IMU_ALL_ONES_WARNING )
		{
			allOnesCounter++;
		}
		else
		{
			nonSuccessCounter++;
		}
	}
	return output;
}
float LSM6DS3::readFloatGyroY( void )
{
	float output = calcGyro(readRawGyroY());
	return output;
}

int16_t LSM6DS3::readRawGyroZ( void )
{
	int16_t output;
	status_t errorLevel = readRegisterInt16( &output, LSM6DS3_ACC_GYRO_OUTZ_L_G );
	if( errorLevel != IMU_SUCCESS )
	{
		if( errorLevel == IMU_ALL_ONES_WARNING )
		{
			allOnesCounter++;
		}
		else
		{
			nonSuccessCounter++;
		}
	}
	return output;
}
float LSM6DS3::readFloatGyroZ( void )
{
	float output = calcGyro(readRawGyroZ());
	return output;
}

float LSM6DS3::calcGyro( int16_t input )
{
	uint8_t gyroRangeDivisor = settings.gyroRange / 125;
	if ( settings.gyroRange == 245 ) {
		gyroRangeDivisor = 2;
	}

	float output = (float)input * 4.375 * (gyroRangeDivisor) / 1000;
	return output;
}

//****************************************************************************//
//
//  Temperature section
//
//****************************************************************************//
int16_t LSM6DS3::readRawTemp( void )
{
	int16_t output;
	readRegisterInt16( &output, LSM6DS3_ACC_GYRO_OUT_TEMP_L );
	return output;
}

float LSM6DS3::readTempC( void )
{
	float output = (float)readRawTemp() / 16; //divide by 16 to scale
	output += 25; //Add 25 degrees to remove offset

	return output;

}

float LSM6DS3::readTempF( void )
{
	float output = (float)readRawTemp() / 16; //divide by 16 to scale
	output += 25; //Add 25 degrees to remove offset
	output = (output * 9) / 5 + 32;

	return output;

}

//****************************************************************************//
//
//  FIFO section
//
//****************************************************************************//
void LSM6DS3::fifoBegin( void ) {
	//CONFIGURE THE VARIOUS FIFO SETTINGS
	//
	//
	//This section first builds a bunch of config words, then goes through
	//and writes them all.

	//Split and mask the threshold
	uint8_t thresholdLByte = settings.fifoThreshold & 0x00FF;
	uint8_t thresholdHByte = (settings.fifoThreshold & 0x0F00) >> 8;
	//Pedo bits not configured (ctl2)

	//CONFIGURE FIFO_CTRL3
	uint8_t tempFIFO_CTRL3 = 0;
	if (settings.gyroFifoEnabled == 1)
	{
		//Set up gyro stuff
		//Build on FIFO_CTRL3
		//Set decimation
		tempFIFO_CTRL3 |= (settings.gyroFifoDecimation & 0x07) << 3;

	}
	if (settings.accelFifoEnabled == 1)
	{
		//Set up accelerometer stuff
		//Build on FIFO_CTRL3
		//Set decimation
		tempFIFO_CTRL3 |= (settings.accelFifoDecimation & 0x07);
	}

	//CONFIGURE FIFO_CTRL4  (nothing for now-- sets data sets 3 and 4
	uint8_t tempFIFO_CTRL4 = 0;


	//CONFIGURE FIFO_CTRL5
	uint8_t tempFIFO_CTRL5 = 0;
	switch (settings.fifoSampleRate) {
	default:  //set default case to 10Hz(slowest)
	case 10:
		tempFIFO_CTRL5 |= LSM6DS3_ACC_GYRO_ODR_FIFO_10Hz;
		break;
	case 25:
		tempFIFO_CTRL5 |= LSM6DS3_ACC_GYRO_ODR_FIFO_25Hz;
		break;
	case 50:
		tempFIFO_CTRL5 |= LSM6DS3_ACC_GYRO_ODR_FIFO_50Hz;
		break;
	case 100:
		tempFIFO_CTRL5 |= LSM6DS3_ACC_GYRO_ODR_FIFO_100Hz;
		break;
	case 200:
		tempFIFO_CTRL5 |= LSM6DS3_ACC_GYRO_ODR_FIFO_200Hz;
		break;
	case 400:
		tempFIFO_CTRL5 |= LSM6DS3_ACC_GYRO_ODR_FIFO_400Hz;
		break;
	case 800:
		tempFIFO_CTRL5 |= LSM6DS3_ACC_GYRO_ODR_FIFO_800Hz;
		break;
	case 1600:
		tempFIFO_CTRL5 |= LSM6DS3_ACC_GYRO_ODR_FIFO_1600Hz;
		break;
	case 3300:
		tempFIFO_CTRL5 |= LSM6DS3_ACC_GYRO_ODR_FIFO_3300Hz;
		break;
	case 6600:
		tempFIFO_CTRL5 |= LSM6DS3_ACC_GYRO_ODR_FIFO_6600Hz;
		break;
	}
	//Hard code the fifo mode here:
	tempFIFO_CTRL5 |= settings.fifoModeWord = 6;  //set mode:

	//Write the data
	writeRegister(LSM6DS3_ACC_GYRO_FIFO_CTRL1, thresholdLByte);
	//Serial.println(thresholdLByte, HEX);
	writeRegister(LSM6DS3_ACC_GYRO_FIFO_CTRL2, thresholdHByte);
	//Serial.println(thresholdHByte, HEX);
	writeRegister(LSM6DS3_ACC_GYRO_FIFO_CTRL3, tempFIFO_CTRL3);
	writeRegister(LSM6DS3_ACC_GYRO_FIFO_CTRL4, tempFIFO_CTRL4);
	writeRegister(LSM6DS3_ACC_GYRO_FIFO_CTRL5, tempFIFO_CTRL5);

}
void LSM6DS3::fifoClear( void ) {
	//Drain the fifo data and dump it
	while( (fifoGetStatus() & 0x1000 ) == 0 ) {
		fifoRead();
	}

}
int16_t LSM6DS3::fifoRead( void ) {
	//Pull the last data from the fifo
	uint8_t tempReadByte = 0;
	uint16_t tempAccumulator = 0;
	readRegister(&tempReadByte, LSM6DS3_ACC_GYRO_FIFO_DATA_OUT_L);
	tempAccumulator = tempReadByte;
	readRegister(&tempReadByte, LSM6DS3_ACC_GYRO_FIFO_DATA_OUT_H);
	tempAccumulator |= ((uint16_t)tempReadByte << 8);

	return tempAccumulator;
}

uint16_t LSM6DS3::fifoGetStatus( void ) {
	//Return some data on the state of the fifo
	uint8_t tempReadByte = 0;
	uint16_t tempAccumulator = 0;
	readRegister(&tempReadByte, LSM6DS3_ACC_GYRO_FIFO_STATUS1);
	tempAccumulator = tempReadByte;
	readRegister(&tempReadByte, LSM6DS3_ACC_GYRO_FIFO_STATUS2);
	tempAccumulator |= (tempReadByte << 8);

	return tempAccumulator;

}
void LSM6DS3::fifoEnd( void ) {
	// turn off the fifo
	writeRegister(LSM6DS3_ACC_GYRO_FIFO_CTRL5, 0x00);  //Disable
}
