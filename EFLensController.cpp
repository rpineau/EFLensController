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

    m_nCurPos = 0;
    m_nTargetPos = 0;
    m_nPosLimit = 0;
    m_bPosLimitEnabled = 0;
    m_nCurrentApperture = 0;
    

#ifdef EFCTL_DEBUG
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
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#if defined EFCTL_DEBUG && EFCTL_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CEFLensController::CEFLensController] Version 2019_02_16_1200.\n", timestamp);
    fprintf(Logfile, "[%s] [CEFLensController::CEFLensController] Constructor Called.\n", timestamp);
    fflush(Logfile);
#endif

}

CEFLensController::~CEFLensController()
{
#ifdef	EFCTL_DEBUG
    // Close LogFile
    if (Logfile) fclose(Logfile);
#endif
}

int CEFLensController::Connect(const char *pszPort)
{
    int nErr = EFCTL_OK;
	bool bGotoZeroDone = false;
	int timeout = 0;
	
    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef EFCTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CEFLensController::Connect Called %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

	nErr = m_pSerx->open(pszPort, 38400, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1");
    if( nErr == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return nErr;

#ifdef EFCTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CEFLensController::Connect connected to %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif
	m_pSleeper->sleep(2000);

#ifdef EFCTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CEFLensController::Connect doing a goto 0\n", timestamp);
	fflush(Logfile);
#endif
	gotoPosition(0);
	do {
		m_pSleeper->sleep(100);
		isGoToComplete(bGotoZeroDone);
		timeout++;
		if( timeout > 15) {
			m_pSerx->close();
			m_bIsConnected = false;
			return ERR_COMMNOLINK;
		}
	} while(!bGotoZeroDone);
	
#ifdef EFCTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CEFLensController::Connect setting apperture to %d\n", timestamp, m_nCurrentApperture);
	fflush(Logfile);
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
    char szCmd[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;


    if (m_bPosLimitEnabled && nPos>m_nPosLimit)
        return ERR_LIMITSEXCEEDED;

#ifdef EFCTL_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CEFLensController::gotoPosition goto position  : %d\n", timestamp, nPos);
    fflush(Logfile);
#endif

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "M%d#", nPos);
    nErr = EFCtlCommand(szCmd, NULL, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    m_nTargetPos = nPos;
    return nErr;
}

int CEFLensController::moveRelativeToPosision(int nSteps)
{
    int nErr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef EFCTL_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CEFLensController::gotoPosition goto relative position  : %d\n", timestamp, nSteps);
    fflush(Logfile);
#endif

    m_nTargetPos = m_nCurPos + nSteps;
    nErr = gotoPosition(m_nTargetPos);
    return nErr;
}

#pragma mark command complete functions

int CEFLensController::isGoToComplete(bool &bComplete)
{
    int nErr = EFCTL_OK;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    getPosition(m_nCurPos);
    if(m_nCurPos == m_nTargetPos)
        bComplete = true;
    else
        bComplete = false;
    return nErr;
}

#pragma mark getters and setters
int CEFLensController::getPosition(int &nPosition)
{
    int nErr = EFCTL_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = EFCtlCommand("P#", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // parse output to extract position value.
    nErr = parseFields(szResp, vFieldsData, 'P');
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

void CEFLensController::setPosLimit(int nLimit)
{
    m_nPosLimit = nLimit;
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
    char szCmd[SERIAL_BUFFER_SIZE];

	m_nCurrentApperture = nAppeture;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;


#ifdef EFCTL_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CEFLensController::setApperture new apperture  : %d\n", timestamp, nAppeture);
    fflush(Logfile);
#endif

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "A%d#", nAppeture);
    nErr = EFCtlCommand(szCmd, NULL, SERIAL_BUFFER_SIZE);
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
	if(nLensIdx >= 0 && nLensIdx < m_LensDefinitions.size())
		return m_LensDefinitions[nLensIdx];
	else
		return {};
}

int CEFLensController::getLensIdxFromName(const char *szLensName)
{
	int i;
	for(i = 0; i < m_LensDefinitions.size(); i++) {
		if(m_LensDefinitions[i].lensName == szLensName)
			return i;
	}
	return 0;
}

int CEFLensController::getLensApertureIdxFromName(const int nLensIdx, const char *szLensAperture)
{
	int i;
	for(i = 0; i < m_LensDefinitions[nLensIdx].fRatios.size(); i++ ) {
		if(m_LensDefinitions[nLensIdx].fRatios[i] == szLensAperture)
			return i;
	}
	return 0;
}


#pragma mark command and response functions

int CEFLensController::EFCtlCommand(const char *pszszCmd, char *pszResult, int nResultMaxLen)
{
    int nErr = EFCTL_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  ulBytesWrite;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    m_pSerx->purgeTxRx();
#ifdef EFCTL_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CEFLensController::EFCtlCommand Sending %s\n", timestamp, pszszCmd);
	fflush(Logfile);
#endif
    nErr = m_pSerx->writeFile((void *)pszszCmd, strlen(pszszCmd), ulBytesWrite);
    m_pSerx->flushTx();

    if(nErr){
        return nErr;
    }

    if(pszResult) {
        // read response
        nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
#ifdef EFCTL_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CEFLensController::EFCtlCommand response \"%s\"\n", timestamp, szResp);
		fflush(Logfile);
#endif
        // printf("Got response : %s\n",resp);
        strncpy(pszResult, szResp, nResultMaxLen);
#ifdef EFCTL_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CEFLensController::EFCtlCommand response copied to pszResult : \"%s\"\n", timestamp, pszResult);
		fflush(Logfile);
#endif
    }
    return nErr;
}

int CEFLensController::readResponse(char *pszRespBuffer, int nBufferLen)
{
    int nErr = EFCTL_OK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    memset(pszRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = pszRespBuffer;

    do {
        nErr = m_pSerx->readFile(pszBufPtr, 1, ulBytesRead, MAX_TIMEOUT);
        if(nErr) {
            return nErr;
        }

        if (ulBytesRead !=1) {// timeout
#ifdef EFCTL_DEBUG
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(Logfile, "[%s] CEFLensController::readResponse timeout\n", timestamp);
			fflush(Logfile);
#endif
            nErr = ERR_NORESPONSE;
            break;
        }
        ulTotalBytesRead += ulBytesRead;
    } while (*pszBufPtr++ != '#' && ulTotalBytesRead < nBufferLen );

    if(ulTotalBytesRead)
        *(pszBufPtr-1) = 0; //remove the #

    return nErr;
}


int CEFLensController::parseFields(const char *pszIn, std::vector<std::string> &svFields, const char &cSeparator)
{
    int nErr = EFCTL_OK;
    std::string sSegment;
    std::stringstream ssTmp(pszIn);

    svFields.clear();
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
	int nErr = EFCTL_OK;
	int i;
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
	
#if defined EFCTL_DEBUG && EFCTL_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CEFLensController::loadLensDef] loading lens definition.\n", timestamp);
	fflush(Logfile);
#endif

#if defined(SB_WIN_BUILD)
		for(i = 0; i < NB_PATH; i++) {
			sPathToLensDef = sAppDir + "\\Resources\\Common\\" + sPluginPath[i] + "\\FocuserPlugIns\\lens.txt";
			m_fLensDef.open(sPathToLensDef);
			if(m_fLensDef.good())
				break;
		}
#else
		for(i = 0; i < NB_PATH; i++) {
			sPathToLensDef = sAppDir + "/Resources/Common/" + sPluginPath[i] + "/FocuserPlugIns/lens.txt";
#if defined EFCTL_DEBUG && EFCTL_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CEFLensController::loadLensDef] sPathToLensDef = '%s'\n", timestamp, sPathToLensDef.c_str());
            fflush(Logfile);
#endif
			m_fLensDef.open(sPathToLensDef);

#if defined EFCTL_DEBUG && EFCTL_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CEFLensController::loadLensDef]  m_fLensDef.good() = %d\n", timestamp,  m_fLensDef.good());
            fflush(Logfile);
#endif
            if(m_fLensDef.good())
				break;
		}
#endif
		
#if defined EFCTL_DEBUG && EFCTL_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEFLensController::loadLensDef] sPathToLensDef = '%s'\n", timestamp, sPathToLensDef.c_str());
		fflush(Logfile);
#endif
	

	if(!m_fLensDef.is_open()) {
#if defined EFCTL_DEBUG && EFCTL_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEFLensController::loadLensDef] ERROR lens definition file not found.\n", timestamp);
		fflush(Logfile);
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

#if defined EFCTL_DEBUG && EFCTL_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CEFLensController::GetAppDir] AppDir = %s\n", timestamp, AppDir.c_str());
	fflush(Logfile);
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
