///////////////////////////////////////////////////////////////////////////////
// GreenLanternConfig.cpp
//
// (C) Protonex Technology Corporation
// The Next Generation of Portable Power(TM)
// 153 Northboro Road
// Southborough, MA 01772-1034
//
// NOTICE: This file contains confidential and proprietary information of
// Protonex Technology Corporation.
///////////////////////////////////////////////////////////////////////////////

#include "ThunderballConfigControls.h"
#include "ThunderballConfigAdc.h"
#include "ThunderballSofcMgr.h"

#include "AdcMonitor.h"
#include "AppEnums.h"
#include "BufferQueue.h"
#include "DataFeedbackReplicator.h"
#include "DebugMemoryMgr.h"
//#include "PeakPowerDcDc.h"
#include "ObjectList.h"
//#include "ObjectGroup.h"

#include "LTC2634_12BitDacDevice.h"
#include "GenericDACDevice.h"
#include "LinkedAirControl.h"
#include "TableActivatedBaseControl.h"
#include "SimpleMachine.h"
#include "TableCalMon.h"
#include "SegmentedLookupTable.h"
#include "ThunderballSensorControl.h"
#include "cPWM.h"

#include "DS1390CalClkDevice.h"
#include "CalClkMgr.h"

#include <stdio.h>
#include "PruPwmInterface.h"
#include "ThunderballSystemStats.h"

// &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&777
//
static DataFeedbackReplicatorWithDifferentTypes*	s_pConvertVoltageOutFeedbackObject;
static AdcMonitor**									s_ppSystemSpiAdcs;
static U8											gs_bfDipMask;

//
// &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&777

static LTC2634_12BitDacDevice*						gs_TrimDacDevice;
static DS1390CalClkDevice*							gs_pCalClkDevice;
static CalClkMgr*									gs_pCalendarClockMgr;

cPWM::cPWM* gs_aPwmDevices[ThunderballSofcMgr::NumCtrls];

static TableActivatedBaseControl* gs_aFlowPwmControl[ThunderballSofcMgr::NumCtrls];
static SimpleMachine* gs_pFuelCellSwitch;
static SimpleMachine* gs_pIgniter;
static SimpleMachine* gs_pValve;
static ThunderballSensorControl* gs_pCOSensorPower;

static void (*gs_hwEnableLeds)(U8 bfLedMask);
static U8 (*gs_hwGetBtnState)();

void ThunderballConfigControls::hwInitUi(void (*hwEnableLeds)(U8 bfLedMask), U8 (*getBtnState)())
{
	gs_hwEnableLeds = hwEnableLeds;
	gs_hwGetBtnState = getBtnState;
}

#include "DebugMemoryMgr.h"
#include "GPIOManager.h"
#include "GPIOConst.h"
#include "LinuxBufferQueue.h"

void ThunderballConfigControls::allocate(BufferQueue* pQ)
{
	gs_TrimDacDevice = new LTC2634_12BitDacDevice(pQ, 0, 10);
		gs_TrimDacDevice->m_nMaxClockSpeed = 100000;
		gs_TrimDacDevice->m_bfSpiMode = 3;
		gs_TrimDacDevice->m_fpSetChipSelect = 0;	
		gs_TrimDacDevice->m_csPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_31", GPIO::HIGH);
	DebugMemoryMgr::instance()->addDevice(gs_TrimDacDevice, "Brick DAC");

	gs_pCalendarClockMgr = new CalClkMgr;
	gs_pCalClkDevice = new DS1390CalClkDevice(pQ, 0);
		gs_pCalClkDevice->m_nMaxClockSpeed = 100000;
		gs_pCalClkDevice->m_bfSpiMode = 3;
		gs_pCalClkDevice->m_fpSetChipSelect = 0;	
		gs_pCalClkDevice->m_csPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P9_17", GPIO::HIGH);
	DebugMemoryMgr::instance()->addDevice(gs_pCalClkDevice, "Cal Clock");

	// Note:: currently these are hardcoded based on the device tree configuration
	gs_aPwmDevices[ThunderballSofcMgr::CathodeAirCtrlIndex] = new cPWM::cPWM("/sys/devices/ocp.3/Cathode_Blower_Drive.12", 40000);
	gs_aPwmDevices[ThunderballSofcMgr::CathodeAirCtrlIndex]->enable(HwDriveModeOff);

	gs_aPwmDevices[ThunderballSofcMgr::PbAirCtrlIndex] = new cPWM::cPWM("/sys/devices/ocp.3/PB_Blower_Drive.13", 40000);
	gs_aPwmDevices[ThunderballSofcMgr::PbAirCtrlIndex]->enable(HwDriveModeOff);

	gs_aPwmDevices[ThunderballSofcMgr::AnodeAirCtrlIndex] = new cPWM::cPWM("/sys/devices/ocp.3/Anode_Pump_Drive.14", 50000);
	gs_aPwmDevices[ThunderballSofcMgr::AnodeAirCtrlIndex]->enable(HwDriveModeOff);

	gs_aPwmDevices[ThunderballSofcMgr::AnodeFuelCtrlIndex] = new cPWM::cPWM("/sys/devices/ocp.3/Anode_Fuel_Drive.16", 111111);
	gs_aPwmDevices[ThunderballSofcMgr::AnodeFuelCtrlIndex]->enable(HwDriveModeOff);

	gs_aPwmDevices[ThunderballSofcMgr::PbFuelCtrlIndex] = new cPWM::cPWM("/sys/devices/ocp.3/PB_Fuel_Drive.15", 111111);
	gs_aPwmDevices[ThunderballSofcMgr::PbFuelCtrlIndex]->enable(HwDriveModeOff);
}

void AddPwmDrive(TableActivatedBaseControl* pCtrl, PwmDevice* pPwm, TableCalMon* pMon, EnumType type, U32 nMaxNom, U16 nGain, U16 nReset, U32 nAveCount, U16 nWindowCount)
{
	pCtrl->init(pPwm);

	static BaseControlConfigBlock PumpConfig =
	{
		0,		// m_nSetPoint;				    -- 55.0 C
		10,		// m_nSetPointDelta;			-- N/A
		1000,	// m_nSetPointCheckPeriod;		-- 1 sec
		1000,	// m_nParamMaxNominal;          -- 10000 steps from 0 to 100%
		0,		// m_nParamMin;
		10,	    // m_nParamGain;
		0,	    // m_nParamReset;				-- 200 sec
		0,		// m_nParamRate;
		1,    // m_nParamStepSize;			-- Deadband filter
		TRUE,	// m_bDirect					-- Indirect Control Action
	};

	pCtrl->config(&PumpConfig, pMon, nAveCount, nWindowCount);
	pCtrl->setMax(nMaxNom);
	pCtrl->setLoopParams(nGain, nReset, 0, 0);
	if (type != AppEnumIllegal)
		pCtrl->appSetup(type, ObjectList::addObject);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThunderballConfigControls::initControls(BufferQueue* pQ, AdcMonitor** ppSystemAdcs, ThunderballSofcMgr* pMgr, int nRemoteSense)
{
	s_ppSystemSpiAdcs = ppSystemAdcs;
	allocate(pQ);

	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^6
	// anode fuel
	// gain 3, reset 60, scale 100, max 20%
	//LinkedAirControl* pCpoxFuel = new LinkedAirControl();
	TableActivatedBaseControl* pCpoxFuel = new TableActivatedBaseControl();
	gs_aFlowPwmControl[ThunderballSofcMgr::AnodeFuelCtrlIndex] = pCpoxFuel;

	// BUTANE_FUEL - there is a chance that the PID constants on the setup function below need to be touched.

	AddPwmDrive(gs_aFlowPwmControl[ThunderballSofcMgr::AnodeFuelCtrlIndex], gs_aPwmDevices[ThunderballSofcMgr::AnodeFuelCtrlIndex], NULL, AppEnumAnodeFuelFlowControl, 800, 0, 0, 10, 1); //3, 60, 5);
	gs_aFlowPwmControl[ThunderballSofcMgr::AnodeFuelCtrlIndex]->setDcDivisor(100);
	gs_aFlowPwmControl[ThunderballSofcMgr::AnodeFuelCtrlIndex]->setParamMax(800);

	// 0.8 slpm = 170
	// 0.3 SLPM = 93
	//gs_aFlowPwmControl[ThunderballSofcMgr::AnodeFuelCtrlIndex]->setupLinearApprox(300, 93, 800, 170);
	gs_aFlowPwmControl[ThunderballSofcMgr::AnodeFuelCtrlIndex]->setupLinearApprox(300, 329, 800, 515); // max has to increase... to 275?

	// hook-up ADCs
	gs_aFlowPwmControl[ThunderballSofcMgr::AnodeFuelCtrlIndex]->setMonitor(ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_ANODE_FUEL_FLOW_SENSE]);
	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_ANODE_FUEL_FLOW_SENSE]->setControlToUpdate(gs_aFlowPwmControl[ThunderballSofcMgr::AnodeFuelCtrlIndex], 0, 1);

	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^6
	// pb fuel
	// gain 3, reset 60, scale 100, max 30%
	LinkedAirControl* pPbFuel = new LinkedAirControl();
	gs_aFlowPwmControl[ThunderballSofcMgr::PbFuelCtrlIndex] = pPbFuel;

	// BUTANE_FUEL - there is a chance that the PID constants on the setup function below need to be touched.
	AddPwmDrive(gs_aFlowPwmControl[ThunderballSofcMgr::PbFuelCtrlIndex], gs_aPwmDevices[ThunderballSofcMgr::PbFuelCtrlIndex], NULL, AppEnumPBFuelFlowControl, 800, 3, 60, 5, 1);

	gs_aFlowPwmControl[ThunderballSofcMgr::PbFuelCtrlIndex]->setDcDivisor(100);
	//gs_aFlowPwmControl[ThunderballSofcMgr::PbFuelCtrlIndex]->setParamMax(500);
	
	// 12 PSI
	// 0.359 slpm = 65
	// 1.399 SLPM = 108
	//gs_aFlowPwmControl[FLOW_CHAN_PB_FUEL_FLOW]->setupLinearApprox(263, 65, 1399, 108);
	gs_aFlowPwmControl[ThunderballSofcMgr::PbFuelCtrlIndex]->setupLinearApprox(900, 320, 1288, 360);

	// hook-up ADCs
	gs_aFlowPwmControl[ThunderballSofcMgr::PbFuelCtrlIndex]->setMonitor(ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_PB_FUEL_FLOW_SENSE]);
	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_PB_FUEL_FLOW_SENSE]->setControlToUpdate(gs_aFlowPwmControl[ThunderballSofcMgr::PbFuelCtrlIndex], 0, 1);

	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^6
	// cpox air
	// gain 3, reset 60, scale 100, max 100%
	TableActivatedBaseControl* pCpoxAir = new TableActivatedBaseControl();
	gs_aFlowPwmControl[ThunderballSofcMgr::AnodeAirCtrlIndex] = pCpoxAir;

	// NEW_ANODE_AIR_PUMP - there is a chance that the PID constants on the setup function below need to be touched.

	AddPwmDrive(gs_aFlowPwmControl[ThunderballSofcMgr::AnodeAirCtrlIndex], gs_aPwmDevices[ThunderballSofcMgr::AnodeAirCtrlIndex], NULL, AppEnumIllegal, 1000, 0, 0, 20, 1); //30, 40);

	pCpoxAir->appSetup(AppEnumAnodeAirFlowControl, ObjectList::addObject);

	gs_aFlowPwmControl[ThunderballSofcMgr::AnodeAirCtrlIndex]->setDcDivisor(100);
	gs_aFlowPwmControl[ThunderballSofcMgr::AnodeAirCtrlIndex]->setParamMax(400);
//	gs_aFlowPwmControl[ThunderballSofcMgr::AnodeAirCtrlIndex]->setParamMin(100);

	// these are lookups to speed up closing in on correct flow - there may be pressure linkage
	// After calibrating manually move the pump (read force) to flows (0.359, 1.4) below and record the pwm raw ticks below (xx, yy)
	// 3 slpm = xx ticks
	// 9 SLPM = yy ticks
	gs_aFlowPwmControl[ThunderballSofcMgr::AnodeAirCtrlIndex]->setupLinearApprox(500, 122, 750, 202);

	// hook-up ADCs
	gs_aFlowPwmControl[ThunderballSofcMgr::AnodeAirCtrlIndex]->setMonitor(ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_ANODE_AIR_FLOW_SENSE]);
	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_ANODE_AIR_FLOW_SENSE]->setControlToUpdate(gs_aFlowPwmControl[ThunderballSofcMgr::AnodeAirCtrlIndex], 0, 1);
	
	// comment out if driveing test stand
	//pCpoxAir->setupLinkedFlow(gs_aFlowPwmControl[ThunderballSofcMgr::AnodeFuelCtrlIndex], 80, 1397, 1550);
	//pCpoxFuel->setupLinkedFlow(gs_aFlowPwmControl[ThunderballSofcMgr::AnodeAirCtrlIndex], 80, 1550, 1397);

	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^6
	// cathode air
	// gain 3, reset 60, scale 100, max 100%
	gs_aFlowPwmControl[ThunderballSofcMgr::CathodeAirCtrlIndex] = new TableActivatedBaseControl();

	AddPwmDrive(gs_aFlowPwmControl[ThunderballSofcMgr::CathodeAirCtrlIndex], gs_aPwmDevices[ThunderballSofcMgr::CathodeAirCtrlIndex], NULL, AppEnumCathodeAirFlowControl, 1000, 0, 0, 5, 4); //50, 500, 5);
	gs_aFlowPwmControl[ThunderballSofcMgr::CathodeAirCtrlIndex]->setDcDivisor(100);

	// these are lookups to speed up closing in on correct flow - there may be pressure linkage
	// After calibrating manually move the pump (read force) to flows (0.359, 1.4) below and record the pwm raw ticks below (xx, yy)
	// 3 slpm = xx ticks
	// 9 SLPM = yy ticks
	gs_aFlowPwmControl[ThunderballSofcMgr::CathodeAirCtrlIndex]->setupLinearApprox(1000, 135, 2500, 220);

	// hook-up ADCs
	gs_aFlowPwmControl[ThunderballSofcMgr::CathodeAirCtrlIndex]->setMonitor(ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_CATHODE_FLOW_SENSE]);
	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_CATHODE_FLOW_SENSE]->setControlToUpdate(gs_aFlowPwmControl[ThunderballSofcMgr::CathodeAirCtrlIndex], 0, 1);

	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^6
	// pb air
	// gain 3, reset 60, scale 100, max 100%
	// make sure we configure concrete object
	TableActivatedBaseControl* pPbAir = new TableActivatedBaseControl();
	gs_aFlowPwmControl[ThunderballSofcMgr::PbAirCtrlIndex] = pPbAir;

	AddPwmDrive(gs_aFlowPwmControl[ThunderballSofcMgr::PbAirCtrlIndex], gs_aPwmDevices[ThunderballSofcMgr::PbAirCtrlIndex], NULL, AppEnumIllegal, 1000, 0, 0, 5, 4);

	pPbAir->appSetup(AppEnumPBAirFlowControl, ObjectList::addObject);

	gs_aFlowPwmControl[ThunderballSofcMgr::PbAirCtrlIndex]->setDcDivisor(100);

	gs_aFlowPwmControl[ThunderballSofcMgr::PbAirCtrlIndex]->setupLinearApprox(2500, 244, 3500, 340);

	// hook-up ADCs
	gs_aFlowPwmControl[ThunderballSofcMgr::PbAirCtrlIndex]->setMonitor(ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_PB_AIR_FLOW_SENSE]);
	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_PB_AIR_FLOW_SENSE]->setControlToUpdate(gs_aFlowPwmControl[ThunderballSofcMgr::PbAirCtrlIndex], 0, 1);
	
	// comment out if driving test stand
	//pPbAir->setupLinkedFlow(gs_aFlowPwmControl[ThunderballSofcMgr::PbFuelCtrlIndex], 80, 375, 1000);
	pPbFuel->setupLinkedFlow(gs_aFlowPwmControl[ThunderballSofcMgr::PbAirCtrlIndex], 80, 1000, 375);

	pMgr->hwInitCtrls(gs_aFlowPwmControl, gs_pValve);

	if (gs_pFuelCellSwitch)
	{
		gs_pFuelCellSwitch->appSetup(AppEnumFuelCellSwitch, ObjectList::addObject);
		pMgr->hwInitFuelCellSwitch(gs_pFuelCellSwitch);
	}

	if (gs_pIgniter)
	{
		gs_pIgniter->appSetup(AppEnumIgniterControl, ObjectList::addObject);
		pMgr->hwInitIgnitier(gs_pIgniter);
	}

	if (gs_pValve)
	{
		gs_pValve->appSetup(AppEnumValve, ObjectList::addObject);
	}

	if (gs_pCOSensorPower)
	{
		gs_pCOSensorPower->appSetup(AppEnumSensorPowerMachine, ObjectList::addObject);
		gs_pCOSensorPower->start();
	}

	initBricks(ppSystemAdcs, pMgr, nRemoteSense);
	pMgr->hwInitLedDriver(gs_hwEnableLeds);
	pMgr->hwInitButton(gs_hwGetBtnState);

	gs_pCalendarClockMgr->setup(AppEnumCalClkMgr, gs_pCalClkDevice, ObjectList::addObject);
	gs_pCalendarClockMgr->start();
}

void ThunderballConfigControls::hwInitFuelCellSwitch(void (*hwEnableFuelCell)(HwDriveMode hwDriveMode))
{
	gs_pFuelCellSwitch = new SimpleMachine();
	gs_pFuelCellSwitch->init(hwEnableFuelCell);
}

void ThunderballConfigControls::hwInitIgniter(void (*hwEnableIgniter)(HwDriveMode hwDriveMode))
{
	gs_pIgniter = new SimpleMachine();
	gs_pIgniter->init(hwEnableIgniter);
}

void ThunderballConfigControls::hwInitFuelValve(void (*hwEnableValve)(HwDriveMode hwDriveMode))
{
	gs_pValve = new SimpleMachine();
	gs_pValve->init(hwEnableValve);
}

void ThunderballConfigControls::hwInitCOSensor(void (*hwEnablePower)(HwDriveMode hwDriveMode), void (*hwEnableLowPower)(HwDriveMode hwDriveMode))
{
	//return;

	gs_pCOSensorPower = new ThunderballSensorControl();
	gs_pCOSensorPower->init(hwEnablePower);
	gs_pCOSensorPower->initPulseHw(hwEnableLowPower);

	static PulseMachineWithEnableConfigBlock PulseConfig =
	{
        65000,			// 65 secs	full power
        85000,           // 85 secs lower power
	};

	gs_pCOSensorPower->config(&PulseConfig);

}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThunderballConfigControls::setupBufferQueues(BufferQueue* pSpiQ)
{
	gs_TrimDacDevice->setBufferQueue(pSpiQ);

	DebugMemoryMgr::instance()->addDevice(gs_TrimDacDevice, "Pwr DAC");
}


struct ThunderballTrimDevice : public PwmDevice 
{
public:
	void init(LTC2634_12BitDacDevice* pDev, U8 index, U32 nMaxDutyCycle) 
	{
		m_TrimDacDevice = pDev;
		m_nIndex = index;
		m_nMaxDutyCycle = nMaxDutyCycle;
	}
	void enable(HwDriveMode hwDriveMode) { }
    void setDutyCycleBig(U32 nCycle, U32 nMax);

private:
	LTC2634_12BitDacDevice*		m_TrimDacDevice;
	U8							m_nIndex;
	U32							m_nMaxDutyCycle;

};
void ThunderballTrimDevice::setDutyCycleBig(U32 nCycle, U32 nMax)
{
	U32 nDc = m_nMaxDutyCycle * nCycle / nMax;
	if (m_nMaxDutyCycle * nCycle % nMax > nMax / 2) nDc++;

	if (nDc > m_nMaxDutyCycle) nDc = m_nMaxDutyCycle;

	m_TrimDacDevice->updateOutput(m_nIndex, nDc);
}

#include "ThunderballDcDc.h"

static ThunderballDcDc* gs_pExternalDcDc;
static ThunderballDcDc* gs_pInternalDcDc;
static void (*gs_hwEnableExternalBrick)(HwDriveMode hwDriveMode, S32 nNumBricks);
static void (*gs_hwEnableInternalBrick)(HwDriveMode hwDriveMode, S32 nNumBricks);

void ThunderballConfigControls::hwInitExternalBrick(void (*hwEnableBrick)(HwDriveMode hwDriveMode, S32 nNumBricks))
{
	 gs_hwEnableExternalBrick = hwEnableBrick;
}

void ThunderballConfigControls::hwInitInternalBrick(void (*hwEnableBrick)(HwDriveMode hwDriveMode, S32 nNumBricks))
{
	 gs_hwEnableInternalBrick = hwEnableBrick;
}

enum {
	VTrimInternalIndex = 0,
	ITrimInternalIndex = 1,
	VTrimExternalIndex = 2,
	ITrimExternalIndex = 3,

	TrimDacMax = 0x3ff,
	FreqScale = 100,
};

void ThunderballConfigControls::initBricks(AdcMonitor** ppSystemAdcs, ThunderballSofcMgr* pMgr, int nRemoteSense)
{
	s_pConvertVoltageOutFeedbackObject	= new DataFeedbackReplicatorWithDifferentTypes(2);

	//gs_TrimDacDevice->shutDown();
	gs_TrimDacDevice->setExternalReference();

	ThunderballTrimDevice* pVtrimInternal = new ThunderballTrimDevice();
	pVtrimInternal->init(gs_TrimDacDevice, VTrimInternalIndex, TrimDacMax);
	ThunderballTrimDevice* pItrimInternal = new ThunderballTrimDevice();
	pItrimInternal->init(gs_TrimDacDevice, ITrimInternalIndex, TrimDacMax);

	gs_pInternalDcDc = new ThunderballDcDc;																			// WARNING this '20' must match real smaple rate
	gs_pInternalDcDc->hwInit(pVtrimInternal, TrimDacMax * FreqScale, pItrimInternal, TrimDacMax * FreqScale, gs_hwEnableInternalBrick, 20, NULL);	// 20 msec sample rate
	gs_pInternalDcDc->appSetup(AppEnumInternalDcDc, ObjectList::addObject);

	// setup defaults knowing that overrides due to stored settings happen after this.
	gs_pInternalDcDc->setInputDevice(ThunderballDcDc::DcPowerSupply500W);
	gs_pInternalDcDc->configureThresholds(0xffffffff);
	gs_pInternalDcDc->setBrickVoltageAvail(FALSE);
		
	gs_pInternalDcDc->setMaxMinSetpoints(
										500 * FreqScale,			// Min V	- really a max voltage of 50V
										TrimDacMax * FreqScale,		// Max V	- really a min
										0					,		// Min I
										TrimDacMax * FreqScale);	// Max I

	gs_pInternalDcDc->configure(11000,	// min Vin 
								10000,  // max power in
								0,		// min power in
								150,	// voltage band mV 
								50,		// average count 
								200,	// turn down
								1);		// num bricks
	gs_pInternalDcDc->stopConverter();

	s_pConvertVoltageOutFeedbackObject->addControl(gs_pInternalDcDc, ThunderballDcDc::DcDcVoltageInFeedback);

	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_BATTERY_I_SENSE]->setControlToUpdate(gs_pInternalDcDc, ThunderballDcDc::DcDcCurrentFeedback);
	//ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_FC_I_SENSE]->setControlToUpdate(gs_pInternalDcDc, ThunderballDcDc::DcDcCurrentInFeedback);
	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_OUTPUT_V_SENSE]->setControlToUpdate(s_pConvertVoltageOutFeedbackObject, 0);
	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_BATTERY_V_SENSE]->setControlToUpdate(gs_pInternalDcDc, ThunderballDcDc::DcDcVoltageFeedback);
	//ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_BOARD_TEMP]->setControlToUpdate(gs_pInternalDcDc, ThunderballDcDc::DcDcTempFeedback);

	gs_pInternalDcDc->configureTended(0, 0);
	gs_pInternalDcDc->configureInput(10000, 100000, 6000);
	gs_pInternalDcDc->setMaxStepSize(1500);
	gs_pInternalDcDc->setUnMeasuredOutputCurrent(3000);
	gs_pInternalDcDc->setDeadVoltage(11200, 4);	// 4 secs
	gs_pInternalDcDc->setPowerDeadband(2000);
	gs_pInternalDcDc->setFloatVoltage(13500, 0);
	gs_pInternalDcDc->setupTempMonitor((TableCalMon*)ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM1]);
	gs_pInternalDcDc->setupTempCompensation(600, 0, -35, -12);
	// do last
	gs_pInternalDcDc->configureOutput(14600, 100000, 5000, 12500, 10500, 10000, 0);

	ThunderballTrimDevice* pVtrimExternal = new ThunderballTrimDevice();
	pVtrimExternal->init(gs_TrimDacDevice, VTrimExternalIndex, TrimDacMax);
	ThunderballTrimDevice* pItrimExternal = new ThunderballTrimDevice();
	pItrimExternal->init(gs_TrimDacDevice, ITrimExternalIndex, TrimDacMax);

	gs_pExternalDcDc = new ThunderballDcDc;																			// WARNING this '20' must match real smaple rate
	gs_pExternalDcDc->hwInit(pVtrimExternal, TrimDacMax * FreqScale, pItrimExternal, TrimDacMax * FreqScale, gs_hwEnableExternalBrick, 20, NULL);	// 20 msec sample rate
	gs_pExternalDcDc->appSetup(AppEnumExternalDcDc, ObjectList::addObject);
	gs_pExternalDcDc->setBrickVoltageAvail(FALSE);

#ifdef TARGET_24V
	gs_pExternalDcDc->setMaxMinSetpoints(
										360 * FreqScale,			// Min V	- really a max voltage of 40V
										TrimDacMax * FreqScale,		// Max V	- really a min
										0					,		// Min I
										TrimDacMax * FreqScale);	// Max I
#else
	gs_pExternalDcDc->setMaxMinSetpoints(
										550 * FreqScale,			// Min V	- really a max voltage of 20V
										TrimDacMax * FreqScale,		// Max V	- really a min
										0					,		// Min I
										TrimDacMax * FreqScale);	// Max I
#endif

	s_pConvertVoltageOutFeedbackObject->addControl(gs_pExternalDcDc, ThunderballDcDc::DcDcVoltageFeedback);

	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_OUTPUT_I_SENSE]->setControlToUpdate(gs_pExternalDcDc, ThunderballDcDc::DcDcCurrentFeedback);
	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_FC_I_SENSE]->setControlToUpdate(gs_pExternalDcDc, ThunderballDcDc::DcDcCurrentInFeedback);
	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_FC_V_SENSE]->setControlToUpdate(gs_pExternalDcDc, ThunderballDcDc::DcDcVoltageInFeedback);
	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_OUTPUT_V_SENSE]->setControlToUpdate(s_pConvertVoltageOutFeedbackObject, 1);
	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_BOARD_TEMP]->setControlToUpdate(gs_pExternalDcDc, ThunderballDcDc::DcDcTempFeedback);
	if (nRemoteSense)
		ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_REMOTE_V_SENSE]->setControlToUpdate(gs_pExternalDcDc, ThunderballDcDc::DcDcRemoteVoltageFeedback);

	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_PROPANE_DETECT_SENSE]->setControlToUpdate(gs_pCOSensorPower, ThunderballSensorControl::RawPropaneSenseData);
	ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_CO_DETECT_SENSE]->setControlToUpdate(gs_pCOSensorPower, ThunderballSensorControl::RawCOSenseData);

	//ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_BRICK_V]->setControlToUpdate(gs_pDcDc, ThunderballDcDc::DcDcBrickVoltageFeedback);

	//ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_FC_V_SENSE]->setRawFilterDisable(TRUE);

	// setup defaults knowing that overrides due to stored settings happen after this.
	//gs_pExternalDcDc->setInputDevice(ThunderballDcDc::DcPowerSupply500W);
	gs_pExternalDcDc->configureThresholds(0xffffffff);

	gs_pExternalDcDc->configure(20000,	// min Vin 
								50000,  // max power in
								0,		// min power in
								150,	// voltage band mV 
								40,		// average count 
								400,	// turn down
								1);		// num bricks
	gs_pExternalDcDc->configureTended(0, 0);
	gs_pExternalDcDc->configureInput(24000, 300000, 20000);
	gs_pExternalDcDc->setMaxStepSize(1500);
	gs_pExternalDcDc->setUnMeasuredOutputCurrent(2000);
	gs_pExternalDcDc->stopConverter();
	gs_pExternalDcDc->setupTempMonitor((TableCalMon*)ppSystemAdcs[ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM1]);
#ifdef TARGET_24V
	gs_pExternalDcDc->configureOutput(26500, 210000, 20000, 23000, 19000, 20000, 0);
#else
	gs_pExternalDcDc->configureOutput(14100, 210000, 20000, 12500, 9000, 10000, 0);
#endif
	
	pMgr->hwInitOutputConverter(gs_pExternalDcDc);
	pMgr->hwInitBatteryConverter(gs_pInternalDcDc);
}

#include "ThunderballSystemStats.h"

const static TableEntryType s_aLeadAcidFloatChargeVoltageVsTempTable[] PROGMEM =
{
	// 10ths C					// mV / cell
	(TableEntryType)(-300),		(TableEntryType)2440,
	(TableEntryType)(-200),		(TableEntryType)2340,
	(TableEntryType)(-100),		(TableEntryType)2320,
	(TableEntryType)(0),		(TableEntryType)2330,
	(TableEntryType)(100),		(TableEntryType)2280,
	(TableEntryType)(200),		(TableEntryType)2260,
	(TableEntryType)(250),		(TableEntryType)2250,
	(TableEntryType)(300),		(TableEntryType)2240,
	(TableEntryType)(400),		(TableEntryType)2220,
	(TableEntryType)(500),		(TableEntryType)2200,
	(TableEntryType)(600),		(TableEntryType)2200,
};

const static TableEntryType s_aLeadAcidTopChargeVoltageVsTempTable[] PROGMEM =
{
	// 10ths C					// mV / cell
	(TableEntryType)(-300),		(TableEntryType)2670,
	(TableEntryType)(-200),		(TableEntryType)2670,
	(TableEntryType)(-100),		(TableEntryType)2610,
	(TableEntryType)(0),		(TableEntryType)2550,
	(TableEntryType)(100),		(TableEntryType)2490,
	(TableEntryType)(200),		(TableEntryType)2430,
	(TableEntryType)(250),		(TableEntryType)2400,
	(TableEntryType)(300),		(TableEntryType)2370,
	(TableEntryType)(400),		(TableEntryType)2310,
	(TableEntryType)(500),		(TableEntryType)2250,
	(TableEntryType)(600),		(TableEntryType)2250,
};


void ThunderballConfigControls::selectBattery(int index, int bChargeAtFloat, int nTempMode)
{
	BatteryChargeProfile* pBat = BatteryChargeProfile::GetProfileFromIndex((size_t)index);

	int nTopChargeVoltage = pBat->m_nTopChargeVoltage;

	if (nTopChargeVoltage >= 48000)
	{
		// 48 Volts
		gs_pExternalDcDc->setMaxMinSetpoints(
											250 * FreqScale,			// Min V	- really a max voltage of 30V
											TrimDacMax * FreqScale,		// Max V	- really a min
											0					,		// Min I
											TrimDacMax * FreqScale);	// Max I
	}
	else if (nTopChargeVoltage >= 24000)
	{
		// 24 Volts
		gs_pExternalDcDc->setMaxMinSetpoints(
											500 * FreqScale,			// Min V	- really a max voltage of 30V
											TrimDacMax * FreqScale,		// Max V	- really a min
											0					,		// Min I
											TrimDacMax * FreqScale);	// Max I
	}
	else
	{
			// 12 Volts
		gs_pExternalDcDc->setMaxMinSetpoints(
											730 * FreqScale,			// Min V	- really a max voltage of 20V
											TrimDacMax * FreqScale,		// Max V	- really a min
											0					,		// Min I
											TrimDacMax * FreqScale);	// Max I
	}

	gs_pExternalDcDc->setDeadVoltage(pBat->m_nDeadVoltage, 4);	// 4 secs
	gs_pExternalDcDc->setFloatVoltage(pBat->m_nFloatChargeVoltage, bChargeAtFloat);
	gs_pExternalDcDc->setupTempCompensation(pBat->m_nMaxChargeTemp, pBat->m_nMinChargeTemp, 
						pBat->m_nTopChargeTempCompensation, pBat->m_nFloatChargeTempCompensation);
	// do last
	gs_pExternalDcDc->configureOutput(pBat->m_nTopChargeVoltage, 
										225000,  // max out power 
										pBat->m_nMaxChargeCurrent, 
										pBat->m_nTendingLowVoltage, 
										pBat->m_nTrickleChargeVoltage, pBat->m_nMaxTrickleCurrent, 
										pBat->m_nMinChargeVoltage);
	gs_pExternalDcDc->setTempCompensationMode(nTempMode);

#if 0
	switch (index)
	{
	case ThunderballSystemStats::Battery12VLeadAcid:
		gs_pExternalDcDc->setDeadVoltage(10500, 2);	// 2 = 30 secs
		gs_pExternalDcDc->setFloatVoltage(13650, bChargeAtFloat);
		gs_pExternalDcDc->setupTempCompensation(600, -400, -35, -12);
		// do last
		// target vout, nM
		gs_pExternalDcDc->configureOutput(14400, 225000, 25000, 12000, 11000, 10000, 0);
		break;
	case ThunderballSystemStats::Battery12VAGM:
		gs_pExternalDcDc->setDeadVoltage(10500, 2);	// 2 = 30 secs
		gs_pExternalDcDc->setFloatVoltage(13600, bChargeAtFloat);
		gs_pExternalDcDc->setupTempCompensation(600, -400, -35, -12);
		// do last
		gs_pExternalDcDc->configureOutput(14100, 225000, 25000, 12000, 11000, 5000, 0);
		break;
	case ThunderballSystemStats::Battery12VGel:
		gs_pExternalDcDc->setDeadVoltage(10500, 2);	// 2 = 30 secs
		gs_pExternalDcDc->setFloatVoltage(13600, bChargeAtFloat);
		gs_pExternalDcDc->setupTempCompensation(600, -400, -35, -12);
		// do last
		gs_pExternalDcDc->configureOutput(13500, 225000, 25000, 12200, 11000, 5000, 0);
		break;
	case ThunderballSystemStats::Battery12VLife04:
		gs_pExternalDcDc->setDeadVoltage(12000, 2);	// 2 = 30 secs
		gs_pExternalDcDc->setFloatVoltage(13500, bChargeAtFloat);
		gs_pExternalDcDc->setupTempCompensation(600, 0, -35, -12);
		// do last
		gs_pExternalDcDc->configureOutput(14300, 225000, 25000, 12500, 11000, 5000, 0);
		break;
	case ThunderballSystemStats::Battery24VLeadAcid:
		gs_pExternalDcDc->setDeadVoltage(21000, 2);	// 2 = 30 secs
		gs_pExternalDcDc->setFloatVoltage(27300, bChargeAtFloat);
		gs_pExternalDcDc->setupTempCompensation(600, -400, -35 * 2, -12 * 2);
		// do last
		gs_pExternalDcDc->configureOutput(28800, 225000, 20000, 25000, 21000, 10000, 0);
		break;
	case ThunderballSystemStats::Battery24VAGM:
		gs_pExternalDcDc->setDeadVoltage(21000, 2);	// 2 = 30 secs
		gs_pExternalDcDc->setFloatVoltage(27200, bChargeAtFloat);
		gs_pExternalDcDc->setupTempCompensation(600, -400, -35 * 2, -12 * 2);
		// do last
		gs_pExternalDcDc->configureOutput(28200, 225000, 20000, 25000, 21000, 3000, 0);
		break;
	case ThunderballSystemStats::Battery24VGel:
		gs_pExternalDcDc->setDeadVoltage(21000, 2);	// 2 = 30 secs
		gs_pExternalDcDc->setFloatVoltage(27200, bChargeAtFloat);
		gs_pExternalDcDc->setupTempCompensation(600, -400, -35 * 2, -12 * 2);
		// do last
		gs_pExternalDcDc->configureOutput(27000, 225000, 20000, 24500, 21000, 3000, 0);
		break;
	case ThunderballSystemStats::Battery24VLife04:
		gs_pExternalDcDc->setDeadVoltage(24000, 2);	// 2 = 30 secs
		gs_pExternalDcDc->setFloatVoltage(27000, bChargeAtFloat);
		gs_pExternalDcDc->setupTempCompensation(600, 0, -35 * 2, -12 * 2);
		// do last
		gs_pExternalDcDc->configureOutput(28600, 225000, 20000, 25000, 21000, 3000, 0);
		break;
	case ThunderballSystemStats::Battery48VLeadAcid:
		gs_pExternalDcDc->setDeadVoltage(42000, 2);	// 2 = 30 secs
		gs_pExternalDcDc->setFloatVoltage(54600, bChargeAtFloat);
		gs_pExternalDcDc->setupTempCompensation(600, -400, -35 * 4, -12 * 4);
		// do last
		gs_pExternalDcDc->configureOutput(57600, 225000, 20000, 50000, 42000, 2000, 0);
		break;
	case ThunderballSystemStats::Battery48VAGM:
		gs_pExternalDcDc->setDeadVoltage(42000, 2);	// 2 = 30 secs
		gs_pExternalDcDc->setFloatVoltage(54400, bChargeAtFloat);
		gs_pExternalDcDc->setupTempCompensation(600, -400, -35 * 4, -12 * 4);
		// do last
		gs_pExternalDcDc->configureOutput(56400, 225000, 20000, 50000, 42000, 2000, 0);
		break;
	case ThunderballSystemStats::Battery48VGel:
		gs_pExternalDcDc->setDeadVoltage(42000, 2);	// 2 = 30 secs
		gs_pExternalDcDc->setFloatVoltage(54400, bChargeAtFloat);
		gs_pExternalDcDc->setupTempCompensation(600, -400, -35 * 4, -12 * 4);
		// do last
		gs_pExternalDcDc->configureOutput(54000, 225000, 20000, 49000, 42000, 2000, 0);
		break;
	case ThunderballSystemStats::Battery48VLife04:
		gs_pExternalDcDc->setDeadVoltage(48000, 2);	// 2 = 30 secs
		gs_pExternalDcDc->setFloatVoltage(54000, bChargeAtFloat);
		gs_pExternalDcDc->setupTempCompensation(600, 0, -35 * 4, -12 * 4);
		// do last
		break;
		gs_pExternalDcDc->configureOutput(57200, 225000, 20000, 50000, 42000, 2000, 0);
	}
	gs_pExternalDcDc->setTempCompensationMode(nTempMode);
#endif
}

void ThunderballConfigControls::updateStats()
{
	// use averages from converters
	ThunderballSystemStats::instance()->m_FuelCellVoltage = gs_pExternalDcDc->getVoltageIn();
	ThunderballSystemStats::instance()->m_FuelCellCurrent = gs_pExternalDcDc->getCurrentIn();
	ThunderballSystemStats::instance()->m_BatteryVoltage = gs_pInternalDcDc->getVoltageOut();
	ThunderballSystemStats::instance()->m_BatteryCurrent = gs_pInternalDcDc->getCurrentOut();
	ThunderballSystemStats::instance()->m_LoadVoltage = gs_pExternalDcDc->getVoltageOut();
	ThunderballSystemStats::instance()->m_LoadCurrent = gs_pExternalDcDc->getCurrentOut();
	ThunderballSystemStats::instance()->m_RemoteBatteryVoltage = gs_pExternalDcDc->getRemoteVoltage();

	// use averages from control
	ThunderballSystemStats::instance()->m_AnodeAirFlow = ((TableCalMon*)ThunderballConfigAdc::s_SystemAdcs[ThunderballConfigAdc::ADC_CHAN_ANODE_AIR_FLOW_SENSE])->lookupValueFromAdc(gs_aFlowPwmControl[ThunderballSofcMgr::AnodeAirCtrlIndex]->getLastFeedbackAverage());
	ThunderballSystemStats::instance()->m_AnodeFuelFlow = ((TableCalMon*)ThunderballConfigAdc::s_SystemAdcs[ThunderballConfigAdc::ADC_CHAN_ANODE_FUEL_FLOW_SENSE])->lookupValueFromAdc(gs_aFlowPwmControl[ThunderballSofcMgr::AnodeFuelCtrlIndex]->getLastFeedbackAverage());
	ThunderballSystemStats::instance()->m_PBAirFlow = ((TableCalMon*)ThunderballConfigAdc::s_SystemAdcs[ThunderballConfigAdc::ADC_CHAN_PB_AIR_FLOW_SENSE])->lookupValueFromAdc(gs_aFlowPwmControl[ThunderballSofcMgr::PbAirCtrlIndex]->getLastFeedbackAverage());
	ThunderballSystemStats::instance()->m_PBFuelFlow = ((TableCalMon*)ThunderballConfigAdc::s_SystemAdcs[ThunderballConfigAdc::ADC_CHAN_PB_FUEL_FLOW_SENSE])->lookupValueFromAdc(gs_aFlowPwmControl[ThunderballSofcMgr::PbFuelCtrlIndex]->getLastFeedbackAverage());
	ThunderballSystemStats::instance()->m_CathodeAirFlow = ((TableCalMon*)ThunderballConfigAdc::s_SystemAdcs[ThunderballConfigAdc::ADC_CHAN_CATHODE_FLOW_SENSE])->lookupValueFromAdc(gs_aFlowPwmControl[ThunderballSofcMgr::CathodeAirCtrlIndex]->getLastFeedbackAverage());

}

#if 0
void ThunderballConfigControls::updateConfigFromDipSwitched(U8 bfDipMask)
{
	// default or left tag is when bit is '1' or switch is up
	enum {
		DipLedOn_OR_OffBit = 0,		// Switch 8
		DipFuelPropane_OR_OtherBit,
		DipTender_OR_ApuBit,
		DipAgm_OR_GelBit,
		DipLifePO4_OR_OtherBit,
		Dip12V_24VBit,
		DipReservedBit,
		DipColdWeatherKitBit,		// Switch 1
	};

	gs_bfDipMask = bfDipMask;

	// AGM only
	gs_pExternalDcDc->configureOutput(14100, 210000, 20000, 12000, 11000, 100000, 0);
	return;

	if ((gs_bfDipMask & (1 << Dip12V_24VBit)) == 0)
	{
		// 24 Volts
		gs_pExternalDcDc->setMaxMinSetpoints(
											500 * FreqScale,			// Min V	- really a max voltage of 30V
											TrimDacMax * FreqScale,		// Max V	- really a min
											0					,		// Min I
											TrimDacMax * FreqScale);	// Max I
		if ((gs_bfDipMask & (1 << DipLifePO4_OR_OtherBit)) == 0)
		{
			// not LifeO4
			if ((gs_bfDipMask & (1 << DipAgm_OR_GelBit)) == 0)
			{
				// Gel
				gs_pExternalDcDc->configureOutput(27000, 210000, 20000, 24500, 21000, 20000, 0);
			}
			else
			{
				// AGM
				gs_pExternalDcDc->configureOutput(28200, 210000, 20000, 25000, 21000, 20000, 0);
			}
		}
		else
		{
			// LiFeO4
			gs_pExternalDcDc->configureOutput(28600, 210000, 20000, 25000, 21000, 20000, 0);
		}

	}
	else
	{
		// 12 Volts
		gs_pExternalDcDc->setMaxMinSetpoints(
											730 * FreqScale,			// Min V	- really a max voltage of 20V
											TrimDacMax * FreqScale,		// Max V	- really a min
											0					,		// Min I
											TrimDacMax * FreqScale);	// Max I

		if ((gs_bfDipMask & (1 << DipLifePO4_OR_OtherBit)) == 0)
		{
			// not LifeO4
			if ((gs_bfDipMask & (1 << DipAgm_OR_GelBit)) == 0)
			{
				// Gel
				gs_pExternalDcDc->configureOutput(13500, 210000, 20000, 12200, 11000, 10000, 0);
			}
			else
			{
				// AGM
				gs_pExternalDcDc->configureOutput(14100, 210000, 20000, 12000, 11000, 10000, 0);
			}
		}
		else
		{
			// LiFeO4
			gs_pExternalDcDc->configureOutput(14300, 210000, 20000, 12500, 11000, 10000, 0);
		}
	}
}

#endif
