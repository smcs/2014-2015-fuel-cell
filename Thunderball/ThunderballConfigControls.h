#ifndef THUNDERBALL_CONFIG_CONTROLS_H
#define THUNDERBALL_CONFIG_CONTROLS_H

///////////////////////////////////////////////////////////////////////////////
// \file ThunderballConfigControls.h
// \brief Module containing a class based configuration script that sets up
// \brief all of the application specific controls of the RPM application.
//
//
// (C) Protonex Technology Corporation
// The Next Generation of Portable Power(TM)
// 153 Northboro Road
// Southborough, MA 01772-1034
//
// NOTICE: This file contains confidential and proprietary information of
// Protonex Technology Corporation.
///////////////////////////////////////////////////////////////////////////////

#include "Types.h"
#include "Device.h"

struct AdcMonitor;
struct BufferQueue;
struct ThunderballSofcMgr;
struct BufferQueue;
struct PruPwm;
struct ThunderballSystemStats;

/**
 * ThunderballConfigControls is the singleton class encapsulating the configuration of controls for the RPM application.
 */
struct ThunderballConfigControls
{
public:

	static void allocate(BufferQueue* pQ);

	static void initControls(BufferQueue* pQ, AdcMonitor** ppSystemAdcs, ThunderballSofcMgr* pMgr, int nRemoteSense);
	static void setupBufferQueues(BufferQueue* pSpiQ);
	static void hwInitFuelCellSwitch(void (*hwEnableFuelCell)(HwDriveMode hwDriveMode));
	static void hwInitIgniter(void (*hwEnableIgniter)(HwDriveMode hwDriveMode));
	static void hwInitFuelValve(void (*hwEnableValve)(HwDriveMode hwDriveMode));
	static void hwInitExternalBrick(void (*hwEnableBrick)(HwDriveMode hwDriveMode, S32 nNumBricks));
	static void hwInitInternalBrick(void (*hwEnableBrick)(HwDriveMode hwDriveMode, S32 nNumBricks));
	static void initBricks(AdcMonitor** ppSystemAdc, ThunderballSofcMgr* pMgrs, int nRemoteSense);
	static void hwInitUi(void (*hwEnableLeds)(U8 bfLedMask), U8 (*getBtnState)());
	static void updateConfigFromDipSwitched(U8 bfDipMask);
	static void hwInitCOSensor(void (*hwEnablePower)(HwDriveMode hwDriveMode), void (*hwEnableLowPower)(HwDriveMode hwDriveMode));
	static void selectBattery(int index, int bChargeAtFloat, int nTempMode);
	static void updateStats();
};

#endif
