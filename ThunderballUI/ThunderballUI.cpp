#include <iostream>
#include <fstream>
#include <string>
#include <array>
using namespace std;

#include <string.h>
#include <stdio.h>
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
#include <linux/input.h>
#include <getopt.h>

#include "ThunderballUI.h"

#include "TcpDataInterface.h"

#include "GPIOManager.h"
#include "GPIOConst.h"
#include "OLEDDriver.h"
#include "PruSpiInterface.h"

#include "LinuxProcess.h"
#include "PtxMsgHandler.h"
#include "TcpPtxMsgInterface.h"
#include "LinuxDataInterface.h"
#include "LinuxDebug.h"

#include "UiPageMgr.h"
#include "UiPage.h"
#include "ThunderballSystemStats.h"
#include "ThunderballSofcMgr.h"
#include "RollingLogFiles.h"
#include "RemoteOLEDInterface.h"
#include "ThunderballUiPages.h"
#include "UiPage.h"

#ifdef TB_DATA_INTERFACE
#include "ThunderballDataInterface.h"
#endif

ThunderballUI* ThunderballUI::m_pTheApp;
OLEDDriver* ThunderballUI::m_pOLEDDriver;
static int m_bNoConnect;
static int m_bRemoteUiSupport;
static int m_bDataInterface;
static int m_bRemoteUiEnabled;

static const char* gs_szChangeSignalFileName;
static const char* gs_szInputSimFileName;

char* appszShortOptions = (char*)"s:";

option appLongOptions[] =
  {
    /* These options set a flag */

    /* These options are distinguished by their index */
    {"pru-app",   required_argument, 0, 'a'},
    {"noconnect", no_argument, &m_bNoConnect,   1},
    {"unit-id",   required_argument, 0, 'U'},

    {"remoteui", no_argument, &m_bRemoteUiSupport,   1},
    {"dataif", no_argument, &m_bDataInterface,   1},
    {"change-file",   required_argument, 0, 'c'},
    {"sim-file",   required_argument, 0, 'S'},

    {0, 0, 0, 0}
  };

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CatchUnitStatus(int fd, PtxMsgHeader* pHdr)
{
	ThunderballUI::m_pTheApp->m_nUnitId = pHdr->dataId;
	ThunderballSystemStats* pStats = ThunderballSystemStats::UnMarshallStatsPayload(fd);
	{
		static int s_nLastUpTime;
		static int s_nTimeLockedUpCount;
		if (s_nLastUpTime == pStats->m_nSystemUptime)
		{
			s_nTimeLockedUpCount++;
			if (s_nTimeLockedUpCount > 10)
			{
				// exit somehow
				// to force restart
				exit(0);
			}
		}
		else
		{
			s_nTimeLockedUpCount = 0;
			s_nLastUpTime = pStats->m_nSystemUptime;
		}
	}
	if (ThunderballUI::m_pTheApp->m_pLogs)
		ThunderballUI::m_pTheApp->m_pLogs->AppendData((unsigned char*)pStats, sizeof(ThunderballSystemStats));

	if (pStats->m_nTendingState == ThunderballSofcMgr::SystemTending)
	{
		GPIO::GPIOManager::getInstance()->setValue(ThunderballUI::m_pTheApp->m_buttonLedPinId, GPIO::HIGH);
	}
	else if (pStats->m_nTendingState == ThunderballSofcMgr::SystemStopped)
	{
		GPIO::GPIOManager::getInstance()->setValue(ThunderballUI::m_pTheApp->m_buttonLedPinId, GPIO::LOW);
	}

	{
		static char* s_pFifthLine;

		if (!s_pFifthLine)
		{
			s_pFifthLine = new char[24];
				sprintf(s_pFifthLine, "0");
		}
		s_pFifthLine[0] = '0' + (char)(pStats->m_nTendingState);
		ThunderballUI::RemoteDisplayLine(4, s_pFifthLine);
	}
#ifdef TB_DATA_INTERFACE
	if (m_bDataInterface)
	{
		ThunderballDataInterface::processDataChunk(pStats);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ThunderballUI::ThunderballUI() : LinuxProcess()
{
	m_nButtonPressState = NoButtonAction;
	m_nButtonPressCount = 0;
	m_pLogs = 0;
}

int ThunderballUI::mainRun()
{
	// 500 ms repeat

	static int s_nBlink;
	s_nBlink++;

#ifdef TB_DATA_INTERFACE
	if (!m_pIf)
	{
		// feed the beast
		ThunderballDataInterface::handleTick();
	}
#endif

	ThunderballSystemStats* pStats = ThunderballSystemStats::instance();

	if (pStats->m_nTendingState == ThunderballSofcMgr::SystemShuttingDown)
	{
		if (s_nBlink & 1)
			GPIO::GPIOManager::getInstance()->setValue(ThunderballUI::m_pTheApp->m_buttonLedPinId, GPIO::HIGH);
		else
			GPIO::GPIOManager::getInstance()->setValue(ThunderballUI::m_pTheApp->m_buttonLedPinId, GPIO::LOW);
	}

	m_pUiPageMgr->handleTimeout();

	if (m_nButtonPressCount)
	{
		if (m_nButtonPressState != NoButtonAction)
		{
			m_nButtonPressCount--;
			if (!m_nButtonPressCount)
			{
				if (m_nButtonPressState == PressStarted)
				{
					ThunderballUI::m_pTheApp->setTendingState((unsigned short)1);
				}
				else if (m_nButtonPressState == UnPressStarted)
				{
					ThunderballUI::m_pTheApp->setTendingState((unsigned short)0);
				}
				m_nButtonPressState = NoButtonAction;
			}
		}
	}
	else
	{
		m_nButtonPressState = NoButtonAction;
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThunderballUI::processArgs(int argc, char** argv)
{
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThunderballUI::processArgOption(int optionId, const char* arg, int option_index)
{
	switch(optionId)
		{
		case 0:
			// If this option set a flag, do nothing else now.
			if (option_index < (int)inherited::argLongOptionCount())
				inherited::processArgOption(optionId, arg, option_index);
			break;
		case 'a':
			m_szPruAppFileName = optarg;
			break;
		case 'U':
			break;
		case 'c':
			gs_szChangeSignalFileName = arg;
			break;
		case 'S':
			gs_szInputSimFileName = arg;
			break;
			
		default:
			inherited::processArgOption(optionId, arg, option_index);
			break;
		}
}

void ThunderballUI::RemoteDisplayLine(int nLineId, const char* pText)
{
	if (ThunderballUI::m_pTheApp->m_pRemoteOLEDIf)
		ThunderballUI::m_pTheApp->m_pRemoteOLEDIf->DisplayLine(nLineId, pText);
}

void ThunderballUI::RemoteButtonPress(int button, BOOL bDown)
{
	if (button == UiPageMgr::Start)
	{
		if (bDown)
			ThunderballUI::m_pTheApp->setTendingState((unsigned short)1);
		else
			ThunderballUI::m_pTheApp->setTendingState((unsigned short)0);
	}
	else
	{
		ThunderballUI::m_pTheApp->m_pUiPageMgr->handleButtonPress((U8)button, bDown);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThunderballUI::refreshEnvironment()
{
    const char* val = ::getenv( "PTXAPP_REMOTEUIMODE" );
    if ( val == 0 ) {
		m_bRemoteUiEnabled = 0;
    }
	else
	{
		m_bRemoteUiEnabled = atoi(val);
	}
}

void ThunderballUI::setup(unsigned short serverPort, unsigned short serverChannel, unsigned short maxServerDataLength)
{
	inherited::setup(serverPort, serverChannel, 32);	// 32 max transfer to server
	
	if (m_pIf) 
	{
		m_pIf->getHandler()->addHandler(CatchUnitStatus, PtxMsgHeader::PtxDisplayUnitStatus, PtxMsgHandler::IdWildCard, PtxMsgHandler::IdWildCard);
	}

	int pinId = GPIO::GPIOConst::getInstance()->getGpioByKey("P9_26");
	GPIO::GPIOManager::getInstance()->exportPin(pinId);
	GPIO::GPIOManager::getInstance()->setDirection(pinId, GPIO::OUTPUT);

	m_pOLEDDriver = new OLEDDriver(pinId);
	m_pOLEDDriver->InitDisplay(4);

	m_pUiPageMgr = new UiPageMgr(m_pOLEDDriver);
	UiPage::CreatePageList(m_pUiPageMgr, this);
	m_pUiPageMgr->refreshPage();

	m_buttonLedPinId = GPIO::GPIOManager::getInstance()->setupOutputPin("P9_12", GPIO::LOW);

	refreshEnvironment();

	if ((m_bRemoteUiSupport) && (m_bRemoteUiEnabled == 0))
	{
		m_pRemoteOLEDIf = new RemoteOLEDInterface(3400, RemoteButtonPress);
		m_pRemoteOLEDIf->initTcpReceivePort(0);
		addServerDataInterface(m_pRemoteOLEDIf);

		m_pOLEDDriver->SetOtherDisplayOption(ThunderballUI::RemoteDisplayLine);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ButtonInterface : public LinuxDataInterface
{
public:
	ButtonInterface(const char* filename);
	virtual ~ButtonInterface() { ; }

	virtual int configureClientFdSet(fd_set* pfds, int maxfd);
	virtual int processClientFdSet(fd_set* pfds);

	virtual const char* name() { return "ButtonInterface"; }
	void checkInitialState();

protected:
	int m_fd;
	int m_InitialState;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ButtonInterface::ButtonInterface(const char* filename)
{
	//int buttonPinId = GPIO::GPIOManager::getInstance()->setupInputPin("P8_7");
    //m_InitialState = GPIO::GPIOManager::getInstance()->getValue(buttonPinId);	
	//GPIO::GPIOManager::getInstance()->unexportPin(buttonPinId);

	m_fd = open(filename, O_RDONLY);

	// Empirical tested
	char data[256];
	unsigned int val;
	int ret = ioctl (m_fd, EVIOCGKEY(sizeof(data)), data);
	if ((ret > 0) && (data[0] == 2))
		m_InitialState = 1;
	else
		m_InitialState = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ButtonInterface::configureClientFdSet(fd_set* pfds, int maxfd)
{
	if (m_fd == -1) return maxfd;

	FD_SET (m_fd, pfds);	// new connects
	if (maxfd < m_fd)
		maxfd = m_fd;
	return maxfd;
}

void ButtonInterface::checkInitialState()
{
	return;
	if (m_InitialState == 1)
	{
		ThunderballUI::m_pTheApp->setTendingState((unsigned short)1);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ButtonInterface::processClientFdSet(fd_set* pfds)
{
	
	if (m_fd == -1) return 0;


	if (FD_ISSET(m_fd, pfds))
	{
			char buf[16];
		read(m_fd, buf, 16);
		if ((buf[10] > 0) && (buf[10] < 6))
		{

			DBG_PR((LinuxDebug::DebugModule::DM_UI, LinuxDebug::DebugFlag::DF_BUTTONS, "Button %d Pressed %d\n", buf[10], buf[12]));
			{
				if (buf[10] == 1)
				{
					// tending button
					if (buf[12] == 1)
					{
						//ThunderballUI::m_pTheApp->setTendingState((unsigned short)1);
						ThunderballUI::m_pTheApp->m_nButtonPressCount = 4;
						ThunderballUI::m_pTheApp->m_nButtonPressState = ThunderballUI::PressStarted;
					}
					else
					{
						//ThunderballUI::m_pTheApp->setTendingState((unsigned short)0);
						ThunderballUI::m_pTheApp->m_nButtonPressCount = 4;
						ThunderballUI::m_pTheApp->m_nButtonPressState = ThunderballUI::UnPressStarted;
					}
				}
				else
				{
					if (buf[12] == 1)	// Only send it on button press
					{
						ThunderballUI::m_pTheApp->m_pUiPageMgr->handleButtonPress((U8)buf[10], TRUE);

						// button event from us (PtxAppThunderballUI) to the main system
						//PtxMsgHeader hdr(PtxMsgHeader::PtxButtonPressed, PtxMsgHeader::PtxAppThunderballUI);
						//hdr.dataId = buf[10];
						//hdr.length = 0;
						//PtxMsgHandler::sendMessage(ThunderballUI::m_pTheApp->getMainMsgIf()->getFd(), &hdr, hdr.length, (unsigned char*)0);
					}
					else //if (buf[12] == 0)
					{
						ThunderballUI::m_pTheApp->m_pUiPageMgr->handleButtonPress((U8)buf[10], FALSE);
					}
				}
			}
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class TcpConsoleClientInterface : public TcpDataInterface
{
public:

	TcpConsoleClientInterface(unsigned short portnumber);
	virtual ~TcpConsoleClientInterface();

	virtual int processClientFdSet(fd_set* pfds);

	virtual const char* name() { return "TcpConsoleClientInterface"; }

private:
	unsigned char m_inbuf[32];
};

TcpConsoleClientInterface::TcpConsoleClientInterface(unsigned short portnumber) : TcpDataInterface(portnumber)
{
	memset(m_inbuf, 0, sizeof(m_inbuf));
}

TcpConsoleClientInterface::~TcpConsoleClientInterface()
{
}

int TcpConsoleClientInterface::processClientFdSet(fd_set* pfds)
{
	if (FD_ISSET(m_fd, pfds))
	{
		int rd = read(m_fd, m_inbuf, sizeof(m_inbuf));
		if (rd == 0)
		{
			// disconnected
			printf(" >>>> Server disconnected\n");
			close(m_fd);
			m_fd = -1;
			return -1;
		}
		else
		{
			printf("%s", m_inbuf);
			memset(m_inbuf, 0, sizeof(m_inbuf));
		}
	}
	return 0;
}


void ThunderballUI::sendRefreshEnvMsg()
{
	if (ThunderballUI::m_pTheApp->getMainMsgIf())
	{
		PtxMsgHeader hdr(PtxMsgHeader::PtxRefreshEnvironment, ThunderballUI::m_nAppId);
		hdr.dataId = 1;
		hdr.length = 0;
		PtxMsgHandler::sendMessage(ThunderballUI::m_pTheApp->getMainMsgIf()->getFd(), &hdr, hdr.length, (unsigned char*)0);
	}
}

void ThunderballUI::sendResetErrorMsg()
{
	if (ThunderballUI::m_pTheApp->getMainMsgIf())
	{
		PtxMsgHeader hdr(PtxMsgHeader::PtxResetError, ThunderballUI::m_nAppId);
		hdr.dataId = 1;
		hdr.length = 0;
		PtxMsgHandler::sendMessage(ThunderballUI::m_pTheApp->getMainMsgIf()->getFd(), &hdr, hdr.length, (unsigned char*)0);
	}
}

void ThunderballUI::setTendingState(unsigned short  status)
{
	if (ThunderballUI::m_pTheApp->getMainMsgIf())
	{
		PtxMsgHeader hdr(PtxMsgHeader::PtxButtonPressed, ThunderballUI::m_nAppId);
		hdr.dataId =  status;
		hdr.length = 0;
		PtxMsgHandler::sendMessage(ThunderballUI::m_pTheApp->getMainMsgIf()->getFd(), &hdr, hdr.length, (unsigned char*)0);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int updateUILine(int callbackArg, std::vector<int>& intArgs, std::vector<string>& stringArgs, 
					   std::ostringstream& ss, std::string sOriginal)
{
	std::string sText = stringArgs[0];
	int nLine = intArgs[0];

	if (nLine < 4)
	{
		char sNewLine[20];
		snprintf(sNewLine, sizeof(sNewLine), "%s", sText.c_str());

		ss << "New Line " << nLine << " Text: " << sNewLine << "\n";
		ThunderballUI::m_pTheApp->m_pUiPageMgr->overrideLine(nLine, sNewLine, 10);
		return true;
	}
	else
	{
		ss << "Bad Line number (" << nLine << ").  Only 0 through 3 is valid";
		return false;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool customParseString(DebugClientData* pClient, std::string s, std::ostringstream& ss)
{
	return ThunderballUI::processCommandData(s, ss);
}

static CommandEntry m_Commands[] = 
{
	{ "updateline", { CommandEntry::ArgTypeString, CommandEntry::ArgTypeInt, CommandEntry::ArgTypeNone, CommandEntry::ArgTypeNone}, 
	  updateUILine, 0, 
	  "updateline           update a line on the UI\n    updateline <\"text\"> <linenumber (zero based)>" },

	// last entry
	{ 0, { CommandEntry::ArgTypeNone, CommandEntry::ArgTypeNone, CommandEntry::ArgTypeNone, CommandEntry::ArgTypeNone}, 
	  0, 0, "" }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern "C" int main(int argc, char **argv)
{
	int retval = 0;
	int daemonErr = 0;

	/*	
	LinuxDebug::setModule(LinuxDebug::DebugModule::DM_LINUXPLATFORM);
	LinuxDebug::setFlag(LinuxDebug::DebugFlag::DF_INIT);

	LinuxDebugObject dbo(LinuxDebug::DebugModule::DM_LINUXPLATFORM, LinuxDebug::DebugFlag::DF_INIT);
		dbo << "12\n34" << LinuxDebugObject::endline;
	*/
	LinuxDebug::setSendToStdOutFlag(true);
	LinuxDebug::setCustomParseFunc(customParseString);

	ThunderballUI::m_pTheApp = new ThunderballUI();
	ThunderballUI::m_pTheApp->processArgs(argc, argv);
	ThunderballUI::m_pTheApp->setCommandList(m_Commands);

	ButtonInterface* pButtons = new ButtonInterface("/dev/input/event1");
	ThunderballUI::m_pTheApp->addClientDataInterface(pButtons);


	PruSpiInterface::init(ThunderballUI::m_pTheApp->m_szPruAppFileName);

	printf("UI started: %s\n", ThunderballUI::m_pTheApp->IpAddress());

	if (ThunderballUI::m_pTheApp->dataLogPath())
	{
		ThunderballUI::m_pTheApp->m_pLogs = new RollingLogFiles(ThunderballUI::m_pTheApp->dataLogPath(), "Data", 10, 0x400);
		ThunderballUI::m_pTheApp->m_pLogs->Load();
	}

	if (m_bNoConnect)
	{
		ThunderballUI::m_pTheApp->m_pPtxSerialIf = 0;
		ThunderballUI::m_pTheApp->setup(0, 1, 0); 
	}
	else
	{
		ThunderballUI::m_pTheApp->m_pPtxSerialIf = new TcpConsoleClientInterface(3309);
		ThunderballUI::m_pTheApp->m_pPtxSerialIf->initTcpSendPort("127.0.0.1");
		ThunderballUI::m_pTheApp->addClientDataInterface(ThunderballUI::m_pTheApp->m_pPtxSerialIf);
		ThunderballUI::m_pTheApp->setup(ThunderballUI::m_pTheApp->m_nPortBase + ThunderballUI::MainAppPtxMessagePortOffset, 1, 0); 
		//ThunderballUI::m_pTheApp->setup(ThunderballUI::m_pTheApp->m_nPortBase + ThunderballUI::MainAppPtxMessagePortOffset, 1, 0); 
	}

#ifdef TB_DATA_INTERFACE
	if (m_bDataInterface)
	{
		ThunderballDataInterface::setup(ThunderballUI::m_pTheApp, gs_szChangeSignalFileName, gs_szInputSimFileName);
	}
#endif

	pButtons->checkInitialState();

	printf("Ip Addr: %s\n", ThunderballUI::m_pTheApp->IpAddress());
	ThunderballUI::m_pTheApp->mainLoop(500);

	return 0;
}

void ThunderballUI::SendSerialData(unsigned char* pData, size_t length)
{
	if (m_pPtxSerialIf)
	{
		m_pPtxSerialIf->SendData(pData, length);
	}
}


////////////////////////////////////////////////////////////////////////////////////
// New Code

void ThunderballUI::showUIMessage(const char* pMsg, int nTimeout)
{
	UiPageMessage* pPage = ThunderballUiPages::GetMessagePage();
	pPage->setLineMsg("", 0);
	pPage->setLineMsg("", 1);
	pPage->setLineMsg(pMsg, 2);
	pPage->setLineMsg("", 3);
	pPage->setTimeout(nTimeout);
	pPage->setLineBlinkMask((1 << 2));	
	m_pUiPageMgr->changePage(pPage->Index(), m_pUiPageMgr->ActivePage()); 
}

