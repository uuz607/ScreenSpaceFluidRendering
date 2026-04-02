/*
* Vulkan Example base class
*
* Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*
*/
//Modified by Liu Linqi 2026

#pragma once

#ifdef _WIN32
#pragma comment(linker, "/subsystem:windows")
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <ShellScalingAPI.h>
#endif

#include <assert.h>
#include <vector>

#include <chrono>
#include <sys/stat.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>

#include "VulkanSwapChain.h"
#include "VulkanDevice.h"

#include "camera.hpp"
#include "benchmark.hpp"

#include "Constants.h"
#include "VulkanContext.h"
#include "VulkanCommand.h"
#include "VulkanSwapChain.h"
#include "Renderer.h"
#include "Simulator.h"

namespace ssfr 
{
    class FluidSystem
    {
    public:
        FluidSystem();
        ~FluidSystem();

        /** @brief Setup the vulkan instance, enable required extensions and connect to the physical device (GPU) */
        bool initVulkan();

        HWND setupWindow(HINSTANCE hinstance, WNDPROC wndproc);

        /** @brief Prepares all Vulkan resources and functions required to run the sample */
        void prepare();

        /** @brief Entry point for the main render loop */
        void renderLoop();

        void handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    private:
        /** @brief Creates the application wide Vulkan instance */
        VkResult createInstance();

        void createSynchronizationPrimitives();

        void waitForAvailableFrame();

        /** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
        void getEnabledFeatures();
        /** @brief (Virtual) Called after the physical device extensions have been read, can be used to enable extensions based on the supported extension listing*/
        void getEnabledExtensions();

        void windowResize();

        /** @brief Render function to be implemented by the sample application */
        void render();

        /** Prepare the next frame for workload submission by acquiring the next swap chain image */
        void prepareFrame();

        /** @brief Presents the current image to the swap chain */
        void submitFrame();

        void nextFrame();

        void setupConsole(std::string title);
        
        void handleMouseMove(int32_t x, int32_t y);

        void OnHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {}

        /** @brief Called after a key was pressed, can be used to do custom key handling */
        void keyPressed(uint32_t) {}

        /** @brief Called after the mouse cursor moved and before internal events (like camera_ rotation) is handled */
        void mouseMoved(double x, double y, bool &handled) {}

        std::string getWindowTitle() const;

    private:
        bool prepared_ = false;
        bool resized_ = false;
        bool viewUpdated_ = false;
        bool resizing_ = false;

        /** @brief Last frame time measured using a high performance timer (if available) */
        float frameTimer_ = 1.0f;
        // Frame counter to display fps
        uint32_t frameCounter_ = 0;
        uint32_t lastFPS_ = 0;
        std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp_, tPrevEnd_;

        vks::Benchmark benchmark_;

        /** @brief Example settings_ that can be changed e.g. by command line arguments */
        struct Settings 
        {
            /** @brief Activates validation layers (and message output) when set to true */
            bool validation = false;
            /** @brief Set to true if fullscreen mode has been requested via command line */
            bool fullscreen = false;
            /** @brief Set to true if v-sync will be forced for the swapchain */
            bool vsync = false;
            /** @brief Enable UI overlay */
            bool overlay = false;
        } settings_;


        /** @brief State of mouse/touch input */
        struct {
            struct {
                bool left = false;
                bool right = false;
                bool middle = false;
            } buttons;
            glm::vec2 position;
        } mouseState_;

        // Defines a frame rate independent timer_ value clamped from -1.0...1.0
        // For use in animations, rotations, etc.
        float timer_ = 0.0f;
        // Multiplier for speeding up (or slowing down) the global timer
        float timerSpeed_ = 0.25f;
        bool paused_ = false;

        Camera camera_;

        std::string title_ = "Vulkan Example";
        std::string name_ = "vulkanExample";
        uint32_t apiVersion_ = VK_API_VERSION_1_3;

        // OS specific
        HWND window_;
        HINSTANCE windowInstance_;

    private:
        uint32_t width_ = WindowWidth;
        uint32_t height_ = WindowHeight;
        uint32_t destWidth_ = 0;
	    uint32_t destHeight_ = 0;

        // Active frame buffer index
	    uint32_t imageIndex_ = 0;
        uint32_t frameIndex_ = 0;

        // Vulkan instance, stores all per-application states
        VkInstance instance_ = VK_NULL_HANDLE;;
        std::vector<std::string> supportedInstanceExtensions_;

        // Physical device (GPU) that Vulkan will use
	    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

        /** @brief Encapsulated physical and logical vulkan device */
        vks::VulkanDevice* vulkanDevice_ = nullptr;
        	
        /** @brief Logical device, application's view of the physical device (GPU) */
	    VkDevice device_ = VK_NULL_HANDLE;

        /** @brief Set of physical device features to be enabled for this example */
        VkPhysicalDeviceFeatures enabledFeatures_ = {};
        VkPhysicalDeviceVulkan12Features features12_ = {};
        VkPhysicalDeviceVulkan13Features features13_ = {};

        /** @brief Set of device extensions to be enabled for this example */
        std::vector<const char*> enabledDeviceExtensions_;
        
        /** @brief Set of instance extensions to be enabled for this example */
        std::vector<const char*> enabledInstanceExtensions_;
        
        /** @brief Set of layer settings_ to be enabled for this example */
        std::vector<VkLayerSettingEXT> enabledLayerSettings_;
        
        /** @brief Optional pNext structure for passing extension structures to device creation */
	    void* deviceCreatepNextChain_ = nullptr;

        /**  @brief Application-level Vulkan context managing initialization of core Vulkan objects (texture, buffer, queues, etc.) */
        vks::VulkanContext vulkanContext_;

        // Wraps the swap chain to present images (framebuffers) to the windowing system
        vks::VulkanSwapChain swapChain_;

        // Synchronization semaphores
        struct Semaphores 
        {
            // Swap chain image presentation
            VkSemaphore presentComplete = VK_NULL_HANDLE;
            // Command buffer submission and execution
            VkSemaphore renderComplete = VK_NULL_HANDLE;
        };
        std::vector<Semaphores> semaphores_;

        struct TimeLineSemaphore 
        {
            VkSemaphore handle = VK_NULL_HANDLE;
            uint64_t value = 0;
        } timeLineSemaphore_;

        // Command buffers used for rendering passes (graphics queue submission)
        std::vector<vks::CommandList> drawCmdLists_;
        // Compute command lists recorded per frame for physics simulation and GPU compute stages
        std::vector<vks::CommandList> simulateCmdLists_;

        // Rendering subsystem responsible for graphics pipeline execution,
        // including shading, post-processing and frame composition
        Renderer renderer_;

        // Simulation subsystem handling physics updates (PBF),
        // particle integration and related compute workloads
        Simulator simulator_;
    };
}
