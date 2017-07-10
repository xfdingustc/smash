
#include "codec_common.h"
#include "bs_reader.h"

void bs_reader_read(bs_reader_t *bsr, const u8 *end_ptr)
{
	if (bsr->read_cb) {
		bsr->read_cb(bsr->ctx, bsr, end_ptr);
	} else {
		if (bsr->underrun) {
			bsr->underrun_cb(bsr->ctx, bsr);
		}
		bsr->underrun = 1;
	}
}

