
#ifndef __AVF_ERROR_CODE_H__
#define __AVF_ERROR_CODE_H__

//-----------------------------------------------------------------------
//
//	error codes
//
//-----------------------------------------------------------------------

enum avf_status_t {
	E_OK = 0,			// success
	E_PERM = -1,		// Operation not permitted
	E_NOENT = -2,		// No such entry
	E_IO = -5,			// I/O error
	E_AGAIN = -11,		// Try again
	E_NOMEM = -12,		// Out of memory
	E_FAULT = -14,		// Bad address
	E_BUSY = -16,		// Device or resource busy
	E_INVAL = -22,		// Invalid argument
	E_REMOTE = -23,	// IPC error
	E_UNIMP = -24,		// not implemented
	E_INTER = -25,		// interrupted
	E_FATAL = -26,		// fatal error
	E_DATA = -27,		// data error

	E_STATE = -1000,	// wrong state
	E_ERROR = -1001,	// general error
	E_UNKNOWN = -1002,	// unknown command
	E_CONNECT = -1003,	// cannot connect
	E_BADCALL = -1004,	// bad call
	E_DEVICE = -1005,	// device error
	E_CANCEL = -1006,	// cancelled
	E_TIMEOUT = -1007,	// timeout
	E_CLOSE = -1008,		// socket closed
	E_OVERFLOW = -1009,	// buffer overflow

	E_IO_CANNOT_CREATE_FILE = -2001,
	E_IO_CANNOT_OPEN_FILE = -2002,
	E_IO_CANNOT_WRITE_FILE = -2003,
	E_IO_CANNOT_READ_FILE = -2004,

	S_END = 1,			// ended. not an error
};

#endif

