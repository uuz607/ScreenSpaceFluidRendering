/*
* Vulkan Example base class
*
* Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*
* Modifications Copyright (c) 2026 Liu Linqi
*/
#include "FluidSystem.h"
#include "keycodes.hpp"
#include "VulkanTools.h"
#include "VulkanDebug.h"

namespace ssfr
{
	FluidSystem::FluidSystem()
	{
		camera_.type = Camera::CameraType::lookat;
		camera_.setPosition(glm::vec3(0.f, 0.f, -4.f));
		camera_.setRotation(glm::vec3(-45.f, 0.f, 0.f));
		camera_.setPerspective(60.f, (float)width_ / (float)height_, 0.1f, 256.f);

		if (settings_.validation)
		{
			setupConsole("Validation");
		}
	}

	FluidSystem::~FluidSystem()
	{
		for (auto& semaphores : semaphores_) 
		{
			vkDestroySemaphore(device_, semaphores.presentComplete, nullptr);
			vkDestroySemaphore(device_, semaphores.renderComplete, nullptr);
		}

		vkDestroySemaphore(device_, timeLineSemaphore_.handle, nullptr);

		if (settings_.validation)
		{
			vks::debug::freeDebugCallback(instance_);
		}

		if (instance_) 
		{
			vkDestroyInstance(instance_, nullptr);
		}

		delete vulkanDevice_;
	}

	VkResult FluidSystem::createInstance()
	{
		std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

		// Enable surface extensions depending on os
		instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

		// Get extensions supported by the instance and store for later use
		uint32_t extCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
		if (extCount > 0)
		{
			std::vector<VkExtensionProperties> extensions(extCount);
			if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
			{
				for (VkExtensionProperties& extension : extensions)
				{
					supportedInstanceExtensions_.push_back(extension.extensionName);
				}
			}
		}

		// Enabled requested instance extensions
		if (!enabledInstanceExtensions_.empty())
		{
			for (const char * enabledExtension : enabledInstanceExtensions_)
			{
				// Output message if requested extension is not available
				if (std::find(supportedInstanceExtensions_.begin(), supportedInstanceExtensions_.end(), enabledExtension) == supportedInstanceExtensions_.end())
				{
					std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
				}
				instanceExtensions.push_back(enabledExtension);
			}
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = name_.c_str();
		appInfo.pEngineName = name_.c_str();
		appInfo.apiVersion = apiVersion_;

		VkInstanceCreateInfo instanceCreateInfo{};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pApplicationInfo = &appInfo;

		VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
		if (settings_.validation) 
		{
			vks::debug::setupDebugingMessengerCreateInfo(debugUtilsMessengerCI);
			debugUtilsMessengerCI.pNext = instanceCreateInfo.pNext;
			instanceCreateInfo.pNext = &debugUtilsMessengerCI;
		}

		// Enable the debug utils extension if available (e.g. when debugging tools are present)
		if (settings_.validation || std::find(supportedInstanceExtensions_.begin(), supportedInstanceExtensions_.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedInstanceExtensions_.end()) 
		{
			instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		if (!instanceExtensions.empty()) 
		{
			instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
			instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
		}

		// The VK_LAYER_KHRONOS_validation contains all current validation functionality.
		// Note that on Android this layer requires at least NDK r20
		const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
		if (settings_.validation) 
		{
			// Check if this layer is available at instance level
			uint32_t instanceLayerCount;
			vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
			std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
			vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
			bool validationLayerPresent = false;
			for (VkLayerProperties& layer : instanceLayerProperties) 
			{
				if (strcmp(layer.layerName, validationLayerName) == 0) 
				{
					validationLayerPresent = true;
					break;
				}
			}
			if (validationLayerPresent) 
			{
				instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
				instanceCreateInfo.enabledLayerCount = 1;
			} else 
			{
				std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
			}
		}

		// If layer settings_ are defined, then activate the sample's required layer settings_ during instance creation.
		// Layer settings_ are typically used to activate specific features of a layer, such as the Validation Layer's
		// printf feature, or to configure specific capabilities of drivers such as MoltenVK on macOS and/or iOS.
		VkLayerSettingsCreateInfoEXT layerSettingsCreateInfo{VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT};
		if (enabledLayerSettings_.size() > 0) {
			layerSettingsCreateInfo.settingCount = static_cast<uint32_t>(enabledLayerSettings_.size());
			layerSettingsCreateInfo.pSettings = enabledLayerSettings_.data();
			layerSettingsCreateInfo.pNext = instanceCreateInfo.pNext;
			instanceCreateInfo.pNext = &layerSettingsCreateInfo;
		}

		VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance_);

		// If the debug utils extension is present we set up debug functions, so samples can label objects for debugging
		if (std::find(supportedInstanceExtensions_.begin(), supportedInstanceExtensions_.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedInstanceExtensions_.end())
		{
			vks::debugutils::setup(instance_);
		}

		return result;
	}

	void FluidSystem::getEnabledFeatures()
	{
		features13_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		features12_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

        VkPhysicalDeviceFeatures2 features2 = {};
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &features13_;
		features13_.pNext = &features12_;
		
		features13_.synchronization2 = VK_TRUE;
		features12_.timelineSemaphore = VK_TRUE;
		//vkGetPhysicalDeviceFeatures2(physicalDevice_, &features2);

		deviceCreatepNextChain_ = &features13_;
	}

	void FluidSystem::getEnabledExtensions()
	{

	}

	bool FluidSystem::initVulkan() 
	{
		// Create the instance
		VkResult result = createInstance();
		if (result != VK_SUCCESS) 
		{
			vks::tools::exitFatal("Could not create Vulkan instance : \n" + vks::tools::errorString(result), result);
			return false;
		}

		// If requested, we enable the default validation layers for debugging
		if (settings_.validation)
		{
			vks::debug::setupDebugging(instance_);
		}

		// Physical device
		uint32_t gpuCount = 0;
		// Get number of available physical devices
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance_, &gpuCount, nullptr));
		if (gpuCount == 0) 
		{
			vks::tools::exitFatal("No device with Vulkan support found", -1);
			return false;
		}
		// Enumerate devices
		std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
		result = vkEnumeratePhysicalDevices(instance_, &gpuCount, physicalDevices.data());
		if (result != VK_SUCCESS) 
		{
			vks::tools::exitFatal("Could not enumerate physical devices : \n" + vks::tools::errorString(result), result);
			return false;
		}

		// GPU selection

		// Select physical device to be used for the Vulkan example
		// Defaults to the first device unless specified by command line
		uint32_t selectedDevice = 0;

		physicalDevice_ = physicalDevices[selectedDevice];

		// Vulkan device creation
		// This is handled by a separate class that gets a logical device representation
		// and encapsulates functions related to a device
		vulkanDevice_ = new vks::VulkanDevice(physicalDevice_);

		// Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
		getEnabledFeatures();
		// Derived examples can enable extensions based on the list of supported extensions read from the physical device
		getEnabledExtensions();

		result = vulkanDevice_->createLogicalDevice(enabledFeatures_, enabledDeviceExtensions_, deviceCreatepNextChain_);
		if (result != VK_SUCCESS) 
		{
			vks::tools::exitFatal("Could not create Vulkan device: \n" + vks::tools::errorString(result), result);
			return false;
		}
		
		device_ = vulkanDevice_->logicalDevice;

		// Create and initialize the Vulkan rendering context.
		vulkanContext_.createContext(vulkanDevice_);

		// Configure swapchain with Vulkan context dependencies.
		swapChain_.setContext(instance_, physicalDevice_, device_);

		return true;
	}

	void FluidSystem::createSynchronizationPrimitives()
	{
		// Create synchronization objects
		semaphores_.resize(swapChain_.imageCount());
		
		VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
		for (auto& semaphores : semaphores_) 
		{
			VK_CHECK_RESULT(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
			VK_CHECK_RESULT(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));
		}

		VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {};
		semaphoreTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		semaphoreTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		semaphoreTypeInfo.initialValue = timeLineSemaphore_.value; 

		semaphoreCreateInfo.pNext = &semaphoreTypeInfo;
		VK_CHECK_RESULT(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &timeLineSemaphore_.handle));
	}


	// Win32 : Sets up a console window_ and redirects standard output to it
	void FluidSystem::setupConsole(std::string title)
	{
		AllocConsole();
		AttachConsole(GetCurrentProcessId());
		FILE *stream;
		freopen_s(&stream, "CONIN$", "r", stdin);
		freopen_s(&stream, "CONOUT$", "w+", stdout);
		freopen_s(&stream, "CONOUT$", "w+", stderr);
		// Enable flags so we can color the output
		HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD dwMode = 0;
		GetConsoleMode(consoleHandle, &dwMode);
		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(consoleHandle, dwMode);
		SetConsoleTitle(TEXT(title.c_str()));
	}

	HWND FluidSystem::setupWindow(HINSTANCE hinstance, WNDPROC wndproc)
	{
		windowInstance_ = hinstance;

		WNDCLASSEX wndClass{};

		wndClass.cbSize = sizeof(WNDCLASSEX);
		wndClass.style = CS_HREDRAW | CS_VREDRAW;
		wndClass.lpfnWndProc = wndproc;
		wndClass.cbClsExtra = 0;
		wndClass.cbWndExtra = 0;
		wndClass.hInstance = hinstance;
		wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wndClass.lpszMenuName = NULL;
		wndClass.lpszClassName = name_.c_str();
		wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

		if (!RegisterClassEx(&wndClass))
		{
			std::cout << "Could not register window_ class!\n";
			fflush(stdout);
			exit(1);
		}

		int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);

		if (settings_.fullscreen)
		{
			if ((width_ != (uint32_t)screenWidth) && (height_ != (uint32_t)screenHeight))
			{
				DEVMODE dmScreenSettings;
				memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
				dmScreenSettings.dmSize       = sizeof(dmScreenSettings);
				dmScreenSettings.dmPelsWidth  = width_;
				dmScreenSettings.dmPelsHeight = height_;
				dmScreenSettings.dmBitsPerPel = 32;
				dmScreenSettings.dmFields     = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
				if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
				{
					if (MessageBox(NULL, "Fullscreen Mode not supported!\n Switch to window_ mode?", "Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
					{
						settings_.fullscreen = false;
					}
					else
					{
						return nullptr;
					}
				}
				screenWidth = width_;
				screenHeight = height_;
			}

		}

		DWORD dwExStyle;
		DWORD dwStyle;

		if (settings_.fullscreen)
		{
			dwExStyle = WS_EX_APPWINDOW;
			dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
		}
		else
		{
			dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
			dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
		}

		RECT windowRect = {
			0L,
			0L,
			settings_.fullscreen ? (long)screenWidth : (long)width_,
			settings_.fullscreen ? (long)screenHeight : (long)height_
		};

		AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

		std::string windowTitle = getWindowTitle();
		window_ = CreateWindowEx(0,
			name_.c_str(),
			windowTitle.c_str(),
			dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
			0,
			0,
			windowRect.right - windowRect.left,
			windowRect.bottom - windowRect.top,
			NULL,
			NULL,
			hinstance,
			NULL);

		if (!window_)
		{
			std::cerr << "Could not create window_!\n";
			fflush(stdout);
			return nullptr;
		}

		if (!settings_.fullscreen)
		{
			// Center on screen
			uint32_t x = (GetSystemMetrics(SM_CXSCREEN) - windowRect.right) / 2;
			uint32_t y = (GetSystemMetrics(SM_CYSCREEN) - windowRect.bottom) / 2;
			SetWindowPos(window_, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		}

		ShowWindow(window_, SW_SHOW);
		SetForegroundWindow(window_);
		SetFocus(window_);

		return window_;
	}

	std::string FluidSystem::getWindowTitle() const
	{
		std::string windowTitle(title_);
		if (!settings_.overlay)
		{
			windowTitle += " - " + std::to_string(frameCounter_) + " fps";
		}
		return windowTitle;
	}

	void FluidSystem::handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_CLOSE:
			prepared_ = false;
			DestroyWindow(hWnd);
			PostQuitMessage(0);
			break;
		case WM_PAINT:
			ValidateRect(window_, NULL);
			break;
		case WM_KEYDOWN:
			switch (wParam)
			{
			case KEY_P:
				paused_ = !paused_;
				break;
			case KEY_F2:
				if (camera_.type == Camera::CameraType::lookat)
				{
					camera_.type = Camera::CameraType::firstperson;
				}
				else 
				{
					camera_.type = Camera::CameraType::lookat;
				}
				break;
			case KEY_ESCAPE:
				PostQuitMessage(0);
				break;
			}

			if (camera_.type == Camera::firstperson)
			{
				switch (wParam)
				{
				case KEY_W:
					camera_.keys.up = true;
					break;
				case KEY_S:
					camera_.keys.down = true;
					break;
				case KEY_A:
					camera_.keys.left = true;
					break;
				case KEY_D:
					camera_.keys.right = true;
					break;
				}
			}

			keyPressed((uint32_t)wParam);
			break;
		case WM_KEYUP:
			if (camera_.type == Camera::firstperson)
			{
				switch (wParam)
				{
				case KEY_W:
					camera_.keys.up = false;
					break;
				case KEY_S:
					camera_.keys.down = false;
					break;
				case KEY_A:
					camera_.keys.left = false;
					break;
				case KEY_D:
					camera_.keys.right = false;
					break;
				}
			}
			break;
		case WM_LBUTTONDOWN:
			mouseState_.position = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
			mouseState_.buttons.left = true;
			break;
		case WM_RBUTTONDOWN:
			mouseState_.position = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
			mouseState_.buttons.right = true;
			break;
		case WM_MBUTTONDOWN:
			mouseState_.position = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
			mouseState_.buttons.middle = true;
			break;
		case WM_LBUTTONUP:
			mouseState_.buttons.left = false;
			break;
		case WM_RBUTTONUP:
			mouseState_.buttons.right = false;
			break;
		case WM_MBUTTONUP:
			mouseState_.buttons.middle = false;
			break;
		case WM_MOUSEWHEEL:
		{
			short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			camera_.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f));
			viewUpdated_ = true;
			break;
		}
		case WM_MOUSEMOVE:
		{
			handleMouseMove(LOWORD(lParam), HIWORD(lParam));
			break;
		}
		case WM_SIZE:
			if ((prepared_) && (wParam != SIZE_MINIMIZED))
			{
				if ((resizing_) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
				{
					destWidth_ = LOWORD(lParam);
					destHeight_ = HIWORD(lParam);
					windowResize();
				}
			}
			break;
		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
			minMaxInfo->ptMinTrackSize.x = 64;
			minMaxInfo->ptMinTrackSize.y = 64;
			break;
		}
		case WM_ENTERSIZEMOVE:
			resizing_ = true;
			break;
		case WM_EXITSIZEMOVE:
			resizing_ = false;
			break;
		}

		OnHandleMessage(hWnd, uMsg, wParam, lParam);
	}

	void FluidSystem::handleMouseMove(int32_t x, int32_t y)
	{
		int32_t dx = (int32_t)mouseState_.position.x - x;
		int32_t dy = (int32_t)mouseState_.position.y - y;

		bool handled = false;

		mouseMoved((float)x, (float)y, handled);

		if (handled) 
		{
			mouseState_.position = glm::vec2((float)x, (float)y);
			return;
		}

		if (mouseState_.buttons.left) 
		{
			camera_.rotate(glm::vec3(dy * camera_.rotationSpeed, -dx * camera_.rotationSpeed, 0.0f));
			viewUpdated_ = true;
		}
		if (mouseState_.buttons.right) 
		{
			camera_.translate(glm::vec3(-0.0f, 0.0f, dy * .005f));
			viewUpdated_ = true;
		}
		if (mouseState_.buttons.middle) 
		{
			camera_.translate(glm::vec3(-dx * 0.005f, -dy * 0.005f, 0.0f));
			viewUpdated_ = true;
		}
		mouseState_.position = glm::vec2((float)x, (float)y);
	}

	void FluidSystem::prepare()
	{
		swapChain_.initSurface(windowInstance_, window_);

		swapChain_.create(width_, height_, settings_.vsync, settings_.fullscreen);

		createSynchronizationPrimitives();

		// Create one command buffer for each swap chain image
		drawCmdLists_.resize(swapChain_.imageCount());
		for(auto& cmdList : drawCmdLists_)
		{
			vulkanContext_.createCommandList(cmdList, false);
		}
		
		simulateCmdLists_.resize(swapChain_.imageCount());
		for(auto& cmdList : simulateCmdLists_)
		{
			vulkanContext_.createCommandList(cmdList, false, vks::QueueType::Compute);
		}

		// Initialize fluid simulation subsystem with Vulkan context,
		// allocating GPU buffers and compute pipelines required for particle updates.
		simulator_.create(&vulkanContext_);

		// Initialize fluid rendering subsystem using Vulkan context and swapchain images,
		// creating graphics pipelines and render targets for surface reconstruction and shading.
		renderer_.create(&vulkanContext_, swapChain_.images());

		// Package simulation output resources to be consumed by the renderer.
		RendererResources renderResources = { simulator_.particlePosition() };
		renderer_.setResources(renderResources);

		prepared_ = true;
	}

	void FluidSystem::renderLoop()
	{
		if (benchmark_.active) 
		{
			benchmark_.run([this] { render(); }, vulkanDevice_->properties);
			vkDeviceWaitIdle(device_);
			if (!benchmark_.filename.empty()) 
			{
				benchmark_.saveResults();
			}
			return;
		}

		destWidth_ = width_;
		destHeight_ = height_;
		
		lastTimestamp_ = std::chrono::high_resolution_clock::now();
		tPrevEnd_ = lastTimestamp_;

		MSG msg;
		bool quitMessageReceived = false;
		while (!quitMessageReceived) 
		{
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (msg.message == WM_QUIT) 
				{
					quitMessageReceived = true;
					break;
				}
			}

			if (prepared_ && !IsIconic(window_)) 
			{
				nextFrame();
			}
		}

		// Flush device to make sure all resources can be freed
		if (device_ != VK_NULL_HANDLE) 
		{
			vkDeviceWaitIdle(device_);
		}
	}

	void FluidSystem::nextFrame()
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		if (viewUpdated_)
		{
			viewUpdated_ = false;
		}

		render();
		frameCounter_++;
		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		frameTimer_ = (float)tDiff / 1000.0f;
		camera_.update(frameTimer_);
		if (camera_.moving())
		{
			viewUpdated_ = true;
		}
		// Convert to clamped timer_ value
		if (!paused_)
		{
			timer_ += timerSpeed_ * frameTimer_;
			if (timer_ > 1.0)
			{
				timer_ -= 1.0f;
			}
		}
		float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - lastTimestamp_).count());
		if (fpsTimer > 1000.0f)
		{
			lastFPS_ = static_cast<uint32_t>((float)frameCounter_ * (1000.0f / fpsTimer));

			if (!settings_.overlay)	{
				std::string windowTitle = getWindowTitle();
				SetWindowText(window_, windowTitle.c_str());
			}

			frameCounter_ = 0;
			lastTimestamp_ = tEnd;
		}
		tPrevEnd_ = tEnd;
	}

	void FluidSystem::windowResize()
	{
		if (!prepared_) return;

		prepared_ = false;

		// Ensure all operations on the device have been finished before destroying resources
		vkDeviceWaitIdle(device_);

		// Recreate swap chain
		width_ = destWidth_;
		height_ = destHeight_;
		swapChain_.create(width_, height_, settings_.vsync, settings_.fullscreen);

		// Recreate the frame buffers
		renderer_.onWindowResize(width_, height_);

		// Update resources to be consumed by the renderer.
		RendererResources renderResources = { simulator_.particlePosition() };
		renderer_.setResources(renderResources);

		if ((width_ > 0.0f) && (height_ > 0.0f)) 
		{
			camera_.updateAspectRatio((float)width_ / (float)height_);
		}

		prepared_ = true;
	}

	void FluidSystem::prepareFrame()
	{
		// Acquire the next image from the swap chain
		VkResult result = swapChain_.acquireNextImage(semaphores_[frameIndex_].presentComplete, imageIndex_);
		// Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE)
		// SRS - If no longer optimal (VK_SUBOPTIMAL_KHR), wait until submitFrame() in case number of swapchain images will change on resize
		if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) 
		{
			if (result == VK_ERROR_OUT_OF_DATE_KHR)
			{
				windowResize();
			}
			return;
		}
		else 
		{
			VK_CHECK_RESULT(result);
		}
	}

	void FluidSystem::submitFrame()
	{
		VkResult result = swapChain_.queuePresent(vulkanContext_.graphicsQueue(), imageIndex_, semaphores_[frameIndex_].renderComplete);
		// Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
		if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) 
		{
			windowResize();
			if (result == VK_ERROR_OUT_OF_DATE_KHR) 
			{
				return;
			}
		}
		else 
		{
			VK_CHECK_RESULT(result);
		}
		//VK_CHECK_RESULT(vkQueueWaitIdle(vulkanContext_.graphicsQueue()));
	}

	void FluidSystem::waitForAvailableFrame()
	{
		const uint64_t signalsPerFrame = 2; 
		const uint64_t maxTimelineLag = signalsPerFrame * (swapChain_.imageCount() - 1);

		uint64_t currentValue = timeLineSemaphore_.value;

		if (currentValue <= maxTimelineLag) return;

		uint64_t waitValue = currentValue - maxTimelineLag;

		VkSemaphoreWaitInfo waitInfo = {};
		waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
		waitInfo.semaphoreCount = 1;
		waitInfo.pSemaphores = &timeLineSemaphore_.handle;
		waitInfo.pValues = &waitValue;

		VK_CHECK_RESULT(vkWaitSemaphores(device_, &waitInfo, UINT64_MAX));
	}

	void FluidSystem::render()
	{
		if (!prepared_) return;

		// Update graphics uniform buffers
		renderer_.updateUniformBuffers(camera_);

		waitForAvailableFrame();

		const uint64_t renderFinished = timeLineSemaphore_.value;
		const uint64_t simulateFinished = timeLineSemaphore_.value + 1;
		const uint64_t allFinished = timeLineSemaphore_.value + 2;

		// Record compute commands
		simulateCmdLists_[frameIndex_].beginCommandBuffer();
		simulator_.run(simulateCmdLists_[frameIndex_]);
		simulateCmdLists_[frameIndex_].endCommandBuffer();

		// Submit compute commands
		VkSemaphoreSubmitInfo simulateWaitInfo = {};
		simulateWaitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		simulateWaitInfo.semaphore = timeLineSemaphore_.handle;
		simulateWaitInfo.value = renderFinished;
		simulateWaitInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

		VkSemaphoreSubmitInfo simulateSignalInfo = {};
		simulateSignalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		simulateSignalInfo.semaphore = timeLineSemaphore_.handle;
		simulateSignalInfo.value = simulateFinished;
		simulateSignalInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

		VkCommandBufferSubmitInfo simulateCmdInfo = {};
		simulateCmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		simulateCmdInfo.commandBuffer = simulateCmdLists_[frameIndex_].handle();

		VkSubmitInfo2 simulateSubmitInfo = {};
		simulateSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
		simulateSubmitInfo.waitSemaphoreInfoCount = 1;
		simulateSubmitInfo.pWaitSemaphoreInfos = &simulateWaitInfo;
		simulateSubmitInfo.commandBufferInfoCount = 1;
		simulateSubmitInfo.pCommandBufferInfos = &simulateCmdInfo;
		simulateSubmitInfo.signalSemaphoreInfoCount = 1;
		simulateSubmitInfo.pSignalSemaphoreInfos = &simulateSignalInfo;

		VK_CHECK_RESULT(vkQueueSubmit2(vulkanContext_.computeQueue(), 1, &simulateSubmitInfo, VK_NULL_HANDLE));

		// Record graphics commands
		prepareFrame();
		
		drawCmdLists_[frameIndex_].beginCommandBuffer();
		renderer_.draw(drawCmdLists_[frameIndex_], imageIndex_);
		drawCmdLists_[frameIndex_].endCommandBuffer();

		// Submit graphics commands
		VkSemaphoreSubmitInfo renderWaitInfos[2] = {};
		renderWaitInfos[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		renderWaitInfos[0].semaphore = timeLineSemaphore_.handle;
		renderWaitInfos[0].value = simulateFinished;
		renderWaitInfos[0].stageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;

		renderWaitInfos[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		renderWaitInfos[1].semaphore = semaphores_[frameIndex_].presentComplete;
		renderWaitInfos[1].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSemaphoreSubmitInfo renderSignalInfos[2] = {};
		renderSignalInfos[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		renderSignalInfos[0].semaphore = timeLineSemaphore_.handle;
		renderSignalInfos[0].value = allFinished;
		renderSignalInfos[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		
		renderSignalInfos[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		renderSignalInfos[1].semaphore = semaphores_[frameIndex_].renderComplete;
		renderSignalInfos[1].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		
		VkCommandBufferSubmitInfo renderCmdInfo = {};
		renderCmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		renderCmdInfo.commandBuffer = drawCmdLists_[frameIndex_].handle();

		VkSubmitInfo2 renderSubmitInfo = {};
		renderSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
		renderSubmitInfo.waitSemaphoreInfoCount = 2;
		renderSubmitInfo.pWaitSemaphoreInfos = renderWaitInfos;
		renderSubmitInfo.commandBufferInfoCount = 1;
		renderSubmitInfo.pCommandBufferInfos = &renderCmdInfo;
		renderSubmitInfo.signalSemaphoreInfoCount = 2;
		renderSubmitInfo.pSignalSemaphoreInfos = renderSignalInfos;

		VK_CHECK_RESULT(vkQueueSubmit2(vulkanContext_.graphicsQueue(), 1, &renderSubmitInfo, VK_NULL_HANDLE));

		submitFrame(); 

		// Increase timeline value base for next frame
		timeLineSemaphore_.value = allFinished;
		frameIndex_ = (frameIndex_ + 1) % swapChain_.imageCount();
	}
}