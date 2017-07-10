
#define LOG_TAG "avf_config"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avf_util.h"
#include "avf_config.h"

// reserved char
// = ; \ [ ] { } " '

avf_u8_t g_char_set[256] = {
/*
Decimal   Octal   Hex	 Binary     Value
-------   -----   ---	 ------     -----
*/

T_LCOM_E|T_COMMENT_E,	//  000    000	  000	00000000      NUL    (Null char.)
0,				//  001    001	  001	00000001      SOH    (Start of Header)
0,				//  002    002	  002	00000010      STX    (Start of Text)
0,				//  003    003	  003	00000011      ETX    (End of Text)
0,				//  004    004	  004	00000100      EOT    (End of Transmission)
0,				//  005    005	  005	00000101      ENQ    (Enquiry)
0,				//  006    006	  006	00000110      ACK    (Acknowledgment)
0,				//  007    007	  007	00000111      BEL    (Bell)

0,						//  008    010	  008	00001000       BS    (Backspace)
T_WSPACE,				//  009    011	  009	00001001       HT    (Horizontal Tab)
T_WSPACE|T_LCOM_E|T_COMMENT_E|T_SPECIAL,	//  010    012	  00A	00001010       LF    (Line Feed)
T_WSPACE,				//  011    013	  00B	00001011       VT    (Vertical Tab)
T_WSPACE,				//  012    014	  00C	00001100       FF    (Form Feed)
T_WSPACE,				//  013    015	  00D	00001101       CR    (Carriage Return)
0,						//  014    016	  00E	00001110       SO    (Shift Out)
0,						//  015    017	  00F	00001111       SI    (Shift In)

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


T_WSPACE|T_SPECIAL,			//  032    040	  020	00100000       SP    (Space)
T_STRING,			//  033    041	  021	00100001	!    (exclamation mark)
T_SPECIAL,				//  034    042	  022	00100010	"    (double quote)
T_STRING,			//  035    043	  023	00100011	#    (number sign)
T_STRING,			//  036    044	  024	00100100	$    (dollar sign)
T_STRING,			//  037    045	  025	00100101	%    (percent)
T_STRING,			//  038    046	  026	00100110	&    (ampersand)
T_SPECIAL,			//  039    047	  027	00100111	'    (single quote)


T_STRING,			//  040    050	  028	00101000	(    (left/opening parenthesis)
T_STRING,			//  041    051	  029	00101001	)    (right/closing parenthesis)
T_STRING|T_COMMENT_E,	//  042    052	  02A	00101010	*    (asterisk)
T_STRING,			//  043    053	  02B	00101011	+    (plus)
T_STRING,			//  044    054	  02C	00101100	,    (comma)
T_STRING,			//  045    055	  02D	00101101	-    (minus or dash)
T_STRING,			//  046    056	  02E	00101110	.    (dot)
T_STRING|T_COMMENT_S|T_COMMENT_E,	//  047    057	  02F	00101111	/    (forward slash)


T_STRING|T_DIGIT,	//  048    060	  030	00110000	0
T_STRING|T_DIGIT,	//  049    061	  031	00110001	1
T_STRING|T_DIGIT,	//  050    062	  032	00110010	2
T_STRING|T_DIGIT,	//  051    063	  033	00110011	3
T_STRING|T_DIGIT,	//  052    064	  034	00110100	4
T_STRING|T_DIGIT,	//  053    065	  035	00110101	5
T_STRING|T_DIGIT,	//  054    066	  036	00110110	6
T_STRING|T_DIGIT,	//  055    067	  037	00110111	7

T_STRING|T_DIGIT,	//  056    070	  038	00111000	8
T_STRING|T_DIGIT,	//  057    071	  039	00111001	9
T_STRING,			//  058    072	  03A	00111010	:    (colon)
T_SPECIAL,		//  059    073	  03B	00111011	;    (semi-colon)
T_STRING,	//  060    074	  03C	00111100	<    (less than)
T_SPECIAL,		//  061    075	  03D	00111101	=    (equal sign)
T_STRING,	//  062    076	  03E	00111110	>    (greater than)
T_STRING,	//  063    077	  03F	00111111	?    (question mark)


T_STRING,	//  064    100	  040	01000000	@    (AT symbol)
T_STRING|T_HEX,	//  065    101	  041	01000001	A
T_STRING|T_HEX,	//  066    102	  042	01000010	B
T_STRING|T_HEX,	//  067    103	  043	01000011	C
T_STRING|T_HEX,	//  068    104	  044	01000100	D
T_STRING|T_HEX,	//  069    105	  045	01000101	E
T_STRING|T_HEX,	//  070    106	  046	01000110	F
T_STRING,	//  071    107	  047	01000111	G

T_STRING,	//  072    110	  048	01001000	H
T_STRING,	//  073    111	  049	01001001	I
T_STRING,	//  074    112	  04A	01001010	J
T_STRING,	//  075    113	  04B	01001011	K
T_STRING,	//  076    114	  04C	01001100	L
T_STRING,	//  077    115	  04D	01001101	M
T_STRING,	//  078    116	  04E	01001110	N
T_STRING,	//  079    117	  04F	01001111	O


T_STRING,	//  080    120	  050	01010000	P
T_STRING,	//  081    121	  051	01010001	Q
T_STRING,	//  082    122	  052	01010010	R
T_STRING,	//  083    123	  053	01010011	S
T_STRING,	//  084    124	  054	01010100	T
T_STRING,	//  085    125	  055	01010101	U
T_STRING,	//  086    126	  056	01010110	V
T_STRING,	//  087    127	  057	01010111	W


T_STRING,	//  088    130	  058	01011000	X
T_STRING,	//  089    131	  059	01011001	Y
T_STRING,	//  090    132	  05A	01011010	Z
T_SPECIAL,		//  091    133	  05B	01011011	[    (left/opening bracket)
T_SPECIAL,		//  092    134	  05C	01011100	\    (back slash)
T_SPECIAL,		//  093    135	  05D	01011101	]    (right/closing bracket)
T_STRING,	//  094    136	  05E	01011110	^    (caret/circumflex)
T_STRING,	//  095    137	  05F	01011111	_    (underscore)


T_STRING,	//  096    140	  060	01100000	`
T_STRING|T_HEX,	//  097    141	  061	01100001	a
T_STRING|T_HEX,	//  098    142	  062	01100010	b
T_STRING|T_HEX,	//  099    143	  063	01100011	c
T_STRING|T_HEX,	//  100    144	  064	01100100	d
T_STRING|T_HEX,	//  101    145	  065	01100101	e
T_STRING|T_HEX,	//  102    146	  066	01100110	f
T_STRING,	//  103    147	  067	01100111	g

T_STRING,	//  104    150	  068	01101000	h
T_STRING,	//  105    151	  069	01101001	i
T_STRING,	//  106    152	  06A	01101010	j
T_STRING,	//  107    153	  06B	01101011	k
T_STRING,	//  108    154	  06C	01101100	l
T_STRING,	//  109    155	  06D	01101101	m
T_STRING,	//  110    156	  06E	01101110	n
T_STRING,	//  111    157	  06F	01101111	o


T_STRING,	//  112    160	  070	01110000	p
T_STRING,	//  113    161	  071	01110001	q
T_STRING,	//  114    162	  072	01110010	r
T_STRING,	//  115    163	  073	01110011	s
T_STRING,	//  116    164	  074	01110100	t
T_STRING,	//  117    165	  075	01110101	u
T_STRING,	//  118    166	  076	01110110	v
T_STRING,	//  119    167	  077	01110111	w


T_STRING,	//  120    170	  078	01111000	x
T_STRING,	//  121    171	  079	01111001	y
T_STRING,	//  122    172	  07A	01111010	z
T_SPECIAL,			//  123    173	  07B	01111011	{    (left/opening brace)
T_STRING,	//  124    174	  07C	01111100	|    (vertical bar)
T_SPECIAL,			//  125    175	  07D	01111101	}    (right/closing brace)
T_STRING,	//  126    176	  07E	01111110	~    (tilde)
0,			//  127    177	  07F	01111111    DEL    (delete)

T_STRING, 	//  128
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  136
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  144
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  152
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  160
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  168
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  176
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  184
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  192
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  200
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  208
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  216
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  224
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  232
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  240
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

T_STRING, 	//  248
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//
T_STRING, 	//

0, 	//  254
0, 	//  255
};

//-----------------------------------------------------------------------
//
//  struct CConfigNode
//
//-----------------------------------------------------------------------

CConfigNode *CConfigNode::AddChild(const char *name, int index, const char *value)
{
	avf_hash_val_t hash_val = avf_calc_hash((const avf_u8_t*)name);
	char *pname = avf_strdup(name);
	char *pvalue = avf_strdup(value);
	return AddChild(hash_val, pname, index, pvalue);
}

CConfigNode *CConfigNode::AddChild(avf_hash_val_t hash_val, char *name, int index, char *value)
{
	CConfigNode *child = avf_new CConfigNode();

	if (child) {
		child->_pParent = this;
		child->_pFirst = NULL;
		child->_pLast = NULL;

		child->hash_val = hash_val ^ index;
		child->_pName = name;
		child->_index = index;
		child->_pValue = value;

		if (_pLast == NULL) {
			_pFirst = child;
			_pLast = child;
			child->_pPrev = NULL;
			child->_pNext = NULL;
		} else {
			_pLast->_pNext = child;
			child->_pPrev = _pLast;
			_pLast = child;
			child->_pNext = NULL;
		}
	}

	return child;
}

CConfigNode *CConfigNode::FindChild(const char *pName, int index)
{
	CConfigNode *child = _pFirst;
	if (child) {
		avf_hash_val_t hash_val = avf_calc_hash((const avf_u8_t*)pName);
		hash_val ^= index;
		do {
			if (hash_val == child->hash_val) {
				if (::strcmp(pName, child->_pName) == 0 && index == child->_index)
					return child;
			}
		} while ((child = child->_pNext) != NULL);
	}
	return child;
}

void CConfigNode::Release()
{
	Clear();
	avf_safe_free(_pName);
	avf_safe_free(_pValue);
	avf_delete this;
}

void CConfigNode::Clear()
{
	CConfigNode *child = _pFirst;
	while (child) {
		CConfigNode *next = child->_pNext;
		child->Release();
		child = next;
	}
	_pFirst = NULL;
	_pLast = NULL;
}

//-----------------------------------------------------------------------
//
//  class CConfig
//
//-----------------------------------------------------------------------

CConfig* CConfig::Create()
{
	CConfig* result = avf_new CConfig();
	CHECK_STATUS(result);
	return result;
}

CConfig::CConfig()
{
	mRoot.Init();
	mBuffer = NULL;
	mBufferLen = 0;
	mBufferPos = 0;
}

CConfig::~CConfig()
{
	Clear();
	avf_safe_free(mBuffer);
}

void CConfig::Clear()
{
	mRoot.Clear();
}

int CConfig::__ResizeBuffer()
{
	char *pNewBuffer = (char*)avf_malloc(mBufferLen + 32);
	if (pNewBuffer == NULL) {
		mError = __LINE__;
		return 0;
	}

	if (mBuffer) {
		::memcpy(pNewBuffer, mBuffer, mBufferLen);
		avf_free(mBuffer);
	}

	mBuffer = pNewBuffer;
	mBufferLen += 32;

	return 1;
}

const avf_u8_t *CConfig::SkipCommentLine(const avf_u8_t *ptr)
{
	avf_uint_t c = *ptr;
	while (true) {
		if (HAS_FLAG(c, T_LCOM_E)) {
			if (c == '\n') {
				mLine++;
				return ptr + 1;
			}
			if (c == 0)
				return ptr;
		}
		c = *++ptr;
	}
}

const avf_u8_t *CConfig::SkipCommentBlock(const avf_u8_t *ptr)
{
	int level = 1;
	avf_uint_t c = *ptr;

	while (true) {
		if (HAS_FLAG(c, T_COMMENT_E)) {
			switch (c) {
			case '*':
				if (ptr[1] == '/') {
					ptr++;
					if (--level == 0)
						return ptr + 1;
				}
				break;
			
			case '/':
				if (ptr[1] == '*') {
					ptr++;
					level++;
				}
				break;
			
			case '\n':
				mLine++;
				break;
			
			default:	// 0
				return ptr;
			}
		}
		c = *++ptr;
	}
}

// skip spaces and comments
const avf_u8_t *CConfig::__SkipSpaceComment(const avf_u8_t *ptr)
{
	while (true) {
		avf_uint_t c = *ptr;

		while (IS_WSPACE(c)) {
			if (c == '\n') mLine++;
			c = *++ptr;
		}

		if (c != '/')
			break;

		c = ptr[1];
		if (c == '/') {
			ptr = SkipCommentLine(ptr + 2);
		} else if (c == '*') {
			ptr = SkipCommentBlock(ptr + 2);
		} else {
			break;
		}
	}

	return ptr;
}

const avf_u8_t *CConfig::GetIndex(const avf_u8_t *ptr, int *pValue)
{
	int value = 0;
	avf_uint_t c = *ptr;
	for (; IS_DIGIT(c); c = *++ptr) {
		value = (value * 10) + (c - '0');
	}
	*pValue = value;
	return ptr;
}

// \xAB
const avf_u8_t *CConfig::GetHexChar(const avf_u8_t *ptr, avf_uint_t *pc)
{
	*pc = 0;
	for (int i = 0; i < 2; i++) {
		avf_uint_t c = *++ptr;
		if (!HAS_FLAG(c, T_DIGIT|T_HEX))
			break;
		if (IS_DIGIT(c))
			*pc = (*pc << 4) + (c - '0');
		else if (c >= 'A')
			*pc = (*pc << 4) + (c - 'A' + 10);
		else
			*pc = (*pc << 4) + (c - 'a' + 10);
	}
	return ptr;
}

// [value]
const avf_u8_t *CConfig::GetString(const avf_u8_t *ptr, bool bGetName)
{
	m_hash_val = HASH_INIT_VAL;
	mBufferPos = 0;

	if (!ResizeBuffer())
		return EmptyString();

	int escape = 0;

	while (1) {
		avf_uint_t c = *ptr;

		if (!IS_STRING(c)) {
			if (c != '\\') {
				if (c == '[') {
					if (bGetName)
						break;
					escape++;
					ptr++;
					continue;
				}
				if (c == ']') {
					if (escape == 0) {
						AVF_LOGE("unexpected ] at line %d", mLine);
						mError = __LINE__;
						return EmptyString();
					}
					escape--;
					ptr++;
					continue;
				}
				if (c == ' ' && escape != 0) {
					goto Next;
				}
				break;
			}

			if (escape == 0) {
				c = *++ptr;

				switch (c) {
				case 'a': c = '\a'; break;
				case 'b': c = '\b'; break;
				case 'f': c = '\f'; break;
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 't': c = '\t'; break;
				case 'v': c = '\v'; break;
				case 'x': ptr = GetHexChar(ptr, &c); break;
				case '\n': mLine++; break;
				default:
					if (!IS_SPECIAL(c)) {
						AVF_LOGE("bad escape code %c (%d) at line %d", c, c, mLine);
						mError = __LINE__;
						return EmptyString();
					}
					break;
				}
			}
		}

Next:
		mBuffer[mBufferPos++] = c;
		HASH_NEXT_VAL(m_hash_val, c);

		if (!ResizeBuffer())
			return EmptyString();

		ptr++;
	}

	mBuffer[mBufferPos] = 0;

	return SkipSpaceComment(ptr);
}

const avf_u8_t *CConfig::ParseError(int line, char *name, char *value)
{
	avf_safe_free(name);
	avf_safe_free(value);
	AVF_LOGE("parse error at line %d, errno %d", mLine, line);
	mError = line;
	return EmptyString();
}

// name[index]=value{content};
const avf_u8_t *CConfig::ParseNode(CConfigNode *parent, const avf_u8_t *ptr)
{
	avf_hash_val_t hash_val;
	char *name = NULL;
	int index = 0;
 	char *value = NULL;

	// get name
	ptr = GetString(ptr, true);
	if (mBufferPos == 0)
		return ptr;

	hash_val = m_hash_val;
	if ((name = avf_strdup(mBuffer)) == NULL)
		return ParseError(__LINE__, name, value);

	// get index
	if (*ptr == '[') {
		ptr = SkipSpaceComment(ptr + 1);
		ptr = GetIndex(ptr, &index);
		ptr = SkipSpaceComment(ptr);
		if (*ptr != ']')
			return ParseError(__LINE__, name, value);
		ptr = SkipSpaceComment(ptr + 1);
	}

	// get value
	if (*ptr == '=') {
		ptr = GetString(ptr + 1, false);
		//if (mBufferPos == 0)
		//	return ParseError(__LINE__, name, value);
		if ((value = avf_strdup(mBuffer)) == NULL)
			return ParseError(__LINE__, name, value);
	}

	// AVF_LOGD("add node %s[%d]: %s", name, index, value);
	CConfigNode *node = parent->AddChild(hash_val, name, index, value);
	if (node == NULL)
		return ParseError(__LINE__, name, value);

	// get body
	if (*ptr == '{') {
		ptr = SkipSpaceComment(ptr + 1);
		while (true) {
			const avf_u8_t *p = ParseNode(node, ptr);
			if (*p == '}') {
				ptr = SkipSpaceComment(p + 1);
				break;
			}
			if (*p == 0 || p == ptr) {
				return ParseError(__LINE__, NULL, NULL);
			}
			ptr = p;
		}
	}

	// optional
	if (*ptr == ';') {
		ptr = SkipSpaceComment(ptr + 1);
	}

	return ptr;
}

bool CConfig::Parse(const char *text)
{
	mLine = 1;
	mError = 0;

	const avf_u8_t *ptr = (const avf_u8_t*)text;
	ptr = SkipSpaceComment(ptr);
	while (*ptr != 0) {
		ptr = ParseNode(&mRoot, ptr);
	}

	if (mError) {
		AVF_LOGE("CConfig::Parse error: %d", mError);
		return false;
	}

	return true;
}

