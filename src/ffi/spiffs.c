#include "prelude.h"
#include "spiffs.h"
#include "spiffs_nucleus.h"
#include "spiffs_config.h"
#include <string.h>


#if SPIFFS_FILEHDL_OFFSET
#define SPIFFS_FH_OFFS(fs, fh)   ((fh) != 0 ? ((fh) + (fs)->cfg.fh_ix_offset) : 0)
#define SPIFFS_FH_UNOFFS(fs, fh) ((fh) != 0 ? ((fh) - (fs)->cfg.fh_ix_offset) : 0)
#else
#define SPIFFS_FH_OFFS(fs, fh)   (fh)
#define SPIFFS_FH_UNOFFS(fs, fh) (fh)
#endif

#if SPIFFS_CACHE == 1
static i32 spiffs_fflush_cache(spiffs* fs, spiffs_file fh);
#endif

#if !SPIFFS_READ_ONLY
static i32 spiffs_hydro_write(spiffs* fs,spiffs_fd* fd,void* buf,u32 offset,i32 len);
#endif

#if SPIFFS_BUFFER_HELP
u32 SPIFFS_buffer_bytes_for_filedescs(spiffs* fs, u32 num_descs) {
  return num_descs * sizeof(spiffs_fd);
}
#if SPIFFS_CACHE
u32 SPIFFS_buffer_bytes_for_cache(spiffs* fs, u32 num_pages) {
  return sizeof(spiffs_cache) + num_pages * (sizeof(spiffs_cache_page) + SPIFFS_CFG_LOG_PAGE_SZ(fs));
}
#endif
#endif


static i32 spiffs_stat_pix(spiffs* fs,spiffs_page_ix pix,spiffs_file file_handle,spiffs_stat* s);


i32 SPIFFS_fstat(spiffs* fs,spiffs_file file_handle,spiffs_stat* stat) {
  if(fs->config_magic!=SPIFFS_CONFIG_MAGIC) {
    fs->err_code = SPIFFS_ERR_NOT_CONFIGURED;
    return SPIFFS_ERR_NOT_CONFIGURED;
  }

  if(!fs->mounted) {
    fs->err_code = SPIFFS_ERR_NOT_MOUNTED;
    return SPIFFS_ERR_NOT_MOUNTED;
  }

  SPIFFS_LOCK(fs);

  spiffs_fd* descriptor;
  i32 res;

  file_handle=SPIFFS_FH_UNOFFS(fs,file_handle);
  res=spiffs_fd_get(fs,file_handle,&descriptor);
  if(res < SPIFFS_OK) {
    fs->err_code = res;
    SPIFFS_UNLOCK(fs);
    return res;
  }

#if SPIFFS_CACHE_WR
  spiffs_fflush_cache(fs,file_handle);
#endif
  res=spiffs_stat_pix(fs,descriptor->objix_hdr_pix,file_handle,stat);
  SPIFFS_UNLOCK(fs);

  return res;
}


i32 SPIFFS_lseek(spiffs* fs, spiffs_file fh, i32 offs, int whence) {
  SPIFFS_API_CHECK_CFG(fs);
  SPIFFS_API_CHECK_MOUNT(fs);
  SPIFFS_LOCK(fs);

  spiffs_fd* fd;
  i32 res;
  fh = SPIFFS_FH_UNOFFS(fs, fh);
  res = spiffs_fd_get(fs, fh, &fd);
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);

#if SPIFFS_CACHE_WR
  spiffs_fflush_cache(fs, fh);
#endif

  switch (whence) {
  case SPIFFS_SEEK_CUR:
    offs = fd->fdoffset+offs;
    break;
  case SPIFFS_SEEK_END:
    offs = (fd->size == SPIFFS_UNDEFINED_LEN ? 0 : fd->size) + offs;
    break;
  }

  if ((offs > (i32)fd->size) && (SPIFFS_UNDEFINED_LEN != fd->size)) {
    res = SPIFFS_ERR_END_OF_OBJECT;
  }
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);

  spiffs_span_ix data_spix = offs / SPIFFS_DATA_PAGE_SIZE(fs);
  spiffs_span_ix objix_spix = SPIFFS_OBJ_IX_ENTRY_SPAN_IX(fs, data_spix);
  if (fd->cursor_objix_spix != objix_spix) {
    spiffs_page_ix pix;
    res = spiffs_obj_lu_find_id_and_span(
        fs, fd->obj_id | SPIFFS_OBJ_ID_IX_FLAG, objix_spix, 0, &pix);
    SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
    fd->cursor_objix_spix = objix_spix;
    fd->cursor_objix_pix = pix;
  }
  fd->fdoffset = offs;

  SPIFFS_UNLOCK(fs);

  return offs;
}



static i32 spiffs_stat_pix(spiffs *fs, spiffs_page_ix pix, spiffs_file fh, spiffs_stat *s) {
  (void)fh;
  spiffs_page_object_ix_header objix_hdr;
  spiffs_obj_id obj_id;
  i32 res =_spiffs_rd(fs,  SPIFFS_OP_T_OBJ_IX | SPIFFS_OP_C_READ, fh,
      SPIFFS_PAGE_TO_PADDR(fs, pix), sizeof(spiffs_page_object_ix_header), (u8*)&objix_hdr);
  SPIFFS_API_CHECK_RES(fs, res);

 usize obj_id_addr = SPIFFS_BLOCK_TO_PADDR(fs, SPIFFS_BLOCK_FOR_PAGE(fs , pix)) +
      SPIFFS_OBJ_LOOKUP_ENTRY_FOR_PAGE(fs, pix) * sizeof(spiffs_obj_id);

  res =_spiffs_rd(fs,  SPIFFS_OP_T_OBJ_LU | SPIFFS_OP_C_READ, fh,
      obj_id_addr, sizeof(spiffs_obj_id), (u8*)&obj_id);
  SPIFFS_API_CHECK_RES(fs, res);

  s->obj_id = obj_id & ~SPIFFS_OBJ_ID_IX_FLAG;
  s->type = objix_hdr.type;
  s->size = objix_hdr.size == SPIFFS_UNDEFINED_LEN ? 0 : objix_hdr.size;
  s->pix = pix;
  strncpy((char *)s->name, (char *)objix_hdr.name, SPIFFS_OBJ_NAME_LEN);

  return res;
}


// Checks if there are any cached writes for the object id associated with
// given filehandle. If so, these writes are flushed.
#if SPIFFS_CACHE == 1
static i32 spiffs_fflush_cache(spiffs* fs,spiffs_file fh) {
  (void)fs;
  (void)fh;
  i32 res = SPIFFS_OK;
#if !SPIFFS_READ_ONLY && SPIFFS_CACHE_WR

  spiffs_fd* fd;
  res = spiffs_fd_get(fs, fh, &fd);
  SPIFFS_API_CHECK_RES(fs, res);

  if ((fd->flags & SPIFFS_O_DIRECT) == 0) {
    if (fd->cache_page == 0) {
      // see if object id is associated with cache already
      fd->cache_page = spiffs_cache_page_get_by_fd(fs, fd);
    }
    if (fd->cache_page) {
      SPIFFS_CACHE_DBG("CACHE_WR_DUMP: dumping cache page %i for fd %i:%04x, flush, offs:%i size:%i\n",
          fd->cache_page->ix, fd->file_nbr,  fd->obj_id, fd->cache_page->offset, fd->cache_page->size);
      res = spiffs_hydro_write(fs, fd,
          spiffs_get_cache_page(fs, spiffs_get_cache(fs), fd->cache_page->ix),
          fd->cache_page->offset, fd->cache_page->size);
      if (res < SPIFFS_OK) {
        fs->err_code = res;
      }
      spiffs_cache_fd_release(fs, fd->cache_page);
    }
  }
#endif

  return res;
}
#endif


#if !SPIFFS_READ_ONLY
static i32 spiffs_hydro_write(spiffs* fs,spiffs_fd* fd,void* buf,u32 offset,i32 len) {
  (void)fs;
  i32 res = SPIFFS_OK;
  i32 remaining = len;
  if (fd->size != SPIFFS_UNDEFINED_LEN && offset < fd->size) {
    i32 m_len = MIN((i32)(fd->size - offset), len);
    res = spiffs_object_modify(fd, offset, (u8*)buf, m_len);
    SPIFFS_CHECK_RES(res);
    remaining -= m_len;
    u8* buf_8 = (u8*)buf;
    buf_8 += m_len;
    buf = buf_8;
    offset += m_len;
  }
  if (remaining > 0) {
    res = spiffs_object_append(fd, offset, (u8*)buf, remaining);
    SPIFFS_CHECK_RES(res);
  }
  return len;
}
#endif // !SPIFFS_READ_ONLY


