/*
* Class wrapping access to the swap chain
* 
* A swap chain is a collection of framebuffers used for rendering and presentation to the windowing system
*
* Copyright (C) 2016-2024 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
* 
* Modifications Copyright (c) 2026 Liu Linqi
*/

#include "VulkanSwapChain.h"

namespace vks
{
	VulkanSwapChain::~VulkanSwapChain()
	{
		if (swapChain_ != VK_NULL_HANDLE) 
		{
			vkDestroySwapchainKHR(device_, swapChain_, nullptr);
		}

		if (surface_ != VK_NULL_HANDLE) 
		{
			vkDestroySurfaceKHR(instance_, surface_, nullptr);
		}
		surface_ = VK_NULL_HANDLE;
		swapChain_ = VK_NULL_HANDLE;
	}

	/** @brief Creates the platform specific surface_ abstraction of the native platform window used for presentation */	
	void VulkanSwapChain::initSurface(void* platformHandle, void* platformWindow)
	{
		VkResult err = VK_SUCCESS;

		// Create the os-specific surface_
		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.hinstance = (HINSTANCE)platformHandle;
		surfaceCreateInfo.hwnd = (HWND)platformWindow;
		err = vkCreateWin32SurfaceKHR(instance_, &surfaceCreateInfo, nullptr, &surface_);

		if (err != VK_SUCCESS) 
		{
			vks::tools::exitFatal("Could not create surface_!", err);
		}

		// Get available queue family properties
		uint32_t queueCount;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueCount, NULL);
		assert(queueCount >= 1);

		std::vector<VkQueueFamilyProperties> queueProps(queueCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueCount, queueProps.data());

		// Iterate over each queue to learn whether it supports presenting:
		// Find a queue with present support
		// Will be used to present the swap chain images to the windowing system
		std::vector<VkBool32> supportsPresent(queueCount);
		for (uint32_t i = 0; i < queueCount; ++i) 
		{
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface_, &supportsPresent[i]);
		}

		// Search for a graphics and a present queue in the array of queue
		// families, try to find one that supports both
		uint32_t graphicsQueueNodeIndex = UINT32_MAX;
		uint32_t presentQueueNodeIndex = UINT32_MAX;
		for (uint32_t i = 0; i < queueCount; ++i) 
		{
			if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) 
			{
				if (graphicsQueueNodeIndex == UINT32_MAX) 
				{
					graphicsQueueNodeIndex = i;
				}

				if (supportsPresent[i] == VK_TRUE) 
				{
					graphicsQueueNodeIndex = i;
					presentQueueNodeIndex = i;
					break;
				}
			}
		}
		if (presentQueueNodeIndex == UINT32_MAX) 
		{	
			// If there's no queue that supports both present and graphics
			// try to find a separate present queue
			for (uint32_t i = 0; i < queueCount; ++i) 
			{
				if (supportsPresent[i] == VK_TRUE) 
				{
					presentQueueNodeIndex = i;
					break;
				}
			}
		}

		// Exit if either a graphics or a presenting queue hasn't been found
		if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX) 
		{
			vks::tools::exitFatal("Could not find a graphics and/or presenting queue!", -1);
		}

		if (graphicsQueueNodeIndex != presentQueueNodeIndex) 
		{
			vks::tools::exitFatal("Separate graphics and presenting queues are not supported yet!", -1);
		}

		queueNodeIndex_ = graphicsQueueNodeIndex;

		// Get list of supported surface_ formats
		uint32_t formatCount;
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, NULL));
		assert(formatCount > 0);

		std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, surfaceFormats.data()));

		// We want to get a format that best suits our needs, so we try to get one from a set of preferred formats
		// Initialize the format to the first one returned by the implementation in case we can't find one of the preffered formats
		VkSurfaceFormatKHR selectedFormat = surfaceFormats[0];
		std::vector<VkFormat> preferredImageFormats = { 
			VK_FORMAT_B8G8R8A8_UNORM,
			VK_FORMAT_R8G8B8A8_UNORM, 
			VK_FORMAT_A8B8G8R8_UNORM_PACK32 
		};

		for (auto& availableFormat : surfaceFormats) 
		{
			if (std::find(preferredImageFormats.begin(), preferredImageFormats.end(), availableFormat.format) != preferredImageFormats.end()) {
				selectedFormat = availableFormat;
				break;
			}
		}

		colorFormat_ = selectedFormat.format;
		colorSpace_ = selectedFormat.colorSpace;
	}

	void VulkanSwapChain::setContext(VkInstance instance_, VkPhysicalDevice physicalDevice_, VkDevice device_)
	{
		this->instance_ = instance_;
		this->physicalDevice_ = physicalDevice_;
		this->device_ = device_;
	}

	void VulkanSwapChain::create(uint32_t& width, uint32_t& height, bool vsync, bool fullscreen)
	{
		assert(physicalDevice_);
		assert(device_);
		assert(instance_);

		// Store the current swap chain handle so we can use it later on to ease up recreation
		VkSwapchainKHR oldSwapchain = swapChain_;

		// Get physical device surface properties and formats
		VkSurfaceCapabilitiesKHR surfCaps;
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &surfCaps));

		VkExtent2D swapchainExtent = {};
		// If width (and height) equals the special value 0xFFFFFFFF, the size of the surface_ will be set by the swapchain
		if (surfCaps.currentExtent.width == (uint32_t)-1)
		{
			// If the surface size is undefined, the size is set to the size of the images requested
			swapchainExtent.width = width;
			swapchainExtent.height = height;
		}
		else
		{
			// If the surface size is defined, the swap chain size must match
			swapchainExtent = surfCaps.currentExtent;
			width = surfCaps.currentExtent.width;
			height = surfCaps.currentExtent.height;
		}


		// Select a present mode for the swapchain
		uint32_t presentModeCount;
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, NULL));
		assert(presentModeCount > 0);

		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, presentModes.data()));

		// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
		// This mode waits for the vertical blank ("v-sync")
		VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

		// If v-sync is not requested, try to find a mailbox mode
		// It's the lowest latency non-tearing present mode available
		if (!vsync)
		{
			for (size_t i = 0; i < presentModeCount; ++i)
			{
				if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
					break;
				}
				if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
				{
					swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
				}
			}
		}

		// Determine the number of images
		uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
		if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
		{
			desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
		}

		// Find the transformation of the surface_
		VkSurfaceTransformFlagsKHR preTransform;
		if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		{
			// We prefer a non-rotated transform
			preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else
		{
			preTransform = surfCaps.currentTransform;
		}

		// Find a supported composite alpha format (not all devices support alpha opaque)
		VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		// Simply select the first composite alpha format available
		std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		};
		for (auto& compositeAlphaFlag : compositeAlphaFlags) 
		{
			if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag) 
			{
				compositeAlpha = compositeAlphaFlag;
				break;
			};
		}

		VkSwapchainCreateInfoKHR swapchainCI = {};
		swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCI.surface = surface_;
		swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
		swapchainCI.imageFormat = colorFormat_;
		swapchainCI.imageColorSpace = colorSpace_;
		swapchainCI.imageExtent = { swapchainExtent.width, swapchainExtent.height };
		swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
		swapchainCI.imageArrayLayers = 1;
		swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCI.queueFamilyIndexCount = 0;
		swapchainCI.presentMode = swapchainPresentMode;
		// Setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
		swapchainCI.oldSwapchain = oldSwapchain;
		// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface_ area
		swapchainCI.clipped = VK_TRUE;
		swapchainCI.compositeAlpha = compositeAlpha;

		// Enable transfer source on swap chain images if supported
		if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) 
		{
			swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}

		// Enable transfer destination on swap chain images if supported
		if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) 
		{
			swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		VK_CHECK_RESULT(vkCreateSwapchainKHR(device_, &swapchainCI, nullptr, &swapChain_));

		// If an existing swap chain is re-created, destroy the old swap chain and the ressources owned by the application (image views, images are owned by the swap chain)
		if (oldSwapchain != VK_NULL_HANDLE) 
		{ 
			for (auto& image : images_) 
			{
				vkDestroyImageView(device_, image.imageView_, nullptr);
			}
			vkDestroySwapchainKHR(device_, oldSwapchain, nullptr);
		}
		uint32_t imageCount = 0;
		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, nullptr));

		// Get the swap chain images
		std::vector<VkImage> images(imageCount);
		std::vector<VkImageView> imageViews(imageCount);
		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, images.data()));

		// Get the swap chain buffers containing the image and imageview
		for (size_t i = 0; i < images.size(); ++i)
		{
			VkImageViewCreateInfo colorAttachmentView = {};
			colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			colorAttachmentView.pNext = NULL;
			colorAttachmentView.format = colorFormat_;
			colorAttachmentView.components = {
				VK_COMPONENT_SWIZZLE_R,
				VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_B,
				VK_COMPONENT_SWIZZLE_A
			};
			colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorAttachmentView.subresourceRange.baseMipLevel = 0;
			colorAttachmentView.subresourceRange.levelCount = 1;
			colorAttachmentView.subresourceRange.baseArrayLayer = 0;
			colorAttachmentView.subresourceRange.layerCount = 1;
			colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorAttachmentView.flags = 0;
			colorAttachmentView.image = images[i];
			VK_CHECK_RESULT(vkCreateImageView(device_, &colorAttachmentView, nullptr, &imageViews[i]));
		}

		// Fill texture
		images_.resize(imageCount);
		for (size_t i = 0; i < images.size(); ++i) 
		{	
			images_[i].device_ = device_;
			images_[i].image_ = images[i];
			images_[i].imageView_ = imageViews[i];
			images_[i].format_ = colorFormat_;
			images_[i].aspectMask_ = VK_IMAGE_ASPECT_COLOR_BIT;
			images_[i].width_ = swapchainExtent.width;
			images_[i].height_ = swapchainExtent.height;
			images_[i].levelCount_ = 1;
			images_[i].layerCount_ = 1;
			images_[i].isSwapchainImage_ = true;
			images_[i].currentImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
			images_[i].currentImageUsage_ = vks::TextureUsage::Undefined;
		}
	}

	VkResult VulkanSwapChain::acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t& imageIndex)
	{
		// By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
		// With that we don't have to handle VK_NOT_READY
		return vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, &imageIndex);
	}

	VkResult VulkanSwapChain::queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore)
	{
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = NULL;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain_;
		presentInfo.pImageIndices = &imageIndex;
		// Check if a wait semaphore has been specified to wait for before presenting the image
		if (waitSemaphore != VK_NULL_HANDLE)
		{
			presentInfo.pWaitSemaphores = &waitSemaphore;
			presentInfo.waitSemaphoreCount = 1;
		}
		return vkQueuePresentKHR(queue, &presentInfo);
	}

	std::vector<Texture*> VulkanSwapChain::images()
	{
		std::vector<Texture*> swapChainImages(images_.size());
		for (size_t i = 0; i < images_.size(); ++i)
		{
			swapChainImages[i] = &images_[i];
		}

		return swapChainImages;
	}
}