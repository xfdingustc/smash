/*******************************************************************************
**       CLanguage.cpp
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <dirent.h>
#include <iostream>
#include <string>

#include "GobalMacro.h"
#include "ClinuxThread.h"
#include "CLanguage.h"

namespace ui{

CLanguageLoader* CLanguageLoader::_pInstance = NULL;

void CLanguageLoader::Create
    (const char* pathToLanguageDirectory
    ,char* language)
{
#ifdef printBootTime
    struct timeval tv, tv2;
    struct timezone tz;
    gettimeofday(&tv, &tz);
#endif
    if (_pInstance == NULL)
    {
        _pInstance = new CLanguageLoader(pathToLanguageDirectory, language);
    }
#ifdef printBootTime
    gettimeofday(&tv2, &tz);
    CAMERALOG("----CLanguageLoader::Create %ld ms",
        (tv2.tv_sec - tv.tv_sec) * 1000 + (tv2.tv_usec - tv.tv_usec) / 1000);
#endif
}

void CLanguageLoader::Destroy()
{
    if (_pInstance != NULL) {
        delete _pInstance;
    }
}

CLanguageLoader::CLanguageLoader
    (const char* pathToLanguageDirectory
    ,char* currentLanguage)
    :_itemNum(0)
{
    DIR *dir;
    struct dirent *ptr;
    for (int i = 0; i < MaxLanguageSupported; i ++) {
        _ppLanguageNameS[i] = new char[LanguageNameLength];
        memset(_ppLanguageNameS[i], 0 ,LanguageNameLength);
    }

    char tmp[256];
    int index;
    _ppItems = new CLanguageItem* [maxLanguageItems];

    dir = opendir(pathToLanguageDirectory);
    if (dir != NULL) {
        while ((ptr = readdir(dir)) != NULL) {
            if (strcmp(ptr->d_name, "index.txt") == 0) {
                memset(tmp, 0, sizeof(tmp));
                snprintf(tmp, 256, "%s/%s", pathToLanguageDirectory, ptr->d_name);
                parseIndexFile(tmp);
                break;
            }
        }

        if (_languageNum > 0) {
            seekdir(dir, 0);
            _ppTxtfilePath = new char* [_languageNum];
            int i;
            for (i = 0; i < _languageNum; i++) {
                _ppTxtfilePath[i] = new char [256];
                memset(_ppTxtfilePath[i], 0, 256);
            }
            while ((ptr = readdir(dir)) != NULL) {
                if ((strstr(ptr->d_name, ".txt") > 0)
                    &&(!(strcmp(ptr->d_name, "index.txt") == 0)))
                {
                    memset(tmp, 0, sizeof(tmp));
                    snprintf(tmp, 256, "%s/%s", pathToLanguageDirectory, ptr->d_name);
                    index = LanguageOfTxtFile(tmp);
                    if (index >= 0) {
                        strcpy(_ppTxtfilePath[index], tmp);
                    }
                }
            }
        }
        closedir(dir);
    }
    //get current language option
    int i;
    for (i = 0; i < _languageNum; i++) {
        //CAMERALOG("--language: %s, %d", _ppLanguageNameS[i], _languageNum);
        if (strcmp(currentLanguage, _ppLanguageNameS[i]) == 0) {
            break;
        }
    }
    if (i < _languageNum) {
        _currentLanguage = i;
        LoadLanguage(NULL);
        /*for(i = 0; i < _itemNum; i++)
        {
        CAMERALOG("_item name: %s, content: %s",_ppItems[i]->name(), _ppItems[i]->getContent());
        }*/
    }
}

CLanguageLoader::~CLanguageLoader()
{
    // save to txt list
    //CAMERALOG("--lanug item number : %d", _itemNum);
#ifdef GENERATE_TOOLFILE
    CFile *pFile = new CFile("/tmp/mmcblk0p1/allString.txxt", CFile::FILE_WRITE);
    if (pFile) {
        for (int i = 0; i < _itemNum; i ++) {
            if (_ppItems[i]->name() != NULL) {
                pFile->write((BYTE*)_ppItems[i]->name(), strlen(_ppItems[i]->name()));
                pFile->write((BYTE*)":=", 2);
                if (_ppItems[i]->getContent() != NULL) {
                    pFile->write(
                        (BYTE*)_ppItems[i]->getContent(),
                        strlen(_ppItems[i]->getContent()));
                }
                pFile->write((BYTE*)"\r\n", 2);
            }
        }
        delete pFile;
    }
#endif

    for (int i = 0; i < MaxLanguageSupported; i ++) {
        delete[] _ppLanguageNameS[i];
    }

    for (int i = 0; i < _languageNum; i++) {
        delete[] _ppTxtfilePath[i];
    }
    delete [] _ppTxtfilePath;

    for (int i = 0; i < _itemNum; i++) {
        delete _ppItems[i];
    }
    delete [] _ppItems;
}


void CLanguageLoader::parseIndexFile(char* path)
{
    char readBuffer[maxLineBufferLength + 4];
    memset(readBuffer + maxLineBufferLength, 0, 4);
    CFile file(path, CFile::FILE_READ);
    int offset = 0;
    _languageNum = 0;
    if (file.IsValid())
    {
        LONGLONG length;
        int lineOffset;
        bool gotHead = false;
        while (1)
        {
            length = file.read(maxLineBufferLength, (UINT8*)readBuffer, maxLineBufferLength);
            //first line should have LANGUAGE insert.
            if (length <= 0) {
                break;
            }

            lineOffset = CLanguageLoader::GetLineEnd(readBuffer, maxLineBufferLength + 4);
            //CAMERALOG("---GetLineEnd : %d", lineOffset);
            if (lineOffset > 0)
            {
                if (gotHead)
                {
                    if (strlen(readBuffer) >= LanguageNameLength)
                    {
                        CAMERALOG("too long language name than defined LanguageNameLength(%d)",
                            LanguageNameLength);
                    } else {
                        strcpy(_ppLanguageNameS[_languageNum], readBuffer);
                        _languageNum ++;
                        if (_languageNum >= MaxLanguageSupported)
                        {
                            CAMERALOG("too much language > defined MaxLanguageSupported(%d) "
                                "listed in index.txt",
                                MaxLanguageSupported);
                            break;
                        }
                    }
                } else if(strstr(readBuffer, "Transee UI Language") > 0) {
                    //CAMERALOG("---got head : %d", lineOffset);
                    gotHead = true;
                }
                offset += lineOffset + 2;
                file.seek(offset);
            } else {
                break;
            }
        }
    }
}

int CLanguageLoader::GetLineEnd(char* buffer, int bufferLen)
{
    char* offset = strstr(buffer, "\r\n");

    if (offset > 0)
    {
        memset(offset, 0, bufferLen + buffer - offset);
        return offset - buffer;
    } else {
        // line lenght bigger than 1024;
        CAMERALOG("-- read line bigger than buffer: %d", bufferLen);
        return -1;
    }
}

int CLanguageLoader::LanguageOfTxtFile(char* path)
{
    //CAMERALOG("pars file : %s", path);
    char readBuffer[maxLineBufferLength + 4];
    memset(readBuffer + maxLineBufferLength, 0, 4);
    CFile file(path, CFile::FILE_READ);
    int offset = 0;
    if (file.IsValid())
    {
        LONGLONG length;
        int lineOffset;
        char* posi;
        //bool gotHead = false;
        while (1)
        {
            length = file.read(maxLineBufferLength, (UINT8*)readBuffer, maxLineBufferLength);
            //first line should have LANGUAGE insert.
            if (length <= 0) {
                break;
            }

            lineOffset = CLanguageLoader::GetLineEnd(readBuffer, maxLineBufferLength + 4);
            if (lineOffset > 0)
            {
                posi = strstr(readBuffer, "Language=");
                for (int i = 0; i < _languageNum; i++)
                {
                    if (strncmp(posi + strlen("Language="), _ppLanguageNameS[i], strlen(_ppLanguageNameS[i])) == 0)
                    {
                        return i;
                    }
                }
                offset += lineOffset + 2;
                file.seek(offset);
            }
        }
        return -1;
    }
    return 0;
}

void CLanguageLoader::LoadLanguage(char* language)
{
    if (language != NULL)
    {
        for (int i = 0; i < _languageNum; i++)
        {
            if (strcmp(language, _ppLanguageNameS[i]) == 0)
            {
                _currentLanguage = i;
                break;
            }
        }
    }
    char readBuffer[maxLineBufferLength + 4];
    memset(readBuffer + maxLineBufferLength, 0, 4);
    CAMERALOG("--open language file: %s", _ppTxtfilePath[_currentLanguage]);
    CFile file(_ppTxtfilePath[_currentLanguage], CFile::FILE_READ);
    int offset = 0;
    if(file.IsValid())
    {
        LONGLONG length;
        int lineOffset;
        char* posi;
        char* name;
        char* content;
        bool gotHead = false;
        while (1)
        {
            length = file.read(maxLineBufferLength, (UINT8*)readBuffer, maxLineBufferLength);
            //first line should have LANGUAGE insert.
            if (length <= 0) {
                break;
            }

            lineOffset = CLanguageLoader::GetLineEnd(readBuffer, maxLineBufferLength + 4);
            if (lineOffset > 0)
            {
                if (gotHead)
                {
                    posi = strstr(readBuffer, ":=");
                    if (posi > 0)
                    {
                        posi[0] = 0;
                        name = readBuffer;
                        content = posi + 2;

                        gotItem(name, content);
                        //debug check each itme here
                        //CAMERALOG("item : %s,\ncontent : %s", name, content);
                        //item->setContent(content);
                    }
                } else if(strstr(readBuffer, "Language=") > 0) {
                    gotHead = true;
                }
                offset += lineOffset + 2;
                file.seek(offset);
            }
        }
    }
}

char* CLanguageLoader::gotText(char* name)
{
    int i;
    for (i = 0; i < _itemNum; i++)
    {
        if (_ppItems[i]->isName(name))
        {
            return _ppItems[i]->getContent();
        }
    }
    if (i >= _itemNum)
    {
        return createItem(name, NULL)->getContent();
    }
    return NULL;
}

CLanguageItem* CLanguageLoader::gotItem(const char* name, char* content)
{
    int i;
    for (i = 0; i < _itemNum; i++)
    {
        if (_ppItems[i]->isName(name))
        {
            if (content != NULL) {
                _ppItems[i]->RestContent(content);
            }
            return _ppItems[i];
        }
    }
    if (i >= _itemNum)
    {
        return createItem((char*)name, content);
    }
    return NULL;
}


CLanguageItem* CLanguageLoader::createItem(char* name, char* content)
{
    if (_itemNum < maxLanguageItems)
    {
        CLanguageItem* item = new CLanguageItem(name, content);
        _ppItems[_itemNum] = item;
        _itemNum++;
        return item;
    } else {
        CAMERALOG("--add too much language item > maxLanguageItems(%d) defined",
            maxLanguageItems);
        return NULL;
    }
    return NULL;
}


CLanguageItem::CLanguageItem
    (char* name
    , char* content)
    :_name(NULL)
    ,_content(NULL)
    ,_subWnd(NULL)
    ,_action(NULL)
{
    if ((name != NULL)&&(strlen(name) > 0))
    {
        _name = new char[strlen(name) + 1];
        memset(_name, 0, strlen(name) + 1);
        strcpy(_name, name);
    }

    if ((content != NULL)&&(strlen(content) > 0))
    {
        _content= new char[strlen(content) + 1];
        memset(_content, 0, strlen(content) + 1);
        strcpy(_content, content);
    }
}
CLanguageItem::~CLanguageItem()
{
    if (_name) {
        delete[] _name;
    }
    if (_content) {
        delete[] _content;
    }
}

bool CLanguageItem::isName(const char* name)
{
    return (strcmp(_name, name) == 0);
}

char* CLanguageItem::getContent()
{
    if (_content)
    {
        return _content;
    } else {
        return _name;
    }
}

char* CLanguageItem::RestContent(char* content)
{
    delete[] _content;
    if ((content != NULL)&&(strlen(content) > 0))
    {
        _content= new char[strlen(content) + 1];
        memset(_content, 0, strlen(content) + 1);
        strcpy(_content, content);
    }
    return NULL;
}

CUIAction::CUIAction(){};
CUIAction::~CUIAction(){};

}