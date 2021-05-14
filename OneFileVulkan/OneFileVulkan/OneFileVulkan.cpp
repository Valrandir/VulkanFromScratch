#pragma warning(disable : 26812)

#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include "vulkan.h"

#include <iostream>
#include <tchar.h>
#include <vector>

#ifdef UNICODE
#define TCERR std::wcerr
#else
#define TCERR std::cerr
#endif

__declspec(noreturn) void Abort(LPCTSTR message);
__declspec(noreturn) void Win32Abort(LPCTSTR message);
__declspec(noreturn) void VkAbort(VkResult result, LPCTSTR fn);

#define APPLICATION_NAME "OneFileVulkan"

#ifdef NDEBUG
#define ASSERT(fn)
#define WIN32_ASSERT(fn) fn
#define VK_ASSERT(fn) fn
#else
#define ASSERT(fn) \
	if(!(fn)) \
	Abort(TEXT(#fn))

#define WIN32_ASSERT(fn) \
	if(!(fn)) \
	Win32Abort(TEXT(#fn))

#define VK_ASSERT(fn) \
	{ \
		VkResult result; \
		if(VK_SUCCESS != (result = (fn))) \
			VkAbort(result, TEXT(#fn)); \
	}
#endif

#define VK_LOAD_FROM_MODULE(hmodule, fn) \
	PFN_##fn fn = (PFN_##fn)GetProcAddress(hmodule, #fn); \
	if(!fn) \
	Win32Abort(TEXT("GetProcAddress failed for " #fn))

#define VK_LOAD_FROM_VULKAN(fn) \
	PFN_##fn fn = (PFN_##fn)vkGetInstanceProcAddr(nullptr, #fn); \
	if(!fn) \
	Abort(TEXT("vkGetInstanceProcAddr failed for " #fn))

#define VK_LOAD_FROM_VULKAN_INSTANCE(instance, fn) \
	PFN_##fn fn = (PFN_##fn)vkGetInstanceProcAddr(instance, #fn); \
	if(!fn) \
	Abort(TEXT("vkGetInstanceProcAddr failed for " #fn))

#define VK_LOAD_FROM_VULKAN_DEVICE(device, fn) \
	PFN_##fn fn = (PFN_##fn)vkGetDeviceProcAddr(device, #fn); \
	if(!fn) \
	Abort(TEXT("vkGetDeviceProcAddr failed for " #fn))

__declspec(noreturn) void Abort(LPCTSTR message)
{
	TCERR << message << std::endl;
	ExitProcess(1);
}

__declspec(noreturn) void Win32Abort(LPCTSTR message)
{
	DWORD last_error = GetLastError();
	LPTSTR buffer;
	DWORD size_in_tchar = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, last_error, 0, (LPTSTR)&buffer, 0, 0);

	if(size_in_tchar) {
		TCERR << message << std::endl
		      << buffer << "std::endl";
		LocalFree(buffer);
	}

	ExitProcess(last_error);
}

__declspec(noreturn) void VkAbort(VkResult result, LPCTSTR message)
{
	TCERR << TEXT("Vulkan operation failed") << std::endl;
	TCERR << message << std::endl;
	ExitProcess(result);
}

int64_t GetTick()
{
	static LARGE_INTEGER frequency;
	LARGE_INTEGER counter;

	if(!frequency.QuadPart) {
		QueryPerformanceFrequency(&frequency); //counts per second
		frequency.QuadPart /= 1000; //counts per milliseconds
	}

	QueryPerformanceCounter(&counter);

	return counter.QuadPart / frequency.QuadPart;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
		case WM_KEYDOWN:
			if(wParam == VK_ESCAPE) {
				PostQuitMessage(0);
				return 0;
			}
			break;
		case WM_CLOSE:
			PostQuitMessage(0);
			return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

struct Window {
	HINSTANCE hInstance;
	HWND hWnd;
	int width, height;
};

Window CreateWnd(int width, int height)
{
	LPCTSTR CLASS_NAME = TEXT(APPLICATION_NAME);
	HINSTANCE hInstance = GetModuleHandle(NULL);
	WNDCLASS wc{sizeof(wc)};

	if(!GetClassInfo(hInstance, CLASS_NAME, &wc)) {
		wc.lpfnWndProc = WndProc;
		wc.hInstance = hInstance;
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
		wc.lpszClassName = CLASS_NAME;
		WIN32_ASSERT(RegisterClass(&wc));
	}

	DWORD style = WS_BORDER | WS_CAPTION | WS_POPUP | WS_SYSMENU;
	RECT rect{0, 0, width, height};
	AdjustWindowRect(&rect, style, FALSE);

	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;
	int x = (GetSystemMetrics(SM_CXSCREEN) - w) >> 1;
	int y = (GetSystemMetrics(SM_CYSCREEN) - h) >> 1;
	HWND hWnd;
	WIN32_ASSERT(hWnd = CreateWindow(CLASS_NAME, CLASS_NAME, style, x, y, w, h, HWND_DESKTOP, FALSE, hInstance, NULL));

	return {hInstance, hWnd, width, height};
}

void WndShow(const Window& window)
{
	ShowWindow(window.hWnd, SW_SHOWNORMAL);
}

bool WndLoop()
{
	MSG msg;
	while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
		DispatchMessage(&msg);
	}

	return msg.message != WM_QUIT;
}

void OneFileVulkan()
{
	HMODULE vulkan = LoadLibrary(TEXT("vulkan-1.dll"));
	if(!vulkan)
		return;

	VK_LOAD_FROM_MODULE(vulkan, vkGetInstanceProcAddr);
	VK_LOAD_FROM_VULKAN(vkEnumerateInstanceExtensionProperties);
	VK_LOAD_FROM_VULKAN(vkCreateInstance);

	//Check available extensions
	std::vector<const char*> instanceExtensions = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
	{
		uint32_t propertyCount;
		VK_ASSERT(vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, nullptr));
		ASSERT(propertyCount > 0);
		std::vector<VkExtensionProperties> properties(propertyCount);
		VK_ASSERT(vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, &properties.front()));
		auto checkExtension = [&](const char* extensionName) -> bool { return properties.cend() != std::find_if(properties.cbegin(), properties.cend(), [&](auto& p) -> bool { return strcmp(extensionName, p.extensionName) == 0; }); };
		for(auto& de : instanceExtensions) {
			auto extensionPresent = checkExtension(de);
			ASSERT(extensionPresent);
		}
	}

	//Create Vulkan Instance
	VkApplicationInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	ai.pApplicationName = APPLICATION_NAME;
	ai.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	ai.pEngineName = ai.pApplicationName;
	ai.engineVersion = ai.applicationVersion;
	ai.apiVersion = VK_VERSION_1_2;
	VkInstanceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pApplicationInfo = &ai;
	ci.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	ci.ppEnabledExtensionNames = &instanceExtensions.front();
	VkInstance instance;
	VK_ASSERT(vkCreateInstance(&ci, nullptr, &instance));
	ASSERT(instance);

	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkEnumeratePhysicalDevices);
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkGetPhysicalDeviceProperties2);
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkGetPhysicalDeviceQueueFamilyProperties2);
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkGetPhysicalDeviceSurfaceSupportKHR);
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkCreateDevice);
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkGetDeviceProcAddr);
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkDestroyInstance);

	//Get First Physical Device
	uint32_t physicalDeviceCount;
	VK_ASSERT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));
	ASSERT(physicalDeviceCount > 0);
	auto physicalDevices = new VkPhysicalDevice[physicalDeviceCount];
	VK_ASSERT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));
	VkPhysicalDevice physicalDevice = physicalDevices[0];
	delete[] physicalDevices;

	//Check physical device available extensions
	std::vector<const char*> physicalDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	{
		VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkEnumerateDeviceExtensionProperties);
		uint32_t propertyCount;
		VK_ASSERT(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &propertyCount, nullptr));
		ASSERT(propertyCount > 0);
		std::vector<VkExtensionProperties> properties(propertyCount);
		VK_ASSERT(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &propertyCount, &properties.front()));
		auto checkExtension = [&](const char* extensionName) -> bool { return properties.cend() != std::find_if(properties.cbegin(), properties.cend(), [&](auto& p) -> bool { return strcmp(extensionName, p.extensionName) == 0; }); };
		for(auto& de : physicalDeviceExtensions)
			ASSERT(checkExtension(de));
	}

	//Create a win32 window
	auto wnd = CreateWnd(640, 480);

	//Create a win32 surface
	VkSurfaceKHR surface;
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkCreateWin32SurfaceKHR);
	{
		VkWin32SurfaceCreateInfoKHR sci = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
		sci.hinstance = wnd.hInstance;
		sci.hwnd = wnd.hWnd;
		VK_ASSERT(vkCreateWin32SurfaceKHR(instance, &sci, nullptr, &surface));
		ASSERT(surface);
	}

	//Find Queue Family Index for VK_QUEUE_GRAPHICS_BIT and for vkGetPhysicalDeviceSurfaceSupportKHR
	uint32_t graphicsQueueFamilyIndex = INT_MAX;
	uint32_t presentQueueFamilyIndex = INT_MAX;
	{
		uint32_t queueFamilyPropertyCount;
		vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertyCount, nullptr);
		ASSERT(queueFamilyPropertyCount > 0);
		std::vector<VkQueueFamilyProperties2> queueFamilyProperties(queueFamilyPropertyCount, {VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2});
		vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertyCount, &queueFamilyProperties.front());
		bool supportGraphics;
		VkBool32 supportKHR;
		for(uint32_t i = 0; i < queueFamilyPropertyCount; ++i) {
			auto& qfp = queueFamilyProperties[i].queueFamilyProperties;
			supportGraphics = qfp.queueCount > 0 && qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT;
			VK_ASSERT(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportKHR));
			if(supportGraphics && supportKHR) {
				graphicsQueueFamilyIndex = presentQueueFamilyIndex = i;
				break;
			}
			if(supportGraphics && graphicsQueueFamilyIndex == INT_MAX)
				graphicsQueueFamilyIndex = i;
			if(supportKHR && presentQueueFamilyIndex == INT_MAX)
				presentQueueFamilyIndex = i;
		}
		ASSERT(graphicsQueueFamilyIndex != INT_MAX && "Found no queue with VK_QUEUE_GRAPHICS_BIT");
		ASSERT(presentQueueFamilyIndex != INT_MAX && "Found no queue with vkGetPhysicalDeviceSurfaceSupportKHR");
	}

	//Create Logical Device And Device Queue
	VkDevice device;
	VkQueue graphicsDeviceQueue, presentDeviceQueue;
	{
		float queue_priorities = 1.0f;
		std::vector<VkDeviceQueueCreateInfo> qcis(2, {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO});
		qcis[0].queueFamilyIndex = graphicsQueueFamilyIndex;
		qcis[0].queueCount = 1;
		qcis[0].pQueuePriorities = &queue_priorities;
		qcis[1].queueFamilyIndex = presentQueueFamilyIndex;
		qcis[1].queueCount = 1;
		qcis[1].pQueuePriorities = &queue_priorities;
		VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
		dci.queueCreateInfoCount = graphicsQueueFamilyIndex == presentQueueFamilyIndex ? 1 : 2;
		dci.pQueueCreateInfos = &qcis.front();
		dci.enabledExtensionCount = 1;
		auto deviceExtensions = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		dci.ppEnabledExtensionNames = &deviceExtensions;
		VK_ASSERT(vkCreateDevice(physicalDevice, &dci, nullptr, &device));
		ASSERT(device);
	}
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkGetDeviceQueue);
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkDeviceWaitIdle);
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkDestroyDevice);
	vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsDeviceQueue);
	ASSERT(graphicsDeviceQueue);
	vkGetDeviceQueue(device, presentQueueFamilyIndex, 0, &presentDeviceQueue);
	ASSERT(presentDeviceQueue);

	//Create swap chain image semaphore
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderingFinishedSemaphore;
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkCreateSemaphore);
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkDestroySemaphore);
	{
		VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		VK_ASSERT(vkCreateSemaphore(device, &sci, nullptr, &imageAvailableSemaphore));
		VK_ASSERT(vkCreateSemaphore(device, &sci, nullptr, &renderingFinishedSemaphore));
		ASSERT(imageAvailableSemaphore);
		ASSERT(renderingFinishedSemaphore);
	}

	//Check Surface Capabilities
	uint32_t swapChainImageCount = 2;
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
	VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));
	ASSERT(surfaceCapabilities.minImageCount <= swapChainImageCount && surfaceCapabilities.maxImageCount >= swapChainImageCount && "surfaceCapabilities.minImageCount / .maxImageCount");
	ASSERT(surfaceCapabilities.maxImageArrayLayers > 0 && "surfaceCapabilities.maxImageArrayLayers is not > 0");
	ASSERT((surfaceCapabilities.supportedUsageFlags & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) && "surfaceCapabilities.supportedUsageFlags do not include VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT and VK_IMAGE_USAGE_TRANSFER_DST_BIT");
	ASSERT(surfaceCapabilities.currentTransform & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR && "surfaceCapabilities.currentTransform flag does not include VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR");
	ASSERT(surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR && "surfaceCapabilities.supportedCompositeAlpha flag does not include VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR");

	//Check Supported Surface Formats
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkGetPhysicalDeviceSurfaceFormatsKHR);
	uint32_t surfaceFormatCount;
	VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr));
	ASSERT(surfaceFormatCount > 0);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
	VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, &surfaceFormats.front()));
	ASSERT(surfaceFormats.cend() != std::find_if(surfaceFormats.cbegin(), surfaceFormats.cend(), [](auto& sf) -> bool { return sf.format == VK_FORMAT_B8G8R8A8_UNORM; }) && "Found no surface format VK_FORMAT_B8G8R8A8_UNORM");

	//Find Supported Present Modes
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkGetPhysicalDeviceSurfacePresentModesKHR);
	uint32_t presentModeCount;
	VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));
	ASSERT(presentModeCount > 0);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, &presentModes.front()));
	ASSERT(presentModes.cend() != std::find_if(presentModes.cbegin(), presentModes.cend(), [](auto& pm) -> bool { return pm == VK_PRESENT_MODE_FIFO_KHR; }) && "Found no present mode VK_PRESENT_MODE_FIFO_KHR");

	//Create swap chain
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkCreateSwapchainKHR);
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkDestroySwapchainKHR);
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkGetSwapchainImagesKHR);
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages(swapChainImageCount);
	{
		VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
		ci.surface = surface;
		ci.minImageCount = swapChainImageCount;
		ci.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
		ci.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		ci.imageExtent = {static_cast<uint32_t>(wnd.width), static_cast<uint32_t>(wnd.height)};
		ci.imageArrayLayers = 1;
		ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		ci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		ci.clipped = VK_TRUE;
		ci.oldSwapchain = VK_NULL_HANDLE;
		VK_ASSERT(vkCreateSwapchainKHR(device, &ci, nullptr, &swapChain));

		uint32_t swapChainImageCountCheck;
		VK_ASSERT(vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCountCheck, nullptr));
		ASSERT(swapChainImageCount == swapChainImageCountCheck);

		VK_ASSERT(vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCountCheck, &swapChainImages.front()));
	}

	//Create command pool
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkCreateCommandPool);
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkDestroyCommandPool);
	VkCommandPool commandPool;
	{
		VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		ci.queueFamilyIndex = presentQueueFamilyIndex;
		VK_ASSERT(vkCreateCommandPool(device, &ci, nullptr, &commandPool));
		ASSERT(commandPool);
	}

	//Allocate command buffers
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkAllocateCommandBuffers);
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkFreeCommandBuffers);
	std::vector<VkCommandBuffer> presentQueueCommandBuffers(swapChainImageCount);
	{
		VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		ai.commandPool = commandPool;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = swapChainImageCount;
		VK_ASSERT(vkAllocateCommandBuffers(device, &ai, &presentQueueCommandBuffers.front()));
	}

	//Record command buffers
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkBeginCommandBuffer);
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkCmdPipelineBarrier);
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkCmdClearColorImage);
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkEndCommandBuffer);
	auto recordCommandBuffer = [&](float red, float green, float blue) {
		VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
		VkClearColorValue clearColorValue = {{red, green, blue, 0.0f}};
		VkImageSubresourceRange imageSubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		VkImageMemoryBarrier barrierFromPresentToClear = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, presentQueueFamilyIndex, presentQueueFamilyIndex, 0, imageSubresourceRange};
		VkImageMemoryBarrier barrierFromClearToPresent = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, presentQueueFamilyIndex, presentQueueFamilyIndex, 0, imageSubresourceRange};

		for(uint32_t i = 0; i < swapChainImageCount; ++i) {
			barrierFromPresentToClear.image = swapChainImages[i];
			barrierFromClearToPresent.image = swapChainImages[i];
			VK_ASSERT(vkBeginCommandBuffer(presentQueueCommandBuffers[i], &commandBufferBeginInfo));
			vkCmdPipelineBarrier(presentQueueCommandBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierFromPresentToClear);
			vkCmdClearColorImage(presentQueueCommandBuffers[i], swapChainImages[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue, 1, &imageSubresourceRange);
			vkCmdPipelineBarrier(presentQueueCommandBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierFromClearToPresent);
			VK_ASSERT(vkEndCommandBuffer(presentQueueCommandBuffers[i]));
		}
	};

	//Loop Window
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkAcquireNextImageKHR);
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkQueueSubmit);
	VK_LOAD_FROM_VULKAN_DEVICE(device, vkQueuePresentKHR);

	//FPS
	int fps_accum = 0;
	int fps = 0;
	auto tick_prev = GetTick();

	//Color changing over frames
	struct ColorCycle {
		float c, cv;
	} cv[3] = {{0.2f, 0.008f}, {0.5f, 0.01f}, {0.8f, 0.012f}};

	WndShow(wnd);
	while(WndLoop()) {
		//Color changing over frames
		auto cu = [](float& c, float& cv) -> float { c += cv; if(c < 0.0f) { c = 0.0f; cv = -cv; } if(c > 1.0f) { c = 1.0f; cv = -cv; } return cv; };
		for(auto& it : cv)
			cu(it.c, it.cv);
		recordCommandBuffer(cv[0].c, cv[1].c, cv[2].c);

		//Acquire image from swap chain
		uint32_t imageIndex;
		VkResult vkAcquireNextImageResult = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
		ASSERT(vkAcquireNextImageResult != VK_SUBOPTIMAL_KHR);
		ASSERT(vkAcquireNextImageResult == VK_SUCCESS);

		//Submit queue
		{
			VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, &imageAvailableSemaphore, &pipelineStageFlags, 1, &presentQueueCommandBuffers[imageIndex], 1, &renderingFinishedSemaphore};
			VK_ASSERT(vkQueueSubmit(presentDeviceQueue, 1, &submitInfo, VK_NULL_HANDLE));
		}

		//Present queue
		{
			VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1, &renderingFinishedSemaphore, 1, &swapChain, &imageIndex, nullptr};
			VK_ASSERT(vkQueuePresentKHR(presentDeviceQueue, &presentInfo));
		}

		//Calculate FPS
		++fps_accum;
		{
			auto now = GetTick();
			auto tick = now - tick_prev;
			if(tick >= 1000) {
				tick_prev = now;
				fps = fps_accum;
				fps_accum = 0;
			}
		}

		//Show FPS
		{
			const size_t BUFFER_SIZE = sizeof(TEXT(APPLICATION_NAME)) + 64;
			TCHAR buffer[BUFFER_SIZE];
			auto cchText = _stprintf_s(buffer, BUFFER_SIZE, TEXT(APPLICATION_NAME " - FPS: %d"), fps);
			ASSERT(cchText != -1);
			SetWindowText(wnd.hWnd, buffer);
		}
	}

	//Destroy
	VK_ASSERT(vkDeviceWaitIdle(device));
	vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(presentQueueCommandBuffers.size()), &presentQueueCommandBuffers.front());
	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroySemaphore(device, renderingFinishedSemaphore, nullptr);
	vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);
	WIN32_ASSERT(FreeLibrary(vulkan));
};

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ INT)
{
	OneFileVulkan();
	return 0;
}
