/**
 * Copyright (C) 2005-2007 Christoph Rupp (chris@crupp.de).
 * All rights reserved. See file LICENSE for licence and copyright
 * information.
 *
 * internal macros and headers
 *
 */

#ifndef HAM_DB_H__
#define HAM_DB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "endian.h"
#include "backend.h"
#include "cache.h"
#include "page.h"
#include "os.h"
#include "freelist.h"
#include "extkeys.h"
#include "error.h"
#include "txn.h"
#include "mem.h"
#include "device.h"
#include "env.h"

#define OFFSET_OF(type, member) ((size_t) &((type *)0)->member)

/*
 * This is the minimum chunk size; all chunks (pages and blobs) are aligned
 * to this size. 
 */
#define DB_CHUNKSIZE        64

/*
 * the maximum number of indices (if this file is an environment with 
 * multiple indices)
 */
#define DB_MAX_INDICES      16  /* 16*32 = 512 byte wasted */

#include "packstart.h"

/*
 * the persistent database header
 */
typedef HAM_PACK_0 HAM_PACK_1 struct
{
    /* magic cookie - always "ham\0" */
    ham_u8_t  _magic[4];

    /* version information - major, minor, rev, reserved */
    ham_u8_t  _version[4];

    /* serial number */
    ham_u32_t _serialno;

    /* size of the page */
    ham_u32_t _pagesize;

    /* padding for SUN Sparc */
    ham_u32_t _reserved2;

    /* maximum number of _indexdata arrays - always set to 
     * DB_MAX_INDICES (not yet needed, but stored for later use) */
    ham_u16_t _max_db;

    /* padding for SUN Sparc */
    ham_u16_t _reserved3;

    /* private data of the index backend */
    ham_u8_t _indexdata[DB_MAX_INDICES][32];

    /* start of the freelist - the freelist spans the rest of the page. 
     * don't add members after this field! */
    ham_u8_t _freelist_start;

} db_header_t;

#include "packstop.h"

/*
 * set the 'magic' field of a file header
 */
#define db_set_magic(db, a,b,c,d)  { db_get_header(db)->_magic[0]=a; \
                                     db_get_header(db)->_magic[1]=b; \
                                     db_get_header(db)->_magic[2]=c; \
                                     db_get_header(db)->_magic[3]=d; }

/*
 * get byte #i of the 'magic'-header
 */
#define db_get_magic(db, i)        (db_get_header(db)->_magic[i])

/*
 * set the version of a file header
 */
#define db_set_version(db,a,b,c,d) { db_get_header(db)->_version[0]=a; \
                                     db_get_header(db)->_version[1]=b; \
                                     db_get_header(db)->_version[2]=c; \
                                     db_get_header(db)->_version[3]=d; }

/*
 * get byte #i of the 'version'-header
 */
#define db_get_version(db, i)      (db_get_header(db)->_version[i])

/*
 * get the serial number
 */
#define db_get_serialno(db)        (ham_db2h32(db_get_header(db)->_serialno))

/*
 * set the serial number
 */
#define db_set_serialno(db, n)     db_get_header(db)->_serialno=ham_h2db32(n)

/*
 * get the key size
 */
#define db_get_keysize(db)         be_get_keysize(db_get_backend(db))

/*
 * get the page size
 */
#define db_get_pagesize(db)        (ham_db2h32(db_get_header(db)->_pagesize))

/*
 * set the page size
 */
#define db_set_pagesize(db, ps)    db_get_header(db)->_pagesize=ham_h2db32(ps)

/**
 * get the size of the usable persistent payload of a page
 */
#define db_get_usable_pagesize(db) (db_get_pagesize(db)-(sizeof(ham_u32_t)*3))

/*
 * get the maximum number of databases for this file
 */
#define db_get_indexdata_size(db)  ham_db2h16(db_get_header(db)->_max_db)

/*
 * set the maximum number of databases for this file
 */
#define db_set_indexdata_size(db,s) db_get_header(db)->_max_db=ham_h2db16(s)

/*
 * get the private data of the backend; interpretation of the
 * data is up to the backend
 */
#define db_get_indexdata(db)      &db_get_header(db)->_indexdata[             \
                                        db_get_indexdata_offset(db)][0]

/*
 * get the private data of the backend; interpretation of the
 * data is up to the backend
 */
#define db_get_indexdata_at(db, i) &db_get_header(db)->_indexdata[i][0]

/*
 * get the currently active transaction
 */
#define db_get_txn(db)             (db_get_env(db)                            \
                                   ? env_get_txn(db_get_env(db))              \
                                   : (db)->_txn)

/*
 * set the currently active transaction
 */
#define db_set_txn(db, txn)        do { if (db_get_env(db))                   \
                                     env_set_txn(db_get_env(db), txn);        \
                                     else (db)->_txn=txn; } while(0)

/*
 * get the cache for extended keys
 */
#define db_get_extkey_cache(db)    (db_get_env(db)                            \
                                   ? env_get_extkey_cache(db_get_env(db))     \
                                   : (db)->_extkey_cache)

/*
 * set the cache for extended keys
 */
#define db_set_extkey_cache(db, c)     ham_assert(db_get_env(db)==0, (""));   \
                                       (db)->_extkey_cache=c

/*
 * the database structure
 */
struct ham_db_t
{
    /* the current transaction ID */
    ham_u64_t _txn_id;

    /* the last error code */
    ham_status_t _error;

    /* a custom error handler */
    ham_errhandler_fun _errh;

    /* the backend pointer - btree, hashtable etc */
    ham_backend_t *_backend;

    /* the memory allocator */
    mem_allocator_t *_allocator;

    /* the device (either a file or an in-memory-db) */
    ham_device_t *_device;

    /* the cache */
    ham_cache_t *_cache;

    /* the private txn object used by the freelist */
    ham_txn_t *_freel_txn;

    /* the size of the last allocated data pointer for records */
    ham_size_t _rec_allocsize;

    /* the last allocated data pointer for records */
    void *_rec_allocdata;

    /* the size of the last allocated data pointer for keys */
    ham_size_t _key_allocsize;

    /* the last allocated data pointer for keys */
    void *_key_allocdata;

    /* the prefix-comparison function */
    ham_prefix_compare_func_t _prefixcompfoo;

    /* the comparison function */
    ham_compare_func_t _compfoo;

    /* the file header page */
    ham_page_t *_hdrpage;

    /* the active txn */
    ham_txn_t *_txn;

    /* the cache for extended keys */
    extkey_cache_t *_extkey_cache;

    /* the database flags - a combination of the persistent flags
     * and runtime flags */
    ham_u32_t _rt_flags;

    /* the offset of this database in the environment _indexdata */
    ham_u16_t _indexdata_offset;

    /* the environment of this database - can be NULL */
    ham_env_t *_env;

    /* the next database in a linked list of databases */
    ham_db_t *_next;
};

/*
 * get the header page
 */
#define db_get_header_page(db)         (db_get_env(db)                        \
                                       ? env_get_header_page(db_get_env(db))  \
                                       : (db)->_hdrpage)

/*
 * set the header page - not allowed when we have an environment!
 */
#define db_set_header_page(db, h)      ham_assert(db_get_env(db)==0, (""));   \
                                       (db)->_hdrpage=(h)

/*
 * get the current transaction ID
 */
#define db_get_txn_id(db)              (db_get_env(db)                        \
                                       ? env_get_txn_id(db_get_env(db))       \
                                       : (db)->_txn_id)

/*
 * set the current transaction ID
 */
#define db_set_txn_id(db, id)          do { if (db_get_env(db))               \
                                         env_set_txn_id(db_get_env(db), id);  \
                                         else (db)->_txn_id=id; } while(0)

/*
 * get the last error code
 */
#define db_get_error(db)               (db)->_error

/*
 * set the last error code
 */
#define db_set_error(db, e)            (db)->_error=e

/*
 * get the backend pointer
 */
#define db_get_backend(db)             (db)->_backend

/*
 * set the backend pointer
 */
#define db_set_backend(db, be)         (db)->_backend=be

/*
 * get the memory allocator
 */
#define db_get_allocator(db)           (db_get_env(db)                        \
                                       ? env_get_allocator(db_get_env(db))    \
                                       : (db)->_allocator)

/*
 * set the memory allocator
 */
#define db_set_allocator(db, a)        ham_assert(db_get_env(db)==0, (""));   \
                                       (db)->_allocator=(a)

/*
 * get the device
 */
#define db_get_device(db)              (db_get_env(db)                        \
                                       ? env_get_device(db_get_env(db))       \
                                       : (db)->_device)

/*
 * set the device - not allowed in an environment
 */
#define db_set_device(db, d)           ham_assert(db_get_env(db)==0, (""));   \
                                       (db)->_device=(d)

/*
 * get the cache pointer
 */
#define db_get_cache(db)               (db_get_env(db)                        \
                                       ? env_get_cache(db_get_env(db))        \
                                       : (db)->_cache)

/*
 * set the cache pointer - not allowed in an environment
 */
#define db_set_cache(db, c)            ham_assert(db_get_env(db)==0, (""));   \
                                       (db)->_cache=c

/*
 * get the freelist's txn
 */
#define db_get_freelist_txn(db)        (db_get_env(db)                        \
                                       ? env_get_freelist_txn(db_get_env(db)) \
                                       : (db)->_freel_txn)

/*
 * set the freelist's txn - not allowed in an environment
 */
#define db_set_freelist_txn(db, t)     ham_assert(db_get_env(db)==0, (""));   \
                                       (db)->_freel_txn=t

/*
 * get the prefix comparison function
 */
#define db_get_prefix_compare_func(db) (db)->_prefixcompfoo

/*
 * set the prefix comparison function
 */
#define db_set_prefix_compare_func(db, f) (db)->_prefixcompfoo=f

/*
 * get the default comparison function
 */
#define db_get_compare_func(db)        (db)->_compfoo

/*
 * set the default comparison function
 */
#define db_set_compare_func(db, f)     (db)->_compfoo=f

/*
 * get the runtime-flags - if this database has an environment, the flags
 * are "mixed"
 */
#define db_get_rt_flags(db)            (db_get_env(db)                        \
                           ? env_get_rt_flags(db_get_env(db))|(db)->_rt_flags \
                           : (db)->_rt_flags)

/*
 * set the runtime-flags - NOT setting environment flags!
 */
#define db_set_rt_flags(db, f)         (db)->_rt_flags=(f)

/*
 * get the index of this database in the indexdata array
 */
#define db_get_indexdata_offset(db)    (db)->_indexdata_offset

/*
 * set the index of this database in the indexdata array
 */
#define db_set_indexdata_offset(db, o) (db)->_indexdata_offset=o

/*
 * get the environment pointer
 */
#define db_get_env(db)                 (db)->_env

/*
 * set the environment pointer
 */
#define db_set_env(db, env)            (db)->_env=env

/*
 * get the next database in a linked list of databases
 */
#define db_get_next(db)                (db)->_next

/*
 * set the pointer to the next database
 */
#define db_set_next(db, next)          (db)->_next=next

/*
 * get the size of the last allocated data blob
 */
#define db_get_record_allocsize(db)    (db)->_rec_allocsize

/*
 * set the size of the last allocated data blob
 */
#define db_set_record_allocsize(db, s) (db)->_rec_allocsize=s

/*
 * get the pointer to the last allocated data blob
 */
#define db_get_record_allocdata(db)    (db)->_rec_allocdata

/*
 * set the pointer to the last allocated data blob
 */
#define db_set_record_allocdata(db, p) (db)->_rec_allocdata=p

/*
 * get the size of the last allocated key blob
 */
#define db_get_key_allocsize(db)       (db)->_key_allocsize

/*
 * set the size of the last allocated key blob
 */
#define db_set_key_allocsize(db, s)    (db)->_key_allocsize=s

/*
 * get the pointer to the last allocated key blob
 */
#define db_get_key_allocdata(db)       (db)->_key_allocdata

/*
 * set the pointer to the last allocated key blob
 */
#define db_set_key_allocdata(db, p)    (db)->_key_allocdata=p
/*
 * get a pointer to the header data
 */
#define db_get_header(db)              ((db_header_t *)(page_get_payload(\
                                          db_get_header_page(db))))

/*
 * get the freelist object of the database
 * add 1 byte because the freelist starts AFTER _freelist_start!
 */
#define db_get_freelist(db)            (freelist_t *)(page_get_payload(  \
                                          db_get_header_page(db))+       \
                                          OFFSET_OF(db_header_t,         \
                                              _freelist_start)+1)

/*
 * get the dirty-flag
 */
#define db_is_dirty(db)                page_is_dirty(db_get_header_page(db))

/*
 * set the dirty-flag
 */
#define db_set_dirty(db, d)            page_set_dirty(db_get_header_page(db), d)

/**
 * uncouple all cursors from a page
 *
 * @remark this is called whenever the page is deleted or becoming invalid
 */
extern ham_status_t
db_uncouple_all_cursors(ham_page_t *page);

/**
 * compare two keys
 *
 * this function will call the prefix-compare function and the
 * default compare function whenever it's necessary.
 *
 * on error, the database error code (db_get_error()) is set; the caller
 * HAS to check for this error!
 *
 * the default key compare function - uses memcmp
 */
extern int
db_default_compare(const ham_u8_t *lhs, ham_size_t lhs_length,
                   const ham_u8_t *rhs, ham_size_t rhs_length);

/**
 * the default prefix compare function - uses memcmp
 */
extern int
db_default_prefix_compare(const ham_u8_t *lhs, ham_size_t lhs_length,
                   ham_size_t lhs_real_length,
                   const ham_u8_t *rhs, ham_size_t rhs_length,
                   ham_size_t rhs_real_length);

/**
 * load an extended key
 * returns the full data of the extended key in ext_key
 */
extern ham_status_t
db_get_extended_key(ham_db_t *db, ham_u8_t *key_data,
                    ham_size_t key_length, ham_u32_t key_flags,
                    ham_u8_t **ext_key);

/**
 * function which compares two keys
 *
 * calls the comparison function
 */
extern int
db_compare_keys(ham_db_t *db, ham_page_t *page,
                long lhs_idx, ham_u32_t lhs_flags,
                const ham_u8_t *lhs, ham_size_t lhs_length,
                long rhs_idx, ham_u32_t rhs_flags,
                const ham_u8_t *rhs, ham_size_t rhs_length);

/**
 * create a backend object according to the database flags
 */
extern ham_backend_t *
db_create_backend(ham_db_t *db, ham_u32_t flags);

/**
 * fetch a page
 */
extern ham_page_t *
db_fetch_page(ham_db_t *db, ham_offset_t address, ham_u32_t flags);
#define DB_ONLY_FROM_CACHE      2

/**
 * flush a page
 */
extern ham_status_t
db_flush_page(ham_db_t *db, ham_page_t *page, ham_u32_t flags);

/**
 * flush all pages, and clear the cache
 *
 * @param flags: set to DB_FLUSH_NODELETE if you do NOT want the cache to
 * be cleared
 */
extern ham_status_t
db_flush_all(ham_db_t *db, ham_u32_t flags);
#define DB_FLUSH_NODELETE       1

/**
 * allocate a new page
 *
 * !!! the page will be aligned at the current page size. any wasted
 * space (due to the alignment) is added to the freelist.
 * TODO nur wenn NO_ALIGN nicht gesetzt ist! (sollte das nicht eher der
 * default sein??)
 *
 * @remark flags can be of the following value:
 *  PAGE_IGNORE_FREELIST        ignores all freelist-operations
    PAGE_CLEAR_WITH_ZERO        memset the persistent page with 0
 */
extern ham_page_t *
db_alloc_page(ham_db_t *db, ham_u32_t type, ham_u32_t flags);
#define PAGE_IGNORE_FREELIST          2
#define PAGE_CLEAR_WITH_ZERO          4

/**
 * free a page
 *
 * @remark will mark the page as deleted; the page will be deleted
 * when the transaction is committed (or not deleted if the transaction
 * is aborted).
 *
 * @remark valid flag: DB_MOVE_TO_FREELIST; marks the page as 'deleted'
 * in the freelist. Ignored in in-memory databases.
 */
extern ham_status_t
db_free_page(ham_page_t *page, ham_u32_t flags);

#define DB_MOVE_TO_FREELIST         1

/**
 * write a page, then delete the page from memory
 *
 * @remark this function is used by the cache; it shouldn't be used
 * anywhere else.
 */
extern ham_status_t
db_write_page_and_delete(ham_page_t *page, ham_u32_t flags);

/**
 * an internal database flag - use mmap instead of read(2)
 */
#define DB_USE_MMAP                  0x00000100

#ifdef __cplusplus
} // extern "C" {
#endif

#endif /* HAM_DB_H__ */
