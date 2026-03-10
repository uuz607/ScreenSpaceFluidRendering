#include "VulkanTexture.h"
#include "VulkanContext.h"
#include <utility>

namespace vks
{
	Texture::~Texture()
	{	
		destroy();
	}

	Texture::Texture(Texture&& other) noexcept
	{
		width_ = std::exchange(other.width_, 0);
		height_ = std::exchange(other.height_, 0);
		levelCount_ = std::exchange(other.levelCount_, 0);
		layerCount_ = std::exchange(other.layerCount_, 0);

		device_ = std::exchange(other.device_, VK_NULL_HANDLE);
		image_ = std::exchange(other.image_, VK_NULL_HANDLE);
		currentImageLayout_ = std::exchange(other.currentImageLayout_, VK_IMAGE_LAYOUT_UNDEFINED);
		memory_ = std::exchange(other.memory_, VK_NULL_HANDLE);
		format_ = std::exchange(other.format_, VK_FORMAT_UNDEFINED);
		imageView_ = std::exchange(other.imageView_, VK_NULL_HANDLE);
	}

    Texture& Texture::operator=(Texture&& other) noexcept
	{
		if (this != &other) 
		{
			destroy();

			width_ = std::exchange(other.width_, 0);
			height_ = std::exchange(other.height_, 0);
			levelCount_ = std::exchange(other.levelCount_, 0);
			layerCount_ = std::exchange(other.layerCount_, 0);

			device_ = std::exchange(other.device_, VK_NULL_HANDLE);
			image_ = std::exchange(other.image_, VK_NULL_HANDLE);
			currentImageLayout_ = std::exchange(other.currentImageLayout_, VK_IMAGE_LAYOUT_UNDEFINED);
			memory_ = std::exchange(other.memory_, VK_NULL_HANDLE);
			format_ = std::exchange(other.format_, VK_FORMAT_UNDEFINED);
			imageView_ = std::exchange(other.imageView_, VK_NULL_HANDLE);
		}

		return *this;
	}

	void Texture::destroy()
	{
		if (imageView_) 
		{
			vkDestroyImageView(device_, imageView_, nullptr);
		}

		if (image_ && !isSwapchainImage_)
		{
			vkDestroyImage(device_, image_, nullptr);
		}

		if (memory_) 
		{
			vkFreeMemory(device_, memory_, nullptr);
		}
	}
}