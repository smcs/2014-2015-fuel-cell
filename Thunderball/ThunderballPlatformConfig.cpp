///////////////////////////////////////////////////////////////////////////////
// ThunderballPlatformConfig.cpp
//
// (C) Protonex Technology Corporation
// The Next Generation of Portable Power(TM)
// 153 Northboro Road
// Southborough, MA 01772-1034
//
// NOTICE: This file contains confidential and proprietary information of
// Protonex Technology Corporation.
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string>

#include "PlatformConfig.h"

#include "ExtCommConfig.h"
#include "AppEnums.h"
#include "OutStreamInterface.h"
#include "BaseObject.h"
#include "TimerMgr.h"
#include "Heap.h"
#include "ObjectList.h"
#include "ObjectMgr.h"
#include "TftpDataMgr.h"
#include "Debug.h"
#include "SysStats.h"

#include "AdcBankMgr.h"
#include "ThunderballConfigAdc.h"
#include "RawDevices.h"
#include "BufferQueue.h"
#include "FcAssert.h"
#include "StorageMgr.h"
#include "ThunderballSofcMgr.h"
#include "ThunderballConfigControls.h"
#include "LinuxBufferQueue.h"
#include "AdcMonitor.h"
#include "TableCalMon.h"
#include "ThunderballSimulation.h"

PlatformConfigBlock* PlatformConfig::getConfigBlock()
{
	static PlatformConfigBlock s_cfgBlock =
	{
#ifdef SIMCODE
		96,												// U16     m_nNumTimers;
		128,											// U16     m_nNumEvents;
		150,											// U16     m_nNumObjects;
		70,												// U16     m_nNumPackets; //this used to be buffers //48
#else
#ifndef USE_INTERNAL_RAM_ONLY
		72,												// U16     m_nNumTimers;
		128,											// U16     m_nNumEvents;
		150,											// U16     m_nNumObjects;
		70,												// U16     m_nNumPackets; //this used to be buffers //48
#else
		8,												// U16     m_nNumTimers;
		32,												// U16     m_nNumEvents;
		32,												// U16     m_nNumObjects;
		3,												// U16     m_nNumPackets;
#endif
#endif
		0,												// U16     m_nQueueDepth		-- Not needed - no ISSC
		600		// DON't lower 							// U16	   m_nPacketSize
	};

	return &s_cfgBlock;
}

int sampleButtonState();

struct blurg : public BaseObject
{
private:
    typedef BaseObject inherited;

public:
	static ThunderballSimulation* s_pSim;

    void appSetup(EnumType nEnumId, void(*fpAddObject)(BaseObject* pObject))
    {
        SETUP_VTABLE(nEnumId);
        if(fpAddObject) fpAddObject(this);
    }

	OS_TEXT static BaseObject::ReplyCode get(BaseObject* pThis, EnumType nEnumId, ObjectCommandData* pCmdData)
    {
	    S32 nVal = 0;

		switch (nEnumId)
		{
		case AppEnumDebug0:
			nVal = sampleButtonState();
			break;
		case AppEnumDebug1:
			break;
        default:
            return inherited::get(pThis, nEnumId, pCmdData);
		}
		pCmdData->setS32Value(nVal);
		return BaseObject::ReplyErrNone;
    }

	OS_TEXT static BaseObject::ReplyCode set(BaseObject* pThis, EnumType nEnumId, ObjectCommandData* pCmdData)
    {
        S32 nValue = pCmdData->getS32Value();
		switch (nEnumId)
		{
#ifdef ADC_MIN_MAX_SUPPORT
	case AppEnumAdcMinMaxClear:
		{
			for(int i = 0 ; i < ThunderballConfigAdc::MAX_ADC_CHANNELS;i++)
			{
				if (ThunderballConfigAdc::s_SystemAdcs[i])
				{
					ThunderballConfigAdc::s_SystemAdcs[i]->m_nRawMin = 0xffff;
					ThunderballConfigAdc::s_SystemAdcs[i]->m_nRawMax = 0;
#ifdef ADC_NOISE_CHECKING
					ThunderballConfigAdc::s_SystemAdcs[i]->m_nNoiseHits = 0;
#endif
				}
			}
		}
		return BaseObject::ReplyErrNone;
#endif
        default:
            return inherited::set(pThis, nEnumId, pCmdData);
        }
	    return BaseObject::ReplyErrNone;
    }
};

ThunderballSimulation* blurg::s_pSim;

void PlatformConfig::preRunInit()
{
	// Protonex platform stuff

	enum {
		SmallMaxBufferSize = 72,
		MaxBufferSize = 0x200,	// for encoding on TFTP we need twice the block transfer just in case.
		BufferPoolSize = (sizeof(QueueBuffer) - QueueBuffer::MIN_BUFFER_LENGTH + MaxBufferSize) * 4,
		SmallBufferPoolSize = (sizeof(QueueBuffer) - QueueBuffer::MIN_BUFFER_LENGTH + SmallMaxBufferSize) * 6,
	};
	char* pBufferPool = new char[BufferPoolSize];
	QueueBuffer::init(pBufferPool, BufferPoolSize, MaxBufferSize);

	char* s_aSmallBufferPool = new char[SmallBufferPoolSize];
	QueueBuffer::initSmallBuffer(s_aSmallBufferPool, SmallBufferPoolSize, SmallMaxBufferSize);

	ExternalMsgFormatBlock *pBlocks;
    U8 nNumBlocks = ExtCommConfig::getBuildSpecificMsgFormatBlocks(pBlocks);
	ObjectMgr::setMsgFormatBlock(pBlocks, nNumBlocks);
	
	// functions exist on OutStreamInterface
	BaseObject::setOutputFunction(writeStreamedData);
	BaseObject::setLogFunction(logToStream);

#ifdef AVR_DEBUG
	Debug::init(nOutStreamIfIndex, writeStreamedData, dumpHexToConsoleCinterface);
	Debug::setModule(DM_MEMORY);
	Debug::setModule(DM_TASKS);
	Debug::setFlag(DF_STATS);
	//Debug::setFlag(DF_RX);
	//Debug::setFlag(DF_TX);
	Debug::setFlag(DF_MSGS);
#endif

	// --------- do this at the end -------------------- 
	// This loads up file system and runs startup batch file

	// display progress maybe
//	if (StorageMgr::LoadFiles() == StorageMgr::SrInvalidData)
	{
		// do something
		// MainObject::instance()->forceFatalError(SysState::FatalErrorConfigurationError, SysState::FatalErrorFactoryClear);
	}

	// change progress

	// MainObject::prepareDevicesForInterrupts();

	// ... last chance before ints and events started

	// MainObject::lastChanceInit();

	blurg* b = new blurg();
    b->appSetup(AppEnumDebugObject, ObjectList::addObject);

	return;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%555
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%555
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%555

#include <stdio.h>
#include <string.h>

static void CalClkTime(char *pDestBuffer, size_t nDestBufferSize, S32 nValue, S32 nScale)
{
	snprintf(pDestBuffer, nDestBufferSize, "%d:%d:%d", (nValue >> 16) & 0xff, (nValue >> 8) & 0xff, nValue & 0xff);
}

static void CalClkDate(char *pDestBuffer, size_t nDestBufferSize, S32 nValue, S32 nScale)
{
	snprintf(pDestBuffer, nDestBufferSize, "%d/%d/%d", (nValue >> 16) & 0xff, (nValue >> 8) & 0xff, nValue & 0xff);
}

//////////////////////////////////////////////////////////////////////////
// Function     : getBuildSpecificMsgFormatBlocks
// Scope        : Public
// Description  : 
//////////////////////////////////////////////////////////////////////////
U8 ExtCommConfig::getBuildSpecificMsgFormatBlocks(ExternalMsgFormatBlock* &pBlocks)
{
    static ExternalMsgFormatBlock s_FormatBlocks[] =
    {
		{
			AppEnumCalClkMgr,						// Base Enum
            AppEnumDate,						// Leaf Enum
            CalClkDate					// Formatter
		},
		{
			AppEnumCalClkMgr,						// Base Enum
            AppEnumTime,						// Leaf Enum
            CalClkTime					// Formatter
		},
    };

    pBlocks = s_FormatBlocks;
    return (U8)(sizeof(s_FormatBlocks) / sizeof(ExternalMsgFormatBlock));
}   // end 
