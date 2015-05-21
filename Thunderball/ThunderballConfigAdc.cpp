///////////////////////////////////////////////////////////////////////////////
// ThunderballConfigAdc.cpp
// 
// (C) 2006
// Protonex Technology Corporation
// The Next Generation of Portable Power(TM)
// 153 Northboro Road
// Southborough, MA 01772-1034
//
// NOTICE: This file contains confidential and proprietary information of
// Protonex Technology Corporation.
///////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <vector>
#include <iostream>
#include <sstream>
using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

#include "ThunderballConfigAdc.h"
#include "LinuxDebug.h"

#include "AdcBankMgr.h"
#include "AdcBankMgrImpl.h"
#include "ADCMgrUtils.h"
#include "AdcMonitor.h"
#include "AppEnums.h"
#include "BufferQueue.h"
#include "EventMgr.h"
#include "ObjectList.h"
#include "TempMon.h"
#include "TimerMgr.h"
#include "LinuxBufferQueue.h"
#include "AD7888_12BitAdcDevice.h"
#include "GPIOManager.h"
#include "GPIOConst.h"
#include "RawDevices.h"
#include "SegmentedLookupTable.h"
#include "TableCalMon.h"
#include "ThunderballSofcMgr.h"
#include "LinearCalMon.h"

enum {
	TB_ADC_BANK,
	TB_ADC2_BANK,
	TB_ADC3_BANK,

	NumAdcBanks
};

AdcMonitor*	ThunderballConfigAdc::s_SystemAdcs[ThunderballConfigAdc::MAX_ADC_CHANNELS];

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
static SegmentedLinearSlopeLookupTable s_ThermistorLookupTable[] = 
{
//  10thsOfdegreesC = (m * 2560 * (ADC - ADCi)) / 256 + (Ti * 10)
//  ADCi, (C * 10), (m * 2560)

    {	3930,	-400,	-203 },
	{	3804,	-300,	-135 },
	{	3615,	-200,	-98 },
	{	3355,	-100,	-78 },
	{	3025,	0,		-68 },
    {	2646,	100,	-64 },
    {	2246,	200,	-66 },
    {	1857,	300,	-73 },
    {	1505,	400,	-85 },
    {	1203,	500,	-103 },
    {	 954,	600,	-129 },
    {	 755,	700,	-163 },
    {	 598,	800,	-208 },
	{	 475,	900,	-269 },
    {	 380,	1000,	-346 },
	{	 306,	1100,	-441 },
    {	 248,	1200,	-569 },
	{	 203,	1300,	-569 },
};

const static TableEntryType s_aPressureVsTempTable[] PROGMEM =
{
	// 10ths PSI						// Temp 10ths C
	(TableEntryType)(36),			(TableEntryType)-400,
	(TableEntryType)(80),			(TableEntryType)-344,
	(TableEntryType)(135),		(TableEntryType)-289,
	(TableEntryType)(200),		(TableEntryType)-233,
	(TableEntryType)(280),		(TableEntryType)-178,
	(TableEntryType)(370),		(TableEntryType)-122,
	(TableEntryType)(470),		(TableEntryType)-67,
	(TableEntryType)(580),		(TableEntryType)-11,	// actual 30 PSI
	(TableEntryType)(720),		(TableEntryType)44,
	(TableEntryType)(860),		(TableEntryType)100,
	(TableEntryType)(1020),		(TableEntryType)156,
	(TableEntryType)(1270),		(TableEntryType)211,
	(TableEntryType)(1400),		(TableEntryType)267,
	(TableEntryType)(1650),		(TableEntryType)322,
	(TableEntryType)(1960),		(TableEntryType)378,
	(TableEntryType)(2200),		(TableEntryType)433,
};

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

enum 
{
	LT6654ReferenceMilliVolts = 2500,
	AD7888BitCount = 12,
};

static S32 Voltage46p3To1AdcCalc(BaseMonitor *pThis, S32 nADCSteps, S32 nCurrentValue)
{
	S32 nValue = (S32)nADCSteps * 2348 / 100;
	return (S32)nValue - nCurrentValue;

	//return ADCMgrUtils::smooth(nValue - nCurrentValue, 50);
}

static S32 Voltage25To1AdcCalc(BaseMonitor *pThis, S32 nADCSteps, S32 nCurrentValue)
{
	S32 nValue = (S32)nADCSteps * 25000 * LT6654ReferenceMilliVolts / (1 << AD7888BitCount);

	return ADCMgrUtils::smooth(nValue - nCurrentValue, 50);
}

static S32 Current50AmpsPerVoltAdcCalc(BaseMonitor *pThis, S32 nADCSteps, S32 nCurrentValue)
{
	S32 nValue = (S32)nADCSteps * 3052 / 100;//6104 / 1000;

	return ADCMgrUtils::smooth(nValue - nCurrentValue, 50);
}

static S32 Current63AmpsRangeAdcCalc(BaseMonitor *pThis, S32 nADCSteps, S32 nCurrentValue)
{
	S32 nValue = ((S32)nADCSteps - 2048) * 1538 / 100;

	return ADCMgrUtils::smooth(nValue - nCurrentValue, 50);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Calculate pressure from a AD623 comparator - this gives us 0.007 PSI per STEP
static S32 AdcFuelPmon2TestCalculate(BaseMonitor *pThis, S32 nADCSteps, S32 nCurrentValue)
{
	//0.0618 PSI per step
	S32 nNewValue = (nADCSteps * 62); 

	return ADCMgrUtils::smooth(nNewValue - nCurrentValue, 10);
}

static S32 AdcNoScaleCalculate(BaseMonitor *pThis, S32 nADCSteps, S32 nCurrentValue)
{
	return (S32)nADCSteps - nCurrentValue;
}   

AD7888_12BitAdcDevice* gs_pFlowBank;
AD7888_12BitAdcDevice* gs_pPowerBank;
AD7888_12BitAdcDevice* gs_pExtraBank;
static RawInputOnlyDevice*				gs_pThermoCouples[4];

//////////////////////////////////////////////////////////
// Pwr Board Adc Bank 1
static AdcBankElementConfig s_AdcBank[8] =
{
	{ 0, ThunderballConfigAdc::ADC_CHAN_FC_I_SENSE, AppEnumFuelCellCurrentMonitor, NULL },
	{ 1, ThunderballConfigAdc::ADC_CHAN_FC_V_SENSE, AppEnumFuelCellVoltageMonitor, NULL },
	{ 2, ThunderballConfigAdc::ADC_CHAN_OUTPUT_V_SENSE, AppEnumVoltageMonitor, NULL },	
	{ 3, ThunderballConfigAdc::ADC_CHAN_OUTPUT_I_SENSE, AppEnumCurrentMonitor, NULL  },
	{ 4, ThunderballConfigAdc::ADC_CHAN_BATTERY_I_SENSE, AppEnumBatteryCurrentMonitor, NULL },
	{ 5, ThunderballConfigAdc::ADC_CHAN_BATTERY_V_SENSE, AppEnumBatteryVoltageMonitor, NULL },	
	{ 6, ThunderballConfigAdc::ADC_CHAN_REMOTE_V_SENSE, AppEnumExternalVoltageMonitor, NULL },	
	{ 7, ThunderballConfigAdc::ADC_CHAN_ADC_SPARE7, AppEnumIllegal, NULL  },
};

static AdcBankElementConfig s_Adc2Bank[8] =
{
	{ 0, ThunderballConfigAdc::ADC_CHAN_PRESSURE_SENSE, AppEnumFuelPressureMonitor, AdcFuelPmon2TestCalculate },
	{ 1, ThunderballConfigAdc::ADC_CHAN_PB_AIR_FLOW_SENSE, AppEnumPBAirFlowMonitor, AdcNoScaleCalculate  },
	{ 2, ThunderballConfigAdc::ADC_CHAN_PB_FUEL_FLOW_SENSE, AppEnumPBFuelFlowMonitor, AdcNoScaleCalculate },	
	{ 3, ThunderballConfigAdc::ADC_CHAN_CATHODE_FLOW_SENSE, AppEnumCathodeAirFlowMonitor, AdcNoScaleCalculate },	
	{ 4, ThunderballConfigAdc::ADC_CHAN_ANODE_FUEL_FLOW_SENSE, AppEnumAnodeFuelFlowMonitor, AdcNoScaleCalculate },
	{ 5, ThunderballConfigAdc::ADC_CHAN_ANODE_AIR_FLOW_SENSE, AppEnumAnodeAirFlowMonitor, AdcNoScaleCalculate },	
	{ 6, ThunderballConfigAdc::ADC_CHAN_PROPANE_DETECT_SENSE, AppEnumPropaneSense, AdcNoScaleCalculate },	
	{ 7, ThunderballConfigAdc::ADC_CHAN_CO_DETECT_SENSE, AppEnumCOSense, AdcNoScaleCalculate  },
};

static AdcBankElementConfig s_Adc3Bank[8] =
{
	{ 0, ThunderballConfigAdc::ADC_CHAN_ADC3_SPARE0, AppEnumIllegal, NULL },
	{ 1, ThunderballConfigAdc::ADC_CHAN_BOARD_TEMP, AppEnumIllegal, NULL },	
	{ 2, ThunderballConfigAdc::ADC_CHAN_IMON_PS, AppEnumIllegal, NULL },	
	{ 3, ThunderballConfigAdc::ADC_CHAN_IMON_PS2, AppEnumIllegal, NULL  },
	{ 4, ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM2, AppEnumIllegal, NULL },
	{ 5, ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM1, AppEnumIllegal, NULL },	
	{ 6, ThunderballConfigAdc::ADC_CHAN_ADC3_SPARE6, AppEnumIllegal, NULL },	
	{ 7, ThunderballConfigAdc::ADC_CHAN_ADC3_SPARE7, AppEnumIllegal, NULL  },
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const static AdcBankConfigConst s_AdcBanks[NumAdcBanks] =
{
	{	(MemoryDevice**)&gs_pPowerBank, 
		8,				// num ADC values
		8,				// number to read including gaps
		2,								// data size of each value (in bytes)
		0,								// address offset of device to start read
		0,								// no right shift
		FALSE,							// NOT little endian
		s_AdcBank,				// Array of adc mappings on bank
		500,							// Sample Rate in tenths of Hz	(every 20ms- 1,000/500 * 10)
		0x0fff,							// valid mask
	},	

	{	(MemoryDevice**)&gs_pFlowBank, 
		8,				// num ADC values
		8,				// number to read including gaps
		2,								// data size of each value (in bytes)
		0,								// address offset of device to start read
		0,								// no right shift
		FALSE,							// NOT little endian
		s_Adc2Bank,				// Array of adc mappings on bank
		800,							// Sample Rate in tenths of Hz 	(every 20ms- 1,000/500 * 10)
		0x0fff,							// valid mask
	},

	{	(MemoryDevice**)&gs_pExtraBank, 
		8,				// num ADC values
		8,				// number to read including gaps
		2,								// data size of each value (in bytes)
		0,								// address offset of device to start read
		0,								// no right shift
		FALSE,							// NOT little endian
		s_Adc3Bank,					// Array of adc mappings on bank
		200,							// Sample Rate in tenths of Hz	(every 20ms- 1,000/500 * 10)
		0xffff,							// valid mask
	},	
};

#include "DebugMemoryMgr.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "TextFile.h"

static const char* gs_pGlobalAppPath;

static SegmentedLookupTable* CreateLookupFromFile(const char* fileName, int maxEntries)
{
	LinuxDebugObject dbo(LinuxDebug::DebugModule::DM_APPLICATION, LinuxDebug::DebugFlag::DF_FILE);

	SegmentedLookupTable* pTbl;
	pTbl = new SegmentedLookupTable(maxEntries);

	for (int i = 0; i < 2;i++)
	{
		std::string name(fileName);
		if (gs_pGlobalAppPath)
		{
			std::string path(gs_pGlobalAppPath);
			if (path.at(path.length() - 1) != '/')
			{
				path = path + "/";
				if (i)
				{
					// use global cal files
					path = path + "package/";
				}
			}
			name = path + name;
		}

		dbo << "Loading calibration file: " << name << dbo.endline;
		IntegerDelimFile tf(name.c_str());

		std::string line;
		int lineId = 0;
		while(tf.good())
		{
			std::vector<int> data = tf.nextData(' ');
			if (data.size() == 0) break;
			lineId++;

			if (data.size() > 1)
				pTbl->addDataPoint((TableEntryType)data[0], (TableEntryType)data[1]);
		}
		if (lineId)
			break;
		else
			dbo << " Failed to load: " << dbo.endline;

	}
	return pTbl;
}

enum {
	AnodeAirCalId,
	CathodeAirCalId,
	PBAirCalId,
	AnodeFuelCalId,
	PBFuelCalId,
	ThermocoupleCalId,
	AmbientThermistorCalId,
	RemoteThermistorCalId,
};

const char* s_CalFileNames[] =
{
	"Calibration/AnodeAir.cal",
	"Calibration/CathodeAir.cal",
	"Calibration/PBAir.cal",
	"Calibration/AnodeFuel.cal",
	"Calibration/PBFuel.cal",
	"Calibration/Thermocouple.cal",
	"Calibration/AmbientThermistor.cal",
	"Calibration/RemoteThermistor.cal",
};

static void WriteLookupToFile(SegmentedLookupTable* pTable, void* pUserData)
{
	printf("Writing calibration file: %s\n", s_CalFileNames[(int)pUserData]);
}

void ThunderballConfigAdc::initSpiBankAdcs(BufferQueue* pQ, AdcMonitor** ppSysAdcs, ThunderballSofcMgr* pMgr, const char* pAppPath, int nFuelTempAdc)
{
	gs_pGlobalAppPath = pAppPath;

	DebugMemoryMgr::instance()->appSetup(AppEnumDebugLogMgr, ObjectList::addObject, 32);

	gs_pPowerBank = new AD7888_12BitAdcDevice(pQ, 0);
	gs_pPowerBank->enableExternalReference(TRUE);
		gs_pPowerBank->m_nMaxClockSpeed = 100000;
		gs_pPowerBank->m_bfSpiMode = 3;
		gs_pPowerBank->m_fpSetChipSelect = 0;	
		gs_pPowerBank->m_csPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_26", GPIO::HIGH);
	DebugMemoryMgr::instance()->addDevice(gs_pPowerBank, "Power Bank");

	gs_pFlowBank = new AD7888_12BitAdcDevice(pQ, 0);
	gs_pFlowBank->enableExternalReference(TRUE);
		gs_pFlowBank->m_nMaxClockSpeed = 100000;
		gs_pFlowBank->m_bfSpiMode = 3;
		gs_pFlowBank->m_fpSetChipSelect = 0;
		gs_pFlowBank->m_csPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_38", GPIO::HIGH);
	DebugMemoryMgr::instance()->addDevice(gs_pFlowBank, "Flow Bank");

	gs_pExtraBank = new AD7888_12BitAdcDevice(pQ, 1);
	gs_pExtraBank->enableExternalReference(TRUE);
		gs_pExtraBank->m_nMaxClockSpeed = 100000;
		gs_pExtraBank->m_bfSpiMode = 3;
		gs_pExtraBank->m_fpSetChipSelect = 0;	
		gs_pExtraBank->m_csPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_18", GPIO::HIGH);
	DebugMemoryMgr::instance()->addDevice(gs_pExtraBank, "Extra Bank");

	gs_pThermoCouples[ThunderballSofcMgr::TcRadiated] = new RawInputOnlyDevice(pQ, 0);
		gs_pThermoCouples[ThunderballSofcMgr::TcRadiated]->m_nMaxClockSpeed = 100000;
		gs_pThermoCouples[ThunderballSofcMgr::TcRadiated]->m_bfSpiMode = 3;
		gs_pThermoCouples[ThunderballSofcMgr::TcRadiated]->m_fpSetChipSelect = 0;
		gs_pThermoCouples[ThunderballSofcMgr::TcRadiated]->m_csPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_35", GPIO::HIGH);
	DebugMemoryMgr::instance()->addDevice(gs_pThermoCouples[ThunderballSofcMgr::TcRadiated], "TC 1 (Rad)");

	gs_pThermoCouples[ThunderballSofcMgr::TcCpox] = new RawInputOnlyDevice(pQ, 0);
		gs_pThermoCouples[ThunderballSofcMgr::TcCpox]->m_nMaxClockSpeed = 100000;
		gs_pThermoCouples[ThunderballSofcMgr::TcCpox]->m_bfSpiMode = 3;
		gs_pThermoCouples[ThunderballSofcMgr::TcCpox]->m_fpSetChipSelect = 0;
		gs_pThermoCouples[ThunderballSofcMgr::TcCpox]->m_csPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_36", GPIO::HIGH);
	DebugMemoryMgr::instance()->addDevice(gs_pThermoCouples[ThunderballSofcMgr::TcCpox], "TC 2 (CPOX)");

	gs_pThermoCouples[ThunderballSofcMgr::TcBlock] = new RawInputOnlyDevice(pQ, 0);
		gs_pThermoCouples[ThunderballSofcMgr::TcBlock]->m_nMaxClockSpeed = 100000;
		gs_pThermoCouples[ThunderballSofcMgr::TcBlock]->m_bfSpiMode = 3;
		gs_pThermoCouples[ThunderballSofcMgr::TcBlock]->m_fpSetChipSelect = 0;
		gs_pThermoCouples[ThunderballSofcMgr::TcBlock]->m_csPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_37", GPIO::HIGH);
	DebugMemoryMgr::instance()->addDevice(gs_pThermoCouples[ThunderballSofcMgr::TcBlock], "TC 3 (Other)");

	int foo;
	//foo = GPIO::GPIOManager::getInstance()->setupOutputPin("P9_17", GPIO::HIGH);	// calendar cloxk
	foo = GPIO::GPIOManager::getInstance()->setupOutputPin("P8_44", GPIO::HIGH);	// U16 DAC

	U16 nTempTblSize = sizeof(s_ThermistorLookupTable) / sizeof(SegmentedLinearSlopeLookupTable);
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_BOARD_TEMP] = new TempMon(ThunderballConfigAdc::ADC_CHAN_BOARD_TEMP, s_ThermistorLookupTable, nTempTblSize);	
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_BOARD_TEMP]->appSetup(AppEnumBoardTempMonitor, ObjectList::addObject, NULL);

	TableCalMon* pMon;
#if 1
	pMon = new TableCalMon(); 
	pMon->hwInit(ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM1, CreateLookupFromFile(s_CalFileNames[AmbientThermistorCalId], 20));
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM1] = pMon;
	pMon->setupWriteFunction((void*)AmbientThermistorCalId, WriteLookupToFile);
#else
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM1] = new TempMon(ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM1, s_ThermistorLookupTable, nTempTblSize);	
#endif	
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM1]->appSetup(AppCaseTempMonitor, ObjectList::addObject, NULL);

#if 1
	pMon = new TableCalMon(); 
	pMon->hwInit(ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM2, CreateLookupFromFile(s_CalFileNames[RemoteThermistorCalId], 20));
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM2] = pMon;
	pMon->setupWriteFunction((void*)RemoteThermistorCalId, WriteLookupToFile);
#else
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM2] = new TempMon(ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM2, s_ThermistorLookupTable, nTempTblSize);	
#endif
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM2]->appSetup(AppEnumTemperatureMonitor, ObjectList::addObject, NULL);

	pMon = new TableCalMon(); 
	pMon->hwInit(ThunderballConfigAdc::ADC_CHAN_ANODE_AIR_FLOW_SENSE, CreateLookupFromFile(s_CalFileNames[AnodeAirCalId], 100));
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_ANODE_AIR_FLOW_SENSE] = pMon;
	pMon->setupWriteFunction((void*)AnodeAirCalId, WriteLookupToFile);

	pMon = new TableCalMon(); 
	pMon->hwInit(ThunderballConfigAdc::ADC_CHAN_CATHODE_FLOW_SENSE, CreateLookupFromFile(s_CalFileNames[CathodeAirCalId], 100));
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_CATHODE_FLOW_SENSE] = pMon;
	pMon->setupWriteFunction((void*)CathodeAirCalId, WriteLookupToFile);

	pMon = new TableCalMon(); 
	pMon->hwInit(ThunderballConfigAdc::ADC_CHAN_PB_AIR_FLOW_SENSE, CreateLookupFromFile(s_CalFileNames[PBAirCalId], 100));
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_PB_AIR_FLOW_SENSE] = pMon;
	pMon->setupWriteFunction((void*)PBAirCalId, WriteLookupToFile);

	pMon = new TableCalMon(); 
	pMon->hwInit(ThunderballConfigAdc::ADC_CHAN_ANODE_FUEL_FLOW_SENSE, CreateLookupFromFile(s_CalFileNames[AnodeFuelCalId], 100));
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_ANODE_FUEL_FLOW_SENSE] = pMon;
	pMon->setupWriteFunction((void*)AnodeFuelCalId, WriteLookupToFile);

	pMon = new TableCalMon(); 
	pMon->hwInit(ThunderballConfigAdc::ADC_CHAN_PB_FUEL_FLOW_SENSE, CreateLookupFromFile(s_CalFileNames[PBFuelCalId], 100));
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_PB_FUEL_FLOW_SENSE] = pMon;
	pMon->setupWriteFunction((void*)PBFuelCalId, WriteLookupToFile);

	SegmentedLookupTable* pTbl = CreateLookupFromFile(s_CalFileNames[ThermocoupleCalId], 100);
	ThunderballSofcMgr::setupWriteFunction((void*)ThermocoupleCalId, WriteLookupToFile);
	ThunderballSofcMgr::setThermoCoupleCalibrationTable(pTbl);

	pMgr->hwInitTCs(gs_pThermoCouples, 3, 4);

	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_FC_V_SENSE] = new LinearCalMon();
	((LinearCalMon*)ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_FC_V_SENSE])->hwInit(ThunderballConfigAdc::ADC_CHAN_FC_V_SENSE, 0, 0, 1000, 23480);
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_OUTPUT_V_SENSE] = new LinearCalMon();
	((LinearCalMon*)ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_OUTPUT_V_SENSE])->hwInit(ThunderballConfigAdc::ADC_CHAN_OUTPUT_V_SENSE, 0, 0, 1000, 23480);
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_BATTERY_V_SENSE] = new LinearCalMon();
	((LinearCalMon*)ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_BATTERY_V_SENSE])->hwInit(ThunderballConfigAdc::ADC_CHAN_BATTERY_V_SENSE, 0, 0, 1000, 23480);

	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_REMOTE_V_SENSE] = new LinearCalMon();
	((LinearCalMon*)ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_REMOTE_V_SENSE])->hwInit(ThunderballConfigAdc::ADC_CHAN_REMOTE_V_SENSE, 0, 0, 1000, 15259);

	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_FC_I_SENSE] = new LinearCalMon();
	((LinearCalMon*)ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_FC_I_SENSE])->hwInit(ThunderballConfigAdc::ADC_CHAN_FC_I_SENSE, 0, 0, 600, 18175);
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_OUTPUT_I_SENSE] = new LinearCalMon();
	((LinearCalMon*)ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_OUTPUT_I_SENSE])->hwInit(ThunderballConfigAdc::ADC_CHAN_OUTPUT_I_SENSE, 1000, -15633, 3000, 14167);
	ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_BATTERY_I_SENSE] = new LinearCalMon();
	((LinearCalMon*)ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_BATTERY_I_SENSE])->hwInit(ThunderballConfigAdc::ADC_CHAN_BATTERY_I_SENSE, 1500, -3122, 2500, 2978);

	pTbl = new SegmentedLookupTable(sizeof(s_aPressureVsTempTable) / sizeof(TableEntryType) / 2);
	pTbl->reset();
	pTbl->addPgmEntries((TableEntryPgmType*)s_aPressureVsTempTable, sizeof(s_aPressureVsTempTable) / sizeof(TableEntryType));
	pMgr->setPressureTable(pTbl);

	for(int i = 0; i < NumAdcBanks; i++)
	{
		AdcBankConfig* pCfg = AdcBankMgr::addBank(&s_AdcBanks[i]);
	}
	// connect up manager to actual monitors
	AdcBankMgr::connectSystemAdcs(ppSysAdcs, ThunderballConfigAdc::MAX_ADC_CHANNELS);
	// .. and run the autoconfig to create monitors if configured in bank data, but not yet created
	AdcBankMgr::autoConfigMonitors();

	// was half ref monitor
	//ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_ADC_SPARE7]->setRawFilterDisable(FALSE);

	pMgr->hwInitBoardTempAdc(ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_BOARD_TEMP], 700);
	pMgr->hwInitStackCanTempAdc(1500);
	pMgr->hwInitPressureAdc(ppSysAdcs[ThunderballConfigAdc::ADC_CHAN_PRESSURE_SENSE], (TableCalMon*)ppSysAdcs[nFuelTempAdc]);

#ifdef NEED_PRESSURE_AVERAGING
	AveragerCtrl* pAveCtrl = new AveragerCtrl();
	Averager* pPresAverager = new Averager();
	pPresAverager->setupSampleWindow(2000, Averager::AveModeNormal);
	pPresAverager->setupDataHandler(SpartaAirPumpCtrl::handleAverageEvent, 
							  this, 
							  SpartaAirPumpCtrl::AirPumpTachFeedback);
	pAveCtrl->addAverager(pPresAverager);
	s_ppSystemSpiAdcs[ThunderballConfigAdc::ADC_CHAN_PRESSURE_SENSE]->setControlToUpdate(pAveCtrl, 0);
#endif
}

