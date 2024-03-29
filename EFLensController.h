//
//  EFLensController.h
//  EF Lens Controller X2 plugin
//
//  Created by Rodolphe Pineau on 2019/02/16.

#ifndef __EFCTL__
#define __EFCTL__
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>

#ifdef SB_WIN_BUILD
#include <direct.h>
#include <time.h>
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
#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"

#include "StopWatch.h"

// #define PLUGIN_DEBUG 3

#define PLUGIN_VERSION      1.5

#define SERIAL_BUFFER_SIZE 256
#define LOG_BUFFER_SIZE 256
#define CMD_SIZE 5000

#define MAX_TIMEOUT 3000
#define MAX_READ_WAIT_TIMEOUT 25
#define INTER_COMMAND_WAIT    100    


enum EFCTL_Errors    {PLUGIN_OK=0, NOT_CONNECTED, CANT_CONNECT, BAD_CMD_RESPONSE, COMMAND_FAILED, COMMAND_TIMEOUT};
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


    int         Connect(const char *pszPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; };

	int			loadLensDef();

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; };
	void        setSleeper(SleeperInterface *pSleeper) { m_pSleeper = pSleeper; };
	void        setTheSkyXForMount(TheSkyXFacadeForDriversInterface *pTheSkyXForMounts) { m_pTheSkyXForMounts = pTheSkyXForMounts; };
    // move commands
    int         gotoPosition(int nPos);
    int         moveRelativeToPosision(int nSteps);

    // command complete functions
    int         isGoToComplete(bool &bComplete);
    void        Abort();

    // getter and setter
    int         getPosition(int &nPosition);
    int         getPosLimit(void);
    void        setPosLimit(const int &nLimit);
	void		setLastPos(const bool &bReturntoLastPos, const int &nPos);
    bool        isPosLimitEnabled(void);
    void        enablePosLimit(bool bEnable);

    int         setApperture(int &nAppeture);
    int         getApperture(void);

	int			getLensesCount();
	tLensDefnition	getLensDef(const int &nLensIdx);
	
	int getLensIdxFromName(const char *szLensName);
	int getLensApertureIdxFromName(const int nLensIdx, const char *szLensAperture);

protected:
    int             EFCtlCommand(const std::string sCmd, std::string &sResp, bool bExpectResponse = true, int nTimeout = MAX_TIMEOUT);
    int             readResponse(std::string &sResp, int nTimeout = MAX_TIMEOUT);

    int             parseFields(const std::string sDataIn, std::vector<std::string> &svFields, const char &cSeparator);
	std::string		GetAppDir(void);
	
    SerXInterface   *m_pSerx;
	SleeperInterface    *m_pSleeper;
	TheSkyXFacadeForDriversInterface	*m_pTheSkyXForMounts;
    CStopWatch      m_cmdDelayTimer;

    bool            m_bDebugLog;
    bool            m_bIsConnected;
    char            m_szFirmwareVersion[SERIAL_BUFFER_SIZE];
    char            m_szLogBuffer[LOG_BUFFER_SIZE];
    bool            m_bAborted;

    int             m_nCurPos;
    int             m_nTargetPos;
    int             m_nPosLimit;
    bool            m_bPosLimitEnabled;
    int             m_nCurrentApperture;
	int				m_nLastPos;
    bool            m_bReturntoLastPos;
    
	std::ifstream 	m_fLensDef;
	std::vector<tLensDefnition>	m_LensDefinitions;
	std::string&    trim(std::string &str, const std::string &filter );
	std::string&    ltrim(std::string &str, const std::string &filter);
	std::string&    rtrim(std::string &str, const std::string &filter);

#ifdef SB_WIN_BUILD
#define NB_PATH 2
	std::string sPluginPath[NB_PATH] = {"PlugIns","PlugIns64"};
#elif defined(SB_LINUX_BUILD)
#define NB_PATH 4
	std::string sPluginPath[NB_PATH] = {"PlugIns","PlugIns64","PlugInsARM32","PlugInsARM64"};
#elif defined(SB_MAC_BUILD)
#define NB_PATH 2
	std::string sPluginPath[NB_PATH] = {"PlugIns","PlugIns64"};
#endif
	
#ifdef PLUGIN_DEBUG
    // timestamp for logs
    const std::string getTimeStamp();
    std::ofstream m_sLogFile;
    std::string m_sLogfilePath;
#endif

};

#endif //__EFCTL__
