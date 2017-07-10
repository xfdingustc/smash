/*******************************************************************************
**       CLanguage.h
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**
**      Description:
**      1.
**
**      Revision History:
**      -----------------
**      01a  11-04-2012 linnsong CREATE AND MODIFY
**
*******************************************************************************/
#ifndef  __INCLanguage_h
#define __INCLanguage_h
namespace ui{
#define MaxLanguageSupported 16
#define LanguageNameLength 128
#define maxLanguageItems 1024

class CWnd;
class CUIAction
{
public:
    CUIAction();
    virtual ~CUIAction();
    virtual void Action(void *para) = 0;
private:
};

#define LoadTxtItem(name) CLanguageLoader::getInstance()->gotItem(name, NULL)
class CLanguageItem
{
public:
    CLanguageItem(char* name, char* content);
    ~CLanguageItem();
    bool isName(const char* name);
    char* getContent();
    char* name(){return _name;};

    char* RestContent(char* content);

    CWnd *GetSubWnd(){return _subWnd;};
    CUIAction* GetAction(){return _action;};
    void setWnd(CWnd* w){_subWnd = w;};
    void SetAction(CUIAction *a){_action = a;};

private:
    char    *_name;
    char    *_content;
    CWnd    *_subWnd;
    CUIAction *_action;
};

#define maxLineBufferLength 1024
class CLanguageLoader
{
public:
    static void Create(const char* pathToLanguageDirectory, char* language);
    static void Destroy();
    static CLanguageLoader* getInstance(){return _pInstance;};

    void parseIndexFile(char* path);
    void parseTxtFile(char* path);

    static int GetLineEnd(char* buffer, int bufferLen);
    void LoadLanguage(char* language);
    char* gotText(char* name);
    CLanguageItem* createItem(char* name, char* content);
    CLanguageItem* gotItem(const char* name, char* content);
    //CLanguageItem* CLanguageLoader::gotItem(char* name)

    char** GetLanguageList(int &num){num = _languageNum; return _ppLanguageNameS;};

private:
    static CLanguageLoader* _pInstance;
    CLanguageLoader(const char* pathToLanguageDirectory, char* language);
    ~CLanguageLoader();
    int LanguageOfTxtFile(char* path);

private:
    char    **_ppTxtfilePath;
    char    *_ppLanguageNameS[MaxLanguageSupported];
    int     _languageNum;

    CLanguageItem   **_ppItems;
    int             _itemNum;
    int             _currentLanguage;
};

}
#endif
