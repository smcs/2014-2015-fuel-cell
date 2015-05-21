///////////////////////////////////////////////////////////////////////////////
// ThunderballHwConfig.cpp
//
// (C) Protonex Technology Corporation
// The Next Generation of Portable Power(TM)
// 153 Northboro Road
// Southborough, MA 01772-1034
//
// NOTICE: This file contains private and proprietary information of
// Protonex Technology Corporation.
///////////////////////////////////////////////////////////////////////////////


#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "HwConfig.h"

#include "TimerMgr.h"
#include "EventMgr.h"
#include "PlatformConfig.h"
#include "Heap.h"
#include "AtomicLock.h"
#include "SysStats.h"

#include "BaseObject.h"
#include "OutStreamInterface.h"
#include "Debug.h"

#include "ThunderballSofcMgr.h"
#include "ThunderballConfigControls.h"
#include "GPIOManager.h"
#include "GPIOConst.h"

static int m_fuelCellSwitchPinId;
static int m_igniterPinId;
static int m_fanPinId;
static int m_fuelValvePinId;
static int m_COPowerEnablePinId;
static int m_COLowPowerEnablePinId;
static int m_enableExternalBrickPinId;
static int m_resetExternalBrickPinId;
static int m_enableInternalBrickPinId;
static int m_buttonLedPinId;
static int m_buttonPinId;

void hwEnableLeds(U8 bfOnLedMask)
{
#if 0
	if (bfOnLedMask & ThunderballSofcMgr::RedLedMask)
		AVR32_GPIO.port[AVR32_PIN_PA06 / 32].ovrs = LED_RED;
	else
		AVR32_GPIO.port[AVR32_PIN_PA06 / 32].ovrc = LED_RED;

	if (bfOnLedMask & ThunderballSofcMgr::GreenLedMask)
		AVR32_GPIO.port[AVR32_PIN_PA06 / 32].ovrs = LED_GREEN;
	else
		AVR32_GPIO.port[AVR32_PIN_PA06 / 32].ovrc = LED_GREEN;

	if (bfOnLedMask & ThunderballSofcMgr::YellowLedMask)
		AVR32_GPIO.port[AVR32_PIN_PA06 / 32].ovrs = LED_YELLOW;
	else
		AVR32_GPIO.port[AVR32_PIN_PA06 / 32].ovrc = LED_YELLOW;
#endif

	if (bfOnLedMask & ThunderballSofcMgr::SwitchLedMask)
		GPIO::GPIOManager::getInstance()->setValue(m_buttonLedPinId, GPIO::HIGH);
	else
		GPIO::GPIOManager::getInstance()->setValue(m_buttonLedPinId, GPIO::LOW);
}

int sampleButtonState()
{
    int nButtonState = GPIO::GPIOManager::getInstance()->getValue(m_buttonPinId);	
	if (nButtonState == GPIO::LOW) return 1000 + m_buttonPinId;
	return m_buttonPinId;
}

U8 hwGetButtonState()
{
    int nButtonState = GPIO::GPIOManager::getInstance()->getValue(m_buttonPinId);	
	if (nButtonState == GPIO::LOW) return 1;
	return 0;
}

void hwEnableFuelCell(HwDriveMode hwDriveMode) 
{ 

	if (hwDriveMode != HwDriveModeOff)
	{
		GPIO::GPIOManager::getInstance()->setValue(m_fuelCellSwitchPinId, GPIO::HIGH);
	}
	else
	{
		GPIO::GPIOManager::getInstance()->setValue(m_fuelCellSwitchPinId, GPIO::LOW);
	}
}

void hwEnableIgniter(HwDriveMode hwDriveMode) 
{ 
	if (hwDriveMode != HwDriveModeOff)
	{
		GPIO::GPIOManager::getInstance()->setValue(m_igniterPinId, GPIO::HIGH);
	}
	else
	{
		GPIO::GPIOManager::getInstance()->setValue(m_igniterPinId, GPIO::LOW);
	}
}

void hwEnableFan(HwDriveMode hwDriveMode) 
{ 
	return;

	if (hwDriveMode != HwDriveModeOff)
	{
		GPIO::GPIOManager::getInstance()->setValue(m_fanPinId, GPIO::HIGH);
	}
	else
	{
		GPIO::GPIOManager::getInstance()->setValue(m_fanPinId, GPIO::LOW);
	}
}

void hwEnableValve(HwDriveMode hwDriveMode) 
{ 
	if (hwDriveMode != HwDriveModeOff)
	{
		GPIO::GPIOManager::getInstance()->setValue(m_fuelValvePinId, GPIO::HIGH);
	}
	else
	{
		GPIO::GPIOManager::getInstance()->setValue(m_fuelValvePinId, GPIO::LOW);
	}
}

void enableCoSensorPower(HwDriveMode hwDriveMode) 
{ 
	if (hwDriveMode != HwDriveModeOff)
	{
		GPIO::GPIOManager::getInstance()->setValue(m_COPowerEnablePinId, GPIO::HIGH);
	}
	else
	{
		GPIO::GPIOManager::getInstance()->setValue(m_COPowerEnablePinId, GPIO::LOW);
	}
}

void enableLowSensorWPower(HwDriveMode hwDriveMode) 
{ 
	if (hwDriveMode != HwDriveModeOff)
	{
		GPIO::GPIOManager::getInstance()->setValue(m_COLowPowerEnablePinId, GPIO::LOW);
	}
	else
	{
		GPIO::GPIOManager::getInstance()->setValue(m_COLowPowerEnablePinId, GPIO::HIGH);
	}
}

static void enableExternalBrick(HwDriveMode bMode, S32 nNumBricks)
{
	if (bMode != HwDriveModeOn)
	{
		GPIO::GPIOManager::getInstance()->setValue(m_enableExternalBrickPinId, GPIO::LOW);
	}
	else
	{

		GPIO::GPIOManager::getInstance()->setValue(m_resetExternalBrickPinId, GPIO::LOW);
		GPIO::GPIOManager::getInstance()->setValue(m_resetExternalBrickPinId, GPIO::HIGH);
		GPIO::GPIOManager::getInstance()->setValue(m_resetExternalBrickPinId, GPIO::LOW);

		GPIO::GPIOManager::getInstance()->setValue(m_enableExternalBrickPinId, GPIO::HIGH);
	}
}

static void enableInternalBrick(HwDriveMode bMode, S32 nNumBricks)
{
	if (bMode != HwDriveModeOn)
	{
		GPIO::GPIOManager::getInstance()->setValue(m_enableInternalBrickPinId, GPIO::LOW);
	}
	else
	{
		GPIO::GPIOManager::getInstance()->setValue(m_enableInternalBrickPinId, GPIO::HIGH);
	}
}


void HwConfig::init()
{
	m_fuelCellSwitchPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_17", GPIO::LOW);
	m_igniterPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_15", GPIO::LOW);
	m_fanPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_14", GPIO::LOW);
	m_fuelValvePinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_16", GPIO::LOW);
	m_COPowerEnablePinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P9_15", GPIO::LOW);
	m_COLowPowerEnablePinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P9_41", GPIO::HIGH);
	m_resetExternalBrickPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P9_23", GPIO::LOW);
	m_enableExternalBrickPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_12", GPIO::LOW);
	m_enableInternalBrickPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P9_24", GPIO::LOW);

	// moved to UI process
	//	m_buttonLedPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P9_12", GPIO::LOW);
	//	m_buttonPinId = GPIO::GPIOManager::getInstance()->setupInputPin("P8_7");

	ThunderballConfigControls::hwInitFuelCellSwitch(hwEnableFuelCell);
	ThunderballConfigControls::hwInitIgniter(hwEnableIgniter);
	ThunderballConfigControls::hwInitFuelValve(hwEnableValve);
	ThunderballConfigControls::hwInitCOSensor(enableCoSensorPower, enableLowSensorWPower);

	ThunderballConfigControls::hwInitExternalBrick(enableExternalBrick);
	ThunderballConfigControls::hwInitInternalBrick(enableInternalBrick);

	// moved to UI process
	//	ThunderballConfigControls::hwInitUi(hwEnableLeds, hwGetButtonState);
}

void HwConfig::enableHardware()
{
}

void HwConfig::idle()
{
}

void HwConfig::reset()
{
	system("/sbin/shutdown -r now");
}

unsigned long HwConfig::resetFlag()
{
    return 0;
}   // end resetFlag

void HwConfig::kickTheDog()
{
}

U32 HwConfig::getHardwareRev()
{
	return 0;
}
