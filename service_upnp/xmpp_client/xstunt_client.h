
#ifndef __H_XSTUNT_client__
#define __H_XSTUNT_client__

#define CONNECT_TRY_TIMES 3	
#define DEF_TIMEOUT 10
#define MAX_MSG_LEN 80
class XSTUNT_client
{
public:
	XSTUNT_client(char* id, char* ip1, char* ip2);
	~XSTUNT_client();

	bool isRegisted(){return _registed;};
	void StopWait(){_bWait = false;};

	SOCKET tryAsServer(int timeout);
	SOCKET tryAsClient(char* svrID, int tryTimes);
private:
	SOCKET	_sConnect;
	SOCKET	_sServer;
	char* 	_id;
	char* 	_ip1;
	char* 	_ip2;
	bool 	_registed;
	INT32 	nErr;
	bool 	_bWait;
};

#endif
