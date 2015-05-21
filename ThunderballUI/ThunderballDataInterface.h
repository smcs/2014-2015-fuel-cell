#ifndef THUNDERBALL_DATA_IF_H
#define THUNDERBALL_DATA_IF_H

///////////////////////////////////////////////////////////////////////////////
// ThunderballDbInterface.h
// 
// (C) 2009
// Protonex Technology Corporation
// The Next Generation of Portable Power(TM)
// 153 Northboro Road
// Southborough, MA 01772-1034
//
// NOTICE: This file contains confidential and proprietary information of
// Protonex Technology Corporation.
///////////////////////////////////////////////////////////////////////////////

#include <string>
#include <sstream>

//forward references
struct ThunderballSystemStats;
class NodeJsInterface;
class ThunderballUI;

class ThunderballDataInterface
{
public:
	static void setup(ThunderballUI* pMyProc, const char* szSignalFileName, const char* szInputSimFileName);
	static void processDataChunk(ThunderballSystemStats* pStats);
	static void handleTick();

private:
	static NodeJsInterface* m_NodeJsInstance;
	static bool processNodeData(std::string s, std::ostringstream& ss);

	static ThunderballUI* m_pMyProc;
	static const char* m_szInputSimFileName;
};

#endif