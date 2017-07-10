
#ifndef __PICTURE_LIST_H__
#define __PICTURE_LIST_H__

class CPictureItem;
class CPictureList;

class IPictureListener
{
public:
	virtual void OnListScaned(int total) = 0;
	virtual void OnListCleared() = 0;
	virtual void OnPictureTaken(string_t *picname, avf_u32_t picture_date) = 0;
	virtual void OnPictureRemoved(string_t *picname) = 0;
};

//-----------------------------------------------------------------------
//
//  CPictureItem
//
//-----------------------------------------------------------------------
class CPictureItem
{
	friend class CPictureList;

	struct rb_node rbnode;
	string_t *dirname;
	avf_u32_t date;

	static INLINE CPictureItem *from_node(struct rb_node *node) {
		return container_of(node, CPictureItem, rbnode);
	}
};

//-----------------------------------------------------------------------
//
//  CPictureList
//
//-----------------------------------------------------------------------

class CPictureList: public CObject
{
	typedef CObject inherited;

public:
	static CPictureList* Create(string_t *p_root_dir);

private:
	CPictureList(string_t *p_root_dir);
	virtual ~CPictureList();

public:
	avf_status_t LoadPictureList();
	INLINE void UnloadPictureList() {
		ClearPictureList(true);
	}
	void ClearPictureList(bool bUnload = false);
	void GetPictureList(CMemBuffer *pmb);
	avf_status_t ReadPicture(const char *picname, 
		const vdb_cmd_ReadPicture_t *cmd, vdb_ack_t *ack, 
		avf_u8_t *buffer, int bufsize, int bufpos, bool bThumbnail,
		CSocketEvent *pSocketEvent, CSocket *pSocket, int timeout, bool bCheckClose);
	avf_status_t RemovePicture(string_t *picname);
	avf_status_t PictureTaken(const char *dir, const char *name);
	avf_status_t GetNumPictures(int *num);

private:
	INLINE void InitList() {
		picture_set.rb_node = NULL;
		m_num_pictures = 0;
		mb_scanned = false;
	}

private:
	avf_status_t ScanL();
	avf_status_t DoScan();
	avf_status_t ScanPictures(const ap<string_t>& path, const char *dir);
	CPictureItem *AddPicture(const char *dir, int dir_len, const char *name, int name_len);
	avf_status_t RemovePictureItemLocked(string_t *picname);
	string_t *CreateFullName(const char *picname);
	avf_status_t ReadPictureLocked(const char *picname, IAVIO *io, 
		const vdb_cmd_ReadPicture_t *cmd, vdb_ack_t *ack, 
		avf_u8_t *buffer, int bufsize, int bufpos, bool bThumbnail,
		CSocketEvent *pSocketEvent, CSocket *pSocket, int timeout, bool bCheckClose);
	static avf_size_t FindThumbnail(const char *picname, int picname_size, IAVIO *io);
	static avf_size_t FindThumbnail_jpg(IAVIO *io);
	static avf_size_t FindThumbnail_dng(IAVIO *io);
	static avf_int_t ReadTiffValue(CReadBuffer& buffer, bool b_le);

private:
	static void ReleasePictureRecursive(struct rb_node *rbnode);
	static INLINE void ReleaseAllPictures(struct rb_node *rbnode) {
		if (rbnode) {
			ReleasePictureRecursive(rbnode);
		}
	}

private:
	CMutex mMutex;
	string_t *mp_root_dir;
	struct rb_root picture_set;
	avf_size_t m_num_pictures;
	bool mb_loaded;
	bool mb_scanned;

public:
	void SetPictureListener(int index, IPictureListener *pListener);

private:
	void OnListScanedL();
	void OnListClearedL();
	void OnPictureTakenL(string_t *picname, avf_u32_t picture_date);
	void OnPictureRemovedL(string_t *picname);

private:
	enum {
		NUM_LISTENER = 2,
	};
	IPictureListener *mpPictureListener[NUM_LISTENER];
};

#endif

