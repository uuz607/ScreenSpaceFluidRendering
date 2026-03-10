/*
* Vulkan buffer class
*
* Encapsulates a Vulkan buffer
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*
* Modifications Copyright (c) 2026 Liu Linqi
*/

#include "VulkanBuffer.h"
#include "VulkanTools.h"

namespace vks
{	
	/** 
	* Release all Vulkan resources held by this buffer
	*/
	Buffer::~Buffer()
	{
		unmap();
		if (buffer_)
		{
			vkDestroyBuffer(device_, buffer_, nullptr);
            buffer_ = VK_NULL_HANDLE;
		}
		
		if (memory_)
		{
			vkFreeMemory(device_, memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
		}
	}


	/** 
	* Map a memory_ range of this buffer. If successful, mapped_ points to the specified buffer range.
	* 
	* @param size (Optional) Size of the memory_ range to map. Pass VK_WHOLE_SIZE to map the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	* 
	* @return VkResult of the buffer mapping call
	*/
	void Buffer::map(VkDeviceSize size, VkDeviceSize offset)
	{
		VK_CHECK_RESULT(vkMapMemory(device_, memory_, offset, size, 0, &mapped_));
	}

	/**
	* Unmap a mapped_ memory_ range
	*
	* @note Does not return a result as vkUnmapMemory can't fail
	*/
	void Buffer::unmap()
	{
		if (mapped_)
		{
			vkUnmapMemory(device_, memory_);
			mapped_ = nullptr;
		}
	}

	/** 
	* Attach the allocated memory_ block to the buffer
	* 
	* @param offset (Optional) Byte offset (from the beginning) for the memory_ region to bind
	* 
	* @return VkResult of the bindBufferMemory call
	*/
	void Buffer::bind(VkDeviceSize offset)
	{
		VK_CHECK_RESULT(vkBindBufferMemory(device_, buffer_, memory_, offset));
	}

	/**
	* Copies the specified data to the mapped_ buffer
	* 
	* @param data Pointer to the data to copy
	* @param size Size of the data to copy in machine units
	*
	*/
	void Buffer::copyTo(void* data, VkDeviceSize size)
	{
		assert(mapped_);
		memcpy(mapped_, data, size);
	}

	/** 
	* Flush a memory_ range of the buffer to make it visible to the device_
	*
	* @note Only required for non-coherent memory_
	*
	* @param size (Optional) Size of the memory_ range to flush. Pass VK_WHOLE_SIZE to flush the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	*
	* @return VkResult of the flush call
	*/
	void Buffer::flush(VkDeviceSize size, VkDeviceSize offset)
	{
		VkMappedMemoryRange mappedRange = {};
		mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRange.memory = memory_;
		mappedRange.offset = offset;
		mappedRange.size = size;
		VK_CHECK_RESULT(vkFlushMappedMemoryRanges(device_, 1, &mappedRange));
	}

	/**
	* Invalidate a memory_ range of the buffer to make it visible to the host
	*
	* @note Only required for non-coherent memory_
	*
	* @param size (Optional) Size of the memory_ range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	*
	* @return VkResult of the invalidate call
	*/
	void Buffer::invalidate(VkDeviceSize size, VkDeviceSize offset)
	{
		VkMappedMemoryRange mappedRange = {};
		mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRange.memory = memory_;
		mappedRange.offset = offset;
		mappedRange.size = size;
		VK_CHECK_RESULT(vkInvalidateMappedMemoryRanges(device_, 1, &mappedRange));
	}
};