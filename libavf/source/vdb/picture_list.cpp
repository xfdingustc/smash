
#define LOG_TAG "piclist"

#include <dirent.h>
#include <sys/stat.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_socket.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_registry.h"
#include "avf_util.h"
#include "avio.h"
#include "file_io.h"
#include "mem_stream.h"

#include "vdb_cmd.h"
#include "picture_list.h"

static bool is_jpg_file(const char *name, int len)
{
	return name[len-4] == '.' && name[len-3] == 'j' && name[len-2] == 'p' && name[len-1] == 'g';
}

static bool is_dng_file(const char *name, int len)
{
	return name[len-4] == '.' && name[len-3] == 'd' && name[len-2] == 'n' && name[len-1] == 'g';
}

//-----------------------------------------------------------------------
//
//  CPictureList
//
//-----------------------------------------------------------------------
CPictureList* CPictureList::Create(string_t *p_root_dir)
{
	CPictureList *result = avf_new CPictureList(p_root_dir);
	CHECK_STATUS(result);
	return result;
}

CPictureList::CPictureList(string_t *p_root_dir)
{
	mp_root_dir = p_root_dir->acquire();
	mb_loaded = false;
	InitList();
}

CPictureList::~CPictureList()
{
	ReleaseAllPictures(picture_set.rb_node);
	avf_safe_release(mp_root_dir);
}

avf_status_t CPictureList::LoadPictureList()
{
	AUTO_LOCK(mMutex);
	mb_loaded = true;
	return ScanL();
}

void CPictureList::ClearPictureList(bool bUnload)
{
	AUTO_LOCK(mMutex);

	if (!mb_scanned)
		return;

	AVF_LOGD(C_YELLOW "ClearPictureList" C_NONE);

	if (bUnload) {
		mb_loaded = false;
	}

	ReleaseAllPictures(picture_set.rb_node);
	InitList();

	OnListClearedL();
}

void CPictureList::GetPictureList(CMemBuffer *pmb)
{
	AVF_LOGD(C_CYAN "GetPictureList" C_NONE);

	AUTO_LOCK(mMutex);
	ScanL();

	pmb->write_le_32(m_num_pictures);

	struct rb_node *node = rb_first(&picture_set);
	for (; node; node = rb_next(node)) {
		CPictureItem *pi = CPictureItem::from_node(node);
		// vdb_picture_info_t
		avf_u32_t filename_size = pi->dirname->size() + 1;
		avf_u32_t info_size = 16 + (4 + ROUND_UP(filename_size, 4));
		pmb->write_le_32(info_size);
		pmb->write_le_32(pi->date);
		pmb->write_le_32(0);	// reserved1
		pmb->write_le_32(0);	// reserved2
		pmb->write_string_aligned(pi->dirname->string(), filename_size);
	}
}

avf_status_t CPictureList::ReadPicture(const char *picname, 
	const vdb_cmd_ReadPicture_t *cmd, vdb_ack_t *ack,
	avf_u8_t *buffer, int bufsize, int bufpos, bool bThumbnail,
	CSocketEvent *pSocketEvent, CSocket *pSocket, int timeout, bool bCheckClose)
{
	AVF_LOGD(C_CYAN "ReadPicture %s" C_NONE, picname);

	AUTO_LOCK(mMutex);

	IAVIO *io = CFileIO::Create();
	if (io == NULL)
		return E_NOMEM;

	string_t *fullname = CreateFullName(picname);

	avf_status_t status = io->OpenRead(fullname->string());
	if (status == E_OK) {
		status = ReadPictureLocked(picname, io, cmd, ack,
			buffer, bufsize, bufpos, bThumbnail,
			pSocketEvent, pSocket, timeout, bCheckClose);
	}

	fullname->Release();
	io->Release();

	return status;
}

// ack->extra_bytes
// vdb_ack_ReadPicture_t:
//		picture_name
//		picture_size
//		picture_data
//		result_ok
avf_status_t CPictureList::ReadPictureLocked(const char *picname, IAVIO *io, 
	const vdb_cmd_ReadPicture_t *cmd, vdb_ack_t *ack, 
	avf_u8_t *buffer, int bufsize, int bufpos, bool bThumbnail,
	CSocketEvent *pSocketEvent, CSocket *pSocket, int timeout, bool bCheckClose)
{
	avf_uint_t picname_size = ::strlen(picname) + 1;
	avf_size_t fsize = bThumbnail ? FindThumbnail(picname, picname_size, io) : io->GetSize();
	avf_size_t padding = 0;
	avf_status_t status;

	avf_u8_t *ptr = buffer + bufpos;
	SAVE_LE_32(ptr, picname_size);

	::memcpy(ptr, picname, picname_size);
	ptr += picname_size;
	int n = picname_size & 3;
	n = n == 0 ? 0 : 4 - n;
	for (; n; n--)
		*ptr++ = 0;

	SAVE_LE_32(ptr, fsize);

	int total = (ptr - buffer) + fsize + 4;	// read_status
	if (total <= VDB_ACK_SIZE) {
		padding = VDB_ACK_SIZE - total;
		ack->extra_bytes = 0;
	} else {
		padding = 0;
		ack->extra_bytes = total - VDB_ACK_SIZE;
	}

	avf_s32_t read_status = 0;

	if (fsize == 0) {
		status = pSocketEvent->TCPSend(pSocket, timeout, buffer, ptr - buffer, bCheckClose);
		if (status != E_OK)
			return status;
		read_status = E_ERROR;
	} else {
		avf_uint_t remain = bufsize - (ptr - buffer);
		do {
			avf_uint_t toread = MIN(remain, fsize);

			if (io->Read(ptr, toread) != (int)toread)
				read_status = E_IO;

			avf_uint_t towrite = ptr - buffer + toread;
			status = pSocketEvent->TCPSend(pSocket, timeout, buffer, towrite, bCheckClose);
			if (status != E_OK)
				return status;

			ptr = buffer;
			remain = bufsize;
			fsize -= toread;
		} while (fsize > 0);
	}

	status = pSocketEvent->TCPSend(pSocket, timeout, (const avf_u8_t*)&read_status, 4, bCheckClose);
	if (status != E_OK)
		return status;

	if (padding > 0) {
		avf_u8_t tmp[VDB_ACK_SIZE];
		::memset(tmp, 0, padding);
		status = pSocketEvent->TCPSend(pSocket, timeout, tmp, padding, bCheckClose);
		if (status != E_OK)
			return status;
	}

	return E_OK;
}

avf_int_t CPictureList::ReadTiffValue(CReadBuffer& buffer, bool b_le)
{
	int field_type = buffer.read_16(b_le);
	int value;

	buffer.Skip(4);	// count
	switch (field_type) {
	case 1:
		value = buffer.read_le_8();
		buffer.Skip(3);
		break;
	case 3:
		value = buffer.read_16(b_le);
		buffer.Skip(2);
		break;
	case 4:
		value = buffer.read_32(b_le);
		break;
	default:
		value = 01;
		buffer.Skip(4);
	}

	return value;
}

avf_size_t CPictureList::FindThumbnail(const char *picname, int picname_size, IAVIO *io)
{
	picname_size--;
	if (is_jpg_file(picname, picname_size))
		return FindThumbnail_jpg(io);
	if (is_dng_file(picname, picname_size))
		return FindThumbnail_dng(io);
	return 0;
}

avf_size_t CPictureList::FindThumbnail_jpg(IAVIO *io)
{
	avf_size_t fsize = 0;// io->GetSize();
	int start_pos = 0;
	bool b_le = 0;
	int tiff_start;
	int tmp;
	int enc_type = -1;
	int offset = -1;
	int size = -1;

	CReadBuffer buffer(io);

	if (buffer.read_be_16() != 0xFFD8 || buffer.read_be_16() != 0xFFE1)
		goto Done;

	buffer.Skip(2);	// L
	if (buffer.read_be_32() != MKFCC('E','x','i','f') || buffer.read_le_16() != 0)
		goto Done;

	tiff_start = buffer.GetReadPos();
	b_le = buffer.read_be_16() == 0x4949;	// II

	if (buffer.read_16(b_le) != 0x2A)
		goto Done;

	tmp = buffer.read_32(b_le);

	do {
		buffer.SeekTo(tiff_start + tmp);

		int num_dir = buffer.read_16(b_le);
		for (int i = num_dir; i > 0; i--) {
			int tag = buffer.read_16(b_le);
			switch (tag) {
			case 0x103:
				enc_type = ReadTiffValue(buffer, b_le);
				break;
			case 0x201:
				offset = ReadTiffValue(buffer, b_le);
				break;
			case 0x202:
				size = ReadTiffValue(buffer, b_le);
				break;
			default:
				ReadTiffValue(buffer, b_le);
				break;
			}
		}

		tmp = buffer.read_32(b_le);
	} while (tmp > 0);

	if (enc_type == 6 && offset > 0 && size > 0) {
		start_pos = tiff_start + offset;
		fsize = size;
	}

Done:
	io->Seek(start_pos);
	return fsize;
}

avf_size_t CPictureList::FindThumbnail_dng(IAVIO *io)
{
	avf_size_t fsize = 0;
	int start_pos = 0;
	bool b_le = 0;
	int tiff_start = 0;
	int tmp;
	int offset = -1;
	int size = -1;

	CReadBuffer buffer(io);

	int endianess = buffer.read_be_16();
	if (endianess != 0x4949 && endianess != 0x4D4D)
		goto Done;

	b_le = endianess == 0x4949;	// II

	if (buffer.read_16(b_le) != 0x2A)
		goto Done;

	tmp = buffer.read_32(b_le);

	do {
		buffer.SeekTo(tiff_start + tmp);

		int num_dir = buffer.read_16(b_le);
		for (int i = num_dir; i > 0; i--) {
			int tag = buffer.read_16(b_le);
			switch (tag) {
			case 0x0111:	// StripOffset
				offset = ReadTiffValue(buffer, b_le);
				break;
			case 0x0117:	// StripByteCounts
				size = ReadTiffValue(buffer, b_le);
				break;
			default:
				ReadTiffValue(buffer, b_le);
				break;
			}
		}

		tmp = buffer.read_32(b_le);
	} while (tmp > 0);

	if (offset > 0 && size > 0) {
		start_pos = tiff_start + offset;
		fsize = size;
	}

Done:
	io->Seek(start_pos);
	return fsize;
}

avf_status_t CPictureList::RemovePicture(string_t *picname)
{
	AVF_LOGD(C_CYAN "RemovePicture %s" C_NONE, picname->string());

	AUTO_LOCK(mMutex);

	if (mb_scanned) {
		RemovePictureItemLocked(picname);
	}

	string_t *fullname = CreateFullName(picname->string());
	bool result = avf_remove_file(fullname->string(), true);
	fullname->Release();

	if (!result)
		return E_ERROR;

	OnPictureRemovedL(picname);

	return E_OK;
}

avf_status_t CPictureList::PictureTaken(const char *dir, const char *name)
{
	AUTO_LOCK(mMutex);

	CPictureItem *pi = AddPicture(dir, ::strlen(dir), name, ::strlen(name));
	if (pi) {
		OnPictureTakenL(pi->dirname, pi->date);
		return E_OK;
	}

	return E_ERROR;
}

avf_status_t CPictureList::GetNumPictures(int *num)
{
	AUTO_LOCK(mMutex);
	ScanL();
	*num = m_num_pictures;
	return E_OK;
}

string_t *CPictureList::CreateFullName(const char *picname)
{
	return string_t::Add(mp_root_dir->string(), mp_root_dir->size(),
		picname, ::strlen(picname), NULL, 0, NULL, 0);
}

void CPictureList::ReleasePictureRecursive(struct rb_node *rbnode)
{
	ReleaseAllPictures(rbnode->rb_left);
	ReleaseAllPictures(rbnode->rb_right);
	CPictureItem *pi = CPictureItem::from_node(rbnode);
	avf_safe_release(pi->dirname);
	avf_delete pi;
}

avf_status_t CPictureList::ScanL()
{
	avf_status_t status = E_OK;
	if (mb_loaded && !mb_scanned) {
		status = DoScan();
		mb_scanned = true;
		OnListScanedL();
	}
	return status;
}

avf_status_t CPictureList::DoScan()
{
	avf_dir_t adir;
	struct dirent *ent;

	AVF_LOGD(C_YELLOW "scan %s" C_NONE, mp_root_dir->string());

	if (!avf_open_dir(&adir, mp_root_dir->string())) {
		AVF_LOGD("cannot open %s", mp_root_dir->string());
		return E_ERROR;
	}

	while ((ent = avf_read_dir(&adir)) != NULL) {
		if (avf_is_parent_dir(ent->d_name))
			continue;

		if (avf_path_is_dir(&adir, ent)) {
			ap<string_t> path(mp_root_dir, ent->d_name, PATH_SEP);
			ScanPictures(path, ent->d_name);
		}
	}

	avf_close_dir(&adir);

	AVF_LOGD(C_YELLOW "total %d pictures" C_NONE, m_num_pictures);

	return E_OK;
}

avf_status_t CPictureList::ScanPictures(const ap<string_t>& path, const char *dir)
{
	int dir_len = ::strlen(dir);
	avf_dir_t adir;
	struct dirent *ent;

	if (!avf_open_dir(&adir, path->string())) {
		AVF_LOGE("cannot open %s", path->string());
		return E_ERROR;
	}

	while ((ent = avf_read_dir(&adir)) != NULL) {
		if (avf_is_parent_dir(ent->d_name))
			continue;
		const char *name = ent->d_name;
		int len = ::strlen(name);
		//if (len >= 5 && (is_jpg_file(name, len) || is_dng_file(name, len))) {
		if (len >= 5 && (is_jpg_file(name, len))) { // || is_dng_file(name, len))) {
			AddPicture(dir, dir_len, name, len);
		}
	}

	avf_close_dir(&adir);

	return E_OK;
}

CPictureItem *CPictureList::AddPicture(const char *dir, int dir_len, const char *name, int name_len)
{
	string_t *dirname = string_t::Add(dir, dir_len, "/", 1, name, name_len, NULL, 0);

	struct rb_node **p = &picture_set.rb_node;
	struct rb_node *parent = NULL;

	while (*p) {
		parent = *p;
		CPictureItem *tmp = CPictureItem::from_node(parent);

		int rval = ::strcmp(dirname->string(), tmp->dirname->string());
		if (rval < 0) {
			p = &parent->rb_left;
		} else if (rval > 0) {
			p = &parent->rb_right;
		} else {	
			AVF_LOGE("%s already exists", dirname->string());
			dirname->Release();
			return NULL;
		}
	}

	string_t *fullname = string_t::Add(mp_root_dir->string(), mp_root_dir->size(),
		dirname->string(), dirname->size(), NULL, 0, NULL, 0);

	struct stat buf;
	if (::stat(fullname->string(), &buf) < 0) {
		AVF_LOGW("stat %s failed", fullname->string());
		fullname->Release();
		dirname->Release();
		return NULL;
	}
	fullname->Release();

	CPictureItem *pi = avf_new CPictureItem();
	pi->dirname = dirname;
	pi->date = buf.st_ctime;

	rb_link_node(&pi->rbnode, parent, p);
	rb_insert_color(&pi->rbnode, &picture_set);
	m_num_pictures++;

	return pi;
}

avf_status_t CPictureList::RemovePictureItemLocked(string_t *picname)
{
	struct rb_node *p = picture_set.rb_node;

	while (p) {
		CPictureItem *tmp = CPictureItem::from_node(p);
		int rval = ::strcmp(picname->string(), tmp->dirname->string());
		if (rval < 0) {
			p = p->rb_left;
		} else if (rval > 0) {
			p = p->rb_right;
		} else {
			rb_erase(&tmp->rbnode, &picture_set);
			avf_safe_release(tmp->dirname);
			avf_delete tmp;
			break;
		}
	}

	return E_OK;
}

void CPictureList::SetPictureListener(int index, IPictureListener *pListener)
{
	AUTO_LOCK(mMutex);
	if (index < 0 || index >= NUM_LISTENER) {
		AVF_LOGE("SetPictureListener index = %d", index);
		return;
	}
	mpPictureListener[index] = pListener;
}

void CPictureList::OnListScanedL()
{
	for (int i = 0; i < NUM_LISTENER; i++) {
		if (mpPictureListener[i]) {
			mpPictureListener[i]->OnListScaned(m_num_pictures);
		}
	}
}

void CPictureList::OnListClearedL()
{
	for (int i = 0; i < NUM_LISTENER; i++) {
		if (mpPictureListener[i]) {
			mpPictureListener[i]->OnListCleared();
		}
	}
}

void CPictureList::OnPictureTakenL(string_t *picname, avf_u32_t picture_date)
{
	for (int i = 0; i < NUM_LISTENER; i++) {
		if (mpPictureListener[i]) {
			mpPictureListener[i]->OnPictureTaken(picname, picture_date);
		}
	}
}

void CPictureList::OnPictureRemovedL(string_t *picname)
{
	for (int i = 0; i < NUM_LISTENER; i++) {
		if (mpPictureListener[i]) {
			mpPictureListener[i]->OnPictureRemoved(picname);
		}
	}
}

