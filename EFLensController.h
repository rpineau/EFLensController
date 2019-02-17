//
//  AAF2.h
//  NexDome
//
//  Created by Rodolphe Pineau on 2017/05/30.
//  NexDome X2 plugin

#ifndef __EFCTL__
#define __EFCTL__
#include <math.h>
#include <string.h>

#include <stdio.h>  /* defines FILENAME_MAX */
// #define WINDOWS  /* uncomment this line to use it for windows.*/
#ifdef WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>

#include <exception>
#include <typeinfo>
#include <stdexcept>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"

#define EFCTL_DEBUG 2

#define SERIAL_BUFFER_SIZE 256
#define MAX_TIMEOUT 1000
#define LOG_BUFFER_SIZE 256

enum EFCTL_Errors    {EFCTL_OK = 0, NOT_CONNECTED, ND_CANT_CONNECT, EFCTL_BAD_CMD_RESPONSE, COMMAND_FAILED};
enum MotorDir       {NORMAL = 0 , REVERSE};
enum MotorStatus    {IDLE = 0, MOVING};

typedef struct lensesDefinitions {
	std::string lensName;
	std::vector<std::string> fRatios;
} tLensDefnition;


class CEFLensController
{
public:
    CEFLensController();
    ~CEFLensController();

	void on_load(void);
	
    int         Connect(const char *pszPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; };

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; };

    // move commands
    int         gotoPosition(int nPos);
    int         moveRelativeToPosision(int nSteps);

    // command complete functions
    int         isGoToComplete(bool &bComplete);

    // getter and setter
    int         getPosition(int &nPosition);
    int         getPosLimit(void);
    void        setPosLimit(int nLimit);
    bool        isPosLimitEnabled(void);
    void        enablePosLimit(bool bEnable);

    int         setApperture(int &nAppeture);
    int         getApperture(void);

protected:

    int             cEFCtlCommand(const char *pszCmd, char *pszResult, int nResultMaxLen);
    int             readResponse(char *pszRespBuffer, int nBufferLen);
    int             parseFields(const char *pszIn, std::vector<std::string> &svFields, const char &cSeparator);
	int				loadLensDef();
	std::string		GetCurrentWorkingDir( void );
	
    SerXInterface   *m_pSerx;

    bool            m_bDebugLog;
    bool            m_bIsConnected;
    char            m_szFirmwareVersion[SERIAL_BUFFER_SIZE];
    char            m_szLogBuffer[LOG_BUFFER_SIZE];

    int             m_nCurPos;
    int             m_nTargetPos;
    int             m_nPosLimit;
    bool            m_bPosLimitEnabled;
    int             m_nCurrentApperture;
	
	std::ifstream 	m_fLensDef;
	std::vector<tLensDefnition>	m_LensDefintions;
	std::string&    trim(std::string &str, const std::string &filter );
	std::string&    ltrim(std::string &str, const std::string &filter);
	std::string&    rtrim(std::string &str, const std::string &filter);

#ifdef EFCTL_DEBUG
    std::string m_sLogfilePath;
    // timestamp for logs
    char *timestamp;
    time_t ltime;
    FILE *Logfile;      // LogFile
#endif

};

#endif //__EFCTL__
