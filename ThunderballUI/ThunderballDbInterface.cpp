#include <iostream>
#include <fstream>
#include <string>
#include <array>
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
#include <assert.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <linux/i2c-dev.h>

#include "ThunderballDbInterface.h"
#include "LinuxDebug.h"

// A sample database static interface class
// It was not used in the first release of the software

/// Prints out a MySQL error message and exits
///
/// This function should be called after a MySQL error has been encountered.  This function will then
/// notify the user of the error that has occurred, clean up the existing MySQL connection, and then
/// exit the program.
///
/// @param The MySQL connection to clean up before exiting

MYSQL *ThunderballDbInterface::s_theDatabaseCon;

static const char* gs_pLogChangeSignalFile;

static void error_exit(MYSQL *con)
{
	printf("MYSQL Error Exit: %s\n", mysql_error(con));

	if (con != NULL)
	{
		mysql_close(con);
	}
	exit(1);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void print_stmt_error (MYSQL_STMT *stmt, const char *message)
{
	LinuxDebugObject dbo(LinuxDebug::DebugModule::DM_DATABASE, LinuxDebug::DebugFlag::DF_TRACE);
	dbo << message << dbo.endline;

	if (stmt != NULL)
	{
		dbo << "Error " << mysql_stmt_errno (stmt) << " " << message << mysql_stmt_sqlstate (stmt) << " " << mysql_stmt_error (stmt) << dbo.endline;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MYSQL * ThunderballDbInterface::connectToDatabase(const char* szUser, const char* szPassword, const char* szDBName, const char* pLogChangeSignalFile)
{
	// this an optional file which can be 'touched' (or rewritten)
	// to signal a database change
	// Node.js has services which can send an event based on a file change
	// this could be useful to puch an update to the client

	gs_pLogChangeSignalFile = pLogChangeSignalFile;

	// Initialize a connection to MySQL
	s_theDatabaseCon = mysql_init(NULL);

	if (s_theDatabaseCon == NULL)
		error_exit(s_theDatabaseCon);

	// Set the connection to automatically reconnect
	my_bool reconnect = 1;
	if (mysql_options(s_theDatabaseCon, MYSQL_OPT_RECONNECT, &reconnect))
		error_exit(s_theDatabaseCon);

	// Connect to MySQL
	// Here we pass in:
	//  host name => localhost
	//  user name => bone
	//  password => bone
	//  database name => TempDB

	if (mysql_real_connect(s_theDatabaseCon, "localhost", szUser, szPassword, szDBName, 0, NULL, 0) == NULL)
		error_exit(s_theDatabaseCon);	

	return s_theDatabaseCon;

}

///////////////////////////////////////////////////////////////////////
// NEW code follows

void ThunderballDbInterface::setupSqlDatabase()
{
	assert(s_theDatabaseCon);

	if (mysql_query(s_theDatabaseCon, "CREATE TABLE IF NOT EXISTS TheLog("
																"Id INT UNSIGNED NOT NULL AUTO_INCREMENT,"
																"LogTime TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
																"MainId INT NOT NULL,"
																"SubId INT,"
																"Description CHAR(128),"
																"PRIMARY KEY(Id)"
																")"))
	{
		printf("Failed to create The Log\n");
		error_exit(s_theDatabaseCon);
	}

	if (mysql_query(s_theDatabaseCon, "CREATE TABLE IF NOT EXISTS TheData("
																"Id INT UNSIGNED NOT NULL AUTO_INCREMENT,"
																"Time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
																"Power INT NOT NULL,"
																"PRIMARY KEY(Id)"
																")"))
	{
		printf("Failed to create The Log\n");
		error_exit(s_theDatabaseCon);
	}

	if (mysql_query(s_theDatabaseCon, "CREATE TABLE IF NOT EXISTS RealData("
		"Time DOUBLE,"
		"Energy DOUBLE,"
		"FuelFlow DOUBLE"
		")"))
	{
		printf("Failed to create The Log\n");
		error_exit(s_theDatabaseCon);
	}

	// Leave connection open

	// Close the MySQL connection
	//mysql_close(s_theDatabaseCon);
}

void ThunderballDbInterface::deleteLogEntry(int id)
{
	if (s_theDatabaseCon == NULL) return;

	char stmt_str[1000];
	MYSQL_STMT *stmt = mysql_stmt_init(s_theDatabaseCon);
	if (stmt == NULL)
	{
		error_exit(s_theDatabaseCon);
	}

	LinuxDebugObject dbo(LinuxDebug::DebugModule::DM_DATABASE, LinuxDebug::DebugFlag::DF_TRACE);
	dbo << "Deleting Log Record(s)" << dbo.endline;

	sprintf(stmt_str, "DELETE FROM TheLog WHERE "
							"Id=%d",
							id);
	if (mysql_stmt_prepare (stmt, stmt_str, strlen (stmt_str)) != 0)
	{
		print_stmt_error(stmt, "Could not prepare DELETE statement");
		return;
	}

	if ((mysql_stmt_execute (stmt) != 0) || (mysql_affected_rows(s_theDatabaseCon) == 0))
	{
		print_stmt_error (stmt, "Could not execute DELETE");
	}
}

void ThunderballDbInterface::AddLogEntry(int mainId, int subId, const char* pMsg)
{
	if (s_theDatabaseCon == NULL) return;

	char stmt_str[1000];
	MYSQL_STMT *stmt = mysql_stmt_init(s_theDatabaseCon);
	if (stmt == NULL)
	{
		error_exit(s_theDatabaseCon);
	}
		sprintf(stmt_str, "Insert INTO TheLog ("
								"MainId,"
								"SubId,"
								"Description) VALUES ("
								"%d,%d,'%s')",
								mainId,
								subId,
								pMsg
								);

		if (mysql_stmt_prepare (stmt, stmt_str, strlen (stmt_str)) != 0)
		{
			print_stmt_error(stmt, "Could not prepare INSERT statement");

			LinuxDebugObject dbo(LinuxDebug::DebugModule::DM_DATABASE, LinuxDebug::DebugFlag::DF_TRACE);
			dbo << stmt_str << dbo.endline;

			return;
		}

		if (mysql_stmt_execute (stmt) != 0)
		{
			print_stmt_error (stmt, "Could not execute INSERT");
			return;
		}

		if (gs_pLogChangeSignalFile)
		{
			// signal to web
			ofstream myfile;
			myfile.open (gs_pLogChangeSignalFile);
			myfile << "Log changed";
			myfile.close();
		}
}
void ThunderballDbInterface::AddData(double energy, double fuelflow, double time)
{
	if (s_theDatabaseCon == NULL) return;

	char stmt_str[1000];
	MYSQL_STMT *stmt = mysql_stmt_init(s_theDatabaseCon);
	if (stmt == NULL)
	{
		error_exit(s_theDatabaseCon);
	}
	sprintf(stmt_str, "Insert INTO RealData ("
		"Time,"
		"Energy,"
		"FuelFlow) VALUES ("
		"%f,%f,'%f')",
		time,
		energy,
		fuelflow
		);

	if (mysql_stmt_prepare(stmt, stmt_str, strlen(stmt_str)) != 0)
	{
		print_stmt_error(stmt, "Could not prepare INSERT statement");

		LinuxDebugObject dbo(LinuxDebug::DebugModule::DM_DATABASE, LinuxDebug::DebugFlag::DF_TRACE);
		dbo << stmt_str << dbo.endline;

		return;
	}

	if (mysql_stmt_execute(stmt) != 0)
	{
		print_stmt_error(stmt, "Could not execute INSERT");
		return;
	}

	if (gs_pLogChangeSignalFile)
	{
		// signal to web
		ofstream myfile;
		myfile.open(gs_pLogChangeSignalFile);
		myfile << "Log changed";
		myfile.close();
	}
}
