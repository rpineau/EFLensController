
#include "x2focuser.h"


X2Focuser::X2Focuser(const char* pszDisplayName, 
												const int& nInstanceIndex,
												SerXInterface						* pSerXIn, 
												TheSkyXFacadeForDriversInterface	* pTheSkyXIn, 
												SleeperInterface					* pSleeperIn,
												BasicIniUtilInterface				* pIniUtilIn,
												LoggerInterface						* pLoggerIn,
												MutexInterface						* pIOMutexIn,
												TickCountInterface					* pTickCountIn)

{
	m_pSerX							= pSerXIn;		
	m_pTheSkyXForMounts				= pTheSkyXIn;
	m_pSleeper						= pSleeperIn;
	m_pIniUtil						= pIniUtilIn;
	m_pLogger						= pLoggerIn;	
	m_pIOMutex						= pIOMutexIn;
	m_pTickCount					= pTickCountIn;

	m_bLinked = false;
	m_nPosition = 0;
    m_fLastTemp = -273.15f; // aboslute zero :)
	m_nLensIdx = 0;
	m_nLensApertureIdx = 0;

	m_EFLensController.SetSerxPointer(m_pSerX);
	m_EFLensController.setSleeper(m_pSleeper);
	m_EFLensController.setTheSkyXForMount(m_pTheSkyXForMounts);
	m_EFLensController.loadLensDef();

    // Read in settings
    if (m_pIniUtil) {
		m_EFLensController.setPosLimit(m_pIniUtil->readInt(PARENT_KEY, POS_LIMIT, 9999));
		m_EFLensController.enablePosLimit(m_pIniUtil->readInt(PARENT_KEY, POS_LIMIT_ENABLED, false));
		memset(m_szLensName, 0, 256);
		memset(m_szLensAperture, 0, 256);
		m_pIniUtil->readString(PARENT_KEY, LENS_NAME, "", m_szLensName, 256);
		if(strlen(m_szLensName)) {
			m_pIniUtil->readString(PARENT_KEY, LENS_APERTURE, "", m_szLensAperture, 256);
			m_nLensIdx = m_EFLensController.getLensIdxFromName(m_szLensName);
			m_nLensApertureIdx = m_EFLensController.getLensApertureIdxFromName(m_nLensIdx, m_szLensAperture);
		}
    }

	m_EFLensController.setApperture(m_nLensApertureIdx);
}

X2Focuser::~X2Focuser()
{
    //Delete objects used through composition
	if (GetSerX())
		delete GetSerX();
	if (GetTheSkyXFacadeForDrivers())
		delete GetTheSkyXFacadeForDrivers();
	if (GetSleeper())
		delete GetSleeper();
	if (GetSimpleIniUtil())
		delete GetSimpleIniUtil();
	if (GetLogger())
		delete GetLogger();
	if (GetMutex())
		delete GetMutex();

}

#pragma mark - DriverRootInterface

int	X2Focuser::queryAbstraction(const char* pszName, void** ppVal)
{
    *ppVal = NULL;

    if (!strcmp(pszName, LinkInterface_Name))
        *ppVal = (LinkInterface*)this;

    else if (!strcmp(pszName, FocuserGotoInterface2_Name))
        *ppVal = (FocuserGotoInterface2*)this;

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);

    else if (!strcmp(pszName, FocuserTemperatureInterface_Name))
        *ppVal = dynamic_cast<FocuserTemperatureInterface*>(this);

    else if (!strcmp(pszName, LoggerInterface_Name))
        *ppVal = GetLogger();

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

    else if (!strcmp(pszName, SerialPortParams2Interface_Name))
        *ppVal = dynamic_cast<SerialPortParams2Interface*>(this);

    return SB_OK;
}

#pragma mark - DriverInfoInterface
void X2Focuser::driverInfoDetailedInfo(BasicStringInterface& str) const
{
        str = "Focuser X2 plugin by Rodolphe Pineau";
}

double X2Focuser::driverInfoVersion(void) const							
{
	return DRIVER_VERSION;
}

void X2Focuser::deviceInfoNameShort(BasicStringInterface& str) const
{
    str="EF Lens Controller";
}

void X2Focuser::deviceInfoNameLong(BasicStringInterface& str) const				
{
    deviceInfoNameShort(str);
}

void X2Focuser::deviceInfoDetailedDescription(BasicStringInterface& str) const		
{
	str = "EF Lens Controller";
}

void X2Focuser::deviceInfoFirmwareVersion(BasicStringInterface& str)				
{
    str="";
}

void X2Focuser::deviceInfoModel(BasicStringInterface& str)							
{
    str="EF Lens Controller";
}

#pragma mark - LinkInterface
int	X2Focuser::establishLink(void)
{
    char szPort[DRIVER_MAX_STRING];
    int nErr;

    X2MutexLocker ml(GetMutex());
    // get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);
    nErr = m_EFLensController.Connect(szPort);
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

    return nErr;
}

int	X2Focuser::terminateLink(void)
{
    if(!m_bLinked)
        return SB_OK;

    X2MutexLocker ml(GetMutex());
    m_EFLensController.Disconnect();
    m_bLinked = false;

	return SB_OK;
}

bool X2Focuser::isLinked(void) const
{
	return m_bLinked;
}

#pragma mark - ModalSettingsDialogInterface
int	X2Focuser::initModalSettingsDialog(void)
{
    return SB_OK;
}

int	X2Focuser::execModalSettingsDialog(void)
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*					ui = uiutil.X2UI();
    X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;
	bool bLimitEnabled = false;
	int nPosLimit = 0;
	tLensDefnition tLens;
	int nLensIndex;
	int nApertureIndex;

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("EFLensController.ui", deviceType(), m_nPrivateMulitInstanceIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    X2MutexLocker ml(GetMutex());
    dx->invokeMethod("comboBox","clear");
    dx->invokeMethod("comboBox_2","clear");
	// set controls values
	// fill the combo boxes
	for(nLensIndex = 0; nLensIndex < m_EFLensController.getLensesCount(); nLensIndex++) {
		tLens = m_EFLensController.getLensDef(nLensIndex);
		dx->comboBoxAppendString("comboBox", tLens.lensName.c_str());
		if(nLensIndex == m_nLensIdx) {
			for(nApertureIndex = 0; nApertureIndex < tLens.fRatios.size(); nApertureIndex++) {
				dx->comboBoxAppendString("comboBox_2", tLens.fRatios[nApertureIndex].c_str());
			}
		}
	}
	dx->setCurrentIndex("comboBox", m_nLensIdx);
	dx->setCurrentIndex("comboBox_2", m_nLensApertureIdx);
	
	
	// debug
    if(m_bLinked) {
    }
    else {
        // disable all controls
    }

	// limit is done in software so it's always enabled.
	dx->setEnabled("posLimit", true);
	dx->setEnabled("limitEnable", true);
	dx->setPropertyInt("posLimit", "value", m_EFLensController.getPosLimit());
	if(m_EFLensController.isPosLimitEnabled())
		dx->setChecked("limitEnable", true);
	else
		dx->setChecked("limitEnable", false);
	

	//Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK) {
		// get limit option
		bLimitEnabled = dx->isChecked("limitEnable");
		dx->propertyInt("posLimit", "value", nPosLimit);
		if(bLimitEnabled && nPosLimit>0) { // a position limit of 0 doesn't make sense :)
			m_EFLensController.setPosLimit(nPosLimit);
			m_EFLensController.enablePosLimit(bLimitEnabled);
		} else {
			m_EFLensController.enablePosLimit(false);
		}
		m_nLensIdx = dx->currentIndex("comboBox");
		m_nLensApertureIdx = dx->currentIndex("comboBox_2");
		tLens = m_EFLensController.getLensDef(m_nLensIdx);

		// save values to config
		nErr  = m_pIniUtil->writeString(PARENT_KEY, LENS_NAME, tLens.lensName.c_str());
		nErr |= m_pIniUtil->writeString(PARENT_KEY, LENS_APERTURE, tLens.fRatios[m_nLensApertureIdx].c_str());
		nErr |= m_pIniUtil->writeInt(PARENT_KEY, POS_LIMIT, nPosLimit);
		nErr |= m_pIniUtil->writeInt(PARENT_KEY, POS_LIMIT_ENABLED, bLimitEnabled);
    }

	m_EFLensController.setApperture(m_nLensApertureIdx);
    return nErr;
}

void X2Focuser::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
	int nLensIndex = 0;
	int nApertureIndex = 0;
	tLensDefnition tLens;
	
	if (!strcmp(pszEvent, "on_comboBox_currentIndexChanged")) {
		nLensIndex = uiex->currentIndex("comboBox");
		if(nLensIndex >= 0) {
			tLens = m_EFLensController.getLensDef(nLensIndex);
			m_EFLensController.setApperture(nApertureIndex);
			// clear combo and refill with newly selected lens apertures
			uiex->invokeMethod("comboBox_2","clear");
			for(nApertureIndex = 0; nApertureIndex < tLens.fRatios.size(); nApertureIndex++) {
				uiex->comboBoxAppendString("comboBox_2", tLens.fRatios[nApertureIndex].c_str());
			}
			uiex->setCurrentIndex("comboBox_2",0);
		}
	}
	else if (!strcmp(pszEvent, "on_comboBox_2_currentIndexChanged")) {
		nApertureIndex = uiex->currentIndex("comboBox_2");
		if(nApertureIndex >= 0) {
			m_EFLensController.setApperture(nApertureIndex);
		}
	}
}

#pragma mark - FocuserGotoInterface2
int	X2Focuser::focPosition(int& nPosition)
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());

    nErr = m_EFLensController.getPosition(nPosition);
    m_nPosition = nPosition;
    return nErr;
}

int	X2Focuser::focMinimumLimit(int& nMinLimit) 		
{
	nMinLimit = 0;
    return SB_OK;
}

int	X2Focuser::focMaximumLimit(int& nPosLimit)			
{

	X2MutexLocker ml(GetMutex());
    if(m_EFLensController.isPosLimitEnabled()) {
        nPosLimit = m_EFLensController.getPosLimit();
    }
	else {
		nPosLimit = 9999;
	}

	return SB_OK;
}

int	X2Focuser::focAbort()								
{
	return SB_OK;
    // return ERR_COMMANDNOTSUPPORTED;
}

int	X2Focuser::startFocGoto(const int& nRelativeOffset)	
{
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    m_EFLensController.moveRelativeToPosision(nRelativeOffset);
    return SB_OK;
}

int	X2Focuser::isCompleteFocGoto(bool& bComplete) const
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2Focuser* pMe = (X2Focuser*)this;
    X2MutexLocker ml(pMe->GetMutex());
	nErr = pMe->m_EFLensController.isGoToComplete(bComplete);

    return nErr;
}

int	X2Focuser::endFocGoto(void)
{
    int nErr;
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nErr = m_EFLensController.getPosition(m_nPosition);
    return nErr;
}

int X2Focuser::amountCountFocGoto(void) const					
{ 
	return 3;
}

int	X2Focuser::amountNameFromIndexFocGoto(const int& nZeroBasedIndex, BasicStringInterface& strDisplayName, int& nAmount)
{
	switch (nZeroBasedIndex)
	{
		default:
		case 0: strDisplayName="10 steps"; nAmount=10;break;
		case 1: strDisplayName="100 steps"; nAmount=100;break;
		case 2: strDisplayName="1000 steps"; nAmount=1000;break;
	}
	return SB_OK;
}

int	X2Focuser::amountIndexFocGoto(void)
{
	return 0;
}

#pragma mark - FocuserTemperatureInterface
int X2Focuser::focTemperature(double &dTemperature)
{
    dTemperature = -100.0;
    return SB_OK;
}

#pragma mark - SerialPortParams2Interface

void X2Focuser::portName(BasicStringInterface& str) const
{
    char szPortName[DRIVER_MAX_STRING];

    portNameOnToCharPtr(szPortName, DRIVER_MAX_STRING);

    str = szPortName;

}

void X2Focuser::setPortName(const char* pszPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort);

}


void X2Focuser::portNameOnToCharPtr(char* pszPort, const int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    snprintf(pszPort, nMaxSize, DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);
    
}




