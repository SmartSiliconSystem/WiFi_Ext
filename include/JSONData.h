/*
 * JSONData.h
 *
 *  Created on: 12 janv. 2017
 *      Author: françois
 */

#ifndef INCLUDE_JSONDATA_H_
#define INCLUDE_JSONDATA_H_

#include <c_types.h>

struct HttpdConnectionSlot;

#define MAGIC							  0xFC
#define SOURCEDEST_APPLIANCE		   	0xF000
#define SOURCEDEST_APPLIANCE_MEMBRANE  	0xF100
#define SOURCEDEST_WIFI_EXT			   	0xF200
#define SOURCEDEST_SENSOR_EXT			0xF400
#define SOURCEDEST_CONSOLE				0xF800
#define SOURCEDEST_NONE					0x0000

#define SOURCE							SOURCEDEST_WIFI_EXT	// Who am I ?
#define MSG_SIZE					   74 // Size of the message data structure exchanged between Appliance and Wifi extension
#define APP_STRUCT_SIZE               193 // Size of the appliance data structure in appliance flash memory

#define STATUS_BIT_DEBUG_ON				   (1<<28) // Turn debug ON
#define STATUS_BIT_TFT_ON		    	   (1<<27) // Indicates that the TFT is ON
#define STATUS_BIT_CU_TEST_SUPPLIED	       (1<<26) // Indicates that the CU test solution has been supplied in the container
#define STATUS_BIT_SUPPLY_CU_TEST	       (1<<25) // Indicates that the CU test solution must be supplied in the container
#define STATUS_BIT_CONTAINER_FILLED        (1<<24) // Indicates that the container is completely filled

#define STATUS_BIT_EMPTY_CONTAINER         (1<<23) // Indicates that the container must be emptied
#define STATUS_BIT_CONTAINER_EMPTIED       (1<<22) // Indicates that the container is completely emptied
#define STATUS_BIT_FILL_CONTAINER          (1<<21) // Indicates that the container must be filled
#define STATUS_BIT_POLARITY                (1<<20) // Indicates that the polarity cycle is normal or reverse (set by the appliance)
#define STATUS_BIT_HIGH_CYCLE	 		   (1<<19) // Indicates that the current Copper cycle is high
#define STATUS_BIT_SLEEP_MODE      		   (1<<18) // Set by the browser to turn appliance in sleep mode
#define STATUS_BIT_KEY3             	   (1<<17) // Indicates that the license key #3 is valid
#define STATUS_BIT_KEY2             	   (1<<16) // Indicates that the license key #2 is valid

#define STATUS_BIT_KEY1             	   (1<<15) // Indicates that the license key #1 is valid
#define STATUS_BIT_PROG_MESSAGE			   (1<<14) // Indicates that the following message is a program message or a communication message
#define STATUS_BIT_UPSTREAM          	   (1<<13) // Indicates that the following message is an upstream message (from appliance to Wifi or Wifi to Wifi)
#define STATUS_BIT_PH_PROBE_UNCALIBRATED   (1<<12) // Indicates that the PH probe requires calibration
#define STATUS_BIT_PH_PROBE_CALIBRATED     (1<<11) // Indicates that the PH probe is calibrated
#define STATUS_BIT_PH_CALIBRATION_3_ON_3   (1<<10) // Indicates that the calibration step 2 is finished (PH 4.01)
#define STATUS_BIT_PH_CALIBRATION_2_ON_3   	(1<<9) // Indicates that the calibration step 2 begin (PH 4.01)
#define STATUS_BIT_PH_CALIBRATION_1_ON_3    (1<<8) // Indicates that the calibration step 1 is finished (PH 6.86)

#define STATUS_BIT_PH_CALIBRATION_0_ON_3    (1<<7) // Indicates that the calibration step 1 begin (PH 6.86)
#define STATUS_BIT_PH_CALIBRATION_STEP2  	(1<<6) // Indicates that the calibration finish to fill container with test solution
#define STATUS_BIT_PH_CALIBRATION_STEP1  	(1<<5) // Indicates that the calibration finish to empty the container
#define STATUS_BIT_PH_PROBE_PRESENT    	 	(1<<4) // Indicates that the PH probe has been detected
#define STATUS_BIT_WATER_FLOW		        (1<<3) // Indicates that the water is flowing in the pipe
#define STATUS_BIT_COPPER_SHORTED      	 	(1<<2) // Indicates that the copper electrodes are either shorted or intensity threshold has been overpassed
#define STATUS_BIT_TITANIUM_SHORTED    	 	(1<<1) // Indicates that the titanium electrodes are either shorted or intensity threshold has been overpassed
#define STATUS_BIT_COPPER_TO_REPLACE   	 	(1<<0) // Indicates that the copper electrode need to be replaced

/*
 * Data structure sent to the browser. Received from and sent to the appliance
 */
typedef struct __attribute__ ((packed)) {
  uint16_t magic;					// 2, magic header number
  uint16_t crc; 					// 2, 16bits crc
  uint16_t SourceDest;				// 2, Source Destination
  uint32_t Status; 					// 4, bitwise status of operation
  uint16_t WaterHardness; 			// 2, ppm
  float PH; 						// 4
  uint16_t Turbidity;				// 2, NTU
  float WaterTemp; 					// 4
  float CopperReleased; 			// 4
  float WaterFlow; 					// 4
  float WaterVolume; 				// 4, volume of water in m3
  uint16_t ContainerMLCapacity;		// 2, the capacity in ML of the container
  uint16_t CopperElectrodeMass; 	// 2, Mass of copper electrodes in g
  uint16_t WaterColor;				// 2, RGB (5-6-5)
  char ApplianceName[10]; 			// 10, Name of the appliance
  uint8_t ContainerCuLevel;			// 1
  uint8_t ContainerPHLevel;			// 1
  uint16_t ScreenSavingTime;		// 2, Inactivity time before turning off the TFT
  uint32_t key1; 					// 4
  uint32_t key2; 					// 4
  uint32_t key3; 					// 4
  uint32_t OnTime; 					// 4
  uint32_t ID; 						// 4 	-> 74 bytes
  // Extra parameters not part of the data message exchanged between appliances
  uint32_t KeyLicenseExpiration;	// 4, grace duration before key activation is required
  uint32_t ValidKey1; 				// 4
  uint32_t ValidKey2; 				// 4
  uint32_t ValidKey3; 				// 4
  uint32_t ValidKey; 				// 4
  float PHOffsetVoltage; 			// 4, Calibration voltage offset for PH probe
  float PHMarginError;				// 4, The acceptable margin error for PH stabilization
  float SolutionMLPerCuTest;		// 4, how much cu solution to dispense per test
  float WaterMLPerCuTest;			// 4, how much water to dispense per test
  uint32_t CopperHighCycleDuration; // 4, the duration of the High cycle cycle upon button press
  float MaxCopperElectrodeCurrent;  // 4, maximum admissible current on the copper electrodes, depends on the number of electrodes present in the system
  float CopperElectrodeCurrent;		// 4, mean value of copper electrode current sense
  float CopperElectrodesKFactor;	// 4, the factor of conductivity (K = A / ppm) for the Copper electrodes
  float MaxTitaniumElectrodeCurrent;// 4, maximum admissible current on the titanium electrodes, depends on the number of electrodes present in the system
  float TitaniumElectrodeCurrent;	// 4, mean value of titanium electrode current sense
  float TitaniumElectrodesKFactor;	// 4, the factor of conductivity (K = A / ppm) for the Titanium electrodes
  uint8_t MaxCopperDutyCycle; 		// 1, maximum dutycycle for the copper HBridge PWM
  uint8_t MaxTitaniumDutyCycle; 	// 1, maximum dutycycle for the titanium HBridge PWM
  uint8_t MotorCuDirection;			// 1, direction of Motor to meet container filling
  uint8_t MotorCuPulsesPerML;		// 1, number of hall detection pulses per milliliter
  uint8_t MotorFillerDirection;		// 1, direction of Motor to meet container filling
  uint8_t MotorFillerPulsesPerML;	// 1, number of hall detection pulses per milliliter
  uint8_t MotorExhaustDirection;	// 1, direction of Motor to meet container exhaust
  uint8_t MotorExhaustPulsesPerML;	// 1, number of hall detection pulses per milliliter
  uint8_t MotorPHMinusDirection;	// 1, direction of Motor to meet PH- pipe filling
  uint8_t MotorPHMinusPulsesPerML;	// 1, number of hall detection pulses per milliliter
  uint8_t MotorPHPlusDirection;		// 1, direction of Motor to meet PH+ pipe filling
  uint8_t MotorPHPlusPulsesPerML;	// 1, number of hall detection pulses per milliliter
  uint8_t ButtonPressDuration;		// 1, Duration of button press before invoking special function
} TApplianceData;

/*
 * Data structure sent by the browser
 */
typedef struct __attribute__ ((packed)) {
  uint32_t Status; 					// 4, bitwise status of operation
  float WaterVolume; 				// 4, volume of water
  char ApplianceName[10]; 			// 10
  uint32_t key1; 					// 4
  uint32_t key2; 					// 4
  uint32_t key3; 					// 4
  // Extra parameters not part of the data message exchanged between appliances
  char StationSSID[32];				// 32, The name of the WiFi access point to connect to
  char StationPwd[64];				// 64, The password to connect to the access point
  char SoftAPSSID[32];				// 32, The name of the WiFi access point to connect to
  char SoftAPPwd[64];				// 64, The password to connect to the access point
} TApplianceWebData;

static TApplianceData MyAppliance;
static TApplianceWebData WebData;

void UpdateMyAppliance(struct HttpdConnectionSlot *slot);
void MyApplianceToJSON(struct HttpdConnectionSlot *slot);

#endif /* INCLUDE_JSONDATA_H_ */
