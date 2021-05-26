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

#ifndef _XVBM_H_
#define _XVBM_H_

#include <stdint.h>
#include <stdbool.h>
#include <xclhal2.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* XvbmPoolHandle;
typedef void* XvbmBufferHandle;

/****************************************************************************/
/* Buffer pool related functions                                            */
/****************************************************************************/

/**
 * Create a memory pool and allocate device buffers
 *
 * @param [in] d_handle   Device handle return from xclOpen
 * @param [in] num_buffer Number of device buffers to allocate
 * @param [in] size       Size of each buffer
 * @param [in] flags      DDR bank to allocate buffer
 *
 * @returns XvbmPoolHandle used for all subsequent memory pool requests
*/
XvbmPoolHandle xvbm_buffer_pool_create(xclDeviceHandle d_handle,
                                       int32_t         num_buffers,
                                       size_t          size,
                                       uint32_t        flags);

/**
 * Create a memory pool and allocate device buffers using a device ID
 *
 * @param [in] device_id  Device ID (from 0-N) containing the buffer pool 
 * @param [in] num_buffer Number of device buffers to allocate
 * @param [in] size       Size of each buffer
 * @param [in] flags      DDR bank to allocate buffer
 *
 * @returns XvbmPoolHandle used for all subsequent memory pool requests
*/
XvbmPoolHandle xvbm_buffer_pool_create_by_device_id(int32_t  device_id,
                                                    int32_t  num_buffers,
                                                    size_t   size,
                                                    uint32_t flags);
/**
 * Set offsets for all buffers in the pool (useful for planes of a frame) 
 *
 * @param [in] p_handle    Handle to an existing buffer pool 
 * @param [in] offsets     Array of offsets 
 * @param [in] num_offsets Number of offsets in array
 *
*/
void xvbm_buffer_pool_offsets_set(XvbmPoolHandle p_handle,
                                  uint32_t       *offsets,
                                  uint32_t       num_offsets);

/**
 * Get offset for a buffer in a pool
 *
 * @param [in] b_handle   Handle to a buffer
 * @param [in] offset_idx Index for a offset (ie., plane-id of a frame)  
 *
*/
uint32_t xvbm_buffer_pool_offset_get(XvbmBufferHandle b_handle,
                                     uint32_t         offset_idx);

/**
 * Extend an existing memory pool allocating additional device buffers
 *
 * @param [in] b_handle   Buffer handle from the buffer pool to be extended 
 * @param [in] num_buffer New total number of buffers 
 *
 * @returns Number of buffers allocated to the pool.  If the current 
 *          number of buffers allocated is greater than the number of
 *          buffers requested, the number of buffers allocated will
 *          remain unchanged.   
*/
int32_t xvbm_buffer_pool_extend(XvbmBufferHandle b_handle,
                                int32_t          num_buffers);

/**
 * Get the number of buffers allocated to the pool associated with a buffer 
 *
 * @param [in] b_handle   Buffer handle from the buffer pool to be extended 
 *
 * @returns number of buffers allocated to the associated pool 
*/
int32_t xvbm_buffer_pool_num_buffers_get(XvbmBufferHandle b_handle);

/**
 * Allocate a free buffer from a memory pool 
 *
 * @param [in] p_handle   Handle to an existing buffer pool 
 *
 * @returns XvbmBufferHandle A non-NULL handle used for buffer operations 
*/
XvbmBufferHandle xvbm_buffer_pool_entry_alloc(XvbmPoolHandle p_handle);

/**
 * Free a buffer and return it back to the memory pool free list 
 *
 * @param [in] b_handle   Handle to a buffer
 *
*/
bool xvbm_buffer_pool_entry_free(XvbmBufferHandle b_handle);

/**
 * Destroy all resources associated with a buffer pool 
 *
 * @param [in] p_handle   Handle to a memory pool
 *
 * @returns true if entry is freed, otherwise false
*/
void xvbm_buffer_pool_destroy(XvbmPoolHandle p_handle);

/****************************************************************************/
/* Buffer related accessor functions                                        */
/****************************************************************************/

/**
 * Get the BO handle from a xvbmBufferHandle 
 *
 * @param [in] b_handle   Handle to a buffer
 *
 * @returns a valid BO handle or -1 if not valid 
*/
uint32_t xvbm_buffer_get_bo_handle(XvbmBufferHandle b_handle);

/**
 * Get the buffer ID used by Host and MPSoC device 
 *
 * @param [in] b_handle   Handle to a buffer
 *
 * @returns an index of the buffer 
*/
uint32_t xvbm_buffer_get_id(XvbmBufferHandle b_handle);

/**
 * Get the buffer size 
 *
 * @param [in] b_handle   Handle to a buffer
 *
 * @returns size of the buffer 
*/
size_t xvbm_buffer_get_size(XvbmBufferHandle b_handle);

/**
 * Get the buffer physical address 
 *
 * @param [in] b_handle   Handle to a buffer
 *
 * @returns the physical address of the buffer
*/
uint64_t xvbm_buffer_get_paddr(XvbmBufferHandle b_handle);

/**
 * Get the buffer handle given a physical address 
 *
 * @param [in] paddr    Physical address of buffer 
 *
 * @returns the buffer handle 
*/
XvbmBufferHandle xvbm_buffer_get_handle(XvbmPoolHandle p_handle,
                                        uint64_t       paddr);

/**
 * Increment the reference count of the buffer 
 *
 * @param [in] b_handle   Handle to a buffer
 *
*/
void xvbm_buffer_refcnt_inc(XvbmBufferHandle b_handle);

/**
 * Get the reference count of the buffer
 *
 * @param [in] b_handle   Handle to a buffer
 *
*/
uint32_t xvbm_buffer_get_refcnt(XvbmBufferHandle b_handle);

/* XHD CHANGES */
XvbmPoolHandle xvbm_get_pool_handle(XvbmBufferHandle b_handle);
XvbmBufferHandle xvbm_get_buffer_handle(XvbmPoolHandle p_handle,
					uint32_t index);
uint32_t xvbm_get_freelist_count(XvbmPoolHandle p_handle);
/* XHD CHANGES END*/

/**
 * Get the host buffer handle
 *
 * @param [in] b_handle   Handle to a xvbm buffer
 *
 * @returns the virtual pointer of the allocated host buffer
*/
void *xvbm_buffer_get_host_ptr(XvbmBufferHandle b_handle);

/****************************************************************************/
/* Functions for reading and writing buffers                                */
/****************************************************************************/

/**
 * Write a buffer to device memory 
 *
 * @param [in] b_handle   Handle to an XVBM buffer
 * @param [in] src        User supplied data to be written to the device 
 * @param [in] size       Size of data to be written 
 * @param [in] offset     Offset into device buffer 
 *
 * @returns 0 on success 
*/
int32_t xvbm_buffer_write(XvbmBufferHandle  b_handle,
                          const void       *src,
                          size_t            size,
                          size_t            offset);

/**
 * Read a buffer from device memory 
 *
 * @param [in] b_handle   Handle to an XVBM buffer
 * @param [in] src        User supplied buffer to receive the data 
 * @param [in] size       Size of data to be read 
 * @param [in] offset     Offset into device buffer 
 *
 * @returns 0 on success 
*/
int32_t xvbm_buffer_read(XvbmBufferHandle  b_handle,
                         void             *dst,
                         size_t            size,
                         size_t            offset);
#ifdef __cplusplus
}
#endif

#endif
