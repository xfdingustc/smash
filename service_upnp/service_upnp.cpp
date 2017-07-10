#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "ClinuxThread.h"
#include "class_upnp_thread.h"
#include "class_upnp_description.h"
#include "upnp_sdcc.h"
#include "class_mail_thread.h"
#include "class_gsensor_thread.h"

#ifdef AVAHI_ENABLE
#include "class_avahi_thread.h"
#endif

#ifdef support_motorControl
	#include "MotorController.h"
#endif


#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/rtc.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "fast_Camera.h"
#include "agbox.h"
#include "aglog.h"
#include "GobalMacro.h"
#include "math.h"


//#define LOCAL_NAME "/tmp/c1"
#define UPNP_WEB_ROOT "/tmp/"
#define UPNP_SDCC_DEVICE_DESCRIPTION "/description/sd_cloud_camera.xml"

static char* upnp_SN_PROPERTY_NAME = "system.device.sn";
enum upnp_transee_device_type
{
	upnp_transee_device_type_unknow = 0,
	upnp_transee_device_type_camera = 1,
	upnp_transee_device_type_decoder = 2,
	upnp_transee_device_type_storage = 3, 
	upnp_transee_device_type_control_point = 4,
	upnp_transee_device_type_end,
};

enum upnp_transee_device_productor
{
	upnp_transee_device_productor_unknow = 0,
	upnp_transee_device_productor_tsf = 1,
	upnp_transee_device_productor_end,
};

static int TRANSEE = 0x74ee;


static char* itemHfielHead = "#import <UIKit/UIKit.h>\n"\
"#import \"CCIcon.h\"";

static char* itemMfielHead ="//linn song creat and modify\n "\
"#import <QuartzCore/QuartzCore.h>\n"\
"#import \"SDUIItems.h\"";

static char* itemFHfielHead ="//linn song creat and modify\n"\
"#import \"SDUIItems.h\"\n"\
"@interface SDUIItemFactory : NSObject\n"\
"{}\n"\
"-(id)initWithSDDescriptor:(StringDeviceDescriptor*)holder;\n"
"@property (strong, nonatomic) StringDeviceDescriptor *pSDDescriptor;\n";

static char* itemFmfielHead = "//linn song creat and modify\n"
"#import <QuartzCore/QuartzCore.h>\n"
"#import\"SDUIItemFactory.h\"\n"
"@implementation SDUIItemFactory\n"
"@synthesize pSDDescriptor = _pSDDescriptor;\n";

int agclkd_open_rtc(int flags)
{
	int ret_val;

	ret_val = open("/dev/rtc", flags);
	if (ret_val >= 0) {
		goto agclkd_open_rtc_exit;
	}
	ret_val = open("/dev/rtc0", flags);
	if (ret_val >= 0) {
		goto agclkd_open_rtc_exit;
	}
	ret_val = open("/dev/misc/rtc", flags);
	if (ret_val >= 0) {
		goto agclkd_open_rtc_exit;
	}

agclkd_open_rtc_exit:
	return ret_val;
}

int agclkd_set_rtc(time_t utc)
{
	int ret_val;
	int rtc_fd;
	struct tm sys_tm;
	struct tm *psys_tm;
	struct tm loctime;
	char tm_buf[256];

	rtc_fd = agclkd_open_rtc(O_WRONLY);
	if (rtc_fd < 0) {
		ret_val = rtc_fd;
		//AGLOG_ERR("%s: agclkd_open_rtc() = %d\n",__func__, ret_val);
		goto agclkd_set_rtc_exit;
	}
	psys_tm = gmtime_r(&utc, &sys_tm);
	if (psys_tm != &sys_tm) {
		ret_val = -1;
		goto agclkd_set_rtc_close;
	}
	ret_val = ioctl(rtc_fd, RTC_SET_TIME, &sys_tm);
	if (ret_val < 0) {
		//AGLOG_ERR("%s: RTC_SET_TIME(%ld) = %d\n",	__func__, utc, ret_val);
		goto agclkd_set_rtc_close;
	}
	localtime_r(&utc, &loctime);
	strftime(tm_buf, sizeof(tm_buf), "%a %b %e %H:%M:%S %Z %Y", &loctime);
	//AGLOG_NOTICE("%s(%ld)\t%s\n", __func__, utc, tm_buf);

agclkd_set_rtc_close:
	close(rtc_fd);
agclkd_set_rtc_exit:
	return ret_val;
}

class UCCFactory
{
public:
	UCCFactory();
	~UCCFactory();

	static UCCFactory* Create();
	static void Destroy();

	void printDoc();
	void testSetState(char* name, char* state);
	void NoBatteryTime();
	
private:
	void GenerateUUID(char* uuidBuffer, int length ,char* snBuffer	,int snLength);
	static UCCFactory* _pInstance;
	CUpnpCloudCamera *_pCamera;
	CUpnpService *_pService;
	//CUpnpThread *_pThread;

	ui::CFastCamera *_pFC;
	char id[128];
	char sn[20];
};
UCCFactory* UCCFactory::_pInstance = NULL;


void UCCFactory::NoBatteryTime()
{
	time_t t;
	time(&t);
	struct tm tt2013, *p;
	tt2013.tm_year = 2013-1900;
	tt2013.tm_mon = 0;
	tt2013.tm_mday = 1;
	tt2013.tm_hour = 0;
	tt2013.tm_min = 0;
	tt2013.tm_sec = 1;
	tt2013.tm_wday = 2;
	time_t t2013 = mktime(&tt2013);
	//p = localtime(&t2013);
	//printf("2013:%d/%d/%d \n", p->tm_year + 1900, p->tm_mon +1, p->tm_mday);
	//p = localtime(&t);
	//printf("current %d/%d/%d \n", p->tm_year + 1900, p->tm_mon +1, p->tm_mday);
	//printf("---t2013: %d, diff: %f;\n", t2013, difftime(t, t2013));
	if(difftime(t, t2013) < 0)
	{
		struct timeval tt;
		tt.tv_sec = t2013;
		tt.tv_usec = 0;
		struct timezone tz;
		tz.tz_minuteswest = 0;
		tz.tz_dsttime = 0;
		settimeofday(&tt, &tz);
		agclkd_set_rtc(t2013);
		char tmp[256];
		memset(tmp, 0, sizeof(tmp));
		sprintf(tmp, "%sGMT%+d",timezongDir,4);
		int rt = remove(timezongLink);
		rt = symlink(tmp, timezongLink);
	}
}


UCCFactory::UCCFactory()
{	
aglog_init("--upnp service",
			(AGLOG_MODE_STD | AGLOG_MODE_SYSLOG),
			AGLOG_LEVEL_NOTICE);
#ifdef SET_DEFAULTTIME_2013
	NoBatteryTime();
#endif
	CTimerThread::Create();
	this->GenerateUUID(id, sizeof(id), sn, sizeof(sn));
	_pCamera = new CUpnpCloudCamera("1", "0",id, id, sn);
	SDWatchThread::create();
	_pFC = new ui::CFastCamera(_pCamera);
	_pFC->Start();
#ifndef noupnp
	printDoc();
#endif
	CPropsEventThread::Create();
	CUpnpThread::Create(UPNP_WEB_ROOT, UPNP_SDCC_DEVICE_DESCRIPTION, _pCamera);

#ifdef MailThreadAvaliable
	CMailServerThread::create();
#endif

#ifdef GSensorTestThread
	CGsensorThread::create();
	CGsensorThread::getInstance()->SetShakeObserver(_pFC);
#endif

#ifdef AVAHI_ENABLE
	CAvahiServerThread::create();
#endif

#ifdef support_motorControl
	MotorControlService::Create();
#endif

#ifdef support_obd
	COBDServiceThread::create();
#endif
}

UCCFactory::~UCCFactory()
{
	//CPropsEventThread::getInstance()->;
	CPropsEventThread::Destroy();
#ifdef support_obd
	COBDServiceThread::destroy();
#endif
	
#ifdef support_motorControl
	MotorControlService::Destroy();
#endif

#ifdef AVAHI_ENABLE
	CAvahiServerThread::destroy();
#endif

#ifdef GSensorTestThread
	CGsensorThread::destroy();
#endif
#ifdef MailThreadAvaliable
	CMailServerThread::destroy();
#endif
	printf("---delete factory\n");
//#ifndef noupnp

	CUpnpThread::Destroy();
//#endif
	_pFC->Stop();
	delete _pFC;
	SDWatchThread::destroy();
	CTimerThread::Destroy();
	delete _pCamera;
	EventProcessThread::destroyEPTObject();	
}

void UCCFactory::GenerateUUID
	(char* uuidBuffer
	,int length
	,char* snBuffer
	,int snLength)
{
	memset(uuidBuffer, 0, length);
	memset(snBuffer, 0, snLength);
	srand((int)time(0));
	int rnd = rand();
	
	sprintf(snBuffer,"%012d",rnd);
	agcmd_property_get(upnp_SN_PROPERTY_NAME, uuidBuffer, snBuffer);
	rnd = atoi(uuidBuffer);
	memset(snBuffer, 0, snLength);
	sprintf(snBuffer,"%012d",rnd);
	
	memset(uuidBuffer, 0, length);	
	sprintf(uuidBuffer, "uuid:0022B09E-%04x-%04x-%04x-%012d",TRANSEE,upnp_transee_device_type_camera,upnp_transee_device_productor_tsf,rnd);
	printf("--uuid for upnp: %s\n", uuidBuffer);

}

static int pathlen = 256;
static int bufferlen = 20480;
void UCCFactory::printDoc()
{
	char* path = new char[pathlen];
	char* buffer = new char[bufferlen];
	memset(path, 0, pathlen);
	sprintf(path, "%s%s", UPNP_WEB_ROOT, UPNP_SDCC_DEVICE_DESCRIPTION);
	//printf("---write device doc to : %s\n", path);
	remove(path);
	_pCamera->getDoc()->writeToDocument(path, buffer, bufferlen);

#ifdef GENERATE_TOOLFILE
	memset(path, 0, pathlen);
	sprintf(path, "%s%s", UPNP_WEB_ROOT, "/objectC/SDUIItems.h");
	CFile itemsHFile = CFile(path, CFile::FILE_WRITE);
	memset(buffer, 0, bufferlen);
	sprintf(buffer, "%s",itemHfielHead);
	itemsHFile.write((BYTE*)buffer, strlen(buffer));

	memset(path, 0, pathlen);
	sprintf(path, "%s%s", UPNP_WEB_ROOT, "/objectC/SDUIItems.mm");
	CFile itemsMFile = CFile(path, CFile::FILE_WRITE);
	memset(buffer, 0, bufferlen);
	sprintf(buffer, "%s",itemMfielHead);
	itemsMFile.write((BYTE*)buffer, strlen(buffer));

	memset(path, 0, pathlen);
	sprintf(path, "%s%s", UPNP_WEB_ROOT, "/objectC/SDUIItemFactory.h");
	CFile itemFactHFile = CFile(path, CFile::FILE_WRITE);
	memset(buffer, 0, bufferlen);
	sprintf(buffer, "%s",itemFHfielHead);
	itemFactHFile.write((BYTE*)buffer, strlen(buffer));

	memset(path, 0, pathlen);
	sprintf(path, "%s%s", UPNP_WEB_ROOT, "/objectC/SDUIItemFactory.mm");
	CFile itemFactMFile = CFile(path, CFile::FILE_WRITE);
	memset(buffer, 0, bufferlen);
	sprintf(buffer, "%s",itemFmfielHead);
	itemFactMFile.write((BYTE*)buffer, strlen(buffer));	
#endif

	for(int i = 0; i < _pCamera->GetServiceNum(); i++)
	{
		memset(path, 0, 256);
		sprintf(path, "%s%s", UPNP_WEB_ROOT, _pCamera->getService(i)->getDocPath());
		_pCamera->getService(i)->getDoc()->writeToDocument(path, buffer, bufferlen);

#ifdef GENERATE_TOOLFILE
		memset(path, 0, 256);
		sprintf(path, "%s%s", UPNP_WEB_ROOT,"/icons/");
		_pCamera->getService(i)->getDoc()->GenerateCodeFiles(path, buffer, bufferlen, &itemsHFile, &itemsMFile, &itemFactHFile, &itemFactMFile);
#endif
	}


#ifdef GENERATE_TOOLFILE
	memset(buffer, 0, bufferlen);
	sprintf(buffer, "-(id)initWithSDDescriptor:(StringDeviceDescriptor*)holder\n{\n	self = [super init];\n	_pSDDescriptor = holder;\n	if(self)\n	{\n");
	itemFactMFile.write((BYTE*)buffer, strlen(buffer));	
	for(int i = 0; i < _pCamera->GetServiceNum(); i++)
	{
		_pCamera->getService(i)->getDoc()->GenerateCodeFiles2(path, buffer, bufferlen, &itemFactHFile, &itemFactMFile);		
	}
	memset(buffer, 0, bufferlen);
	sprintf(buffer, "	}\n	return self;\n}\n@end\n");
	itemFactMFile.write((BYTE*)buffer, strlen(buffer));

	memset(buffer, 0, bufferlen);
	sprintf(buffer, "@end\n");
	itemFactHFile.write((BYTE*)buffer, strlen(buffer));
#endif	
	

	delete[] path;
	delete[] buffer;
	
}

UCCFactory* UCCFactory::Create()
{
	if(_pInstance == NULL)
	{
		_pInstance = new UCCFactory();		
	}
	return _pInstance;
}

void UCCFactory::Destroy()
{
	printf("UCCFactory::Destroy\n");
	if(_pInstance != NULL)
	{
		delete _pInstance;		
	}
}


void UCCFactory::testSetState(char* name, char* state)
{
	if((strlen(name) == 0)||(strlen(state) == 0))
		return;
	_pCamera->testSetState(name, state);
}

static CSemaphore *pSystemSem;
static char systemCMD[256];

static void signal_handler_SIGTERM(int sig)
{
	printf("----terminate signal got\n");
	memset(systemCMD, 0, sizeof(systemCMD));
	sprintf(systemCMD, "terminate");
	pSystemSem->Give();
}


class testC
{
public:
	testC(char* name);
	virtual ~testC();
private:
	void* nouse;
	bool b;
	char tmp[40];
};

testC::testC(char* name):b(false)
{
	printf("--name : %s\n", name);
	strcpy(tmp, name);
	printf("--tmp : %s\n", tmp);
}
testC::~testC()
{

}

int main(int argc, char **argv)
{
	bool bBkgrd = false;
	//CTimerThread::Create();
	//new testC("timer2222---");
	
	if(argc >= 1)
	{
		for(int j = 1; j < argc; j++)
		{
			if(strcmp(argv[j], "-bkgrd") == 0)
			{	
				bBkgrd = true;
			}
			else if(strcmp(argv[j], "-svr") == 0)
			{	
				//CCourier *pCourier = new CCourier(argv[j + 1]);
			}
			else if(strcmp(argv[j], "-send") == 0)
			{	
			}
			else if(strcmp(argv[j], "-test") == 0)
			{	
				// test get properties
				char tmp[AGCMD_PROPERTY_SIZE];
				agcmd_property_get(powerstateName, tmp, "Unknown");
				printf("power status: %s\n", tmp);
				return 0;
			}
		}
	}
	
	UCCFactory* fact = UCCFactory::Create();
	
	bool bsend = false;
	bool bPowerOff = false;
	bool bReboot = false;

	pSystemSem = new CSemaphore(0,1, "service upnp main sem");
	if(bBkgrd)
	{		
		signal(SIGTERM, signal_handler_SIGTERM);
		signal(SIGHUP, signal_handler_SIGTERM);
		signal(SIGINT, signal_handler_SIGTERM);
		while(1)
		{
			pSystemSem->Take(-1);
			if(strcmp(systemCMD, "poweroff") == 0)
			{
				bPowerOff = true;
				break;
				//printf("should :remote poweroff\n");
			}
			else if(strcmp(systemCMD, "terminate") == 0)
			{
				//bPowerOff = true;
				break;
			}
			else if(strcmp(systemCMD, "reboot") == 0)
			{
				bReboot = true;
				break;
				//printf("should :remote reboot\n");
			}
		}
	}
	else
	{	
		char ch[256];
		char tmp;
		int i;
		while(1)
		{
			i = 0;
			memset(ch, 0, sizeof(ch));
			do
			{
				tmp = getchar();
				ch[i] = tmp;
				i++;
			}while((tmp!= '\n')&&(i < 256));		

			if(bsend)
			{	
				char* name;
				char* state;
				state = index(ch, ' ');
				*state = 0;
				state++;
				name = ch;
				state[strlen(state)-1] = 0;
				fact->testSetState(name, state);

				bsend = false;
			}

			if(0 == strcmp(ch, "exit\n"))	
			{
				break;
			}
			if(0 == strcmp(ch, "send\n"))	
			{
				bsend = true;
			}

			if(0 == strcmp(ch, "mail\n"))	
			{
				// triger mail
#ifdef MailThreadAvaliable
				CMailServerThread::getInstance()->TrigerMail( CMail::TrigerMailEvent_Alert, "/tmp/mmcblk0p1/test.mp4", "none");
#endif
			}

			if(0 == strcmp(ch, "poweroff\n"))	
			{
				bPowerOff = true;
				break;
			}
		}
	}
	
#ifndef USE_AVF
	sleep(3);
#endif
	if(bPowerOff)
	{		
		const char *tmp_args[8];

		tmp_args[0] = "agsh";
		tmp_args[1] = "poweroff";
		tmp_args[2] = NULL;
		//printf("--upnp agsh poweroff0\n");
		agbox_sh(2, (const char *const *)tmp_args);
	  	printf("--upnp agsh poweroff\n");
		//system("agsh poweroff\n");
	}
	else if(bReboot)
	{
		const char *tmp_args[8];

		tmp_args[0] = "agsh";
		tmp_args[1] = "reboot";
		tmp_args[2] = NULL;
		agbox_sh(2, (const char *const *)tmp_args);
		printf("--upnp agsh reboot\n");
	}
	else
	{
		delete pSystemSem;
		UCCFactory::Destroy();
	}
	return 0;
}


void DestroyUpnpService(bool bReboot)
{
	printf("--- destroy Upnp service\n");
	//UCCFactory::Destroy();
	
	memset(systemCMD, 0, sizeof(systemCMD));
	if(bReboot)
		sprintf(systemCMD, "reboot");
	else
		sprintf(systemCMD, "poweroff");
	pSystemSem->Give();
}

