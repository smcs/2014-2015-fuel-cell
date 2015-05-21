#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <sstream>
using namespace std;using namespace std;

#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/timerfd.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <getopt.h>
#include <assert.h>

#include "TcpDataInterface.h"
#include "TcpSerialInterface.h"
#include "PtxMsgHandler.h"
#include "TcpPtxMsgInterface.h"

#include "LinuxProcess.h"
#include "LinuxDebug.h"
#include "AppMainSetup.h"
#include "ThunderballSystemStats.h"
#include "AdcBankMgr.h"
#include "ThunderballConfigAdc.h"
#include "ThunderballSofcMgr.h"
#include "ThunderballConfigControls.h"
#include "LinuxBufferQueue.h"
#include "TableCalMon.h"
#include "PruPwmInterface.h"
#include "StorageMgr.h"
#include "ProductStorageMgr.h"
#include "ObjectMgr.h"
#include "ObjectStore.h"
#include "TextFile.h"

class ThunderballProcess : public LinuxProcess
{
public:

	ThunderballProcess();
	virtual ~ThunderballProcess() { ; }

	void processArgs(int argc, char** argv);
	void processArgOption(int optionId, const char* arg, int option_index);
	void setup(unsigned short serverPort, unsigned short serverChannel, unsigned short maxServerDataLength);
	int mainRun();
	void broadcastStats();
	void refreshEnvironment();

	int m_fd_DiplayProcConnection;
	ThunderballSofcMgr* m_pSofcMgr;

protected:
	typedef LinuxProcess inherited;

	TcpPtxMsgInterface* m_thePtxInstance;

	const char* m_szSpiDeviceFileName;
	const char* m_szSimFileName;
	const char* m_szPruAppFileName;
	const char* m_szParameterFileName;

public:
	static ThunderballProcess* m_pTheApp;
	const char* paramFile() { return m_szParameterFileName; }
};

ThunderballProcess* ThunderballProcess::m_pTheApp;

ThunderballProcess::ThunderballProcess() : LinuxProcess()
{
	m_szParameterFileName = "./Parameter.data";
}

int ThunderballProcess::mainRun()
{
	return 0;
}

char* appszShortOptions = (char*)"s:T:a:P:";

option appLongOptions[] =
  {
    /* These options set a flag */

    /* These options are distinguished by their index */
    {"spidev",   required_argument, 0, 's'},
    {"tcsimfile",   required_argument, 0, 'T'},
    {"pru-app",   required_argument, 0, 'a'},
    {"paramfile",   required_argument, 0, 'R'},

    {0, 0, 0, 0}
  };

void ThunderballProcess::processArgs(int argc, char** argv)
{
	// some ugly combining code
	int nComboOptionCount  = (sizeof(appLongOptions) / sizeof(option)) - 1 + inherited::argLongOptionCount();
	option* comboOptions = new option[nComboOptionCount + 1];
	memcpy(comboOptions, inherited::m_aLongOptions, inherited::argLongOptionCount() * sizeof(option));
	memcpy(&comboOptions[inherited::argLongOptionCount()], appLongOptions, sizeof(appLongOptions));
	size_t nShortStringLen = strlen(inherited::m_szShortOptions);
	char* comboShort = new char[nShortStringLen + strlen(appszShortOptions) + 1];
	strcpy(comboShort, inherited::m_szShortOptions);
	strcpy(&comboShort[nShortStringLen], appszShortOptions);

	int c;
	int option_index = 0;
	while ((c = getopt_long(argc, argv, comboShort, comboOptions, &option_index)) != -1)
	{
		processArgOption(c, optarg, option_index);
	}
}

void ThunderballProcess::processArgOption(int optionId, const char* arg, int option_index)
{
	switch(optionId)
	{
	case 0:
		// If this option set a flag, do nothing else now.
		if (option_index < (int)inherited::argLongOptionCount())
			inherited::processArgOption(optionId, arg, option_index);
		break;
	case 's':
		m_szSpiDeviceFileName = arg;
		break;
	case 'T':
		m_szSimFileName = arg;
		break;
	case 'a':
		m_szPruAppFileName = optarg;
		break;
	case 'R':
		m_szParameterFileName = optarg;
		break;
	default:
		inherited::processArgOption(optionId, arg, option_index);
		break;
	}
}

void CatchDisplayProcInitMsg(int fd, PtxMsgHeader* pHdr)
{
	DBG_PR((LinuxDebug::DebugModule::DM_UI, LinuxDebug::DebugFlag::DF_MSGS, "UI Message %d\n", pHdr->msgId));

	ThunderballProcess::m_pTheApp->m_fd_DiplayProcConnection = fd;
	PtxMsgHandler::ReadPayloadToDevNull(fd, pHdr);
}

#include "TimerMgr.h"

static void broadcastStatusTimerExpired(void* pData, EventSize nData)
{
	((ThunderballProcess*)pData)->broadcastStats();
}

void ThunderballProcess::broadcastStats()
{
	if (m_fd_DiplayProcConnection != -1)
	{
		PtxMsgHeader hdr(PtxMsgHeader::PtxDisplayUnitStatus, PtxMsgHeader::PtxAppThunderball);
		hdr.dataId = 0;
		hdr.length = sizeof(ThunderballSystemStats);

		// get stats from converters and controls
		ThunderballConfigControls::updateStats();

		ThunderballSystemStats::instance()->m_nFuelPressure = ThunderballConfigAdc::s_SystemAdcs[ThunderballConfigAdc::ADC_CHAN_PRESSURE_SENSE]->input();
		ThunderballSystemStats::instance()->m_nBoardTemperature = ThunderballConfigAdc::s_SystemAdcs[ThunderballConfigAdc::ADC_CHAN_BOARD_TEMP]->input();
		ThunderballSystemStats::instance()->m_nAmbientTemperature = ((TableCalMon*)ThunderballConfigAdc::s_SystemAdcs[ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM1])->getCalculatedInput();
		ThunderballSystemStats::instance()->m_nBatteryTemperature = ((TableCalMon*)ThunderballConfigAdc::s_SystemAdcs[ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM2])->getCalculatedInput();

		ThunderballSystemStats::instance()->m_nCOSense = ThunderballConfigAdc::s_SystemAdcs[ThunderballConfigAdc::ADC_CHAN_CO_DETECT_SENSE]->input();
		ThunderballSystemStats::instance()->m_nPropaneSense = ThunderballConfigAdc::s_SystemAdcs[ThunderballConfigAdc::ADC_CHAN_PROPANE_DETECT_SENSE]->input();

		PtxMsgHandler::sendMessage(m_fd_DiplayProcConnection, &hdr, hdr.length, (unsigned char*)ThunderballSystemStats::instance());
	}

	TimerMgr::addTimer(2000, broadcastStatusTimerExpired, this);
}


void CatchInitMessages(int fd, PtxMsgHeader* pHdr)
{
	assert(pHdr->applicationId > 0);

	PtxMsgHandler::ReadPayloadToDevNull(fd, pHdr);

	if (pHdr->applicationId == PtxMsgHeader::PtxAppThunderballUI)
	{
		printf("Caught Init Message from UI\n");

		ThunderballProcess::m_pTheApp->m_fd_DiplayProcConnection = fd;
		// starts stats update to ui
		ThunderballProcess::m_pTheApp->broadcastStats();
	}
	else
	{
		printf("Caught Init Message from unknown: %d\n", pHdr->applicationId);
	}	
}

void CatchDisplayProcMsgs(int fd, PtxMsgHeader* pHdr)
{
	PtxMsgHandler::ReadPayloadToDevNull(fd, pHdr);

	if (pHdr->msgId == PtxMsgHeader::PtxRefreshEnvironment)
	{
		printf("RE-sourcing environment\n");
		ThunderballProcess::m_pTheApp->sourceEnvironemnt();
		ThunderballProcess::m_pTheApp->refreshEnvironment();
	}
	else if (pHdr->msgId == PtxMsgHeader::PtxResetError)
	{
		printf("Clearing error\n");
		ThunderballProcess::m_pTheApp->m_pSofcMgr->ClearSystemError();
	}
	else if (pHdr->msgId == PtxMsgHeader::PtxButtonPressed)
	{
		printf("Button Press %d\n", pHdr->dataId);
		ThunderballProcess::m_pTheApp->m_pSofcMgr->pressButton(pHdr->dataId);
	}
	else
	{
		//MicroGrid3OneWireInterface::setRemoteUiDescriptor(fd);
	}
}

void ThunderballProcess::refreshEnvironment()
{
	int batType = 0;
	int opMode = 0;
	int tempMode = 0;

    const char* val = ::getenv( "PTXAPP_BATTERYTYPE" );
    if ( val == 0 ) {
		batType = 0;
    }
	else
	{
		batType = atoi(val);
	}

    val = ::getenv( "PTXAPP_FUELCELLOPMODE" );
    if ( val == 0 ) {
		opMode = 0;
    }
	else
	{
		opMode = atoi(val);
	}

    val = ::getenv( "PTXAPP_BATTERYTEMPMODE" );
    if ( val == 0 ) {
		tempMode = 0;
    }
	else
	{
		tempMode = atoi(val);
	}

	printf("Reconfiguring battery\n");
	ThunderballConfigControls::selectBattery(batType, opMode == ThunderballSofcMgr::HybridizationMode, tempMode);

	m_pSofcMgr->setOpMode(opMode);

	val = ::getenv( "PTXAPP_FUELTEMPMODE" );
    if ( val == 0 ) {
		tempMode = 0;
    }
	else
	{
		tempMode = atoi(val);
	}
	m_pSofcMgr->setFuelTempMode(tempMode);
}

void ThunderballProcess::setup(unsigned short serverPort, unsigned short serverChannel, unsigned short maxServerDataLength)
{
	PruPwm* pPwm = new PruPwm(m_szPruAppFileName, 1);

	inherited::setup(serverPort, serverChannel, 32);	// 32 max transfer to server

	int nFuelTempSense;
    const char* val = ::getenv( "PTXAPP_FUELTEMPSENSE" );
    if ( val == 0 ) {
		// default use ambient inside fuel cell case
		nFuelTempSense = ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM1;
    }
	else if (!strcmp(val, "Remote")) 
	{
		nFuelTempSense = ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM2;
		printf("Using Remote temperature sense\n");
	}
	else
	{
		// default use ambient inside fuel cell case
		nFuelTempSense = ThunderballConfigAdc::ADC_CHAN_EXTERN_THERM1;
	}

	int nRemoteSense;
    val = ::getenv( "PTXAPP_REMOTEBATTERYVOLTAGE" );
    if ( val == 0 ) {
		// default use No remote sense
		nRemoteSense = 0;
    }
	else
	{
		nRemoteSense = atoi(val);
	}

	{
		m_thePtxInstance = new TcpPtxMsgInterface(m_nPortBase + MainAppPtxMessagePortOffset);
		m_thePtxInstance->initTcpReceivePort(0);
		addServerDataInterface(m_thePtxInstance);

		// order for now matters
		m_thePtxInstance->getHandler()->addHandler(CatchInitMessages, PtxMsgHeader::PtxInitProcessMsg, PtxMsgHeader::PtxAppIdWildCard, PtxMsgHandler::IdWildCard);
		m_thePtxInstance->getHandler()->addHandler(CatchDisplayProcMsgs, PtxMsgHandler::IdWildCard, PtxMsgHeader::PtxAppThunderballUI, PtxMsgHandler::IdWildCard);
	}

	TcpSerialInterface* theInstance = new TcpSerialInterface(3309);
	theInstance->initTcpReceivePort(0);
	addServerDataInterface(theInstance);

    const char* snId = ::getenv( "PTX_SERIALNUMBER" );
    const char* hzId = ::getenv( "PTX_HOTZONE_ID" );
    const char* verId = ::getenv( "PTX_APPVERSION" );

	m_pSofcMgr = new ThunderballSofcMgr(snId, hzId, verId);
	LinuxGpioBufferQueue* pQ = new LinuxGpioBufferQueue("/dev/spidev1.0");

	ThunderballConfigAdc::initSpiBankAdcs(pQ, ThunderballConfigAdc::s_SystemAdcs, m_pSofcMgr, appPath(), nFuelTempSense);
	ThunderballConfigControls::initControls(pQ, ThunderballConfigAdc::s_SystemAdcs, m_pSofcMgr, nRemoteSense);

	refreshEnvironment();

	m_pSofcMgr->setup(NULL, pPwm);

	if (m_szSimFileName)
		m_pSofcMgr->setupSimFile(m_szSimFileName);

	AdcBankMgr::startConverting();

}

int processStatus(int callbackArg, std::vector<int>& intArgs, std::vector<string>& stringArgs, 
					   std::ostringstream& ss, std::string sOriginal)
{
	ss << "There is status" << std::endl;
	return true;
}

static CommandEntry m_Commands[] = 
{
	{ "fcinfo", { CommandEntry::ArgTypeNone, CommandEntry::ArgTypeNone, CommandEntry::ArgTypeNone, CommandEntry::ArgTypeNone}, 
	  processStatus, 0, 
	  "fcinfo           show system status\n    fcinfo" },

	// last entry
	{ 0, { CommandEntry::ArgTypeNone, CommandEntry::ArgTypeNone, CommandEntry::ArgTypeNone, CommandEntry::ArgTypeNone}, 
	  0, 0, "" }
};

static bool customParseString(DebugClientData* pClient, std::string s, std::ostringstream& ss)
{
	return LinuxProcess::processCommandData(s, ss);
}

extern "C" int main(int argc, char **argv)
{

	{
		// platform setup stuff
		AppMainSetup();

		LinuxDebug::setCustomParseFunc(customParseString);

		ThunderballProcess::m_pTheApp = new ThunderballProcess();
		ThunderballProcess::m_pTheApp->processArgs(argc, argv);
		ThunderballProcess::m_pTheApp->setCommandList(m_Commands);

		LinuxDebugObject dbo(LinuxDebug::DebugModule::DM_APPLICATION, LinuxDebug::DebugFlag::DF_TRACE);
		dbo << "------------- Configuring Thunderball process ----------" << dbo.endline;

		ThunderballProcess::m_pTheApp->setup(0, 0, 0); // - use for standalone
		StorageMgr::LoadFiles();

		dbo << "------------- Starting Main loop of Thunderball process ----------" << dbo.endline;
	}

	ThunderballProcess::m_pTheApp->MainPtxLoop();

	//ThunderballProcess::m_pTheApp->mainLoop(1000);

	return 0;
}

#include "AppEnums.h"
static void WriteParameters(EnumType *pnEnumId, S32 nValue)
{

	if (((pnEnumId[0] == SysEnumSystem) && (pnEnumId[1] == AppEnumTotalUptime)) ||
		((pnEnumId[0] == SysEnumSystem) && (pnEnumId[1] == AppEnumPowerCycles)))
	{
		// skip uptime stuff
		return;
	}

	char buffer[256];

	size_t len = ObjectMgr::createObjectString(pnEnumId, buffer, sizeof(buffer));
	sprintf(&buffer[len], " %d\n", nValue);
		
	printf("Writing to file: %s", buffer);

	AppendTextFile ofs(ThunderballProcess::m_pTheApp->paramFile());
	ofs.append((const char*)buffer);

#if 0
	std::ofstream ofs;
	ofs.open ("./Parameter.data", std::ofstream::out | std::ofstream::app);
	if (ofs.good())
	{
		ofs << buffer;
		ofs.close();
	}

	FILE *fp = fopen ("./Parameter.data", "a");
	if (fp != -0)
	{
		fprintf(fp, buffer, strlen(buffer));
		fclose(fp);
	}
	else
	{
		perror("Open Param file");
	}
#endif
}

StorageMgr::StorageResult StorageMgr::LoadFiles()
{
	ObjectStore::setupWriteObjectFunc(WriteParameters);

	// Parameter data read
	printf("Opening Param File: %s\n", ThunderballProcess::m_pTheApp->paramFile());

	TextFile infile(ThunderballProcess::m_pTheApp->paramFile());
	std::string line;
	while(1)
	{
		line = infile.nextLine();
		if (line.size() == 0) break;

		printf("Setting: %s\n", line.c_str());
		line = "set " + line;
		ObjectMgr::processCommand((char*)line.c_str(), -1, -1);
	}
#if 0
	int len;
	FILE* fp = fopen ("./Parameter.data", "r");
	
	if (fp != NULL)
	{
		char line[128];
		strcpy(line, "set ");

		do {
			len = 0;
			char*ptr = fgets(&line[4], sizeof(line) - 4, fp);
			if (ptr != NULL)
			{
				len = strlen(line);
				while ((line[len-1] < 32) && (len > 4))
				{
					len--;
					line[len] = 0;
				}
				if (len > 4)
				{
					ObjectMgr::processCommand((char*)line, -1, -1);
				}
			}
		} while (len > 0);
	}
#endif
	printf("Load files complete\n");
	return StorageMgr::SrOk;
}
