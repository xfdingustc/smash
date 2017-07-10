
#define LOG_TAG "vdb_file_list"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"
#include "avio.h"
#include "buffer_io.h"
#include "mem_stream.h"

#include "rbtree.h"
#include "vdb_file_list.h"
#include "vdb_util.h"

//-----------------------------------------------------------------------
//
//  CVdbFile
//
//-----------------------------------------------------------------------
CVdbFile::CVdbFile(avf_time_t local_time, avf_u32_t size, int type, bool bUse)
{
	this->local_time = local_time;
	this->size = size;
	this->type = type;
	this->bUse = bUse;
}

CVdbFileList::CVdbFileList(const char *file_ext, const char *file_ext2):
	m_file_ext(file_ext),
	m_file_ext2(file_ext2)
{
	Init();
}

CVdbFileList::~CVdbFileList()
{
	Clear();
}

void CVdbFileList::Clear()
{
	SafeFreeFileItem(file_tree.rb_node);
	Init();
}

void CVdbFileList::FreeFileItem(struct rb_node *rbnode)
{
	SafeFreeFileItem(rbnode->rb_left);
	SafeFreeFileItem(rbnode->rb_right);
	CVdbFile *file = CVdbFile::from_node(rbnode);
	avf_delete file;
}

// used -> unused
void CVdbFileList::UnuseFile(string_t *path, avf_time_t local_time, int type, bool bRemove)
{
	struct rb_node *n = file_tree.rb_node;

	while (n) {
		CVdbFile *file = CVdbFile::from_node(n);

		if (local_time < file->local_time) {
			n = n->rb_left;
		} else if (local_time > file->local_time) {
			n = n->rb_right;
		} else {
			if (type == file->type) {
				if (file->bUse) {
					file->bUse = false;
					size_used -= file->size;

					if (bRemove) {
						local_time_name_t lname;
						local_time_to_name(local_time, GetFileExt(type), lname);
						ap<string_t> fullname(path, lname.buf);
						AVF_LOGD(C_YELLOW "remove file %s" C_NONE, fullname->string());
						avf_remove_file(fullname->string());

						rb_erase(n, &file_tree);
						avf_delete file;
						num_files--;

					} else {
						size_unused += file->size;
					}

				} else {
					AVF_LOGW("unuse file");
				}

				return;

			} else if (type < file->type) {
				n = n->rb_left;
			} else {
				n = n->rb_right;
			}
		}
	}

	local_time_name_t lname;
	local_time_to_name(local_time, GetFileExt(type), lname);
	AVF_LOGE("file not found: %s%s", path->string(), lname.buf);
}

// unused -> remove
bool CVdbFileList::RemoveUnusedFile(local_time_name_t& lname, avf_u32_t *size)
{
	if (size_unused == 0)
		return false;

	struct rb_node *node = rb_first(&file_tree);
	for (; node; node = rb_next(node)) {
		CVdbFile *file = CVdbFile::from_node(node);

		if (!file->bUse) {
			size_unused -= file->size;
			*size = file->size;

			local_time_to_name(file->local_time, GetFileExt(file->type), lname);

			rb_erase(node, &file_tree);
			avf_delete file;
			num_files--;

			return true;
		}
	}

	return false;
}

avf_u32_t CVdbFileList::AddFile(string_t *path, const char *filename, avf_time_t local_time, int type, bool bUse)
{
	struct rb_node **p = &file_tree.rb_node;
	struct rb_node *parent = NULL;
	CVdbFile *file;

	while (*p) {
		parent = *p;
		file = CVdbFile::from_node(parent);

		if (local_time < file->local_time) {
			p = &parent->rb_left;
		} else if (local_time > file->local_time) {
			p = &parent->rb_right;
		} else {
			if (type == file->type) {
				if (bUse) {
					AVF_LOGW("try to use a used/unused file %s (bUse: %d)", filename, file->bUse);
				} else if (!file->bUse) {
					AVF_LOGW("try to unuse an unused file %s", filename);
				} else {
					//AVF_LOGW("try to unuse a used file %s", filename);
				}
				return file->size;
			} else if (type < file->type) {
				p = &parent->rb_left;
			} else {
				p = &parent->rb_right;
			}
		}
	}

	local_time_name_t lname;
	if (filename == NULL) {
		local_time_to_name(local_time, GetFileExt(type), lname);
		filename = lname.buf;
	}

	ap<string_t> fullname(path, filename);
	avf_u32_t size = avf_get_file_size(fullname->string());

//	size = ROUND_UP(size, BUFFER_SIZE);
	if (size == 0 && !bUse) {
		AVF_LOGD(C_YELLOW "file is empty, remove: %s" C_NONE, fullname->string());
		avf_remove_file(fullname->string());
		return 0;
	}

	file = avf_new CVdbFile(local_time, size, type, bUse);

	if (bUse) {
		size_used += size;
	} else {
		size_unused += size;
	}

	num_files++;
	rb_link_node(&file->rbnode, parent, p);
	rb_insert_color(&file->rbnode, &file_tree);

	return size;
}

void CVdbFileList::Print(const char *path, bool bAll)
{
	AVF_LOGD("total %d files in %s, size used: " LLD ", size unused: " LLD,
		num_files, path, size_used, size_unused);
	struct rb_node *node = rb_first(&file_tree);
	for (; node; node = rb_next(node)) {
		CVdbFile *file = CVdbFile::from_node(node);
		if (bAll || !file->bUse) {
			local_time_name_t lname;
			local_time_to_name(file->local_time, GetFileExt(file->type), lname);
			AVF_LOGD(C_BROWN "  %s size %d %s" C_NONE, lname.buf, 
				file->size, file->bUse ? "" : "not used");
		}
	}
}

void CVdbFileList::Serialize(CMemBuffer *pmb)
{
	pmb->write_le_32(num_files);
	pmb->write_le_64(size_unused);
	pmb->write_le_64(size_used);
	pmb->write_length_string(m_file_ext, 0);
	pmb->write_length_string(m_file_ext2, 0);

	struct rb_node *node = rb_first(&file_tree);
	for (; node; node = rb_next(node)) {
		CVdbFile *file = CVdbFile::from_node(node);
		pmb->write_le_8(1);
		pmb->write_le_32(file->local_time);
		pmb->write_le_32(file->size);
		pmb->write_le_8(file->bUse);
	}
	pmb->write_le_8(0);
}

