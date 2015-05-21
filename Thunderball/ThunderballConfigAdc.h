#ifndef THUNDERBALL_CONFIG_ADC_H
#define THUNDERBALL_CONFIG_ADC_H

///////////////////////////////////////////////////////////////////////////////
// ThunderballConfigAdc.h
// 
// (C) 2013
// Protonex Technology Corporation
// The Next Generation of Portable Power(TM)
// 153 Northboro Road
// Southborough, MA 01772-1034
//
// NOTICE: This file contains confidential and proprietary information of
// Protonex Technology Corporation.
///////////////////////////////////////////////////////////////////////////////

#include "Types.h"

struct AdcMonitor;
struct BufferQueue;
struct QueueBuffer;
struct ThunderballSofcMgr;
class TcpAdcInterface;
struct ThunderballSofcMgr;
struct BufferQueue;

struct ThunderballConfigAdc 
{
	enum
	{

		// ADC Bank
		ADC_CHAN_FC_I_SENSE = 0,
		ADC_CHAN_FC_V_SENSE,
		ADC_CHAN_OUTPUT_V_SENSE,
		ADC_CHAN_OUTPUT_I_SENSE,
		ADC_CHAN_BATTERY_I_SENSE,
		ADC_CHAN_BATTERY_V_SENSE,
		ADC_CHAN_REMOTE_V_SENSE,
		ADC_CHAN_ADC_SPARE7,

		// ADC2 Bank
		ADC_CHAN_PRESSURE_SENSE,
		ADC_CHAN_PB_AIR_FLOW_SENSE,			// Switched w/ Cathode  
		ADC_CHAN_PB_FUEL_FLOW_SENSE,
		ADC_CHAN_CATHODE_FLOW_SENSE,		// relative to schematic
		ADC_CHAN_ANODE_FUEL_FLOW_SENSE,
		ADC_CHAN_ANODE_AIR_FLOW_SENSE,
		ADC_CHAN_PROPANE_DETECT_SENSE,		// switched with below at rev 9
		ADC_CHAN_CO_DETECT_SENSE,			// see above

		// ADC3 Bank
		ADC_CHAN_ADC3_SPARE0,
		ADC_CHAN_BOARD_TEMP,
		ADC_CHAN_IMON_PS,
		ADC_CHAN_IMON_PS2,
		ADC_CHAN_EXTERN_THERM2,
		ADC_CHAN_EXTERN_THERM1,
		ADC_CHAN_ADC3_SPARE6,
		ADC_CHAN_ADC3_SPARE7,

		ADC_CHAN_CPOX_TEMP,
		ADC_CHAN_BLOCK_TEMP,
		ADC_CHAN_RADIATED_TEMP,
		ADC_CHAN_EXTRA_TEMP,

		MAX_ADC_CHANNELS,
	};

	static void initSpiBankAdcs(BufferQueue* pQ, AdcMonitor** ppSysAdcs, ThunderballSofcMgr* pMgr, const char* pAppPath, int nFuelTempAdc);

	static AdcMonitor*	s_SystemAdcs[ThunderballConfigAdc::MAX_ADC_CHANNELS];
};

#endif
