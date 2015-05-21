#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <array>
#include <iomanip>      
#include <string.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <assert.h>

#include <linux/types.h>
using namespace std;

//#include "MicroGridUI.h"
#include "UiPage.h"
#include "ThunderballUI.h"
#include "ThunderballUiPages.h"
#include "ThunderballSystemStats.h"
#include "TextFile.h"
#include "ArgParser.h"
#include "ThunderballSofcMgr.h"
#include "ThunderballDcDc.h"
#include "ThunderballUI.h"
#include "TcpDataInterface.h"

static ThunderballSystemStats* gs_pSystemStats;
static ThunderballUI* m_pMainProc;
static int gs_nBatteryType;
static int gs_nOperatingMode;
static int gs_nTempMode;
static int gs_nFuelTempMode;
static char gs_aUserPassword[32];
static int gs_nUsbOpMode;
static int gs_nUpgradeMode;
static int gs_nRemoteUiMode;
static bool gs_bShowInternals;

std::string FormatIt(int num, int scale, size_t maxWidth)
{
	std::string res;
	std::ostringstream ostr;

	if (scale > 1)
	{
		int whole = num / scale;
		if (whole != 0)
			ostr << whole;
		else
		{
			if (num < 0)
				ostr << "-";
			ostr << whole;
		}
		ostr << ".";
		ostr << std::setw(scale) << std::setfill('0') << (num % scale);
	}
	else
	{
		ostr << num;	
	}

	res = ostr.str();
	while (res.size() < maxWidth)
		res = " " + res;
	if (res.size() > maxWidth)
		res = res.substr(0, maxWidth);

	return res;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
class UiPageMain : public UiPage
{
public:
	UiPageMain(S16 nIndex, S16 nLinkedScreen) : UiPage(nIndex, nLinkedScreen, -1)
	{
		m_ShowCursor = FALSE;
		m_nViewId = 0;
		m_nTickCount = 0;
	}
	
	virtual void reset()
	{
		m_nViewId = 0;
		m_nTickCount = 0;
		m_nWarningIndex = 0;
	}

	S16 HandleTimeout(S16* nNextPagesPrevious)
	{
		m_nTickCount++;
		if (m_nTickCount >= 4)
		{
			m_nTickCount = 0;

			m_nViewId++;
			if (m_nViewId > 1) m_nViewId = 0;

			refresh();
		}
		return -1;
	}

	virtual void refresh()
	{
		  
		//|                    | + |                   | + |                   |
		//  - 24 V Lead Acid -   +   -   Tending     -   +  - Active  Tending -  
		// Charge: [-----|    ]
		// Fuel:   [-------|  ]
		// Power:  [----------]

		////////////////////////  Idle
		//  - 24 V Lead Acid -
		// Charge: [-----|    ]
		// Fuel:   [-------|  ]
		//  Hit button to start

		BatteryChargeProfile* pBat = BatteryChargeProfile::GetProfileFromIndex((size_t)gs_nBatteryType);
		assert(pBat);

		if (!m_nViewId)
		{
			snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), " -%s- ", pBat->m_pBatteryName);
		}

		if (gs_pSystemStats->m_bfWarningMask)
		{
			while (((1 << m_nWarningIndex) & gs_pSystemStats->m_bfWarningMask) == 0)
			{
				m_nWarningIndex++;
				if (m_nWarningIndex >= ThunderballSystemStats::NumWarnings)
				{
					m_nWarningIndex = 0;
				}
			}
			if (!m_nViewId)
				snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "**%s", ThunderballSystemStats::GetWarningString(m_nWarningIndex));
			else
				snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "  %s", ThunderballSystemStats::GetWarningString(m_nWarningIndex));
		}
		else
		{
			m_nWarningIndex = 0;
		}

		{
			if ((gs_pSystemStats->m_nErrorState == ThunderballSofcMgr::SystemNoError) &&
				(gs_pSystemStats->m_nTendingState == ThunderballSofcMgr::SystemTending))
			{
				if ((gs_pSystemStats->m_nDeviceState == ThunderballSofcMgr::SystemOff) ||
					(gs_pSystemStats->m_nDeviceState > ThunderballSofcMgr::SystemActive))
				{
					if (m_nViewId)
						snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), " -    Tending     - ");
					if (gs_pSystemStats->m_bfWarningMask == 0)
						snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "Battery:  %2d.%d V", gs_pSystemStats->m_LoadVoltage / 1000, (gs_pSystemStats->m_LoadVoltage % 1000) / 100);
				}
				else
				{
					if (m_nViewId)
						snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "- Tending / Active -");

					if (gs_pSystemStats->m_bfWarningMask == 0)
					{
						if (gs_pSystemStats->m_nDeviceState < ThunderballSofcMgr::WaitForAnodeFuel4)
						{
							snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "Warming:[          ]");
							int meas = gs_pSystemStats->m_nRadiatedTemp;
							if (meas < 0) meas = 0;
							if (meas > 6500) meas = 6500;
							int percent = 10 - (6500 - meas)  / 650;
							int i = 0;
							for(; i < percent - 1; i++)
								m_Lines[3].m_LineText[9 + i] = '-';
							m_Lines[3].m_LineText[9 + i] = '|';
						}
						else
						{
							snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "Power:  [          ]");

							int meas;
							meas = gs_pSystemStats->loadPower();
							if (meas < 0) meas = 0;
							if (meas > 220000) meas = 220000;
							int percent = 10 - (220000 - meas)  / 22000;
							int i = 0;
							for(; i < percent - 1; i++)
								m_Lines[3].m_LineText[9 + i] = '-';
							m_Lines[3].m_LineText[9 + i] = '|';
						}
					}
				}
			}
			else
			{
				if (gs_pSystemStats->m_nErrorState != ThunderballSofcMgr::SystemNoError)
				{
					if (m_nViewId)
					{
						snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), " - Stopped: Error - ");
						snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "!! %s", ThunderballSystemStats::GetErrorString(gs_pSystemStats->m_nErrorState));
					}
					else
					{
						snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "   %s", ThunderballSystemStats::GetErrorString(gs_pSystemStats->m_nErrorState));
					}
				}
				else
				{
					if (m_nViewId)
						snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "- Tending Inactive -");

					if (gs_pSystemStats->m_bfWarningMask == 0)
						snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), " Hit button to start");
				}
			}
		}

		{
			snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[1].m_LineText), "Charge: [          ]");

			int percent;
			int meas = gs_pSystemStats->m_nLoadBatterySOC;
			if (meas < 0) meas = 0;
			if (meas > 100) meas = 100;
			percent = 10 - (100 - meas)  / 10;

			int i = 0;
			for(; i < percent - 1; i++)
				m_Lines[1].m_LineText[9 + i] = '-';
			m_Lines[1].m_LineText[9 + i] = '|';
		}

		{
			snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[2].m_LineText), "Fuel:   [          ]");

			int percent;
			int meas = gs_pSystemStats->m_nFuelCapcityPercent;
			if (meas < 0) meas = 0;
			if (meas > 100) meas = 100;
			percent = 10 - (100 - meas)  / 10;

			int i = 0;
			for(; i < percent - 1; i++)
				m_Lines[2].m_LineText[9 + i] = '-';
			m_Lines[2].m_LineText[9 + i] = '|';
		}
#if 0
		if ((gs_pSystemStats->m_nDeviceState == ThunderballSofcMgr::SystemOff) ||
			(gs_pSystemStats->m_nDeviceState == ThunderballSofcMgr::SystemShutdownAnodeOff))
		{
			if ((gs_pSystemStats->m_nErrorState == ThunderballSofcMgr::SystemNoError) &&
				(gs_pSystemStats->m_nTendingState == ThunderballSofcMgr::SystemTending))
			{
				snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[3].m_LineText), "State:  Tending");
				snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[3].m_LineText), "Battery:  %2d.%d V", gs_pSystemStats->m_LoadVoltage / 1000, (gs_pSystemStats->m_LoadVoltage % 1000) / 100);
				if (gs_pSystemStats->m_nShutdownReason != ThunderballSofcMgr::NoShutdown)
				{
					snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "%s", ThunderballSystemStats::GetShutdownString(gs_pSystemStats->m_nShutdownReason));
				}
				else
				{
					memset(m_Lines[2].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
				}
			}
			else
			{
				if (gs_pSystemStats->m_nErrorState != ThunderballSofcMgr::SystemNoError)
				{
					snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[3].m_LineText), "State:  Stopped");
					snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[3].m_LineText), "Error shutdown:");
					snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "%s", ThunderballSystemStats::GetErrorString(gs_pSystemStats->m_nErrorState));
				}
				else
				{
					snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[3].m_LineText), "State:  Idle");
					snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[3].m_LineText), "    Press button");
					snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "  to start tending");
				}
			}
		}
		else if (gs_pSystemStats->m_nDeviceState < ThunderballSofcMgr::WaitForAnodeFuel4)
		{
			snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[3].m_LineText), "State:  Warming");
			int min;
			if (gs_pSystemStats->m_nRadiatedTemp < 5000)
				min = (5000 - gs_pSystemStats->m_nRadiatedTemp) / 600 + 10;
			else
				min = (6500 - gs_pSystemStats->m_nRadiatedTemp) / 150;
			if (!min) min = 1;

			snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[1].m_LineText), "Time to Power: %2d m", (6500 - gs_pSystemStats->m_nRadiatedTemp) / 5 / 60);
			if (gs_pSystemStats->m_nDeviceState > ThunderballSofcMgr::WaitForAnodeFuel)
			{
				snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[1].m_LineText), "* Fuel cell active *");
			}
			else
			{
				memset(m_Lines[3].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
			}
		}
		else if (gs_pSystemStats->m_nDeviceState < ThunderballSofcMgr::SystemShutdown)
		{
			S32 nPower = 0 - gs_pSystemStats->loadPower();
			nPower /= 1000;
			snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[3].m_LineText), "State:  Active");
			snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[1].m_LineText), "Out Power: %3d W", nPower);
			snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[1].m_LineText), "* Fuel cell active *");
		}
		else if (gs_pSystemStats->m_nDeviceState < ThunderballSofcMgr::SystemShutdownAnodeOff)
		{
			snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[3].m_LineText), "State:  Cooling");
			int min = (gs_pSystemStats->m_nRadiatedTemp - 4000) / 12 / 60;
			if (!min) min = 1;
			snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[1].m_LineText), "Time to Off: %4d m", min);
			if (gs_pSystemStats->m_nShutdownReason != ThunderballSofcMgr::NoShutdown)
			{
				snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "%s", ThunderballSystemStats::GetShutdownString(gs_pSystemStats->m_nShutdownReason));
			}
			else
			{
				snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[1].m_LineText), "* Fuel cell active *");
			}
		}
		else
		{
			snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[3].m_LineText), "State:  Unknown");
			memset(m_Lines[2].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
			snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "- Shutdown system -");
		}
#endif
		m_bfRefreshMask = 0xf;
	}

private:
	typedef UiPage inherited;
	int m_nViewId;
	int m_nTickCount;
	int m_nWarningIndex;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
class UiPageTemps : public UiPage
{
public:
	UiPageTemps(S16 nIndex, S16 nLinkedScreen, S16 nPrevScreen) : UiPage(nIndex, nLinkedScreen, -1)
	{
		m_ShowCursor = FALSE;
		m_ForcedPrevIndex = nPrevScreen;
		m_nTopIndex = 0;

		if (gs_bShowInternals)
			m_nFirstIndex = 0;
		else
			m_nFirstIndex = 2;

#ifdef REMOTE_BATTERY_TEMP
		m_nTempCount = 6;
#else
		m_nTempCount = 5;
#endif

	}

	S16 HandleTimeout(S16* nNextPagesPrevious)
	{
		refresh();
		return -1;
	}

	virtual void refresh()
	{
		m_PreviousIndex = m_ForcedPrevIndex;

		snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "  - Temperatures -");

		for(int i = 1; i < 4; i++)
		{
			if (m_nTopIndex + i + m_nFirstIndex > m_nTempCount)
			{
				memset(m_Lines[i].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
			}
			else
			{
				char* pLine;
				switch (m_nTopIndex + i + m_nFirstIndex)
				{
				case 1:
					snprintf(m_Lines[i].m_LineText, sizeof(m_Lines[1].m_LineText), "Radiated:  %4d.%d C", gs_pSystemStats->m_nRadiatedTemp / 10, (gs_pSystemStats->m_nRadiatedTemp >= 0 ? gs_pSystemStats->m_nRadiatedTemp % 10 : (0 - gs_pSystemStats->m_nRadiatedTemp) % 10));
					break;
				case 2:
					snprintf(m_Lines[i].m_LineText, sizeof(m_Lines[2].m_LineText), "CPOX:      %4d.%d C", gs_pSystemStats->m_nCpoxTemp / 10, (gs_pSystemStats->m_nCpoxTemp >= 0 ? gs_pSystemStats->m_nCpoxTemp % 10 : (0 - gs_pSystemStats->m_nCpoxTemp) % 10));
					break;
				case 3:
					snprintf(m_Lines[i].m_LineText, sizeof(m_Lines[3].m_LineText), "Board:     %4d.%d C", gs_pSystemStats->m_nBoardTemperature / 10, (gs_pSystemStats->m_nBoardTemperature >= 0 ? gs_pSystemStats->m_nBoardTemperature % 10 : (0 - gs_pSystemStats->m_nBoardTemperature) % 10));
					break;
				case 4:
					snprintf(m_Lines[i].m_LineText, sizeof(m_Lines[3].m_LineText), "Fuel Cell: %4d.%d C", gs_pSystemStats->m_nOtherTemp / 10, (gs_pSystemStats->m_nOtherTemp >= 0 ? gs_pSystemStats->m_nOtherTemp % 10 : (0 - gs_pSystemStats->m_nOtherTemp) % 10));
					break;
				case 5:
					snprintf(m_Lines[i].m_LineText, sizeof(m_Lines[3].m_LineText), "Ambient:   %4d.%d C", gs_pSystemStats->m_nAmbientTemperature / 10, (gs_pSystemStats->m_nAmbientTemperature >= 0 ? gs_pSystemStats->m_nAmbientTemperature % 10 : (0 - gs_pSystemStats->m_nAmbientTemperature) % 10));
					break;
				case 6:
					snprintf(m_Lines[i].m_LineText, sizeof(m_Lines[3].m_LineText), "Rem Batt:  %4d.%d C", gs_pSystemStats->m_nBatteryTemperature / 10, (gs_pSystemStats->m_nBatteryTemperature >= 0 ? gs_pSystemStats->m_nBatteryTemperature % 10 : (0 - gs_pSystemStats->m_nBatteryTemperature) % 10));
					break;
				default:
					memset(m_Lines[i].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
					break;
				}
			}
		}
		m_bfRefreshMask = 0xf;
	}

	S16 IncrementCursor(S16* nNextPagesPrevious)
	{
		if (m_nTopIndex) m_nTopIndex--;
		refresh();
		return -1;
	}

	S16 DecrementCursor(S16* nNextPagesPrevious)
	{
		if (m_nTopIndex < (m_nTempCount - m_nFirstIndex - 3)) m_nTopIndex++;
#if 0
#ifdef REMOTE_BATTERY_TEMP
		if (m_nTopIndex < 3) m_nTopIndex++;
#else
		if (m_nTopIndex < 2) m_nTopIndex++;
#endif
#endif

		refresh();
		return -1;
	}

	void reset()
	{
		if (gs_bShowInternals)
			m_nFirstIndex = 0;
		else
			m_nFirstIndex = 2;
	}

private:
	typedef UiPage inherited;
	S16		m_ForcedPrevIndex;
	int m_nTopIndex;
	int m_nFirstIndex;
	int m_nTempCount;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
class UiPageFlows : public UiPage
{
public:
	UiPageFlows(S16 nIndex, S16 nLinkedScreen, S16 nPrevScreen) : UiPage(nIndex, nLinkedScreen, -1)
	{
		m_ShowCursor = FALSE;
		m_ForcedPrevIndex = nPrevScreen;
	}

	S16 HandleTimeout(S16* nNextPagesPrevious)
	{
		refresh();
		return -1;
	}

	virtual void refresh()
	{
		m_PreviousIndex = m_ForcedPrevIndex;

		if (gs_bShowInternals)
		{
			snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "Flows  Air    Fuel");
			snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[1].m_LineText), "Anode: %4d %% %4d %%", gs_pSystemStats->m_AnodeAirDutyCycle, gs_pSystemStats->m_AnodeFuelDutyCycle);
			snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[2].m_LineText), "PB:    %4d %% %4d %%", gs_pSystemStats->m_PBAirDutyCycle, gs_pSystemStats->m_PBFuelDutyCycle);
			snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "Cath:  %4d %%", gs_pSystemStats->m_CathodeAirDutyCycle);
		}
		else
		{
			snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "Flow  Power");
			memset(m_Lines[1].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
			snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[2].m_LineText), "Cooling Fan: %3d %%", gs_pSystemStats->m_FanDutyCycle);
			memset(m_Lines[3].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
		}
		m_bfRefreshMask = 0xf;
	}

private:
	typedef UiPage inherited;
	S16		m_ForcedPrevIndex;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
class UiPageFlowSenses : public UiPage
{
public:
	UiPageFlowSenses(S16 nIndex, S16 nLinkedScreen, S16 nPrevScreen) : UiPage(nIndex, nLinkedScreen, -1)
	{
		m_ShowCursor = FALSE;
		m_ForcedPrevIndex = nPrevScreen;
		m_nTopIndex = 0;
	}

	virtual void refresh()
	{
		m_PreviousIndex = m_ForcedPrevIndex;

		if (gs_bShowInternals)
		{
			snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "   - Flows  (SLPM) -");

			for(int i = 1; i < 4; i++)
			{
				char* pLine;
				switch (m_nTopIndex + i)
				{
				case 1:
					snprintf(m_Lines[i].m_LineText, sizeof(m_Lines[0].m_LineText), "Anode Air:  %2d.%02d", gs_pSystemStats->m_AnodeAirFlow / 100, (gs_pSystemStats->m_AnodeAirFlow >= 0 ? gs_pSystemStats->m_AnodeAirFlow % 100 : (0 - gs_pSystemStats->m_AnodeAirFlow) % 100));
					break;
				case 2:
					snprintf(m_Lines[i].m_LineText, sizeof(m_Lines[0].m_LineText), "Anode Fuel: %2d.%03d", gs_pSystemStats->m_AnodeFuelFlow / 1000, (gs_pSystemStats->m_AnodeFuelFlow >= 0 ? gs_pSystemStats->m_AnodeFuelFlow % 1000 : (0 - gs_pSystemStats->m_AnodeFuelFlow) % 1000));
					break;
				case 3:
					snprintf(m_Lines[i].m_LineText, sizeof(m_Lines[0].m_LineText), "PB Air:    %3d.%02d", gs_pSystemStats->m_PBAirFlow / 100, (gs_pSystemStats->m_PBAirFlow >= 0 ? gs_pSystemStats->m_PBAirFlow % 100 : (0 - gs_pSystemStats->m_PBAirFlow) % 100));
					break;
				case 4:
					snprintf(m_Lines[i].m_LineText, sizeof(m_Lines[0].m_LineText), "PB Fuel:    %2d.%03d", gs_pSystemStats->m_PBFuelFlow / 1000, (gs_pSystemStats->m_PBFuelFlow >= 0 ? gs_pSystemStats->m_PBFuelFlow % 1000 : (0 - gs_pSystemStats->m_PBFuelFlow) % 1000));
					break;
				case 5:
					snprintf(m_Lines[i].m_LineText, sizeof(m_Lines[0].m_LineText), "Cathode:   %3d.%02d", gs_pSystemStats->m_CathodeAirFlow / 100, (gs_pSystemStats->m_CathodeAirFlow >= 0 ? gs_pSystemStats->m_CathodeAirFlow % 100 : (0 - gs_pSystemStats->m_CathodeAirFlow) % 100));
					break;
				}
			}
		}
		else
		{
			int nFuelFLowSlpm = gs_pSystemStats->m_AnodeFuelFlow + gs_pSystemStats->m_PBFuelFlow;
			// convert here
			// 0.00406 lbs/SL
			int nFuelFlowLbsPerHour = nFuelFLowSlpm * 2436 / 10000;
			if (nFuelFlowLbsPerHour < 50)
				nFuelFlowLbsPerHour = 0;

			snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "Fuel Flow");
			memset(m_Lines[1].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
			snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[2].m_LineText), "Propane %2d.%02d lbs/hr", nFuelFlowLbsPerHour / 1000, (nFuelFlowLbsPerHour % 1000) /10);
			memset(m_Lines[3].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
		}
		m_bfRefreshMask = 0xf;
	}

	S16 HandleTimeout(S16* nNextPagesPrevious)
	{
		refresh();
		return -1;
	}

	S16 IncrementCursor(S16* nNextPagesPrevious)
	{
		if (gs_bShowInternals)
		{
			if (m_nTopIndex) m_nTopIndex--;
			refresh();
		}
		return -1;
	}

	S16 DecrementCursor(S16* nNextPagesPrevious)
	{
		if (gs_bShowInternals)
		{
			if (m_nTopIndex < 2) m_nTopIndex++;
			refresh();
		}
		return -1;
	}

private:
	typedef UiPage inherited;
	S16		m_ForcedPrevIndex;
	int m_nTopIndex;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
class UiPageVoltages : public UiPage
{
public:
	UiPageVoltages(S16 nIndex, S16 nLinkedScreen, S16 nPrevScreen) : UiPage(nIndex, nLinkedScreen, -1)
	{
		m_ShowCursor = FALSE;
		m_ForcedPrevIndex = nPrevScreen;
		m_bShowPower = FALSE;
	}

	S16 HandleTimeout(S16* nNextPagesPrevious)
	{
		refresh();
		return -1;
	}

	virtual void refresh()
	{
		m_PreviousIndex = m_ForcedPrevIndex;

		if (!m_bShowPower)
		{
			snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "      Volts    Amps");
			if (gs_bShowInternals)
			{
				snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[1].m_LineText), "FC:   %2d.%03d %2d.%03d", 
						gs_pSystemStats->m_FuelCellVoltage / 1000, 
						(gs_pSystemStats->m_FuelCellVoltage >= 0 ? gs_pSystemStats->m_FuelCellVoltage % 1000 : (0 - gs_pSystemStats->m_FuelCellVoltage) % 1000),
						gs_pSystemStats->m_FuelCellCurrent / 1000, 
						(gs_pSystemStats->m_FuelCellCurrent >= 0 ? gs_pSystemStats->m_FuelCellCurrent % 1000 : (0 - gs_pSystemStats->m_FuelCellCurrent) % 1000));
			}
			else
			{
				memset(m_Lines[1].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
			}
			snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[2].m_LineText), "Load: %2d.%03d %2d.%03d", 
					gs_pSystemStats->m_LoadVoltage / 1000, 
					(gs_pSystemStats->m_LoadVoltage >= 0 ? gs_pSystemStats->m_LoadVoltage % 1000 : (0 - gs_pSystemStats->m_LoadVoltage) % 1000),
					gs_pSystemStats->m_LoadCurrent / 1000, 
					(gs_pSystemStats->m_LoadCurrent >= 0 ? gs_pSystemStats->m_LoadCurrent % 1000 : (0 - gs_pSystemStats->m_LoadCurrent) % 1000));
			snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "Bat:  %2d.%03d %2d.%03d", 
					gs_pSystemStats->m_BatteryVoltage / 1000, 
					(gs_pSystemStats->m_BatteryVoltage >= 0 ? gs_pSystemStats->m_BatteryVoltage % 1000 : (0 - gs_pSystemStats->m_BatteryVoltage) % 1000),
					gs_pSystemStats->m_BatteryCurrent / 1000, 
					(gs_pSystemStats->m_BatteryCurrent >= 0 ? gs_pSystemStats->m_BatteryCurrent % 1000 : (0 - gs_pSystemStats->m_BatteryCurrent) % 1000));
		}
		else
		{
#ifdef REMOTE_V_SENSE
			snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "      Watts Sense(V)");
			if (gs_bShowInternals)
			{
				snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[1].m_LineText), "FC:   %4d", 
						gs_pSystemStats->fuelCellPower() / 1000); 
			}
			else
			{
				memset(m_Lines[1].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
			}
			snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[2].m_LineText), "Load: %4d   %2d.%03d", 
					gs_pSystemStats->loadPower() / 1000,
					gs_pSystemStats->m_RemoteBatteryVoltage / 1000,
					(gs_pSystemStats->m_RemoteBatteryVoltage >= 0 ? gs_pSystemStats->m_RemoteBatteryVoltage % 1000 : (0 - gs_pSystemStats->m_RemoteBatteryVoltage) % 1000));
			snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "Bat:  %4d", 
					gs_pSystemStats->batteryPower() / 1000); 
#else
			snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "      Watts");
			if (gs_bShowInternals)
			{
				snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[1].m_LineText), "FC:   %4d", 
						gs_pSystemStats->fuelCellPower() / 1000); 
			}
			else
			{
				memset(m_Lines[1].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
			}
			snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[2].m_LineText), "Load: %4d", 
					gs_pSystemStats->loadPower() / 1000);
			snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "Bat:  %4d", 
					gs_pSystemStats->batteryPower() / 1000); 
#endif
		}
		m_bfRefreshMask = 0xf;
	}

	S16 IncrementCursor(S16* nNextPagesPrevious)
	{
		m_bShowPower = !m_bShowPower;
		refresh();
		return -1;
	}

	S16 DecrementCursor(S16* nNextPagesPrevious)
	{
		m_bShowPower = !m_bShowPower;
		refresh();
		return -1;
	}

private:
	S16		m_ForcedPrevIndex;
	bool	m_bShowPower;
	typedef UiPage inherited;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct CommandSelector {
	const char* m_pCommand;
	const char* m_pAntiCommand;
	const char* m_pObject;
	int m_nParamMin;
	int m_nParamMax;
	int m_nParamStep;
	
	void getCommandString(int val, bool bAntiActive, char* pBuffer, size_t bufLen)
	{
		if (bAntiActive)
			snprintf(pBuffer, bufLen, "%s %s %d\n", m_pAntiCommand, m_pObject, val);
		else
			snprintf(pBuffer, bufLen, "%s %s %d\n", m_pCommand, m_pObject, val);
	}
};

static CommandSelector gs_CommandList[] =
{
	{ "force", "clear", "fan2.state", 0, 1, 1 },
	{ "force", "clear", "fan2.outdc", 0, 100, 10 },
	{ "force", "clear", "cathairctrl.state", 0, 1, 1 },
	{ "force", "clear", "cathairctrl.outdc", 0, 100, 10 },
	{ "force", "clear", "cathairctrl.valsp", 0, 10000, 500 },
	{ "force", "clear", "pbairctrl.state", 0, 1, 1 },
	{ "force", "clear", "pbairctrl.outdc", 0, 100, 10 },
	{ "force", "clear", "pbairctrl.valsp", 0, 10000, 500 },
	{ "force", "clear", "anairctrl.state", 0, 1, 1 },
	{ "force", "clear", "anairctrl.outdc", 0, 100, 10 },
	{ "force", "clear", "anairctrl.valsp", 0, 1000, 50 },
	{ "force", "clear", "pbfuelctrl.state", 0, 1, 1 },
	{ "force", "clear", "pbfuelctrl.outdc", 0, 100, 10 },
	{ "force", "clear", "pbfuelctrl.valsp", 0, 2000, 100 },
	{ "force", "clear", "anfuelctrl.state", 0, 1, 1 },
	{ "force", "clear", "anfuelctrl.outdc", 0, 100, 10 },
	{ "force", "clear", "anfuelctrl.valsp", 0, 1000, 100 },
	{ "force", "clear", "valve.state", 0, 1, 1 },
	{ "force", "clear", "igniter.state", 0, 1, 1 },
	{ "force", "clear", "fcswitch.state", 0, 1, 1 },
};

#define NumCommands sizeof(gs_CommandList) / sizeof(CommandSelector)

class UiPageSelector : public UiPage
{
	int m_nParamValue;
	CommandSelector* m_pSelector;
	bool m_bAntiActive;

public:
	UiPageSelector(S16 nIndex, CommandSelector* pSelector) : UiPage(nIndex, -1, -1)
	{
		m_ShowCursor = FALSE;
		m_nParamValue = pSelector->m_nParamMin;
		m_pSelector = pSelector;
		m_bAntiActive = false;
	}

	virtual void refresh()
	{
		snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "-- Run Command --");

		if (m_bAntiActive)
		{
			snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[0].m_LineText), "%s", m_pSelector->m_pAntiCommand);
			memset(m_Lines[3].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
		}
		else
		{
			snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[0].m_LineText), "%s", m_pSelector->m_pCommand);
			snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[0].m_LineText), " %d", m_nParamValue);
		}		
		snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[0].m_LineText), "%s", m_pSelector->m_pObject);

		m_bfRefreshMask = 0xf;
	}

	S16 IncrementCursor(S16* nNextPagesPrevious)
	{
		m_bAntiActive = false;
		m_nParamValue += m_pSelector->m_nParamStep;
		if (m_nParamValue > m_pSelector->m_nParamMax)
			m_nParamValue = m_pSelector->m_nParamMax;

		refresh();
		return -1;
	}

	S16 DecrementCursor(S16* nNextPagesPrevious)
	{
		m_nParamValue -= m_pSelector->m_nParamStep;
		if (m_nParamValue < m_pSelector->m_nParamMin)
		{
			if (m_pSelector->m_pAntiCommand != 0)
				m_bAntiActive = true;
			m_nParamValue = m_pSelector->m_nParamMin;
		}
		else {
			m_bAntiActive = false;
		}

		refresh();
		return -1;
	}

	S16 enterHandled(S16* nNextPagesPrevious)
	{
		char buff[64];
		m_pSelector->getCommandString(m_nParamValue, m_bAntiActive, buff, sizeof(buff));
		printf("Sending: %s", buff);
		m_pMainProc->SendSerialData((unsigned char*)buff, strlen(buff)); 

		*nNextPagesPrevious = -1;
		return m_Index;
	}

private:
	typedef UiPage inherited;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
class UiPageSense : public UiPage
{
public:
	UiPageSense(S16 nIndex, S16 nLinkedScreen, S16 nPrevScreen) : UiPage(nIndex, nLinkedScreen, -1)
	{
		m_ShowCursor = FALSE;
		m_ForcedPrevIndex = nPrevScreen;
	}

	S16 HandleTimeout(S16* nNextPagesPrevious)
	{
		refresh();
		return -1;
	}

	virtual void refresh()
	{
		m_PreviousIndex = m_ForcedPrevIndex;

		snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "System Conditions");
		snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[1].m_LineText), "Fuel Pres: %3d PSI", gs_pSystemStats->m_nFuelPressure / 1000);
		snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[2].m_LineText), "CO:        %s", 
			((gs_pSystemStats->m_bfWarningMask & ThunderballSystemStats::WarnCOHigh) ? "High" : "Normal"));
		snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "Propane:   %s",
			((gs_pSystemStats->m_bfWarningMask & ThunderballSystemStats::WarnPropaneHigh) ? "High" : "Normal"));
		m_bfRefreshMask = 0xf;
	}

private:
	typedef UiPage inherited;
	S16		m_ForcedPrevIndex;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
class UiPageNetStatus : public UiPage
{
public:
	UiPageNetStatus(S16 nIndex) : UiPage(nIndex, -1, -1)
	{
		m_ShowCursor = FALSE;
		snprintf(m_Lines[0].m_LineText, sizeof(m_Lines[0].m_LineText), "Network Status");
		snprintf(m_Lines[1].m_LineText, sizeof(m_Lines[1].m_LineText), "IP Addr:");
	}

	S16 HandleTimeout(S16* nNextPagesPrevious)
	{
		m_nRefreshCount--;
		if (m_nRefreshCount <= 0)
			reset();

		refresh();
		return -1;
	}

	virtual void refresh()
	{
		snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), " ");
		m_bfRefreshMask |= 0x8;
	}

	void reset()
	{
		m_nRefreshCount = 4;

		struct ifaddrs * ifAddrStruct=NULL;
		struct ifaddrs * ifa=NULL;
		void * tmpAddrPtr=NULL;
		char addressBuffer[INET_ADDRSTRLEN];
		
		getifaddrs(&ifAddrStruct);

		for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
			if (!ifa->ifa_addr) {
				continue;
			}
			if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
				// is a valid IP4 Address
				tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
				inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
				if (!strcmp(ifa->ifa_name, "eth0"))
					break;
			}
		}
		
		if (!ifa)
		{
			snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[2].m_LineText), " Not connected");
			memset(m_Lines[3].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
		}
		else
		{
			snprintf(m_Lines[2].m_LineText, sizeof(m_Lines[2].m_LineText), " %s", addressBuffer);
			
			char hostname[24];
			int res = gethostname(hostname, sizeof(hostname));
			if (res == -1)
				snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[2].m_LineText), " %s", hostname);
			else
				memset(m_Lines[3].m_LineText, 0, sizeof(m_Lines[3].m_LineText));
		}

		inherited::reset();

		m_bfRefreshMask = 0xC;
	}

private:
	typedef UiPage inherited;
	int m_nRefreshCount;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
class UiUpgradePage : public UiMessagePage
{
public:
	UiUpgradePage(S16 nIndex, const char** ppLines, int nLineCount, void (*fnAction)(), 
		S16 nGotoIndex = -1, S16 nPageTimeout = -1, BOOL bLinkPreviousToGoto = FALSE) :
	UiMessagePage(nIndex, ppLines, nLineCount, fnAction, nGotoIndex, -1, bLinkPreviousToGoto)
	{
		m_nTimeLeft = nPageTimeout;
		m_nMaxTime = nPageTimeout;
	}

	S16 HandleTimeout(S16* nNextPagesPrevious)
	{
		snprintf(m_Lines[3].m_LineText, sizeof(m_Lines[3].m_LineText), "Timeout: %d secs", m_nTimeLeft);
		m_nTimeLeft--;
		m_bfRefreshMask |= 0x08;

		if (!m_nTimeLeft)
		{
			system("service smbd stop");
			return m_LinkedScreen;
		}
		return -1;
	}

	S16 escapeHandled(S16* nNextPagesPrevious) 
	{ 
		system("service smbd stop");
		return m_LinkedScreen;
	}

	virtual void refresh()
	{
		inherited::refresh();
	}
	
	virtual void reset()
	{
		m_nTimeLeft = m_nMaxTime;
		inherited::reset();
	}

private:
	typedef UiPage inherited;
	int m_nTimeLeft;
	int m_nMaxTime;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This creates the UI
#include "UiPageMgr.h"

S16 CheckForLiveFuelCell()
{

	if (gs_pSystemStats->FuelCellActive())
		return RebootDamageWarning;
	else if (!gs_pSystemStats->SystemIdle())
		return RebootTendingResetWarning;
	return RebootScreen;
}

static UiMenu s_MainMenu[] = {
	{ "Configuration", ConfigMenu, 0 },
	{ "Status", StatusMenu, 0 },
	{ "System", SystemMenu, 0 },
};

static UiMenu s_StatusMenu[] = {
	{ "Network", NetworkStatusScreen, 0 },
	{ "Temperatures", TempScreen, 0 },
	{ "Flows", FlowSenseScreen, 0 },
	{ "Flow Power", FlowDutyCycleScreen, 0 },
	{ "Volts / Amps", VoltagesScreen, 0 },
	{ "Conditions", ConditionsScreen, 0 },
	{ "Version", AppInfoScreen, 0 },
};

static UiMenu s_SystemMenu[] = {
	{ "Reset Error", ResetErrorScreen, 0 },
	{ "Reboot System", RebootScreen, CheckForLiveFuelCell },
	{ "Change Password", ChangePasswordInfoScreen, 0 },
	{ "Internal Battery", BatteryChargeScreen, 0 },
#ifdef INTERNAL_ONLY
	{ "Run Command", CommandMenu, 0 },
#endif
};

static UiMenu s_ConfigMenu[] = {
	{ "Network (IP)", NetworkConfigScreen, 0 },
	{ "External Battery", BatteryMenuScreen, 0 },
	{ "Fuel Configuration", FuelTempScreen, 0 },
	{ "Operating Mode", OpModeScreen, 0 },
	{ "USB Access", UsbConfigScreen, 0 },
//	{ "Device Info", DeviceScreen, 0 },
	{ "Upgrade Support", UpgradeScreen, 0 },
	{ "Remote UI Support", RemoteUiConfigScreen, 0 },
};

static UiMenu s_ExternalBatMenu[] = {
	{ "Battery Type", BatteryConfigScreen, 0 },
	{ "Temperature Config", BatteryTempConfigScreen, 0 },
};

static UiMenu s_NetConfigMenu[] = {
	{ "Interface Config", NetworkEnableScreen, 0 },
	{ "IP Config", IpConfigScreen, 0 },
	{ "Save Config", SaveConfigScreen, 0 },
};

static const char* s_EnableOptions[LinuxProcess::NumNetworkOptions] =
{
	// These match the enumeration list in LinuxProcess
	"  Off",
	"  Use DHCP",
	"  Use Static IP addr"
};

static const char* s_BatteryChargeOptions[ThunderballSofcMgr::NumBatteryChargeOptions] =
{
	// Match ThunderballSofcMgr

	"  Auto",
	"  Disabled",
	"  Charge at 10 W",
	"  Charge at 20 W",
	"  Charge at 30 W",
	"  Charge at 40 W",
	"  Charge at 50 W"
};


static const char* s_OpModeOptions[ThunderballSofcMgr::NumOpModes] =
{
	// Match ThunderballSofcMgr

	"  Top Charge Battery",
	"  Hybridize Battery"
};

static const char* s_TempModeOptions[ThunderballDcDc::NumTempModes] =
{
	// Match ThunderballDcDc

	"  Auto Sense Temp",
	"  Cold Temp (0 C)",
	"  Mild Temp (25 C)",
	"  Hot Temp (40 C)",
};

static const char* s_FuelTempModeOptions[ThunderballSofcMgr::NumFuelTempModes] =
{
	// Match ThunderballSofcMgr

	"  Auto Sense Temp",
	"  Cold Temp (0 C)",
	"  Mild Temp (25 C)",
	"  Hot Temp (40 C)",
	"  Frigid (-30 C)",
};

enum {
	UsbEnabled,
	UsbDisabled,

	NumUsbModes
};

static const char* s_UsbModeOptions[NumUsbModes] =
{
	"  Enabled",
	"  Disabled"
};

enum {
	UpgradeEnabled,
	UprgradeDisabled,

	NumUpgradeModes
};

static const char* s_UpgradeOptions[NumUpgradeModes] =
{
	"  Enabled",
	"  Disabled"
};

enum {
	RemoteUiEnabled,
	RemoteUiDisabled,

	NumRemoteUiModes
};

static const char* s_RemoteUiOptions[NumRemoteUiModes] =
{
	"  Enabled",
	"  Disabled"
};

static const char* s_RebootMessage[] =
{
	"Hit <ENTER> to",
	" reboot the",
	"   system."
};

static const char* s_ResetErrorMessage[] =
{
	"Hit <ENTER> to",
	" reset system",
	"   error."
};

static const char* s_CalFuelMessage[] =
{
	"Hit <ENTER> to",
	" mark fuel as",
	"   full."
};


static UiMenu s_IpConfigMenu[] = {
	{ "IP Address", SetIpAddrScreen, 0 },
	{ "IP Mask", SetIpMaskScreen, 0 },
	{ "IP Gateway", SetIpGatewayScreen, 0 },
};

static const char* s_SaveConfigMessage[] =
{
	"   ----------",
	"Hit <ENTER> to save",
	" network config.",
	"   ----------",
	"**** Reboot required"
};

static const char* s_SaveInfoMessage[] =
{
	"* Reboot required",
	"* to reconfigure",
	"* the system with",
	"* saved values."
};

static const char* s_UpgradeIntroMessage[] =
{
	"Hit ENTER to enable ",
	" upgrades.",
	" ",
	" "
};

static const char* s_UpgradeMessage[] =
{
	"Start upgrade ",
	" application.",
	" ",
	"Timeout: 600 secs "
};

static const char* s_PasswordInfoMessage[] =
{
	"The following process",
	"will reconfigure the",
	"system password. Hit",
	"<ENTER> to continue."
};

static const char* s_PasswordHelpMessage[] =
{
	"When entering new",
	"password, press the",
	"desired keys then",
	"wait for time = 0."
};

static const char* s_PasswordConfirmMessage[] =
{
	"Hit <ENTER> if the",
	"password was entered",
	"correctly. Hit <ESC>",
	"to abort or retry."
};

static const char* s_PasswordChangedMessage[] =
{
	"Password has been",
	"successfully changed",
	"Hit <ENTER> to",
	"continue."
};

static const char* s_PasswordNotChangedMessage[] =
{
	"Password has NOT",
	"been changed.",
	"Hit <ENTER> to",
	"continue."
};

static const char* s_DamageMessage[] =
{
	"* Fuel Cell Active *",
	"Hit <ENTER> to",
	"reboot and risk",
	"damage to system!",
};

static const char* s_TendingResetMessage[] =
{
	"** Tending Active **",
	"Hit <ENTER> to",
	"continue reboot",
	"and stop tending.",
};

static UiIpEditPage* gs_pIpAddrPage;
static UiIpEditPage* gs_pIpMaskPage;
static UiIpEditPage* gs_pIpGatewayPage;
static UiOptionPage* gs_pIpEnablePage;
static UiOptionPage* gs_pBatteryConfigPage;
static UiOptionPage* gs_pOpModeConfigPage;
static UiOptionPage* gs_pTempConfigPage;
static UiOptionPage* gs_pFuelTempConfigPage;
static UiUserTextPage* gs_pIdPage;
static UiPageMessage* gs_pMessagePage;

static void SaveNetworkConfig()
{
	printf("Saving network config\n");
	
	std::ofstream ofs ("/etc/network/interfaces", std::ofstream::out | std::ofstream::trunc);

	ofs << "# interfaces(5) file used by ifup(8) and ifdown(8)" << std::endl;
	ofs << std::endl;
	ofs << "# loopback network interface" << std::endl;
	ofs << "auto lo" << std::endl;
	ofs << "iface lo inet loopback" << std::endl;
	ofs << std::endl;

	if (gs_pIpEnablePage->selectedOptionIndex() != LinuxProcess::NetworkOff)
	{
		ofs << "# primary network interface" << std::endl;
		ofs << "# auto eth0 -- not started so process does not wait for ethernet" << std::endl;
		if (gs_pIpEnablePage->selectedOptionIndex() == LinuxProcess::NetworkDHCP)
		{
			ofs << "iface eth0 inet dhcp" << std::endl;
		}
		else
		{
			ofs << "iface eth0 inet static" << std::endl;

			unsigned int addr = gs_pIpAddrPage->addr();
			unsigned char* pAddr = (unsigned char*)&addr;
			ofs << "    address " << (int)pAddr[0] << "." << (int)pAddr[1] << "." << (int)pAddr[2] << "." << (int)pAddr[3] << std::endl;

			addr = gs_pIpMaskPage->addr();
			ofs << "    netmask " << (int)pAddr[0] << "." << (int)pAddr[1] << "." << (int)pAddr[2] << "." << (int)pAddr[3] << std::endl;

			addr = gs_pIpGatewayPage->addr();
			ofs << "    gateway " << (int)pAddr[0] << "." << (int)pAddr[1] << "." << (int)pAddr[2] << "." << (int)pAddr[3] << std::endl;
		}
	}

	ofs.close();

	system("sync");
}

static void RebootSystem()
{
	printf("Rebooting system\n");
	system("sync");
	system("/sbin/shutdown -r now");
}

static void ResetError()
{
	ThunderballUI::m_pTheApp->sendResetErrorMsg();
}

static void EnableUpgrade()
{
	system("service smbd start");
	system("restart smbd");
}

static void DisableUpgrade()
{
	system("service smbd stop");
}

static void DeviceNameChanged(const char* pNewName)
{
	if (pNewName)
	{
		printf("New name: %s\n", pNewName);
		std::string n(pNewName);
		n = "\"" + n + "\"";

		setenv("PTXAPP_DEVICENAME", n.c_str(), 1);
		ThunderballUI::m_pTheApp->saveEnvironemnt();
		
		ThunderballUI::m_pTheApp->sendRefreshEnvMsg();
	}
}

static void BatteryChanged(int index)
{

	index = BatteryChargeProfile::GetEntryIndexFromDisplayIndex((size_t)index);
	assert (index >= 0);

	char btype[8];
	snprintf(btype, sizeof(btype), "%d", index);
	gs_nBatteryType = index;

	setenv("PTXAPP_BATTERYTYPE", btype, 1);
	ThunderballUI::m_pTheApp->saveEnvironemnt();
		
	ThunderballUI::m_pTheApp->sendRefreshEnvMsg();
}

static void BatteryChargeModeChanged(int index)
{
	printf("Changing internal charge mode\n");
	char buff[64];
	snprintf(buff, sizeof(buff), "set fccmgr.chargemode %d\n", index);

	m_pMainProc->SendSerialData((unsigned char*)buff, strlen(buff)); 
}

static void OpModeChanged(int index)
{
	char btype[8];
	snprintf(btype, sizeof(btype), "%d", index);
	gs_nOperatingMode = index;

	setenv("PTXAPP_FUELCELLOPMODE", btype, 1);
	ThunderballUI::m_pTheApp->saveEnvironemnt();
		
	ThunderballUI::m_pTheApp->sendRefreshEnvMsg();
}

static void TempModeChanged(int index)
{
	char btype[8];
	snprintf(btype, sizeof(btype), "%d", index);
	gs_nTempMode = index;

	setenv("PTXAPP_BATTERYTEMPMODE", btype, 1);
	ThunderballUI::m_pTheApp->saveEnvironemnt();
		
	ThunderballUI::m_pTheApp->sendRefreshEnvMsg();
}

static void FuelTempModeChanged(int index)
{
	char btype[8];
	snprintf(btype, sizeof(btype), "%d", index);
	gs_nFuelTempMode = index;

	setenv("PTXAPP_FUELTEMPMODE", btype, 1);
	ThunderballUI::m_pTheApp->saveEnvironemnt();
		
	ThunderballUI::m_pTheApp->sendRefreshEnvMsg();
}

static void UsbModeChanged(int index)
{
	gs_nUsbOpMode = index;

	if (index == UsbEnabled)
	{
		system("rm /etc/init/serial.override && sync");
	}
	else
	{
		system("echo manual > /etc/init/serial.override && sync");
	}
}

static void UpgradeModeChanged(int index)
{
	gs_nUpgradeMode = index;

	if (index == UpgradeEnabled)
	{
		system("rm /etc/init/smbd.override && sync");
	}
	else
	{
		system("echo manual > /etc/init/smbd.override && sync");
	}
}

static void RemoteUiModeChanged(int index)
{
	char btype[8];
	snprintf(btype, sizeof(btype), "%d", index);
	gs_nRemoteUiMode = index;

	setenv("PTXAPP_REMOTEUIMODE", btype, 1);
	ThunderballUI::m_pTheApp->saveEnvironemnt();
		
	ThunderballUI::m_pTheApp->sendRefreshEnvMsg();
}

static void PasswordChanged(const char* pNewPwd)
{
	if (pNewPwd)
	{
		printf("New name: %s\n", pNewPwd);
		snprintf(gs_aUserPassword, sizeof(gs_aUserPassword), "%s", pNewPwd);

		std::string n(pNewPwd);
		n = "\"" + n + "\"";

		setenv("PTXAPP_UIPASSWORD", n.c_str(), 1);
		ThunderballUI::m_pTheApp->saveEnvironemnt();
		
		ThunderballUI::m_pTheApp->sendRefreshEnvMsg();
	}
}

static void CalibrateFuel()
{
	printf("Calibrating Fuel\n");
	m_pMainProc->SendSerialData((unsigned char*)"set fccmgr.highpval 1\n", 22); 
}

static void EnableInternals()
{
	printf("Changing internal viewing\n");
	gs_bShowInternals = !gs_bShowInternals;

	char btype[8];
	if (gs_bShowInternals)
		snprintf(btype, sizeof(btype), "%d", 1);
	else
		snprintf(btype, sizeof(btype), "%d", 0);

	setenv("PTXAPP_SHOWINTERNALS", btype, 1);
	ThunderballUI::m_pTheApp->saveEnvironemnt();
		
	ThunderballUI::m_pTheApp->sendRefreshEnvMsg();
}

void UiPage::CreatePageList(UiPageMgr* pMgr, LinuxProcess* pProc)
{
	m_pMainProc = (ThunderballUI*)pProc;
	struct sockaddr_in sa;

	gs_pSystemStats = ThunderballSystemStats::instance();

	unsigned int ipaddr;
	unsigned int ipmask;
	unsigned int ipgw;

	// defaults
	int nNetConfig = LinuxProcess::NetworkOff;

	m_pMainProc->getIpInformation(&ipaddr, &ipmask, &ipgw, &nNetConfig);

#if 0
	inet_pton(AF_INET, "192.168.2.2", &(sa.sin_addr));
	ipaddr = (unsigned int)sa.sin_addr.s_addr;
	inet_pton(AF_INET, "255.255.255.0", &(sa.sin_addr));
	ipmask = (unsigned int)sa.sin_addr.s_addr;
	inet_pton(AF_INET, "0.0.0.0", &(sa.sin_addr));
	ipgw = (unsigned int)sa.sin_addr.s_addr;

	TextFile tf(std::string("/etc/network/interfaces"));
	std::string line;
	int lineId = 0;
	while(1)
	{
		line = tf.nextLine();
		lineId++;

		if (line.size() == 0) break;
		{
			ArgParser parser(line);
			if (!parser.good())
				break;

			std::string arg = parser.nextArg();
			if (arg.size() == 0)
				continue;
			if (arg == "iface")
			{
				arg = parser.nextArg();
				if (arg.size() == 0)
					continue;
				if (arg == "eth0")
				{
					arg = parser.nextArg();
					if ((arg.size() == 0) || (arg != "inet"))
						continue;

					arg = parser.nextArg();
					if (arg.size() == 0)
						continue;
					if (arg == "dhcp")
						nNetConfig = LinuxProcess::NetworkDHCP;
					else if (arg == "static")
						nNetConfig = LinuxProcess::NetworkStatic;
				}
			}
			else if ((arg == "address") && (nNetConfig == LinuxProcess::NetworkStatic))
			{
				arg = parser.nextArg();
				if (arg.size() == 0)
					continue;
				inet_pton(AF_INET, arg.c_str(), &(sa.sin_addr));
				ipaddr = (unsigned int)sa.sin_addr.s_addr;
			}
			else if ((arg == "netmask") && (nNetConfig == LinuxProcess::NetworkStatic))
			{
				arg = parser.nextArg();
				if (arg.size() == 0)
					continue;
				inet_pton(AF_INET, arg.c_str(), &(sa.sin_addr));
				ipmask = (unsigned int)sa.sin_addr.s_addr;
			}
			else if ((arg == "gateway") && (nNetConfig == LinuxProcess::NetworkStatic))
			{
				arg = parser.nextArg();
				if (arg.size() == 0)
					continue;
				inet_pton(AF_INET, arg.c_str(), &(sa.sin_addr));
				ipgw = (unsigned int)sa.sin_addr.s_addr;
			}
		}
	}
#endif

	{
		TextFile tf(std::string("/etc/init/serial.override"));
		std::string line = tf.nextLine();
		if (line == "manual")
			gs_nUsbOpMode = UsbDisabled;
		else
			gs_nUsbOpMode = UsbEnabled;
	}

	{
		TextFile tf(std::string("/etc/init/smbd.override"));
		std::string line = tf.nextLine();
		if (line == "manual")
			gs_nUpgradeMode = UsbDisabled;
		else
			gs_nUpgradeMode = UsbEnabled;
	}

	const char* val = ::getenv( "PTXAPP_UIPASSWORD" );
    if ( val == 0 ) {
		snprintf(gs_aUserPassword, sizeof(gs_aUserPassword), "aaa");
    }
	else
	{
		snprintf(gs_aUserPassword, sizeof(gs_aUserPassword), "%s", val);
	}

	val = ::getenv( "PTXAPP_SHOWINTERNALS" );
    if ( val != 0 ) 
	{
		if (atoi(val) == 1)
			gs_bShowInternals = true;
		else
			gs_bShowInternals = false;
	}
	else
	{
		gs_bShowInternals = false;
	}

	pMgr->addNewScreenToEndOfList(new UiPageSplash(SplashScreen, MainScreen, 15));
	pMgr->addNewScreenToEndOfList(new UiPageMain(MainScreen, LoginScreen));
	pMgr->addNewScreenToEndOfList(new UiMenuPage(MainMenu, "Main Menu", s_MainMenu, sizeof(s_MainMenu) / sizeof(UiMenu)));
	pMgr->addNewScreenToEndOfList(new UiMenuPage(StatusMenu, "Status Menu", s_StatusMenu, sizeof(s_StatusMenu) / sizeof(UiMenu)));
	pMgr->addNewScreenToEndOfList(new UiMenuPage(ConfigMenu, "Config Menu", s_ConfigMenu, sizeof(s_ConfigMenu) / sizeof(UiMenu)));

	pMgr->addNewScreenToEndOfList(new UiPageTemps(TempScreen, FlowDutyCycleScreen, StatusMenu));
	pMgr->addNewScreenToEndOfList(new UiPageFlows(FlowDutyCycleScreen, FlowSenseScreen, StatusMenu));
	pMgr->addNewScreenToEndOfList(new UiPageFlowSenses(FlowSenseScreen, VoltagesScreen, StatusMenu));
	pMgr->addNewScreenToEndOfList(new UiPageVoltages(VoltagesScreen, ConditionsScreen, StatusMenu));

	pMgr->addNewScreenToEndOfList(new UiMenuPage(NetworkConfigScreen, "Network Config Menu", s_NetConfigMenu, sizeof(s_NetConfigMenu) / sizeof(UiMenu)));
	pMgr->addNewScreenToEndOfList(gs_pIpEnablePage = new UiOptionPage(NetworkEnableScreen, "Network State:", s_EnableOptions, LinuxProcess::NumNetworkOptions, "** Save -> Reboot **"));
	gs_pIpEnablePage->SetSelectedIndex(nNetConfig);
	pMgr->addNewScreenToEndOfList(new UiMessagePage(RebootScreen, s_RebootMessage, 3, RebootSystem));

	char buf[16];
    val = ::getenv( "PTXAPP_DEVICENAME" );
    if ( val == 0 ) {
		snprintf(buf, 16, "Unit %d", ThunderballUI::m_pTheApp->m_nUnitId);
		val = buf;
    }
	pMgr->addNewScreenToEndOfList(gs_pIdPage = new UiUserTextPage(DeviceScreen, "Enter device name:", val, "", DeviceNameChanged));

	pMgr->addNewScreenToEndOfList(new UiMenuPage(IpConfigScreen, "IP Config Menu", s_IpConfigMenu, sizeof(s_IpConfigMenu) / sizeof(UiMenu)));
	pMgr->addNewScreenToEndOfList(gs_pIpAddrPage = new UiIpEditPage(SetIpAddrScreen, "Enter IP Addr:", ipaddr, ""));
	pMgr->addNewScreenToEndOfList(gs_pIpMaskPage = new UiIpEditPage(SetIpMaskScreen, "Enter IP Mask:", ipmask, ""));
	pMgr->addNewScreenToEndOfList(gs_pIpGatewayPage = new UiIpEditPage(SetIpGatewayScreen, "Enter IP Addr:", ipgw, ""));

	pMgr->addNewScreenToEndOfList(new UiConfirmMessagePage(SaveConfigScreen, 
														s_SaveConfigMessage, sizeof(s_SaveConfigMessage) / sizeof(const char*), 
														SaveNetworkConfig,
														s_SaveInfoMessage, sizeof(s_SaveInfoMessage) / sizeof(const char*)));

	UiOptionPage* gs_pUpgradeConfigPage;
	pMgr->addNewScreenToEndOfList(gs_pUpgradeConfigPage = new UiOptionPage(UpgradeScreen, "Network upgrade:", s_UpgradeOptions, NumUpgradeModes, "Hit Enter to confirm", UpgradeModeChanged));
	gs_pUpgradeConfigPage->SetSelectedIndex(gs_nUpgradeMode);	
	gs_pUpgradeConfigPage->setupFinalMessage(s_SaveInfoMessage, sizeof(s_SaveInfoMessage) / sizeof(const char*));	
	
	pMgr->addNewScreenToEndOfList(new UiPageNetStatus(NetworkStatusScreen));
	pMgr->addNewScreenToEndOfList(new UiPageSense(ConditionsScreen, TempScreen, StatusMenu));
	pMgr->addNewScreenToEndOfList(new UiMenuPage(SystemMenu, "System Menu", s_SystemMenu, sizeof(s_SystemMenu) / sizeof(UiMenu)));
	pMgr->addNewScreenToEndOfList(new UiMessagePage(ResetErrorScreen, s_ResetErrorMessage, 3, ResetError, SystemMenu));

    val = ::getenv( "PTXAPP_BATTERYTYPE" );
    if ( val == 0 ) {
		gs_nBatteryType = 0;
    }
	else
	{
		gs_nBatteryType = atoi(val);
	}
	
    val = ::getenv( "PTXAPP_FUELCELLOPMODE" );
    if ( val == 0 ) {
		gs_nOperatingMode = 0;
    }
	else
	{
		gs_nOperatingMode = atoi(val);
	}

    val = ::getenv( "PTXAPP_BATTERYTEMPMODE" );
    if ( val == 0 ) {
		gs_nTempMode = 0;
    }
	else
	{
		gs_nTempMode = atoi(val);
	}

    val = ::getenv( "PTXAPP_FUELTEMPMODE" );
    if ( val == 0 ) {
		gs_nFuelTempMode = 0;
    }
	else
	{
		gs_nFuelTempMode = atoi(val);
	}

    val = ::getenv( "PTXAPP_REMOTEUIMODE" );
    if ( val == 0 ) {
		gs_nRemoteUiMode = 0;
    }
	else
	{
		gs_nRemoteUiMode = atoi(val);
	}
	

	size_t nNumBatteries = BatteryChargeProfile::NumSupportedBatteries();
	const char** s_BatteryOptions = new const char*[nNumBatteries];
	for(size_t i =0;i < nNumBatteries; i++)
	{
		int idx = BatteryChargeProfile::GetEntryIndexFromDisplayIndex(i);
		assert (idx >= 0);
		BatteryChargeProfile* pBat = BatteryChargeProfile::GetProfileFromIndex((size_t)idx);
		s_BatteryOptions[i] = pBat->m_pBatteryName;
	}

	pMgr->addNewScreenToEndOfList(gs_pBatteryConfigPage = new UiOptionPage(BatteryConfigScreen, "Battery:", s_BatteryOptions, nNumBatteries, "Hit Enter to confirm", BatteryChanged));
	gs_pBatteryConfigPage->SetSelectedIndex(gs_nBatteryType);	

	pMgr->addNewScreenToEndOfList(new UiPageSplash(AppInfoScreen, -1, -1, FALSE, EnableInternals));

	pMgr->addNewScreenToEndOfList(gs_pFuelTempConfigPage = new UiOptionPage(FuelTempScreen, "Fuel Temp Mode:", s_FuelTempModeOptions, ThunderballSofcMgr::NumFuelTempModes, "Hit Enter to confirm", FuelTempModeChanged));
	gs_pFuelTempConfigPage->SetSelectedIndex(gs_nFuelTempMode);	

	//pMgr->addNewScreenToEndOfList(new UiMessagePage(FuelTempScreen, s_CalFuelMessage, 3, CalibrateFuel, SystemMenu));

	pMgr->addNewScreenToEndOfList(new UiPasswordPage(LoginScreen, "Enter password:", gs_aUserPassword, 20, MainMenu));

	UiOptionPage* pBatteryChargePage;
	pMgr->addNewScreenToEndOfList(pBatteryChargePage = new UiOptionPage(BatteryChargeScreen, "Internal Charge:", s_BatteryChargeOptions, ThunderballSofcMgr::NumBatteryChargeOptions, "Hit Enter to confirm", BatteryChargeModeChanged));
	pBatteryChargePage->SetSelectedIndex(ThunderballSofcMgr::AutoCharge);

	pMgr->addNewScreenToEndOfList(gs_pOpModeConfigPage = new UiOptionPage(OpModeScreen, "Operating Mode:", s_OpModeOptions, ThunderballSofcMgr::NumOpModes, "Hit Enter to confirm", OpModeChanged));
	gs_pOpModeConfigPage->SetSelectedIndex(gs_nOperatingMode);	

	pMgr->addNewScreenToEndOfList(new UiMenuPage(BatteryMenuScreen, "Battery Menu", s_ExternalBatMenu, sizeof(s_ExternalBatMenu) / sizeof(UiMenu)));

	pMgr->addNewScreenToEndOfList(gs_pTempConfigPage = new UiOptionPage(BatteryTempConfigScreen, "Temperature Mode:", s_TempModeOptions, ThunderballDcDc::NumTempModes, "Hit Enter to confirm", TempModeChanged));
	gs_pTempConfigPage->SetSelectedIndex(gs_nTempMode);	

	UiPasswordPage* pNewPwdScreen;

	pMgr->addNewScreenToEndOfList(new UiContinueMessagePage(ChangePasswordInfoScreen, s_PasswordInfoMessage, 4, ChangePasswordLoginInScreen, PasswordNotChangedScreen));
	pMgr->addNewScreenToEndOfList(new UiPasswordPage(ChangePasswordLoginInScreen, "Current password:", gs_aUserPassword, 20, ChangePasswordHelpScreen, PasswordNotChangedScreen));
	pMgr->addNewScreenToEndOfList(new UiContinueMessagePage(ChangePasswordHelpScreen, s_PasswordHelpMessage, 4, EnterPasswordScreen, PasswordNotChangedScreen));
	pMgr->addNewScreenToEndOfList(pNewPwdScreen = new UiPasswordPage(EnterPasswordScreen, "Enter New password:", NULL, 20, ConfirmPasswordScreen, PasswordNotChangedScreen));
	pMgr->addNewScreenToEndOfList(new UiContinueMessagePage(ConfirmPasswordScreen, s_PasswordConfirmMessage, 4, ReEnterPasswordScreen, PasswordNotChangedScreen));
	pMgr->addNewScreenToEndOfList(new UiPasswordPage(ReEnterPasswordScreen, "Confirm New passwrd:", pNewPwdScreen->enteredPassword(), 20, PasswordChangedScreen, PasswordNotChangedScreen, PasswordChanged));
	pMgr->addNewScreenToEndOfList(new UiContinueMessagePage(PasswordChangedScreen, s_PasswordChangedMessage, 4, SystemMenu, SystemMenu));
	pMgr->addNewScreenToEndOfList(new UiContinueMessagePage(PasswordNotChangedScreen, s_PasswordNotChangedMessage, 4, SystemMenu, SystemMenu));
		
	UiOptionPage* gs_pUsbConfigPage;
	pMgr->addNewScreenToEndOfList(gs_pUsbConfigPage = new UiOptionPage(UsbConfigScreen, "USB Connection:", s_UsbModeOptions, NumUsbModes, "Hit Enter to confirm", UsbModeChanged));
	gs_pUsbConfigPage->SetSelectedIndex(gs_nUsbOpMode);	
	gs_pUsbConfigPage->setupFinalMessage(s_SaveInfoMessage, sizeof(s_SaveInfoMessage) / sizeof(const char*));	

	pMgr->addNewScreenToEndOfList(new UiContinueMessagePage(RebootDamageWarning, s_DamageMessage, 4, RebootScreen, SystemMenu));
	pMgr->addNewScreenToEndOfList(new UiContinueMessagePage(RebootTendingResetWarning, s_TendingResetMessage, 4, RebootScreen, SystemMenu));

	UiOptionPage* gs_pRemoteUiConfigPage;
	pMgr->addNewScreenToEndOfList(gs_pRemoteUiConfigPage = new UiOptionPage(RemoteUiConfigScreen, "Remote UI Connect:", s_RemoteUiOptions, NumRemoteUiModes, "Hit Enter to confirm", RemoteUiModeChanged));
	gs_pRemoteUiConfigPage->SetSelectedIndex(gs_nRemoteUiMode);	
	gs_pRemoteUiConfigPage->setupFinalMessage(s_SaveInfoMessage, sizeof(s_SaveInfoMessage) / sizeof(const char*));	

	pMgr->addNewScreenToEndOfList(gs_pMessagePage = new UiPageMessage(UserMessageScreen, 10));
	
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^
	// Add new screens above here

	UiMenu* pCmdMenu = new UiMenu[NumCommands];
	for(size_t i = 0; i < NumCommands;i++)
	{
		pCmdMenu[i].m_title = gs_CommandList[i].m_pObject;
		pCmdMenu[i].m_index = FirstCommandScreen + i;
	}
	pMgr->addNewScreenToEndOfList(new UiMenuPage(CommandMenu, "Command Menu", pCmdMenu, NumCommands));



	for(size_t i = 0; i < NumCommands;i++)
	{
		pMgr->addNewScreenToEndOfList(new UiPageSelector(FirstCommandScreen + i, &gs_CommandList[i]));
	}

	pMgr->setActivePage(SplashScreen);
}

UiPageMessage* ThunderballUiPages::GetMessagePage()
{
	return gs_pMessagePage;
}

#if 0

# interfaces(5) file used by ifup(8) and ifdown(8)

# loopback network interface
auto lo
iface lo inet loopback

# primary network interface
auto eth0
#iface eth0 inet dhcp
iface eth0 inet static
        address 192.168.2.2
        netmask 255.255.255.0
        gateway 192.168.2.0

#hwaddress ether DE:AD:BE:EF:CA:FE

# wireless network interface
#auto wlan0
#iface wlan0 inet dhcp
#   wpa-ssid "my_wifi_name"
#   wpa-psk  "my_wifi_pass"

#endif