
#ifndef __AVF_CONF_H__
#define __AVF_CONF_H__


#define T_WSPACE		(1 << 0)	// \t \n \v \f \r SP
#define T_COMMENT_S		(1 << 1)
#define T_COMMENT_E		(1 << 2)	// 0 \n * /
#define T_DIGIT			(1 << 3)	// 0..9
#define T_HEX			(1 << 4)	// A..F a..f
#define T_STRING		(1 << 5)	
#define T_SPECIAL		(1 << 6)
#define T_LCOM_E		(1 << 7)	// end of line comment

extern avf_u8_t g_char_set[];

#define HAS_FLAG(_c, _t) \
	(g_char_set[_c] & (_t))

#define IS_WSPACE(_c) \
	HAS_FLAG(_c, T_WSPACE)

#define IS_DIGIT(_c) \
	HAS_FLAG(_c, T_DIGIT)

#define IS_STRING(_c) \
	HAS_FLAG(_c, T_STRING)

#define IS_SPECIAL(_c) \
	HAS_FLAG(_c, T_SPECIAL)


struct CConfigNode
{
	CConfigNode *_pParent;
	CConfigNode *_pFirst;
	CConfigNode *_pLast;
	CConfigNode *_pPrev;
	CConfigNode *_pNext;

	avf_hash_val_t hash_val;
	char *_pName;	// need free
	int _index;
	char *_pValue;	// need free, may be NULL

	INLINE const char *GetName() { return _pName ? _pName : ""; }
	INLINE int GetIndex() { return _index; }
	INLINE const char *GetValue(const char *def_value = "") { return _pValue ? _pValue : def_value; }
	INLINE CConfigNode *GetFirst() { return _pFirst; }
	INLINE CConfigNode *GetNext() { return _pNext; }

	CConfigNode *AddChild(const char *name, int index, const char *value);
	CConfigNode *AddChild(avf_hash_val_t hash_val, char *name, int index, char *value);
	CConfigNode *FindChild(const char *name, int index = 0);

	INLINE void Init() {
		_pParent = NULL;
		_pFirst = NULL;
		_pLast = NULL;
		_pPrev = NULL;
		_pNext = NULL;

		hash_val = 0;
		_pName = NULL;
		_index = 0;
		_pValue = NULL;
	}

	void Release();
	void Clear();
};

//-----------------------------------------------------------------------
//
//  class CConfig
//
//-----------------------------------------------------------------------
class CConfig: public CObject
{
	typedef CObject inherited;

public:
	static CConfig* Create();

private:
	CConfig();
	virtual ~CConfig();

public:
	bool Parse(const char *text);
	INLINE CConfigNode *FindChild(const char *pName, int index) {
		return mRoot.FindChild(pName, index);
	}
	INLINE CConfigNode *FindChild(const char *pName) {
		return mRoot.FindChild(pName);
	}
	void Clear();

private:
	const avf_u8_t *SkipCommentLine(const avf_u8_t *ptr);
	const avf_u8_t *SkipCommentBlock(const avf_u8_t *ptr);
	const avf_u8_t *__SkipSpaceComment(const avf_u8_t *ptr);
	INLINE const avf_u8_t *SkipSpaceComment(const avf_u8_t *ptr) {
		return (g_char_set[*ptr] && (T_WSPACE|T_COMMENT_S)) ?
			__SkipSpaceComment(ptr) : ptr;
	}

	const avf_u8_t *GetIndex(const avf_u8_t *ptr, int *pValue);
	const avf_u8_t *GetHexChar(const avf_u8_t *ptr, avf_uint_t *pc);
	const avf_u8_t *GetString(const avf_u8_t *ptr, bool bGetName);

	const avf_u8_t *ParseNode(CConfigNode *parent, const avf_u8_t *ptr);

private:
	int __ResizeBuffer();
	INLINE int ResizeBuffer() {
		if (mBufferPos == mBufferLen)
			return __ResizeBuffer();
		return 1;
	}
	INLINE const avf_u8_t *EmptyString() {
		return (const avf_u8_t *)"";
	}
	const avf_u8_t *ParseError(int line, char *name, char *value);

private:
	CConfigNode mRoot;
	int mLine;
	int mError;

	avf_hash_val_t m_hash_val;
	char *mBuffer;	// need release
	int mBufferLen;
	int mBufferPos;
};

#endif

