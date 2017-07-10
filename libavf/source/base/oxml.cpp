
#define LOG_TAG "oxml"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "mem_stream.h"

#include "oxml.h"

// should refer to http://www.w3.org/TR/REC-xml/

//-----------------------------------------------------------------------
//
//  class CXmlDoc
//
//-----------------------------------------------------------------------
CXmlDoc* CXmlDoc::Create()
{
	CXmlDoc* result = avf_new CXmlDoc();
	CHECK_STATUS(result);
	return result;
}

CXmlDoc::CXmlDoc()
{
	::memset(&mRoot, 0, sizeof(mRoot));
	mpError = "OK";
	mBuffer = NULL;
	mBufferLen = 0;
}

CXmlDoc::~CXmlDoc()
{
	Clear();
	avf_free(mBuffer);
}

void CXmlDoc::Clear()
{
	ReleaseElement(&mRoot);
	mRoot.~CXmlElement();
	::memset(&mRoot, 0, sizeof(mRoot));
}

void CXmlDoc::ElementToStream(CMemBuffer& buffer, CXmlElement *pElem, int level)
{
	buffer.write_char_n('\t', level);
	buffer.write_char('<');
	buffer.write_string(pElem->pName);

	CXmlAttribute *pAttr = pElem->pFirstAttr;
	if (pAttr) {
		for (; pAttr; pAttr = pAttr->pNext) {
			buffer.write_char(' ');
			buffer.write_string(pAttr->pName);
			buffer.write_char('=');
			buffer.write_char('"');
			buffer.write_string(pAttr->pValue);
			buffer.write_char('"');
		}
	}

	buffer.write_char('>');
	buffer.write_char('\n');

	CXmlElement *pChild = pElem->pFirstChild;
	for (; pChild; pChild = pChild->pNext)
		ElementToStream(buffer, pChild, level + 1);

	buffer.write_char_n('\t', level);
	buffer.write_char('<');
	buffer.write_char('/');
	buffer.write_string(pElem->pName);
	buffer.write_char('>');
	buffer.write_char('\n');
}

void CXmlDoc::ToStream(CMemBuffer& buffer)
{
	ElementToStream(buffer, &mRoot, 0);
	buffer.write_char(0);
}

// change node's value and/or value
int CXmlDoc::SetNode(CXmlNode *pNode, const char *pName, avf_size_t name_len,
	const char *pValue, avf_size_t value_len)
{
	if (pNode == NULL)
		return 0;

	if (pNode == ELEM_ROOT)
		pNode = &mRoot;

	// dup name
	if (pName != NULL) {
		__avf_safe_release(pNode->pName);
		pNode->pName = string_t::CreateFrom(pName, name_len);
	}

	// dup value
	if (pValue != NULL) {
		__avf_safe_release(pNode->pValue);
		pNode->pValue = string_t::CreateFrom(pValue, value_len);
	}

	return 1;
}

CXmlElement* CXmlDoc::CreateElement(const char *pName, avf_size_t name_len,
	const char *pValue, avf_size_t value_len)
{
	CXmlElement *pElem = avf_new CXmlElement();

	if (pElem == NULL)
		return NULL;

	::memset(pElem, 0, sizeof(CXmlElement));

	if (!SetNode(pElem, pName, name_len, pValue, value_len)) {
		avf_delete pElem;
		return NULL;
	}

	return pElem;
}

// if pParentElem is NULL, pElem will be freed
int CXmlDoc::AddElement(CXmlElement *pParentElem, CXmlElement *pPrevElem, CXmlElement *pElem)
{
	CXmlElement *pPrev;
	CXmlElement *pNext;

	if (pElem == NULL)
		return 0;

	if (pParentElem == NULL) {
		avf_delete pElem;
		return 0;
	}

	if (pParentElem == ELEM_ROOT)
		pParentElem = &mRoot;

	if (pPrevElem == ELEM_FIRST) {
		// add as first child
		pPrev = NULL;
		pNext = pParentElem->pFirstChild;
	} else if (pPrevElem == ELEM_LAST) {
		// add as last child
		pPrev = pParentElem->pLastChild;
		pNext = NULL;
	} else {
		// add after pPrevElem
		pParentElem = pPrevElem->pParent;
		pPrev = pPrevElem;
		pNext = pPrevElem->pNext;
	}

	pElem->pParent = pParentElem;
	pElem->pPrev = pPrev;
	pElem->pNext = pNext;

	if (pPrev)
		pPrev->pNext = pElem;
	else
		pParentElem->pFirstChild = pElem;

	if (pNext)
		pNext->pPrev = pElem;
	else
		pParentElem->pLastChild = pElem;

	return 1;
}

CXmlAttribute* CXmlDoc::CreateAttribute(const char *pName, avf_size_t name_len,
	const char *pValue, avf_size_t value_len)
{
	CXmlAttribute *pAttr = avf_new CXmlAttribute();

	if (pAttr == NULL)
		return NULL;

	::memset(pAttr, 0, sizeof(CXmlAttribute));

	if (!SetNode(pAttr, pName, name_len, pValue, value_len)) {
		avf_delete pAttr;
		return NULL;
	}

	return pAttr;
}

// if pElem is NULL, pAttr will be freed
int CXmlDoc::AppendAttribute(CXmlElement *pElem, CXmlAttribute *pAttr)
{
	if (pAttr == NULL)
		return 0;

	if (pElem == NULL) {
		avf_delete pAttr;
		return 0;
	}

	if (pElem == ELEM_ROOT)
		pElem = &mRoot;

	if (pElem->pFirstAttr == NULL) {
		pElem->pFirstAttr = pAttr;
		pElem->pLastAttr = pAttr;
	} else {
		pElem->pLastAttr->pNext = pAttr;
		pElem->pLastAttr = pAttr;
	}

	return 1;
}

CXmlAttribute* CXmlElement::GetAttributeByName(const char *pName)
{
	CXmlAttribute *pAttr = pFirstAttr;
	for (; pAttr; pAttr = pAttr->pNext) {
		if (::strcmp(pAttr->pName->string(), pName) == 0)
			return pAttr;
	}
	return NULL;
}

CXmlElement* CXmlDoc::GetElementByName(CXmlElement *pElem, const char *pName)
{
	if (pElem == NULL)
		return NULL;

	if (pElem == ELEM_ROOT)
		pElem = &mRoot;

	CXmlElement *pChild = pElem->pFirstChild;
	for (; pChild; pChild = pChild->pNext) {
		if (::strcmp(pChild->pName->string(), pName) == 0)
			return pChild;
	}

	return NULL;
}

CXmlAttribute* CXmlDoc::GetAttributeByName(CXmlElement *pElem, const char *pName)
{
	if (pElem == NULL)
		return NULL;

	if (pElem == ELEM_ROOT)
		pElem = &mRoot;

	return pElem->GetAttributeByName(pName);
}

CXmlNode::~CXmlNode()
{
	__avf_safe_release(pName);
	__avf_safe_release(pValue);
}

void CXmlDoc::ReleaseElement(CXmlElement *pElem)
{
	if (pElem == NULL)
		return;

	CXmlAttribute *pAttr = pElem->pFirstAttr;
	while (pAttr) {
		CXmlAttribute *pNext = pAttr->pNext;
		avf_delete pAttr;
		pAttr = pNext;
	}

	CXmlElement *pChild = pElem->pFirstChild;
	while (pChild) {
		CXmlElement *pNext = pChild->pNext;
		ReleaseElement(pChild);
		avf_delete pChild;
		pChild = pNext;
	}
}

//-----------------------------------------------------------------------
//
//  CXmlDoc::Parse
//
//-----------------------------------------------------------------------

#define CT_WSPACE		(1 << 0)
#define CT_NAME_END		(1 << 1)
#define CT_STR_END		(1 << 2)
#define CT_ELEM_END		(1 << 3)
#define CT_AND_RETURN	(1 << 4)

static unsigned char g_char_set[256] = {
/*
Decimal   Octal   Hex	 Binary     Value
-------   -----   ---	 ------     -----
*/

CT_NAME_END|CT_STR_END|CT_ELEM_END,	//  000    000	  000	00000000      NUL    (Null char.)
0,	//  001    001	  001	00000001      SOH    (Start of Header)
0,	//  002    002	  002	00000010      STX    (Start of Text)
0,	//  003    003	  003	00000011      ETX    (End of Text)
0,	//  004    004	  004	00000100      EOT    (End of Transmission)
0,	//  005    005	  005	00000101      ENQ    (Enquiry)
0,	//  006    006	  006	00000110      ACK    (Acknowledgment)
0,	//  007    007	  007	00000111      BEL    (Bell)

0,	//  008    010	  008	00001000       BS    (Backspace)
CT_WSPACE|CT_NAME_END,	//  009    011	  009	00001001       HT    (Horizontal Tab)
CT_WSPACE|CT_NAME_END|CT_AND_RETURN,	//  010    012	  00A	00001010       LF    (Line Feed)
0,	//  011    013	  00B	00001011       VT    (Vertical Tab)
0,	//  012    014	  00C	00001100       FF    (Form Feed)
CT_WSPACE|CT_NAME_END,	//  013    015	  00D	00001101       CR    (Carriage Return)
0,	//  014    016	  00E	00001110       SO    (Shift Out)
0,	//  015    017	  00F	00001111       SI    (Shift In)

0,	//  016    020	  010	00010000      DLE    (Data Link Escape)
0,	//  017    021	  011	00010001      DC1 (XON) (Device Control 1)
0,	//  018    022	  012	00010010      DC2	(Device Control 2)
0,	//  019    023	  013	00010011      DC3 (XOFF)(Device Control 3)
0,	//  020    024	  014	00010100      DC4	(Device Control 4)
0,	//  021    025	  015	00010101      NAK    (Negative Acknowledgement)
0,	//  022    026	  016	00010110      SYN    (Synchronous Idle)
0,	//  023    027	  017	00010111      ETB    (End of Trans. Block)

0,	//  024    030	  018	00011000      CAN    (Cancel)
0,	//  025    031	  019	00011001       EM    (End of Medium)
0,	//  026    032	  01A	00011010      SUB    (Substitute)
0,	//  027    033	  01B	00011011      ESC    (Escape)
0,	//  028    034	  01C	00011100       FS    (File Separator)
0,	//  029    035	  01D	00011101       GS    (Group Separator)
0,	//  030    036	  01E	00011110       RS    (Request to Send)(Record Separator)
0,	//  031    037	  01F	00011111       US    (Unit Separator)


CT_WSPACE|CT_NAME_END,	//  032    040	  020	00100000       SP    (Space)
0,	//  033    041	  021	00100001	!    (exclamation mark)
CT_STR_END,	//  034    042	  022	00100010	"    (double quote)
0,	//  035    043	  023	00100011	#    (number sign)
0,	//  036    044	  024	00100100	$    (dollar sign)
0,	//  037    045	  025	00100101	%    (percent)
CT_AND_RETURN,	//  038    046	  026	00100110	&    (ampersand)
0,	//  039    047	  027	00100111	'    (single quote)


0,	//  040    050	  028	00101000	(    (left/opening parenthesis)
0,	//  041    051	  029	00101001	)    (right/closing parenthesis)
0,	//  042    052	  02A	00101010	*    (asterisk)
0,	//  043    053	  02B	00101011	+    (plus)
0,	//  044    054	  02C	00101100	,    (comma)
0,	//  045    055	  02D	00101101	-    (minus or dash)
0,	//  046    056	  02E	00101110	.    (dot)
CT_NAME_END,	//  047    057	  02F	00101111	/    (forward slash)


0,	//  048    060	  030	00110000	0
0,	//  049    061	  031	00110001	1
0,	//  050    062	  032	00110010	2
0,	//  051    063	  033	00110011	3
0,	//  052    064	  034	00110100	4
0,	//  053    065	  035	00110101	5
0,	//  054    066	  036	00110110	6
0,	//  055    067	  037	00110111	7

0,	//  056    070	  038	00111000	8
0,	//  057    071	  039	00111001	9
0,	//  058    072	  03A	00111010	:    (colon)
0,	//  059    073	  03B	00111011	;    (semi-colon)
CT_ELEM_END,	//  060    074	  03C	00111100	<    (less than)
CT_NAME_END,	//  061    075	  03D	00111101	=    (equal sign)
CT_NAME_END,	//  062    076	  03E	00111110	>    (greater than)
0,	//  063    077	  03F	00111111	?    (question mark)


0,	//  064    100	  040	01000000	@    (AT symbol)
0,	//  065    101	  041	01000001	A
0,	//  066    102	  042	01000010	B
0,	//  067    103	  043	01000011	C
0,	//  068    104	  044	01000100	D
0,	//  069    105	  045	01000101	E
0,	//  070    106	  046	01000110	F
0,	//  071    107	  047	01000111	G

0,	//  072    110	  048	01001000	H
0,	//  073    111	  049	01001001	I
0,	//  074    112	  04A	01001010	J
0,	//  075    113	  04B	01001011	K
0,	//  076    114	  04C	01001100	L
0,	//  077    115	  04D	01001101	M
0,	//  078    116	  04E	01001110	N
0,	//  079    117	  04F	01001111	O


0,	//  080    120	  050	01010000	P
0,	//  081    121	  051	01010001	Q
0,	//  082    122	  052	01010010	R
0,	//  083    123	  053	01010011	S
0,	//  084    124	  054	01010100	T
0,	//  085    125	  055	01010101	U
0,	//  086    126	  056	01010110	V
0,	//  087    127	  057	01010111	W


0,	//  088    130	  058	01011000	X
0,	//  089    131	  059	01011001	Y
0,	//  090    132	  05A	01011010	Z
0,	//  091    133	  05B	01011011	[    (left/opening bracket)
0,	//  092    134	  05C	01011100	\    (back slash)
0,	//  093    135	  05D	01011101	]    (right/closing bracket)
0,	//  094    136	  05E	01011110	^    (caret/circumflex)
0,	//  095    137	  05F	01011111	_    (underscore)


0,	//  096    140	  060	01100000	`
0,	//  097    141	  061	01100001	a
0,	//  098    142	  062	01100010	b
0,	//  099    143	  063	01100011	c
0,	//  100    144	  064	01100100	d
0,	//  101    145	  065	01100101	e
0,	//  102    146	  066	01100110	f
0,	//  103    147	  067	01100111	g

0,	//  104    150	  068	01101000	h
0,	//  105    151	  069	01101001	i
0,	//  106    152	  06A	01101010	j
0,	//  107    153	  06B	01101011	k
0,	//  108    154	  06C	01101100	l
0,	//  109    155	  06D	01101101	m
0,	//  110    156	  06E	01101110	n
0,	//  111    157	  06F	01101111	o


0,	//  112    160	  070	01110000	p
0,	//  113    161	  071	01110001	q
0,	//  114    162	  072	01110010	r
0,	//  115    163	  073	01110011	s
0,	//  116    164	  074	01110100	t
0,	//  117    165	  075	01110101	u
0,	//  118    166	  076	01110110	v
0,	//  119    167	  077	01110111	w


0,	//  120    170	  078	01111000	x
0,	//  121    171	  079	01111001	y
0,	//  122    172	  07A	01111010	z
0,	//  123    173	  07B	01111011	{    (left/opening brace)
0,	//  124    174	  07C	01111100	|    (vertical bar)
0,	//  125    175	  07D	01111101	}    (right/closing brace)
0,	//  126    176	  07E	01111110	~    (tilde)
0,	//  127    177	  07F	01111111    DEL    (delete)
};

#define ADD_CSET(_c, _t) \
	do { g_char_set[_c] |= _t; } while (0)

#define HAS_FLAG(_c, _t) \
	(g_char_set[(unsigned char)(_c)] & (_t))

#define IS_WSPACE(_c) \
	HAS_FLAG(_c, CT_WSPACE)

#define IS_NAME_END(_c) \
	HAS_FLAG(_c, CT_NAME_END)

#define IS_STR_END(_c) \
	HAS_FLAG(_c, CT_STR_END)

#define IS_ELEM_END(_c) \
	HAS_FLAG(_c, CT_ELEM_END)

INLINE const avf_u8_t *CXmlDoc::SkipWS(const avf_u8_t *p)
{
	while (IS_WSPACE(*p)) {
		if (*p == '\n')
			mLine++;
		p++;
	}
	return p;
}

int CXmlDoc::Parse(const avf_u8_t *pText)
{
	const avf_u8_t *p = pText;

	mpError = "OK";
	mLevel = 0;
	mLine = 1;
	mDecl = 0;

	mBufferPos = 0;
	if (mBuffer == NULL) {
		mBufferLen = 64;
		if ((mBuffer = (char*)avf_malloc(mBufferLen)) == NULL) {
			return 0;
		}
	}

	p = SkipWS(p);
	while (*p) {
		if ((p = ParseItem(p, ELEM_ROOT)) == NULL)
			return 0;
		p = SkipWS(p);
	}

	return 1;
}

const avf_u8_t *CXmlDoc::ParseItem(const avf_u8_t *p, CXmlElement *pParent)
{
	if (*p != '<') {
		SetError("'<' expected");
		return NULL;
	}
	p++;	// <

	// <?xml version="1.0" encoding="ISO-8859-1"?>
	if (*p == '?') {
		p++;	// ?

		if ((p = (const avf_u8_t*)::strstr((const char*)p, "?>")) == NULL) {
			SetError("?> not found");
			return NULL;
		}
		p += 2;	// ?>

		if (mLevel > 0) {
			SetError("cannot define declaration here");
			return NULL;
		}

		if (mDecl) {
			SetError("declaration already defined");
			return NULL;
		}

		return p;
	}

	// <!--  comment string -->
	if (*p == '!') {
		p++;	// !

		if (p[0] != '-' || p[1] != '-') {
			SetError("Bad comment");
			return NULL;
		}
		p += 2;	// --

		if ((p = (const avf_u8_t*)::strstr((const char*)p, "-->")) == NULL) {
			SetError("--> not found");
			return NULL;
		}

		p += 3;	// -->
		return p;
	}

	// <elem attr="val" attr="val">element</elem>
	if ((p = GetName(p)) == NULL)
		return NULL;

	CXmlElement *pElem;

	if (pParent == ELEM_ROOT) {
		pElem = &mRoot;
		if (pElem->pName != NULL) {
			SetError("There can only be one root element");
			return NULL;
		}

		if (!SetNode(pElem, mBuffer, mBufferPos, NULL, 0))
			return NULL;

	} else {
		if ((pElem = CreateElement(mBuffer, mBufferPos, NULL, 0)) == NULL)
			return NULL;

		AddElement(pParent, ELEM_LAST, pElem);
	}

	p = SkipWS(p);
	if (*p == 0) {
		SetError("'>' expected");
		return NULL;
	}

	// />
	if (*p == '/' && *(p+1) == '>') {
		return p + 2;
	}

	// >
	// element </elem>
	if (*p == '>') {
		p++;
		return ParseElementBody(p, pElem);
	}

	if ((p = ParseAttributes(p, pElem)) == NULL)
		return NULL;

	if (*p == 0) {
		SetError("'>' expected");
		return NULL;
	}

	// />
	if (*p == '/' && *(p+1) == '>') {
		return p + 2;
	}

	// > element </elem>
	if (*p == '>') {
		p++;
		return ParseElementBody(p, pElem);
	}

	SetError("'/>' expected");
	return NULL;
}

// *p == 0
const avf_u8_t *CXmlDoc::ParseElementBody(const avf_u8_t *p, CXmlElement *pElem)
{
	p = SkipWS(p);
	while (1) {

		// </elem>
		if (*p == '<' && *(p+1) == '/') {
			p += 2;

			if ((p = GetName(p)) == NULL)
				return NULL;
			if (::strcmp(pElem->pName->string(), mBuffer) != 0) {
				SetError("element not match");
				return NULL;
			}

			if (*p != '>') {
				SetError("'>' expected");
				return NULL;
			}

			return p + 1;
		}

		if (*p != '<') {
			if ((p = GetName(p)) == NULL)
				return NULL;
			if (!SetNode(pElem, NULL, 0, mBuffer, mBufferPos))
				return NULL;
		}

		mLevel++;
		while (*p) {
			if ((p = ParseItem(p, pElem)) == NULL) {
				mLevel--;
				return NULL;
			}
			p = SkipWS(p);
			if (*p != '<') {
				SetError("'<' expected");
				return NULL;
			}
			if (*(p+1) == '/')
				break;
		}
		mLevel--;
	}
}

// attr="value" attr="value" /> | >
const avf_u8_t *CXmlDoc::ParseAttributes(const avf_u8_t *p, CXmlElement *pElem)
{
	while (1) {
		if (*p == 0) {
			SetError("'>' expected");
			return NULL;
		}

		if (*p == '>' || (*p == '/' && *(p+1) == '>'))
			return p;

		if ((p = GetName(p)) == NULL)
			return NULL;

		CXmlAttribute *pAttr = CreateAttribute(mBuffer, mBufferPos, NULL, 0);
		if (pAttr == NULL)
			return NULL;

		AppendAttribute(pElem, pAttr);

		p = SkipWS(p);
		if (*p != '=') {
			SetError("'=' expected");
			return NULL;
		}
		p++;

		p = SkipWS(p);

		if ((p = GetString(p)) == NULL)
			return NULL;

		if (!SetNode(pAttr, NULL, 0, mBuffer, mBufferPos))
			return NULL;

		p = SkipWS(p);
	}
}

void CXmlDoc::SetError(const char *pError)
{
	mpError = pError;
	AVF_LOGE("Error at line %d: %s", mLine, mpError);
}

int CXmlDoc::__ResizeBuffer()
{
	char *pNewBuffer = (char*)avf_malloc(mBufferLen + 32);
	if (pNewBuffer == NULL)
		return 0;

	::memcpy(pNewBuffer, mBuffer, mBufferLen);

	avf_free(mBuffer);
	mBuffer = pNewBuffer;
	mBufferLen += 32;

	return 1;
}


// &lt;		<
// &gt;		>
// &amp;	&
// &apos;	'
// &quot;	"

const avf_u8_t *CXmlDoc::ParseEscape(const avf_u8_t *p)
{
	char c;

	if (::strncmp((const char*)(p+1), "lt;", 3) == 0) {
		p += 4;
		c = '<';
	} else if (::strncmp((const char*)(p+1), "gt;", 3) == 0) {
		p += 4;
		c = '>';
	} else if (::strncmp((const char*)(p+1), "amp;", 4) == 0) {
		p += 5;
		c = '&';
	} else if (::strncmp((const char*)(p+1), "apos;", 5) == 0) {
		p += 6;
		c = '\'';
	} else if (::strncmp((const char*)(p+1), "quot;", 5) == 0) {
		p += 6;
		c = '"';
	} else {
		return p;
	}

	if (!ResizeBuffer())
		return NULL;

	mBuffer[mBufferPos++] = c;

	return p;
}

const avf_u8_t *CXmlDoc::GetName(const avf_u8_t *p)
{
	mBufferPos = 0;

	while (1) {
		avf_uint_t c = *p;

		if (HAS_FLAG(c, CT_NAME_END|CT_AND_RETURN)) {
			if (IS_NAME_END(c)) {
				if (mBufferPos == 0) {
					AVF_LOGE("%s", p);
					SetError("no name");
					return NULL;
				}
				break;
			}

			if (c == '&') {
				if ((p = ParseEscape(p)) == NULL)
					return NULL;
				continue;
			}

			if (c == '\n')
				mLine++;
		}

		if (!ResizeBuffer())
			return NULL;

		mBuffer[mBufferPos++] = c;
		p++;
	}

	if (!ResizeBuffer())
		return NULL;

	mBuffer[mBufferPos] = 0;

	return p;
}

const avf_u8_t *CXmlDoc::GetString(const avf_u8_t *p)
{
	if (*p != '"') {
		SetError("'\"' expected");
		return NULL;
	}
	p++;

	mBufferPos = 0;

	while (1) {
		avf_uint_t c = *p;

		if (HAS_FLAG(c, CT_STR_END|CT_AND_RETURN)) {
			if (IS_STR_END(c)) {
				//if (mBufferPos == 0) {
				//	SetError("bad string");
				//	return NULL;
				//}
				break;
			}

			if (c == '&') {
				if ((p = ParseEscape(p)) == NULL)
					return NULL;
				continue;
			}

			if (c == '\n')
				mLine++;
		}

		if (!ResizeBuffer())
			return NULL;

		mBuffer[mBufferPos++] = c;
		p++;
	}

	if (*p != '"') {
		SetError("'\"' expected");
		return NULL;
	}
	p++;

	if (!ResizeBuffer())
		return NULL;

	mBuffer[mBufferPos] = 0;

	return p;
}

