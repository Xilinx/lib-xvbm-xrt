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

#include "xvbm.h"
#include <list>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>

class PoolTest : public ::testing::Test
{
protected:
    xclDeviceHandle d_handle;

    virtual void SetUp() { d_handle = xclOpen(0, NULL, XCL_QUIET); }
    virtual void TearDown() { xclClose(d_handle); }
};

void alloc_multiple_buffers(XvbmPoolHandle p_handle, 
                           int32_t         num_entries,
                           size_t          size)
{
    XvbmBufferHandle b_handle;
    int32_t          id;

    // Allocate the number of requested buffers 
    for (int i = 0; i < num_entries; i++)
    {
        b_handle = xvbm_buffer_pool_entry_alloc(p_handle);
        ASSERT_TRUE(b_handle != NULL);

        id = xvbm_buffer_get_id(b_handle);
        ASSERT_TRUE((id >= 0) && (id < (num_entries * 2)));
        ASSERT_TRUE(xvbm_buffer_get_paddr(b_handle) != -1);
        ASSERT_TRUE(xvbm_buffer_get_bo_handle(b_handle) != -1);
        EXPECT_EQ(xvbm_buffer_get_size(b_handle), size);
    }
}
    

TEST_F(PoolTest, PositiveCreateDestroy)
{
    XvbmPoolHandle   p_handle;
    size_t size = 1920*1080*1.5;
    uint32_t num_entries = 10;
    uint32_t flags = 0;

    // Create the buffers
    p_handle = xvbm_buffer_pool_create(d_handle, 
                                       num_entries,
                                       size,
                                       flags);
    ASSERT_TRUE(p_handle != NULL);
    ASSERT_TRUE(p_handle != (XvbmPoolHandle)-1);

    // Destroy the pool
    xvbm_buffer_pool_destroy(p_handle);
}

TEST_F(PoolTest, PositiveCreateByIdAndDestroy)
{
    XvbmPoolHandle   p_handle;
    size_t size = 1920*1080*1.5;
    uint32_t num_entries = 10;
    uint32_t flags = 0;

    // Create the buffers
    p_handle = xvbm_buffer_pool_create_by_device_id(0, 
                                                    num_entries,
                                                    size,
                                                    flags);
    ASSERT_TRUE(p_handle != NULL);
    ASSERT_TRUE(p_handle != (XvbmPoolHandle)-1);

    // Destroy the pool
    xvbm_buffer_pool_destroy(p_handle);
}

TEST_F(PoolTest, PositiveAllocFreeExtend)
{
    XvbmPoolHandle   p_handle;
    XvbmBufferHandle b_handle;
    size_t size = 1920*1080*1.5;
    uint32_t num_entries = 5;
    uint32_t flags = 0;
    std::list<XvbmBufferHandle> handle_list;

    // Create the buffers
    p_handle = xvbm_buffer_pool_create(d_handle, 
                                       num_entries,
                                       size,
                                       flags);
    ASSERT_TRUE(p_handle != NULL);
    ASSERT_TRUE(p_handle != (XvbmPoolHandle)-1);

    // Allocate all of the buffers
    for (int i = 0; i < num_entries; i++)
    {
        b_handle = xvbm_buffer_pool_entry_alloc(p_handle);
        ASSERT_TRUE(b_handle != NULL);

        EXPECT_EQ(xvbm_buffer_get_id(b_handle), i);
        ASSERT_TRUE(xvbm_buffer_get_paddr(b_handle) != -1);
        ASSERT_TRUE(xvbm_buffer_get_bo_handle(b_handle) != -1);
        EXPECT_EQ(xvbm_buffer_get_size(b_handle), size);
        handle_list.push_back(b_handle);
    }

    // Extend the pool by 10 buffers
    EXPECT_EQ(xvbm_buffer_pool_extend(handle_list.front(), 10), num_entries+10);

    // Allocate new batch of the buffers
    for (int i = 0; i < 10; i++)
    {
        b_handle = xvbm_buffer_pool_entry_alloc(p_handle);
        ASSERT_TRUE(b_handle != NULL);

        uint64_t paddr = xvbm_buffer_get_paddr(b_handle);
        ASSERT_TRUE(xvbm_buffer_get_paddr(b_handle) != -1);
        ASSERT_TRUE(xvbm_buffer_get_bo_handle(b_handle) != -1);
        EXPECT_EQ(xvbm_buffer_get_size(b_handle), size);
        EXPECT_EQ(xvbm_buffer_pool_num_buffers_get(b_handle), num_entries+10);
        EXPECT_EQ(xvbm_buffer_get_handle(p_handle, paddr), b_handle);
        handle_list.push_back(b_handle);
    }

    // Move all buffers back to the free list
    for (auto &my_b_handle : handle_list)
        EXPECT_EQ(xvbm_buffer_pool_entry_free(my_b_handle), true);

    // Allocate all buffers 
    for (int i = 0; i < num_entries+10; i++)
    {
        b_handle = xvbm_buffer_pool_entry_alloc(p_handle);
        ASSERT_TRUE(b_handle != NULL);

        uint64_t paddr = xvbm_buffer_get_paddr(b_handle);
        ASSERT_TRUE(xvbm_buffer_get_paddr(b_handle) != -1);
        ASSERT_TRUE(xvbm_buffer_get_bo_handle(b_handle) != -1);
        EXPECT_EQ(xvbm_buffer_get_size(b_handle), size);
        EXPECT_EQ(xvbm_buffer_pool_num_buffers_get(b_handle), num_entries+10);
        EXPECT_EQ(xvbm_buffer_get_handle(p_handle, paddr), b_handle);
    }

    // Destroy the pool
    xvbm_buffer_pool_destroy(p_handle);
}

TEST_F(PoolTest, WriteReadBuffer)
{
    XvbmPoolHandle   p_handle;
    XvbmBufferHandle b_handle;
    XvbmBufferHandle bad_b_handle;
    size_t size = 1920*1080*1.5;
    uint32_t num_entries = 1;
    uint32_t flags = 0;

    // Create the buffers
    p_handle = xvbm_buffer_pool_create(d_handle, 
                                       num_entries,
                                       size,
                                       flags);
    ASSERT_TRUE(p_handle != NULL);
    ASSERT_TRUE(p_handle != (XvbmPoolHandle)-1);

    // Allocate a buffer
    b_handle = xvbm_buffer_pool_entry_alloc(p_handle);
    ASSERT_TRUE(b_handle != NULL);

    EXPECT_EQ(xvbm_buffer_get_id(b_handle), 0);
    ASSERT_TRUE(xvbm_buffer_get_paddr(b_handle) != -1);
    ASSERT_TRUE(xvbm_buffer_get_bo_handle(b_handle) != -1);
    EXPECT_EQ(xvbm_buffer_get_size(b_handle), size);

    // Create a host buffer and populate with a known pattern
    uint8_t *w_host_buff = (uint8_t *)xvbm_buffer_get_host_ptr(b_handle);
    for (int i = 0; i < size/2; i+=2)
    {
        w_host_buff[i] = 0xaa;
        w_host_buff[i+1] = 0x55;
    }

    // Write the buffer to the device
    EXPECT_EQ(xvbm_buffer_write(b_handle, w_host_buff, size, 0), 0);

    // Read and verify the buffer read from the device
    uint8_t *r_host_buff = (uint8_t *)xvbm_buffer_get_host_ptr(b_handle);

    //clear buffer
    memset(r_host_buff, 0, size);

    EXPECT_EQ(xvbm_buffer_read(b_handle, r_host_buff, size, 0), 0);

    for (int i = 0; i < size/2; i+=2)
    {
        EXPECT_EQ(r_host_buff[i], 0xaa);
        EXPECT_EQ(r_host_buff[i+1], 0x55);
    }

    // Write the buffer with the wrong size
    EXPECT_NE(xvbm_buffer_write(b_handle, w_host_buff, size*2, 0), 0);

    // Read the buffer with the wrong size
    EXPECT_NE(xvbm_buffer_read(b_handle, r_host_buff, size*2, 0), 0);
}

TEST_F(PoolTest, NegativeInc)
{
    XvbmPoolHandle   p_handle;
    XvbmBufferHandle b_handle;
    XvbmBufferHandle bad_b_handle;
    size_t size = 1920*1080*1.5;
    uint32_t num_entries = 1;
    uint32_t flags = 0;

    // Create the buffers
    p_handle = xvbm_buffer_pool_create(d_handle, 
                                       num_entries,
                                       size,
                                       flags);
    ASSERT_TRUE(p_handle != NULL);
    ASSERT_TRUE(p_handle != (XvbmPoolHandle)-1);

    // Allocate the buffer
    b_handle = xvbm_buffer_pool_entry_alloc(p_handle);
    ASSERT_TRUE(b_handle != NULL);

    EXPECT_EQ(xvbm_buffer_get_id(b_handle), 0);
    ASSERT_TRUE(xvbm_buffer_get_paddr(b_handle) != -1);
    ASSERT_TRUE(xvbm_buffer_get_bo_handle(b_handle) != -1);
    EXPECT_EQ(xvbm_buffer_get_size(b_handle), size);

    // Attempt to allocate another buffer - this should fail
    bad_b_handle = xvbm_buffer_pool_entry_alloc(p_handle);
    ASSERT_TRUE(bad_b_handle == NULL);

    // Move buffer back to the free list
    EXPECT_EQ(xvbm_buffer_pool_entry_free(b_handle), true);

    // Attempt to allocate the free buffer
    b_handle = xvbm_buffer_pool_entry_alloc(p_handle);
    ASSERT_TRUE(b_handle != NULL);
    EXPECT_EQ(xvbm_buffer_get_id(b_handle), 0);
    ASSERT_TRUE(xvbm_buffer_get_paddr(b_handle) != -1);

    // Increment the reference count on the buffer to 3
    xvbm_buffer_refcnt_inc(b_handle);
    xvbm_buffer_refcnt_inc(b_handle);

    // Free the buffer - this won't move the buffer to the free list 
    // but it will decrement the free count to 2
    EXPECT_EQ(xvbm_buffer_pool_entry_free(b_handle), false);
    EXPECT_EQ(xvbm_buffer_pool_entry_free(b_handle), false);
    EXPECT_EQ(xvbm_buffer_pool_entry_free(b_handle), true);
    EXPECT_EQ(xvbm_buffer_pool_entry_free(b_handle), false);

    // Attempt to allocate the free buffer
    b_handle = xvbm_buffer_pool_entry_alloc(p_handle);
    ASSERT_TRUE(b_handle != NULL);
    EXPECT_EQ(xvbm_buffer_get_id(b_handle), 0);
    ASSERT_TRUE(xvbm_buffer_get_paddr(b_handle) != -1);
    EXPECT_EQ(xvbm_buffer_pool_entry_free(b_handle), true);

    // Destroy the pool
    xvbm_buffer_pool_destroy(p_handle);
}

TEST_F(PoolTest, MultipleThreads)
{
    XvbmPoolHandle   p_handle;
    size_t size = 1920*1080*1.5;
    uint32_t num_entries = 10;
    uint32_t flags = 0;

    // Create the buffers
    p_handle = xvbm_buffer_pool_create(d_handle, 
                                       num_entries,
                                       size,
                                       flags);
    ASSERT_TRUE(p_handle != NULL);
    ASSERT_TRUE(p_handle != (XvbmPoolHandle)-1);

    std::thread t1(alloc_multiple_buffers, p_handle, num_entries / 2, size );
    std::thread t2(alloc_multiple_buffers, p_handle, num_entries / 2, size);
    t1.join();
    t2.join();

    // Destroy the pool
    xvbm_buffer_pool_destroy(p_handle);
}

TEST_F(PoolTest, SetGetOffsets)
{
    XvbmPoolHandle   p_handle;
    XvbmBufferHandle b_handle;
    size_t size = 1920*1080*1.5;
    uint32_t num_entries = 10;
    uint32_t flags = 0;
    uint32_t planes[] = {0, 1920*1080, (1920*1080) + ((1920*1080)<<2)};

    // Create the buffers
    p_handle = xvbm_buffer_pool_create(d_handle, 
                                       num_entries,
                                       size,
                                       flags);
    ASSERT_TRUE(p_handle != NULL);
    ASSERT_TRUE(p_handle != (XvbmPoolHandle)-1);

    // Set the offsets of the buffer pool entries
    xvbm_buffer_pool_offsets_set(p_handle, planes, 3);

    // Attempt to allocate the free buffer
    b_handle = xvbm_buffer_pool_entry_alloc(p_handle);
    ASSERT_TRUE(b_handle != NULL);
    EXPECT_EQ(xvbm_buffer_get_id(b_handle), 0);
    ASSERT_TRUE(xvbm_buffer_get_paddr(b_handle) != -1);

    // Verify the offsets
    EXPECT_EQ(xvbm_buffer_pool_offset_get(b_handle, 0), planes[0]);
    EXPECT_EQ(xvbm_buffer_pool_offset_get(b_handle, 1), planes[1]);
    EXPECT_EQ(xvbm_buffer_pool_offset_get(b_handle, 2), planes[2]);

    // Destroy the pool
    xvbm_buffer_pool_destroy(p_handle);
}

TEST_F(PoolTest, GetHandleByPaddr)
{
    XvbmPoolHandle   p_handle;
    XvbmBufferHandle b_handle;
    size_t size = 1920*1080*1.5;
    uint32_t num_entries = 10;
    uint32_t flags = 0;
    uint32_t planes[] = {0, 1920*1080, (1920*1080) + ((1920*1080)<<2)};

    // Create the buffers
    p_handle = xvbm_buffer_pool_create(d_handle, 
                                       num_entries,
                                       size,
                                       flags);
    ASSERT_TRUE(p_handle != NULL);
    ASSERT_TRUE(p_handle != (XvbmPoolHandle)-1);

    // Set the offsets of the buffer pool entries
    xvbm_buffer_pool_offsets_set(p_handle, planes, 3);

    // Attempt to allocate the free buffer
    b_handle = xvbm_buffer_pool_entry_alloc(p_handle);
    ASSERT_TRUE(b_handle != NULL);
    EXPECT_EQ(xvbm_buffer_get_id(b_handle), 0);
    uint64_t paddr = xvbm_buffer_get_paddr(b_handle);
    ASSERT_TRUE(paddr != -1);
    EXPECT_EQ(xvbm_buffer_get_handle(p_handle, paddr), b_handle);

    // Destroy the pool
    xvbm_buffer_pool_destroy(p_handle);
}
