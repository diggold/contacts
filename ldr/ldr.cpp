// ldr.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "windows.h"

typedef int(*PF_test)(void);
PF_test pf_test = 0;
int _tmain(int argc, _TCHAR* argv[])
{
	HINSTANCE hMod = LoadLibraryA("contacts.dll");
	pf_test = (PF_test)::GetProcAddress(hMod, "test");
	pf_test();
	return 0;
}

