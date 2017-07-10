
#ifndef __VDB_FILE_LIST_H__
#define __VDB_FILE_LIST_H__

class CVdbFile;
class CVdbFileList;
struct local_time_name_s;

//-----------------------------------------------------------------------
//
//  CVdbFile
//
//-----------------------------------------------------------------------
class CVdbFile
{
public:
	struct rb_node rbnode;
	avf_time_t local_time;
	avf_u32_t size;
	int type;
	bool bUse;

public:
	CVdbFile(avf_time_t local_time, avf_u32_t size, int type, bool bUse);
	INLINE ~CVdbFile() { }

public:
	static INLINE CVdbFile *from_node(struct rb_node *node) {
		return container_of(node, CVdbFile, rbnode);
	}
};

//-----------------------------------------------------------------------
//
//  CVdbFileList
//
//-----------------------------------------------------------------------
class CVdbFileList
{
public:
	explicit CVdbFileList(const char *file_ext, const char *file_ext2);
	~CVdbFileList();

public:
	avf_u32_t AddFile(string_t *path, const char *filename, avf_time_t local_time, int type, bool bUsing);
	void UnuseFile(string_t *path, avf_time_t local_time, int type, bool bRemove);
	bool RemoveUnusedFile(local_time_name_s& lname, avf_u32_t *size);
	void Serialize(CMemBuffer *pmb);

	INLINE avf_u64_t GetUnusedSize() {
		return size_unused;
	}

	INLINE avf_u64_t GetUsedSize() {
		return size_used;
	}

	INLINE const char *GetFileExt(int type) {
		return type == 0 ? m_file_ext : m_file_ext2;
	}

	INLINE void Init() {
		file_tree.rb_node = NULL;
		num_files = 0;
		size_unused = 0;
		size_used = 0;
	}

	void Clear();

	void Print(const char *path, bool bAll);

private:
	static void FreeFileItem(struct rb_node *rbnode);
	static INLINE void SafeFreeFileItem(struct rb_node *rbnode) {
		if (rbnode) {
			FreeFileItem(rbnode);
		}
	}

private:
	struct rb_root file_tree;
	const char *const m_file_ext;	// type 0, default
	const char *const m_file_ext2;	// type 1
	avf_uint_t num_files;
	avf_u64_t size_unused;
	avf_u64_t size_used;
};

#endif

