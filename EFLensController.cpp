//
//  EFLensController.cpp
//  EF Lens Controller X2 plugin
//
//  Created by Rodolphe Pineau on 2019/02/16.


#include "EFLensController.h"


CEFLensController::CEFLensController()
{

    m_pSerx = NULL;
    m_bDebugLog = false;
    m_bIsConnected = false;
    m_bAborted = false;

    m_nCurPos = 0;
    m_nTargetPos = 0;
    m_nPosLimit = 0;
    m_bPosLimitEnabled = 0;
    m_nCurrentApperture = 0;
	m_nLastPos = 0;
    m_bReturntoLastPos = false;
    

#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\EFLensControllerLog.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/EFLensControllerLog.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/EFLensControllerLog.txt";
#endif
    m_sLogFile.open(m_sLogfilePath, std::ios::out |std::ios::trunc);
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CEFLensController] Version " << std::fixed << std::setprecision(2) << PLUGIN_VERSION << " build  2019_05_8_1140." << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CEFLensController] Constructor Called." << std::endl;
    m_sLogFile.flush();
#endif

}

CEFLensController::~CEFLensController()
{
#ifdef    PLUGIN_DEBUG
    // Close LogFile
    if(m_sLogFile.is_open())
        m_sLogFile.close();
#endif
}

int CEFLensController::Connect(const char *pszPort)
{
    int nErr = PLUGIN_OK;
	bool bGotoZeroDone = false;
	int timeout = 0;
	
    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Called with port " << pszPort << std::endl;
    m_sLogFile.flush();
#endif

	nErr = m_pSerx->open(pszPort, 38400, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1");
    if( nErr == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return nErr;

    m_pSerx->purgeTxRx();
    m_cmdDelayTimer.Reset();

#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] connected to " << pszPort << std::endl;
    m_sLogFile.flush();
#endif
	m_pSleeper->sleep(2000);

#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] doing a goto 0" << std::endl;
    m_sLogFile.flush();
#endif
	gotoPosition(0);
	timeout = 0;
	do {
        if( timeout > MAX_TIMEOUT) {
            m_pSerx->close();
            m_bIsConnected = false;
            return ERR_COMMNOLINK;
        }
		m_pSleeper->sleep(MAX_READ_WAIT_TIMEOUT*5);
        timeout += (MAX_READ_WAIT_TIMEOUT*5);
		isGoToComplete(bGotoZeroDone);
	} while(!bGotoZeroDone);

	if(m_bReturntoLastPos && m_nLastPos) {
#ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] doing a goto to last position : " << m_nLastPos << std::endl;
        m_sLogFile.flush();
#endif
		gotoPosition(m_nLastPos);
		timeout = 0;
		do {
            if( timeout > MAX_TIMEOUT) {
                m_pSerx->close();
                m_bIsConnected = false;
                return ERR_COMMNOLINK;
            }
            m_pSleeper->sleep(MAX_READ_WAIT_TIMEOUT*5);
            timeout += (MAX_READ_WAIT_TIMEOUT*5);
			isGoToComplete(bGotoZeroDone);
		} while(!bGotoZeroDone);
	}

#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect]  setting apperture to  : " << m_nCurrentApperture << std::endl;
    m_sLogFile.flush();
#endif

	setApperture(m_nCurrentApperture);

    return nErr;
}

void CEFLensController::Disconnect()
{
	gotoPosition(0);
	m_pSleeper->sleep(250);
    if(m_bIsConnected && m_pSerx)
        m_pSerx->close();
 
	m_bIsConnected = false;
}

#pragma mark move commands
int CEFLensController::gotoPosition(int nPos)
{
    int nErr;
    std::stringstream ssTmp;
    std::string sResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;


    if (m_bPosLimitEnabled && nPos>m_nPosLimit)
        return ERR_LIMITSEXCEEDED;

#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [gotoPosition] doing a goto to : " << nPos << std::endl;
    m_sLogFile.flush();
#endif

    ssTmp << "M" << nPos << "#";
    nErr = EFCtlCommand(ssTmp.str(), sResp, false);
    if(nErr)
        return nErr;
    m_nTargetPos = nPos;
    m_bAborted = false;
    return nErr;
}

int CEFLensController::moveRelativeToPosision(int nSteps)
{
    int nErr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [moveRelativeToPosision] goto relative position : " << nSteps << std::endl;
    m_sLogFile.flush();
#endif

    m_nTargetPos = m_nCurPos + nSteps;
    nErr = gotoPosition(m_nTargetPos);
    return nErr;
}

#pragma mark command complete functions

int CEFLensController::isGoToComplete(bool &bComplete)
{
    int nErr = PLUGIN_OK;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(m_bAborted) {
        m_nTargetPos = m_nCurPos;
        bComplete = true;
        return nErr;
    }

    getPosition(m_nCurPos);
    if(m_nCurPos == m_nTargetPos)
        bComplete = true;
    else
        bComplete = false;

    return nErr;
}

void CEFLensController::Abort()
{
    m_bAborted = true;
}

#pragma mark getters and setters
int CEFLensController::getPosition(int &nPosition)
{
    int nErr = PLUGIN_OK;
    std::vector<std::string> vFieldsData;
    std::string sResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = EFCtlCommand("P#", sResp);
    if(nErr)
        return nErr;

    // parse output to extract position value.
    nErr = parseFields(sResp, vFieldsData, 'P');
    // convert response
    if(vFieldsData.size()) {
        nPosition = std::stoi(vFieldsData[0]);
        m_nCurPos = nPosition;
    }

    return nErr;
}

int CEFLensController::getPosLimit()
{
    return m_nPosLimit;
}

void CEFLensController::setPosLimit(const int &nLimit)
{
    m_nPosLimit = nLimit;
}

void CEFLensController::setLastPos(const bool &bReturntoLastPos, const int &nPos)
{
    m_bReturntoLastPos = bReturntoLastPos;
	m_nLastPos = nPos;
#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setLastPos] m_bReturntoLastPos : " << (m_bReturntoLastPos?"True":"False") << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setLastPos] m_nLastPos         : " << m_nLastPos << std::endl;
    m_sLogFile.flush();
#endif

}


bool CEFLensController::isPosLimitEnabled()
{
    return m_bPosLimitEnabled;
}

void CEFLensController::enablePosLimit(bool bEnable)
{
    m_bPosLimitEnabled = bEnable;
}


int CEFLensController::setApperture(int &nAppeture)
{
    int nErr;
    std::stringstream ssTmp;
    std::string sResp;

	m_nCurrentApperture = nAppeture;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;


#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setApperture] new apperture : " << nAppeture << std::endl;
    m_sLogFile.flush();
#endif

    ssTmp << "A" << nAppeture <<"#";
    nErr = EFCtlCommand(ssTmp.str(), sResp, false);
    if(nErr)
        return nErr;
    return nErr;
}

int CEFLensController::getApperture(void)
{
    return m_nCurrentApperture;
}

int CEFLensController::getLensesCount()
{
	return int(m_LensDefinitions.size());
}

tLensDefnition CEFLensController::getLensDef(const int &nLensIdx)
{
    if(getLensesCount()>nLensIdx) {
        if(nLensIdx >= 0 && nLensIdx < m_LensDefinitions.size())
            return m_LensDefinitions[nLensIdx];
    }
    return {};

}

int CEFLensController::getLensIdxFromName(const char *szLensName)
{
	int i;

    if(getLensesCount()==0)
        return 0;

	for(i = 0; i < m_LensDefinitions.size(); i++) {
		if(m_LensDefinitions[i].lensName == szLensName)
			return i;
	}
	return 0;
}

int CEFLensController::getLensApertureIdxFromName(const int nLensIdx, const char *szLensAperture)
{
	int i;

    if(getLensesCount()==0)
        return 0;

    if(getLensesCount()>nLensIdx) {
        for(i = 0; i < m_LensDefinitions[nLensIdx].fRatios.size(); i++ ) {
            if(m_LensDefinitions[nLensIdx].fRatios[i] == szLensAperture)
                return i;
        }
    }
    return 0;
}


#pragma mark command and response functions

int CEFLensController::EFCtlCommand(const std::string sCmd, std::string &sResp, bool bExpectResponse, int nTimeout)
{
    int nErr = PLUGIN_OK;
    unsigned long  ulBytesWrite;
    std::string localResp;
    int dDelayMs;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    // do we need to wait ?
    if(m_cmdDelayTimer.GetElapsedSeconds()<INTER_COMMAND_WAIT) {
        dDelayMs = INTER_COMMAND_WAIT - int(m_cmdDelayTimer.GetElapsedSeconds() *1000);
        if(dDelayMs>0)
            m_pSleeper->sleep(dDelayMs);
    }

    m_pSerx->purgeTxRx();
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [EFCtlCommand] Sending : " << sCmd << std::endl;
    m_sLogFile.flush();
#endif
    nErr = m_pSerx->writeFile((void *)(sCmd.c_str()), sCmd.size(), ulBytesWrite);
    m_cmdDelayTimer.Reset();
    m_pSerx->flushTx();

    if(nErr){
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [EFCtlCommand] writeFile error : " << nErr << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }

    if(!bExpectResponse) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [EFCtlCommand] no response expected. Done." << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }
    // read response
    nErr = readResponse(sResp, nTimeout);
    if(nErr)
        return nErr;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [EFCtlCommand] response : " << sResp << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}

int CEFLensController::readResponse(std::string &sResp, int nTimeout)
{
    int nErr = PLUGIN_OK;
    char pszBuf[SERIAL_BUFFER_SIZE];
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;
    int nBytesWaiting = 0 ;
    int nbTimeouts = 0;

    sResp.clear();
    memset(pszBuf, 0, SERIAL_BUFFER_SIZE);
    pszBufPtr = pszBuf;

    do {
        nErr = m_pSerx->bytesWaitingRx(nBytesWaiting);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] nBytesWaiting = " << nBytesWaiting << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] nBytesWaiting nErr = " << nErr << std::endl;
        m_sLogFile.flush();
#endif
        if(!nBytesWaiting) {
            nbTimeouts += MAX_READ_WAIT_TIMEOUT;
            if(nbTimeouts >= nTimeout) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
                m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] bytesWaitingRx timeout, no data for" << nbTimeouts <<" ms" << std::endl;
                m_sLogFile.flush();
#endif
                nErr = COMMAND_TIMEOUT;
                break;
            }
            m_pSleeper->sleep(MAX_READ_WAIT_TIMEOUT);
            continue;
        }
        nbTimeouts = 0;
        if(ulTotalBytesRead + nBytesWaiting <= SERIAL_BUFFER_SIZE)
            nErr = m_pSerx->readFile(pszBufPtr, nBytesWaiting, ulBytesRead, nTimeout);
        else {
            nErr = ERR_RXTIMEOUT;
            break; // buffer is full.. there is a problem !!
        }
        if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile error." << std::endl;
            m_sLogFile.flush();
#endif
            return nErr;
        }

        if (ulBytesRead != nBytesWaiting) { // timeout
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile Timeout Error." << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile nBytesWaiting = " << nBytesWaiting << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile ulBytesRead =" << ulBytesRead << std::endl;
            m_sLogFile.flush();
#endif
        }

        ulTotalBytesRead += ulBytesRead;
        pszBufPtr+=ulBytesRead;
    } while (ulTotalBytesRead < SERIAL_BUFFER_SIZE  && *(pszBufPtr-1) != '#');

    if(!ulTotalBytesRead)
        nErr = COMMAND_TIMEOUT; // we didn't get an answer.. so timeout
    else
        *(pszBufPtr-1) = 0; //remove the #

    sResp.assign(pszBuf);
    return nErr;
}


int CEFLensController::parseFields(const std::string sDataIn, std::vector<std::string> &svFields, const char &cSeparator)
{
    int nErr = PLUGIN_OK;
    std::string sSegment;
    std::stringstream ssTmp(sDataIn);

    svFields.clear();

    if(sDataIn.size()==0)
        return nErr;

    // split the string into vector elements
    while(std::getline(ssTmp, sSegment, cSeparator))
    {
        if(sSegment.size())
            svFields.push_back(sSegment);
    }

    if(svFields.size()==0) {
        nErr = ERR_BADFORMAT;
    }
    return nErr;
}

int CEFLensController::loadLensDef()
{
	int nErr = PLUGIN_OK;
	std::string line;
	std::string sAppDir;
	std::string sPathToLensDef;
	std::vector<std::string> lensEntry;
	std::vector<std::string> lensFRatios;
	tLensDefnition tLens;
	
	m_LensDefinitions.clear();
	
	sAppDir = GetAppDir();
	if(!sAppDir.size())
		return ERR_PATHNOTFOUND;
	
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [loadLensDef] loading lens definition." << std::endl;
    m_sLogFile.flush();
#endif

#if defined(SB_WIN_BUILD)
		for(i = 0; i < NB_PATH; i++) {
			sPathToLensDef = sAppDir + "\\Resources\\Common\\" + sPluginPath[i] + "\\FocuserPlugIns\\lens.txt";
			m_fLensDef.open(sPathToLensDef);
			if(m_fLensDef.good())
				break;
		}
#elif defined(SB_MAC_BUILD)
    sPathToLensDef = sAppDir + "/PlugIns/FocuserPlugIns/lens.txt";
    m_fLensDef.open(sPathToLensDef);
    if(!m_fLensDef.good())
        return ERR_OPENINGFILE;
#else
		for(i = 0; i < NB_PATH; i++) {
			sPathToLensDef = sAppDir + "/Resources/Common/" + sPluginPath[i] + "/FocuserPlugIns/lens.txt";
			m_fLensDef.open(sPathToLensDef);
            if(m_fLensDef.good())
				break;
		}
#endif
		
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [loadLensDef] sPathToLensDef : " << sPathToLensDef << std::endl;
    m_sLogFile.flush();
#endif
	

	if(!m_fLensDef.is_open()) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [loadLensDef] ERROR lens definition file not found." << std::endl;
        m_sLogFile.flush();
#endif
		return ERR_OPENINGFILE;
	}
	
	while (m_fLensDef) {
		// Read a Line from File
		getline(m_fLensDef, line);
		if(!line.size())
			continue;
		// parse it
		parseFields(line.c_str(), lensEntry, '|');
		tLens.lensName = trim(lensEntry[0]," \n\r");
		parseFields(trim(lensEntry[1], " \n\r").c_str(), lensFRatios, ' ');
		tLens.fRatios = lensFRatios;
		// append line to vector
		m_LensDefinitions.push_back(tLens);
	}
	
	// Close the file
	m_fLensDef.close();
	
	return nErr;
}

std::string CEFLensController::GetAppDir(void)
{
	std::string	InstallPath;
	std::string	AppDir;
	std::ifstream fTmp;
	char appDirInOut[CMD_SIZE];
	char szVersion[LOG_BUFFER_SIZE];
	int nBuild;
	float dVersion;
	if(!m_pTheSkyXForMounts)
		return std::string("");

	memset(appDirInOut, 0, CMD_SIZE);
	m_pTheSkyXForMounts->version(szVersion, LOG_BUFFER_SIZE);
	dVersion = atof(szVersion);
	nBuild = m_pTheSkyXForMounts->build();

	if(dVersion>= 15.0 && nBuild >= 12107) {
		sprintf(appDirInOut, "applicationDirPath");
		m_pTheSkyXForMounts->pathToWriteConfigFilesTo(appDirInOut, CMD_SIZE);
		AppDir = std::string(appDirInOut);
	}
	else {
		m_pTheSkyXForMounts->pathToWriteConfigFilesTo(appDirInOut, CMD_SIZE);
#if defined(SB_WIN_BUILD)
		InstallPath = std::string(appDirInOut) + "\\TheSkyXInstallPath.txt";
#else
		InstallPath = std::string(appDirInOut) + "/TheSkyXInstallPath.txt";
#endif
		
		fTmp.open(InstallPath);
		if(!fTmp.good())
			return "";
		
		getline(fTmp, AppDir);
		fTmp.close();
	}

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [GetAppDir] AppDir =" << AppDir << std::endl;
    m_sLogFile.flush();
#endif
	return AppDir;
}

std::string& CEFLensController::trim(std::string &str, const std::string& filter )
{
	return ltrim(rtrim(str, filter), filter);
}

std::string& CEFLensController::ltrim(std::string& str, const std::string& filter)
{
	str.erase(0, str.find_first_not_of(filter));
	return str;
}

std::string& CEFLensController::rtrim(std::string& str, const std::string& filter)
{
	str.erase(str.find_last_not_of(filter) + 1);
	return str;
}

#ifdef PLUGIN_DEBUG
const std::string CEFLensController::getTimeStamp()
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
#endif
