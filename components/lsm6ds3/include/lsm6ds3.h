/******************************************************************************
lsm6ds3.h
LSM6DS3 driver definitions and API

ESP-IDF-native port with direct I2C/SPI device handle support.
******************************************************************************/

#pragma once

#include <cstdint>

#include "driver/i2c_master.h"
#include "driver/spi_master.h"

#define I2C_MODE 0
#define SPI_MODE 1

typedef enum
{
	IMU_SUCCESS,
	IMU_HW_ERROR,
	IMU_NOT_SUPPORTED,
	IMU_GENERIC_ERROR,
	IMU_OUT_OF_BOUNDS,
	IMU_ALL_ONES_WARNING,
} status_t;

namespace lsm6ds3 {
inline constexpr uint8_t kWhoAmIRegister = 0x0F;
inline constexpr uint8_t kWhoAmIValueLsm6ds3 = 0x69;
inline constexpr uint8_t kWhoAmIValueLsm6ds3trc = 0x6A;
inline constexpr uint8_t kInt1CtrlRegister = 0x0D;
inline constexpr uint8_t kStatusRegister = 0x1E;
inline constexpr uint8_t kInt1AccelDataReadyMask = 0x01;
inline constexpr uint8_t kInt1GyroDataReadyMask = 0x02;
inline constexpr uint8_t kStatusAccelDataReadyMask = 0x01;
inline constexpr uint8_t kStatusGyroDataReadyMask = 0x02;
}

class LSM6DS3Core
{
public:
	explicit LSM6DS3Core( i2c_master_dev_handle_t i2c_device );
	explicit LSM6DS3Core( spi_device_handle_t spi_device );
	~LSM6DS3Core() = default;

	status_t beginCore( void );
	status_t readRegisterRegion(uint8_t*, uint8_t, uint8_t );
	status_t readRegister(uint8_t*, uint8_t);
	status_t readRegisterInt16(int16_t*, uint8_t offset );
	status_t writeRegister(uint8_t, uint8_t);
	status_t embeddedPage( void );
	status_t basePage( void );

private:
	uint8_t commInterface;
	i2c_master_dev_handle_t i2c_dev;
	spi_device_handle_t spi_dev;
};

struct SensorSettings {
public:
	uint8_t gyroEnabled;
	uint16_t gyroRange;
	uint16_t gyroSampleRate;
	uint16_t gyroBandWidth;
	uint8_t gyroFifoEnabled;
	uint8_t gyroFifoDecimation;
	uint8_t accelEnabled;
	uint8_t accelODROff;
	uint16_t accelRange;
	uint16_t accelSampleRate;
	uint16_t accelBandWidth;
	uint8_t accelFifoEnabled;
	uint8_t accelFifoDecimation;
	uint8_t tempEnabled;
	uint8_t commMode;
	uint16_t fifoThreshold;
	int16_t fifoSampleRate;
	uint8_t fifoModeWord;
};

class LSM6DS3 : public LSM6DS3Core
{
public:
	SensorSettings settings;
	uint16_t allOnesCounter;
	uint16_t nonSuccessCounter;

	explicit LSM6DS3( i2c_master_dev_handle_t i2c_device );
	explicit LSM6DS3( spi_device_handle_t spi_device );
	~LSM6DS3() = default;

	status_t begin(SensorSettings* pSettingsYouWanted = nullptr);
	int16_t readRawAccelX( void );
	int16_t readRawAccelY( void );
	int16_t readRawAccelZ( void );
	int16_t readRawGyroX( void );
	int16_t readRawGyroY( void );
	int16_t readRawGyroZ( void );
	float readFloatAccelX( void );
	float readFloatAccelY( void );
	float readFloatAccelZ( void );
	float readFloatGyroX( void );
	float readFloatGyroY( void );
	float readFloatGyroZ( void );
	int16_t readRawTemp( void );
	float readTempC( void );
	float readTempF( void );
	void fifoBegin( void );
	void fifoClear( void );
	int16_t fifoRead( void );
	uint16_t fifoGetStatus( void );
	void fifoEnd( void );
	float calcGyro( int16_t );
	float calcAccel( int16_t );
};
