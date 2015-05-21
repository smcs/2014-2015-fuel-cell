#include <sstream>
#include <string>
using namespace std;

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
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
#include <linux/i2c-dev.h>

#include "ThunderballDataInterface.h"
#include "ThunderballDbInterface.h"
#include "ThunderballSystemStats.h"
#include "NodeJsInterface.h"
#include "ThunderballUI.h"
#include "ArgParser.h"
#include "TextFile.h"

NodeJsInterface* ThunderballDataInterface::m_NodeJsInstance;
ThunderballUI* ThunderballDataInterface::m_pMyProc;
const char* ThunderballDataInterface::m_szInputSimFileName;

bool ThunderballDataInterface::processNodeData(std::string s, std::ostringstream& ss)
{
	// Stock platform callback for handling data from connecting processes
	// It was not used in the first release of the software

	///////////////////////////////////////////////////////////////////////
	// NEW code follows
	printf("IN:%s\n", s.c_str());

	std::string arg;
	ArgParser parser(s);		// processes quotes
	do {
		arg = parser.nextArg(':');
		if (!arg.empty())
		{
			if (arg == "log")
			{
				ThunderballDbInterface::AddLogEntry(1, 1, parser.remaining().c_str());
			}
			else if (arg == "start")
			{
				m_pMyProc->setTendingState(1);
				m_pMyProc->showUIMessage("Starting", 10);
			}
			else if (arg == "stop")
			{
				m_pMyProc->setTendingState(0);
			}
			// add more commands here
			else
			{
				printf("Unknown command %s\n", arg.c_str());
			}
		}
		break;	// single arg for now
	} while (!arg.empty());

	return false;
}

void ThunderballDataInterface::setup(ThunderballUI* pMyProc, const char* szSignalFileName, const char* szInputSimFileName)
{
	// Stock platform code for creating an TCP server interface to allow
	// remote process to connect to this one
	// It was not used in the first release of the software
	m_pMyProc = pMyProc;
	m_NodeJsInstance = new NodeJsInterface(3300, ThunderballDataInterface::processNodeData);
	m_NodeJsInstance->initTcpReceivePort(0);
	pMyProc->addServerDataInterface(m_NodeJsInstance);

	// connect to database
	// TODO-FIX hard coded user info for now
	ThunderballDbInterface::connectToDatabase("webuser", "webuser", "Thunderball", szSignalFileName);


	///////////////////////////////////////////////////////////////////////
	// NEW code follows

	// squirrel away input sim file name
	m_szInputSimFileName = szInputSimFileName;

	// may be make some test code to test parsing of it

	// this sets up a connection to the statistics database
	ThunderballDbInterface::setupSqlDatabase();

	// one could read the data here to create some aggregation structures in C/C++
	// OR one could do that from node.js in Javascript
}

void ThunderballDataInterface::processDataChunk(ThunderballSystemStats* pStats)
{
	// This interface is supported by the platform to allow
	// accesss to real time data (in the pStats struct)
	// It was not used in the first release of the software

	// One could aggregate this data and add to a table in the database

	///////////////////////////////////////////////////////////////////////
	// NEW code follows

	// this sends a human readable string to the node.js application 
	// the 'stats' followed by the delimeter '|' is the type of data
	// The ';' indcates the end of the data
	std::ostringstream ss;
	ss << "stats|Some,Fuel,Cell,Data," << pStats->m_nSystemUptime << ";";

	// General call to send string data to node
	m_NodeJsInstance->sendData(ss.str().c_str());
}

static double convertFromString(std::string s)
{
	double numberArg;
	if ( (std::istringstream(s) >> numberArg) ) 
	{
		return numberArg;
	}
	return 0;
}

void ThunderballDataInterface::handleTick()
{
	static int gs_nCount;
	if (gs_nCount < 1)
	{
		if (m_szInputSimFileName)
		{
			StringDelimFile file(m_szInputSimFileName);

			// get column headings
			std::vector<std::string>tokens = file.nextData(',');

			// allocate space
			double* ArrayOfArgs = new double[tokens.size()];
			for(int i = 0; i != tokens.size(); i++)
			{
				ArrayOfArgs[i] = 0;
			}
			double power, fuelpounds, voltage, current, fuelflow = 0, anodeflow, pbflow, energy = 0, lastuptime = 0, uptime, lastintervalstarttime = 0;
			double interval = 3600;
			power = 1;
			for(int j = 0; j < 500000; j++)
			{
				// get next data line from file
				std::vector<std::string>values = file.nextData(',');

				if (values.size() == 0) break;
				// Loop over ten rows
				
				for (std::vector<std::string>::size_type i = 0; i != values.size(); i++)
				{
					if (tokens[i] == "Date")
					{
						// string
					}
					else if (tokens[i] == "Time")
					{
						// string
					}
					else if (tokens[i] == "m_nDeviceState")
					{
						// string
					}
					else if (tokens[i] == "m_AnodeFuelFlow"){
						anodeflow = convertFromString(values[i]);
					}
					else if (tokens[i] == "m_PBFuelFlow"){
						pbflow = convertFromString(values[i]);
					}
					else if (tokens[i] == "m_nSystemUptime"){
						uptime = convertFromString(values[i]);
					}
					else if (tokens[i] == "m_LoadVoltage"){
						voltage = convertFromString(values[i]);
					}
					else if (tokens[i] == "m_LoadCurrent"){
						current = convertFromString(values[i]);
					}
					
					double numberArg = convertFromString(values[i]);
					ArrayOfArgs[i] = numberArg;
				}
				power = current*voltage;
				fuelflow = anodeflow + pbflow;
				
				//printf("$$$$energy %lf uptime %lf, last %lf\n", energy, uptime, lastuptime);
				while (uptime - lastintervalstarttime >= interval)
				{
					fuelpounds += fuelflow*(((lastintervalstarttime + interval) - lastuptime) / 60) * .00406;
					energy += ((lastintervalstarttime + interval) - lastuptime) * power/3600;
					lastintervalstarttime += interval;
					lastuptime = lastintervalstarttime;
					//printf("fuelpounds %lf uptime %lf, last %lf power %lf\n", fuelpounds, uptime, lastuptime, power);
					ThunderballDbInterface::AddData(energy, fuelpounds, lastuptime/3600);
					energy = 0;
					fuelpounds = 0;
				}
				energy += (uptime - lastuptime) * power/3600;
				fuelpounds += fuelflow*((uptime - lastuptime) / 60)*.00406;
				lastuptime = uptime;
				
			}

			printf("\n Sum of first tem rows of data \n\n");
			for(int i = 0; i != tokens.size(); i++)
			{
				if (tokens[i] == "Date")
				{
					// string
					printf("  %s:  %f\n", tokens[i].c_str(), ArrayOfArgs[i]);
				}
				else if (tokens[i] == "Time")
				{
					// string
				}
				else if (tokens[i] == "m_nDeviceState")
				{
					// string
				}
				else
				{
					printf("  %s:  %f\n", tokens[i].c_str(), ArrayOfArgs[i]);
				}
			}
		}
	}
	gs_nCount++;

	std::ostringstream ss;
	ss << "stats|Some,Fuel,Cell,Data," << gs_nCount << ";";

	// General call to send string data to node
	m_NodeJsInstance->sendData(ss.str().c_str());
}
