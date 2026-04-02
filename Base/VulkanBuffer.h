/*
* Vulkan buffer class
*
* Encapsulates a Vulkan buffer
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*
*/
// Modified by Liu Linqi 2026

#pragma once

#include <type_traits>
#include <vulkan/vulkan.h>

namespace vks
{	
	class VulkanContext;

	enum class BufferUsage : uint32_t
	{
		Undefined,
		VertexRead,
		IndexRead,
		IndirectRead,
		UniformRead,
		ShaderRead,
		ShaderWrite,
		TransferSrc,
		TransferDst,
		HostWrite,
		ComputeUniformRead,
		ComputeShaderRead,
		ComputeShaderWrite,
		ComputeShaderReadWrite
	};

	/**
	* @brief Encapsulates access to a Vulkan buffer backed up by device memory
	* @note To be filled by an external source like the VulkanDevice
	*/
	class Buffer
	{
	public:
		Buffer() = default;
		virtual ~Buffer();

		Buffer(const Buffer&) = delete;
		Buffer& operator=(const Buffer&) = delete;

		VkBuffer handle() const { return buffer_; }
		VkDeviceSize size() const { return size_; }

		VkDescriptorBufferInfo descriptorInfo() const { return { buffer_, 0, VK_WHOLE_SIZE }; }
		VkBufferUsageFlags usageFlags() const { return usageFlags_; }

		BufferUsage getCurrentUsage() const { return currentBufferUsage_; }
		void setCurrentUsage(BufferUsage bufferUsage) { this->currentBufferUsage_ = bufferUsage; }

		void map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void unmap();
		void bind(VkDeviceSize offset = 0);
		void copyTo(void* data, VkDeviceSize size);
		void flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		
	private:
		VkDevice device_ = VK_NULL_HANDLE;

		VkBuffer buffer_ = VK_NULL_HANDLE;
		VkDeviceMemory memory_ = VK_NULL_HANDLE;
		VkDeviceSize size_ = 0;
		void* mapped_ = nullptr;
		/** @brief Usage flags to be filled by external source at buffer creation (to query at some later point) */
		VkBufferUsageFlags usageFlags_;
		/** @brief Memory property flags to be filled by external source at buffer creation (to query at some later point) */
		VkMemoryPropertyFlags memoryPropertyFlags_;

		BufferUsage currentBufferUsage_ = BufferUsage::Undefined;

		friend class VulkanContext;
	};

	template<typename T>
	class UniformBuffer : public Buffer
	{
	public:
		UniformBuffer() : Buffer() {}
		~UniformBuffer() = default;

		static_assert(std::is_trivially_copyable_v<T>, "UniformBuffer<T> must be trivially copyable");

		void update()
		{
			copyTo(&data, sizeof(T));

			setCurrentUsage(BufferUsage::HostWrite);
		}

		T data = {};
	};

	template<typename T>
	class DeviceBuffer : public Buffer
	{
	public:
		DeviceBuffer() : Buffer() {}
		~DeviceBuffer() = default;	

		static_assert(std::is_trivially_copyable_v<T>, "DeviceBuffer<T> must be trivially copyable");

		uint32_t count() const { return count_; }
	
	private:
		uint32_t count_ = 0;

		friend class VulkanContext;
	};
}
