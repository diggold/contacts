// ���� ifdef ���Ǵ���ʹ�� DLL �������򵥵�
// ��ı�׼�������� DLL �е������ļ��������������϶���� CONTACTS_EXPORTS
// ���ű���ġ���ʹ�ô� DLL ��
// �κ�������Ŀ�ϲ�Ӧ����˷��š�������Դ�ļ��а������ļ����κ�������Ŀ���Ὣ
// CONTACTS_API ������Ϊ�Ǵ� DLL ����ģ����� DLL ���ô˺궨���
// ������Ϊ�Ǳ������ġ�
#ifdef CONTACTS_EXPORTS
#define CONTACTS_API __declspec(dllexport)
#else
#define CONTACTS_API __declspec(dllimport)
#endif

// �����Ǵ� contacts.dll ������
class CONTACTS_API Ccontacts {
public:
	Ccontacts(void);
	// TODO:  �ڴ�������ķ�����
};

extern CONTACTS_API int ncontacts;

extern "C" CONTACTS_API int test(void);
