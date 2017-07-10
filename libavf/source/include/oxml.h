
#ifndef __OXML_H__
#define __OXML_H__

class CMemBuffer;
struct CXmlElement;
struct CXmlAttribute;
class CXmlDoc;

struct CXmlNode {
	string_t *pName;
	string_t *pValue;
	~CXmlNode();

	INLINE const char *Name() { return pName ? pName->string() : NULL; }
	INLINE const char *Value() { return pValue ? pValue->string() : NULL; }
};

struct CXmlElement: public CXmlNode {
	CXmlElement *pParent;
	CXmlElement *pFirstChild;
	CXmlElement *pLastChild;
	CXmlElement *pPrev;
	CXmlElement *pNext;
	CXmlAttribute *pFirstAttr;
	CXmlAttribute *pLastAttr;

	CXmlAttribute *GetAttributeByName(const char *pName);
};

struct CXmlAttribute: public CXmlNode {
	CXmlAttribute *pNext;
};

//-----------------------------------------------------------------------
//
//  class CXmlDoc
//
//-----------------------------------------------------------------------

class CXmlDoc: public CObject
{
	typedef CObject inherited;

public:
	static CXmlDoc* Create();
	void Clear();

private:
	CXmlDoc();
	virtual ~CXmlDoc();

public:
	int Parse(const avf_u8_t *pText);
	void ToStream(CMemBuffer& buffer);

public:
	int SetNode(CXmlNode *pNode, const char *pName, avf_size_t name_len, const char *pValue, avf_size_t value_len);

	CXmlElement *CreateElement(const char *pName, avf_size_t name_len, const char *pValue, avf_size_t value_len);
	int AddElement(CXmlElement *pParentElem, CXmlElement *pPrevElem, CXmlElement *pElem);

	CXmlAttribute *CreateAttribute(const char *pName, avf_size_t name_len, const char *pValue, avf_size_t value_len);
	int AppendAttribute(CXmlElement *pElem, CXmlAttribute *pAttribute);

public:
	INLINE CXmlElement *GetRoot() {
		return &mRoot;
	}
	CXmlElement *GetElementByName(CXmlElement *pElem, const char *pName);
	CXmlAttribute *GetAttributeByName(CXmlElement *pElem, const char *pName);

private:
	CXmlElement mRoot;	// need release

	const char *mpError;
	int mLevel;
	int mLine;
	int mDecl;

	char *mBuffer;	// need release
	int mBufferLen;
	int mBufferPos;

private:
	void ReleaseElement(CXmlElement *pElem);

	INLINE const avf_u8_t *SkipWS(const avf_u8_t *p);

	const avf_u8_t *ParseItem(const avf_u8_t *p, CXmlElement *pParent);
	const avf_u8_t *ParseElementBody(const avf_u8_t *p, CXmlElement *pElem);
	const avf_u8_t *ParseAttributes(const avf_u8_t *p, CXmlElement *pElem);

	void SetError(const char *pError);

	int __ResizeBuffer();
	INLINE int ResizeBuffer() {
		if (mBufferPos == mBufferLen)
			return __ResizeBuffer();
		return 1;
	}

	const avf_u8_t *GetName(const avf_u8_t *p);
	const avf_u8_t *GetString(const avf_u8_t *p);
	const avf_u8_t *ParseEscape(const avf_u8_t *p);

	static void ElementToStream(CMemBuffer& buffer, CXmlElement *pElem, int level);
};

#define ELEM_ROOT 	((CXmlElement*)1)
#define ELEM_FIRST	((CXmlElement*)2)
#define ELEM_LAST	((CXmlElement*)3)

#endif

