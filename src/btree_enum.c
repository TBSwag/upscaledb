/**
 * Copyright (C) 2005-2008 Christoph Rupp (chris@crupp.de).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 *
 * See files COPYING.* for License information.
 *
 *
 * btree enumeration
 *
 */

#include "config.h"

#include <string.h>
#include <stdio.h>
#include "db.h"
#include "error.h"
#include "btree.h"

/*
 * enumerate a whole level in the tree - start with "page" and traverse
 * the linked list of all the siblings
 */
static ham_status_t 
my_enumerate_level(ham_page_t *page, ham_u32_t level, 
        ham_enumerate_cb_t cb, void *context);

/*
 * enumerate a single page
 */
static ham_status_t
my_enumerate_page(ham_page_t *page, ham_u32_t level, ham_u32_t count, 
        ham_enumerate_cb_t cb, void *context);

ham_status_t
btree_enumerate(ham_btree_t *be, ham_enumerate_cb_t cb,
        void *context)
{
    ham_page_t *page;
    ham_u32_t level=0;
    ham_offset_t ptr_left;
    btree_node_t *node;
    ham_status_t st;
    ham_db_t *db=btree_get_db(be);

    ham_assert(btree_get_rootpage(be)!=0, ("invalid root page"));
    ham_assert(cb!=0, ("invalid parameter"));

    /* get the root page of the tree */
    page=db_fetch_page(db, btree_get_rootpage(be), 0);
    if (!page)
        return (db_get_error(db));

    /* while we found a page... */
    while (page) {
        node=ham_page_get_btree_node(page);
        ptr_left=btree_node_get_ptr_left(node);

        cb(ENUM_EVENT_DESCEND, (void *)&level, 0, context);

        /*
         * enumerate the page and all its siblings
         */
        st=my_enumerate_level(page, level, cb, context);
        if (st)
            return (st);

        /*
         * follow the pointer to the smallest child
         */
        if (ptr_left)
            page=db_fetch_page(db, ptr_left, 0);
        else
            page=0;

        ++level;
    }

    return (0);
}

static ham_status_t 
my_enumerate_level(ham_page_t *page, ham_u32_t level, 
        ham_enumerate_cb_t cb, void *context)
{
    ham_status_t st;
    ham_size_t count=0;
    btree_node_t *node;

    while (page) {
        /*
         * enumerate the page
         */
        st=my_enumerate_page(page, level, count, cb, context);
        if (st)
            return (st);

        /* 
         * get the right sibling
         */
        node=ham_page_get_btree_node(page);
        if (btree_node_get_right(node))
            page=db_fetch_page(page_get_owner(page), 
                    btree_node_get_right(node), 0);
        else
            break;

        ++count;
    }

    return (0);
}

ham_status_t
my_enumerate_page(ham_page_t *page, ham_u32_t level, ham_u32_t sibcount, 
        ham_enumerate_cb_t cb, void *context)
{
    ham_size_t i, count;
    ham_db_t *db=page_get_owner(page);
    int_key_t *bte;
    btree_node_t *node=ham_page_get_btree_node(page);
    ham_bool_t is_leaf;

    if (btree_node_get_ptr_left(node))
        is_leaf=0;
    else
        is_leaf=1;

    count=btree_node_get_count(node);

    cb(ENUM_EVENT_PAGE_START, (void *)page, &is_leaf, context);

    for (i=0; i<count; i++) {
        bte=btree_node_get_key(db, node, i);

        cb(ENUM_EVENT_ITEM, (void *)bte, (void *)&count, context);
    }

    cb(ENUM_EVENT_PAGE_STOP, (void *)page, &is_leaf, context);

    return (0);
}

