#ifndef THUNDERBALL_3_DB_IF_H
#define THUNDERBALL_3_DB_IF_H

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

#include <mysql.h>

#include "Types.h"

class ThunderballDbInterface
{
public:
	// existing database connection interface
	static MYSQL * connectToDatabase(const char* szUser, const char* szPassword, const char* szDBName, const char* pLogChangeSignalFile);

	//////////////////////////////////////////////////////
	// New code below
	static void setupSqlDatabase();
	static void deleteLogEntry(int id);
	static void AddLogEntry(int mainId, int subId, const char* pMsg);
	static void AddData(double energy, double fuelflow, double time);

private:
	static MYSQL *s_theDatabaseCon;
};

#endif