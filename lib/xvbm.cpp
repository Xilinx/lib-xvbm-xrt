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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "xvbm.h"
#include "xvbm_private.h"

#define ALIGN_4K        4096

//@TODO use syslog or xmalog for logging

//////////////////////////////////////////////////////////////////////////////
// Class method for creating a buffer
//////////////////////////////////////////////////////////////////////////////
//@TODO handle out of memory
XvbmBuffer* XvbmBufferPool::create_buffer(int32_t index)
{
    void *host_ptr = nullptr;
    uint32_t bo_handle;
    int rc = -1;

    //allocate host buffer (4K aligned)
    /*@TODO Do not allocate host buffer when not required.
        'flags' argument for 'DDR bank to allocate buffer' is not being used.
        Maybe that can be re-used to specify type of buffers like
        Device-only, Host+device etc.
        Host buffer is required in cases where:
        1. Padding needs to be done on the host side before sending
           the buffer to the device side.
        2. The user provided host buffer is not aligned at 4K
    */
    if(posix_memalign(&host_ptr, ALIGN_4K, m_size)) {
        std::cout << "xvbm : aligned alloc failed" << std::endl;
        throw std::bad_alloc();
    }
    memset(host_ptr, 0, m_size);

    bo_handle = xclAllocBO(m_dev_handle, m_size, 0, XCL_BO_FLAGS_DEV_ONLY);
    if (bo_handle == NULLBO) {
        std::cerr << "xvbm : xclAllocBO failed" << std::endl;
        throw std::bad_alloc();
    }

    uint64_t paddr = xclGetDeviceAddr(m_dev_handle, bo_handle);
    XvbmBuffer *buffer = new XvbmBuffer(this, bo_handle, index, m_size, paddr, host_ptr);
    assert(buffer != nullptr);

    if (buffer->write_buffer(host_ptr, m_size, 0)) {
        delete buffer;
        free(host_ptr);
        throw std::bad_alloc();
    }

    m_alloc_vector.push_back(buffer);
    m_paddr_map.insert(std::pair<uint64_t, XvbmBuffer*>(paddr, buffer));
    m_free_list.push_back(buffer);

    return buffer;
}

//////////////////////////////////////////////////////////////////////////////
// Class method for creating the buffer pool
//////////////////////////////////////////////////////////////////////////////
//@TODO handle out of memory
void XvbmBufferPool::create()
{
    int32_t i;

    std::lock_guard<std::mutex> guard(m_lock);

    for (i = 0; i < m_num_buffers; i++) {
        try {
            create_buffer(i);
        } catch (const std::bad_alloc&) {
            std::cerr <<  "buffer #" << i << "failed" << std::endl;
            throw std::bad_alloc();
        }
    }
}

bool XvbmBufferPool::destroy_l()
{
    bool des = false;
    m_ref_cnt--;
    if (m_ref_cnt == 0) {
        assert(m_inuse_list.size() == 0);
        assert(m_free_list.size() == m_alloc_vector.size());
        if ((m_inuse_list.size() != 0) || (m_free_list.size() != m_alloc_vector.size())) {
            std::cerr << "Error : Something went wrong, Pool: " << this << " may leak" << std::endl;
            std::cerr << this << " : free buffers : " << m_free_list.size() << std::endl;
            std::cerr << this << " : Allocated buffers : " << m_alloc_vector.size() << std::endl;
            std::cerr << this << " : In Use buffers : " << m_inuse_list.size() << std::endl;
            return false;
        }
        for (auto buf : m_free_list) {
            xclFreeBO(m_dev_handle, buf->m_bo_handle);
            if (buf->m_hptr) {
                free(buf->m_hptr);
                buf->m_hptr = nullptr;
            }
            delete buf;
        }
        m_free_list.clear();
        des = true;
    }
    return des;
}

//@TODO return status
void XvbmBufferPool::destroy()
{
    bool des = false;
    {
        std::lock_guard<std::mutex> guard(m_lock);
        des = destroy_l();
    }
    if (des) {
        delete this;
    }
}
//////////////////////////////////////////////////////////////////////////////
// Class method for extending a buffer pool
//////////////////////////////////////////////////////////////////////////////
int32_t XvbmBufferPool::extend(int32_t num_buffers)
{
    int32_t i;

    std::lock_guard<std::mutex> guard(m_lock);

    for (i = m_num_buffers; i < m_num_buffers+num_buffers; i++) {
        try {
            create_buffer(i);
        } catch (const std::bad_alloc&) {
            std::cerr <<  i << "th buffer allocation failed during extension" << std::endl;
            throw std::bad_alloc();
        }
    }

    m_num_buffers += num_buffers;

    return m_num_buffers;
}

//////////////////////////////////////////////////////////////////////////////
// Class method for allocating a buffer from the buffer pool
//////////////////////////////////////////////////////////////////////////////
XvbmBuffer* XvbmBufferPool::entry_alloc()
{
    XvbmBuffer *buffer = NULL;

    std::lock_guard<std::mutex> guard(m_lock);

    if (m_free_list.size())
    {
        buffer = m_free_list.front();
        ++buffer->m_ref_cnt;
        std::lock_guard<std::mutex> guard(buffer->m_rdlock);
        m_free_list.pop_front();
        m_inuse_list.push_back(buffer);
        m_ref_cnt++;
    }

    return buffer;
}

//////////////////////////////////////////////////////////////////////////////
// Class method for freeing a buffer back to the buffer pool
//////////////////////////////////////////////////////////////////////////////
bool XvbmBufferPool::entry_free(XvbmBuffer *buffer)
{
    bool ret = false;
    bool des = false;
    {
        std::lock_guard<std::mutex> guard(m_lock);

        if (buffer->m_ref_cnt > 0) {
            --buffer->m_ref_cnt;

            if (buffer->m_ref_cnt == 0) {
                auto it = std::find(m_inuse_list.begin(), m_inuse_list.end(), buffer);
                if (it != m_inuse_list.end())
                {
                    m_inuse_list.erase(it);
                    m_free_list.push_back(buffer);
                    des = destroy_l();
                    ret = true;
                }
            }
        }
    }
    if (des) {
        delete this;
    }
    return ret;
}

//////////////////////////////////////////////////////////////////////////////
// Class method for getting the buffer handle by physical address
//////////////////////////////////////////////////////////////////////////////
XvbmBuffer* XvbmBufferPool::get_handle_by_paddr(uint64_t paddr)
{
    XvbmBuffer* buffer = (XvbmBuffer*)NULL;

    std::lock_guard<std::mutex> guard(m_lock);
    auto it = m_paddr_map.find(paddr);
    if (it != m_paddr_map.end())
        buffer = it->second;

    return buffer;
}

XvbmPoolHandle xvbm_get_pool_handle(XvbmBufferHandle b_handle)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    XvbmPoolHandle p_handle = buffer->get_pool_handle();

    return p_handle;
}

XvbmBuffer* XvbmBufferPool::get_buffer_handle(uint32_t index)
{
    XvbmBuffer *buffer = (XvbmBuffer*)NULL;

    std::lock_guard<std::mutex> guard(m_lock);
    buffer = m_alloc_vector.at(index);

    return buffer;
}

uint32_t xvbm_get_freelist_count(XvbmPoolHandle p_handle)
{
   XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(p_handle);
   return pool->get_freelist_count();
}

XvbmBufferHandle xvbm_get_buffer_handle(XvbmPoolHandle p_handle, uint32_t index)
{
    XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(p_handle);
    XvbmBuffer *buffer = pool->get_buffer_handle(index);

    return buffer;
}

//////////////////////////////////////////////////////////////////////////////
// Class method for writing a device buffer
//////////////////////////////////////////////////////////////////////////////

int32_t XvbmBuffer::write_buffer(const void *src,
                                 size_t      size,
                                 size_t      offset)
{
    XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(m_p_handle);
    int32_t rc;


    if (m_size < (size+offset)) {
        std::cerr << "write_buffer with invalid size:" << size << " offset:" << offset <<std::endl;
        return (-1);
    }

    // Check if the user provided host buffer is 4k aligned
    if ((size_t)src & 0xFFF) {
        void *aligned_src = (unsigned char*)m_hptr + offset;
        memcpy(aligned_src, src, size);
        rc = xclWriteBO(pool->m_dev_handle,
                        m_bo_handle, aligned_src, size, offset);
    } else {
        rc = xclWriteBO(pool->m_dev_handle,
                        m_bo_handle, src, size, offset);
    }
    if (rc != 0) {
        std::string err = "xclSyncBO to device failed rc=";
        std::cerr << err << rc << std::endl;
    }

    return rc;

}

//////////////////////////////////////////////////////////////////////////////
// Class method for reading a device buffer
//////////////////////////////////////////////////////////////////////////////
int32_t XvbmBuffer::read_buffer(void       *dst,
                                size_t      size,
                                size_t      offset)
{
    XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(m_p_handle);
    int32_t rc = 0;

    std::lock_guard<std::mutex> guard(m_rdlock);

    if (m_size < (size+offset)) {
        std::cerr << "read_buffer with invalid size:" << size << " offset:" << offset <<std::endl;
        return (-1);
    }
    //if there is at-least 1 ref
    if(m_ref_cnt) {
        // Check if the user provided host buffer is 4k aligned
        if ((size_t)dst & 0xFFF) {
            void *aligned_dst = (unsigned char*)m_hptr + offset;
            rc = xclReadBO(pool->m_dev_handle, m_bo_handle, aligned_dst, size, offset);
            if (rc == 0) {
                memcpy(dst, aligned_dst, size);
            }
        } else {
            rc = xclReadBO(pool->m_dev_handle, m_bo_handle, dst, size, offset);
        }
    }
    if (rc != 0) {
        std::string err = "xclSyncBO from device failed rc=";
        std::cerr << err << rc << std::endl;
    }

    return rc;
}

//////////////////////////////////////////////////////////////////////////////
XvbmPoolHandle xvbm_buffer_pool_create(xclDeviceHandle d_handle,
                                       int32_t         num_buffers,
                                       size_t          size,
                                       uint32_t        flags)
{
    XvbmBufferPool *pool = new XvbmBufferPool(d_handle,
                                              num_buffers,
                                              size,
                                              flags);
    try {
        pool->create();
    } catch (const std::bad_alloc&) {
        std::cerr << "xrt : failed to create a pool" << std::endl;
        pool = nullptr;
    }
    return pool;
}

//////////////////////////////////////////////////////////////////////////////
XvbmPoolHandle xvbm_buffer_pool_create_by_device_id(int32_t  device_id,
                                                    int32_t  num_buffers,
                                                    size_t   size,
                                                    uint32_t flags)
{
    xclDeviceHandle d_handle = xclOpen(device_id, NULL, XCL_QUIET);
    return xvbm_buffer_pool_create(d_handle, num_buffers, size, flags);
}

//////////////////////////////////////////////////////////////////////////////
void xvbm_buffer_pool_offsets_set(XvbmPoolHandle p_handle,
                                  uint32_t       *offsets,
                                  uint32_t       num_offsets)
{
    XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(p_handle);

    for ( uint32_t i = 0; i < num_offsets; i++)
        pool->set_offset(offsets[i]);

}

//////////////////////////////////////////////////////////////////////////////
uint32_t xvbm_buffer_pool_offset_get(XvbmBufferHandle b_handle,
                                     uint32_t         offset_idx)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    XvbmPoolHandle p_handle = buffer->get_pool_handle();
    XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(p_handle);

    return pool->get_offset(offset_idx);
}

//////////////////////////////////////////////////////////////////////////////
/*@TODO asking a pool to extend itself is more logical
int32_t xvbm_buffer_pool_extend(XvbmPoolHandle    p_handle,
                                int32_t           num_buffers)

*/
int32_t xvbm_buffer_pool_extend(XvbmBufferHandle  b_handle,
                                int32_t           num_buffers)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    XvbmPoolHandle p_handle = buffer->get_pool_handle();
    XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(p_handle);

    int32_t ret;
    try {
        ret = pool->extend(num_buffers);
    } catch (const std::bad_alloc&)
    {
        std::cerr << "xrt : failed to extend/allocate memory" << std::endl;
        ret = 0;
    }
    return ret;
}

//////////////////////////////////////////////////////////////////////////////
int32_t xvbm_buffer_pool_num_buffers_get(XvbmBufferHandle  b_handle)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    XvbmPoolHandle p_handle = buffer->get_pool_handle();
    XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(p_handle);

    return pool->get_num_buffers();
}

//////////////////////////////////////////////////////////////////////////////
XvbmBufferHandle xvbm_buffer_pool_entry_alloc(XvbmPoolHandle p_handle)
{
    XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(p_handle);
    XvbmBuffer *buffer = pool->entry_alloc();

    return buffer;
}

//////////////////////////////////////////////////////////////////////////////
bool xvbm_buffer_pool_entry_free(XvbmBufferHandle b_handle)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    if (buffer == NULL) {
        std::cerr << "Warning: xvbm trying to free a buffer which is already null" << std::endl;
        return true;
    }
    XvbmPoolHandle p_handle = buffer->get_pool_handle();
    if (p_handle == NULL) {
        std::cerr << "Warning : xvbm buffer pool has already destroyed" << std::endl;
        return true;
    }
    XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(p_handle);
    return pool->entry_free(buffer);
}

//////////////////////////////////////////////////////////////////////////////
//@TODO return bool to notify status
void xvbm_buffer_pool_destroy(XvbmPoolHandle p_handle)
{
    XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(p_handle);
    if (pool) {
        pool->destroy();
    }
}

//////////////////////////////////////////////////////////////////////////////
uint32_t xvbm_buffer_get_bo_handle(XvbmBufferHandle b_handle)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    return buffer->get_bo_handle();
}

//////////////////////////////////////////////////////////////////////////////
uint32_t xvbm_buffer_get_id(XvbmBufferHandle b_handle)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    return buffer->get_id();
}

//////////////////////////////////////////////////////////////////////////////
size_t xvbm_buffer_get_size(XvbmBufferHandle b_handle)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    return buffer->get_size();
}

//////////////////////////////////////////////////////////////////////////////
uint64_t xvbm_buffer_get_paddr(XvbmBufferHandle b_handle)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    return buffer->get_paddr();
}

//////////////////////////////////////////////////////////////////////////////
void *xvbm_buffer_get_host_ptr(XvbmBufferHandle b_handle)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    return buffer->get_host_ptr();
}

//////////////////////////////////////////////////////////////////////////////
XvbmBufferHandle xvbm_buffer_get_handle(XvbmPoolHandle p_handle,
                                        uint64_t       paddr)
{
    XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(p_handle);
    XvbmBuffer *buffer = pool->get_handle_by_paddr(paddr);
    return buffer;
}

//////////////////////////////////////////////////////////////////////////////
uint32_t xvbm_buffer_get_refcnt(XvbmBufferHandle b_handle)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    return(buffer->m_ref_cnt);
}

//////////////////////////////////////////////////////////////////////////////
/*@TODO
    return bool to notify status
    Add API for decrementing the buffer refcount for symmetry
*/
void xvbm_buffer_refcnt_inc(XvbmBufferHandle b_handle)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    XvbmPoolHandle p_handle = buffer->get_pool_handle();
    XvbmBufferPool *pool = static_cast<XvbmBufferPool*>(p_handle);

    std::lock_guard<std::mutex> guard(pool->m_lock);
    assert(buffer->m_ref_cnt > 0);
    if (buffer->m_ref_cnt <= 0) {
        std::cerr << "Error : Can not increment ref count of a free buffer : " << buffer << std::endl;
        return;
    }
    ++buffer->m_ref_cnt;
}

//////////////////////////////////////////////////////////////////////////////
int32_t xvbm_buffer_write(XvbmBufferHandle  b_handle,
                          const void       *src,
                          size_t            size,
                          size_t            offset)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);

    //if (buffer->get_host_ptr() != src)   return(-1);
    return buffer->write_buffer(src, size, offset);
}

//////////////////////////////////////////////////////////////////////////////
int32_t xvbm_buffer_read(XvbmBufferHandle  b_handle,
                         void             *dst,
                         size_t            size,
                         size_t            offset)
{
    XvbmBuffer *buffer = static_cast<XvbmBuffer*>(b_handle);
    //if (buffer->get_host_ptr() != dst)   return(-1);
    return buffer->read_buffer(dst, size, offset);
}
