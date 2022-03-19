/* SPDX-License-Identifier: LGPL-3.0-or-later OR Apache-2.0 */

/*
 * Copyright (C) 2019-2021 Xilinx Inc - All rights reserved
 * Xilinx Video Buffer Manager (Xvbm)
 *
 * This file is dual-licensed; you may select either the GNU
 * Lesser General Public License version 3 or
 * Apache License, Version 2.0.
 *
 */

#ifndef _XVBM_PRIVATE_H_
#define _XVBM_PRIVATE_H_

#include <vector>
#include <list>
#include <map>
#include <atomic>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <xclhal2.h>

//@TODO decouple XvbmBuffer/XvbmBufferPool

/* @TODO
    All members need not be public
*/

typedef struct XvbmBuffer
{
    XvbmPoolHandle        m_p_handle;
    uint32_t              m_bo_handle;
    uint32_t              m_buffer_id;
    size_t                m_size;
    uint64_t              m_paddr;
    void                 *m_hptr;
    uint32_t              m_ref_cnt;
    std::mutex            m_rdlock;

    XvbmBuffer(XvbmPoolHandle p_handle,
               uint32_t       bo_handle,
               uint32_t       buffer_id,
               size_t         size,
               uint64_t       paddr,
               void          *hptr) :
                   m_p_handle(p_handle),
                   m_bo_handle(bo_handle),
                   m_buffer_id(buffer_id),
                   m_size(size),
                   m_paddr(paddr),
                   m_hptr(hptr),
                   m_ref_cnt(0) {}

    ~XvbmBuffer() {}

    XvbmPoolHandle get_pool_handle() { return m_p_handle; }

    int32_t write_buffer(const void *src,
                         size_t      size,
                         size_t      offset);

    int32_t read_buffer(void    *dst,
                        size_t   size,
                        size_t   offset);

    uint32_t get_bo_handle() { return m_bo_handle; }

    uint32_t get_id() { return m_buffer_id; }

    size_t get_size() { return m_size; }

    uint64_t get_paddr() { return m_paddr; }
    void *get_host_ptr() { return m_hptr; }

} XvbmBuffer;

typedef struct XvbmBufferPool
{
    xclDeviceHandle                      m_dev_handle;
    int32_t                              m_num_buffers;
    size_t                               m_size;
    uint32_t                             m_flags;
    std::vector<uint32_t>                m_offsets;
    uint32_t                             m_ref_cnt;
    std::mutex                           m_lock;

    std::vector<XvbmBuffer*>             m_alloc_vector;
    std::map<uint64_t, XvbmBuffer*>      m_paddr_map;
    std::list<XvbmBuffer*>               m_free_list;
    std::list<XvbmBuffer*>               m_inuse_list;

    XvbmBufferPool(xclDeviceHandle dev_handle,
                   int32_t         num_buffers,
                   size_t          size,
                   uint32_t        flags) :
                       m_dev_handle(dev_handle),
                       m_num_buffers(num_buffers),
                       m_size(size),
                       m_flags(flags),
                       m_ref_cnt(1) {}

    ~XvbmBufferPool() {}

    XvbmBuffer* create_buffer(int32_t i);
    void create();
    void set_offset(uint32_t offset) { m_offsets.push_back(offset); }
    uint32_t get_offset(uint32_t offset_idx) { return m_offsets[offset_idx]; }
    int32_t extend(int32_t num_buffers);
    int32_t get_num_buffers() { return m_num_buffers; }
    XvbmBuffer* entry_alloc();
    bool entry_free(XvbmBuffer *buffer);
    XvbmBuffer* get_handle_by_paddr(uint64_t paddr);
    void destroy();
    XvbmBuffer* get_buffer_handle(uint32_t index);
    uint32_t get_freelist_count() { return m_free_list.size(); }
    bool destroy_l();
} XvbmBufferPool;

#endif
