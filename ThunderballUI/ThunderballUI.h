#ifndef MICROGRID_UI_H
#define MICROGRID_UI_H
#include <string>

#include "Types.h"
#include "LinuxProcess.h"

class UiPageMgr;
class MemoryDevice;
class QueueBuffer;
class OLEDDriver;
class TcpDataInterface;
class RollingLogFiles;
class RemoteOLEDInterface;

class ThunderballUI : public LinuxProcess
{
public:
	ThunderballUI();
	virtual ~ThunderballUI() { ; }

	void processArgs(int argc, char** argv);
	void processArgOption(int optionId, const char* arg, int option_index);
	void setup(unsigned short serverPort, unsigned short serverChannel, unsigned short maxServerDataLength);
	int mainRun();
	void sendRefreshEnvMsg();
	void sendResetErrorMsg();
	void setTendingState(unsigned short bOn);
	void refreshEnvironment();
	void showUIMessage(const char* pMsg, int nTimeout);
	void SendSerialData(unsigned char* pData, size_t length);

	const char* m_szPruAppFileName;
	static OLEDDriver* m_pOLEDDriver;
	static char* m_IpAddress;

	static void RemoteDisplayLine(int nLineId, const char* pText);
	static void RemoteButtonPress(int button, BOOL bDown);

	UiPageMgr* m_pUiPageMgr;
	void SendDataToNode(std::string msg);
	TcpDataInterface* m_pPtxSerialIf;
	int m_buttonLedPinId;
	RollingLogFiles* m_pLogs;

	enum {
		NoButtonAction,
		PressStarted,
		UnPressStarted
	};
	int m_nButtonPressState;
	int m_nButtonPressCount;

protected:
	typedef LinuxProcess inherited;

	MemoryDevice* m_pDev;
	int m_nReadLen;
	QueueBuffer* m_pBuf;
	RemoteOLEDInterface* m_pRemoteOLEDIf;

public:
	static ThunderballUI* m_pTheApp;
};

#endif