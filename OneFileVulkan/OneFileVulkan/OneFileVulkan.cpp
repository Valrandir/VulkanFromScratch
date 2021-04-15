#pragma warning(disable : 26812)

#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include "vulkan.h"

#include <iostream>

#ifdef UNICODE
#define TCERR std::wcerr
#else
#define TCERR std::cerr
#endif

__declspec(noreturn) void Abort(LPCTSTR message);
__declspec(noreturn) void Win32Abort(LPCTSTR message);
__declspec(noreturn) void VkAbort(VkResult result, LPCTSTR fn);

#ifdef NDEBUG
#define ASSERT(fn) fn
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
#include <vector>

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if(msg == WM_CLOSE)
		PostQuitMessage(0);

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

struct Window {
	HINSTANCE hInstance;
	HWND hWnd;
	int width, height;
};

Window CreateWnd(int width, int height)
{
	LPCTSTR CLASS_NAME = TEXT("OneFileVulkan");
	HINSTANCE hInstance = GetModuleHandle(NULL);
	WNDCLASS wc{sizeof(wc)};

	if(!GetClassInfo(hInstance, TEXT(""), &wc)) {
		wc.lpfnWndProc = WndProc;
		wc.hInstance = hInstance;
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
		wc.lpszClassName = CLASS_NAME;
		WIN32_ASSERT(RegisterClass(&wc));
	}

	DWORD style = WS_BORDER | WS_CAPTION | WS_POPUP | WS_SYSMENU | WS_VISIBLE;
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

void WndLoop()
{
	MSG msg;

	while(GetMessage(&msg, 0, 0, 0))
		DispatchMessage(&msg);
}

void VulkanPeek()
{
	HMODULE vulkan = LoadLibrary(TEXT("vulkan-1.dll"));
	if(!vulkan)
		return;

	VK_LOAD_FROM_MODULE(vulkan, vkGetInstanceProcAddr);
	VK_LOAD_FROM_VULKAN(vkEnumerateInstanceExtensionProperties);
	VK_LOAD_FROM_VULKAN(vkCreateInstance);

	//Check available extensions
	uint32_t propertyCount;
	VK_ASSERT(vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, nullptr));
	ASSERT(propertyCount > 0);
	std::vector<VkExtensionProperties> properties(propertyCount);
	VK_ASSERT(vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, &properties.front()));
	std::vector<const char*> desiredExtensions = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
	auto checkExtension = [&](const char* extensionName) -> bool { return properties.cend() != std::find_if(properties.cbegin(), properties.cend(), [&](auto& p) -> bool { return strcmp(extensionName, p.extensionName) == 0; }); };
	for(auto& de : desiredExtensions)
		ASSERT(checkExtension(de));

	//Create Vulkan Instance
	VkApplicationInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	ai.pApplicationName = __FILE__;
	ai.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	ai.pEngineName = ai.pApplicationName;
	ai.engineVersion = ai.applicationVersion;
	ai.apiVersion = VK_VERSION_1_2;
	VkInstanceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pApplicationInfo = &ai;
	uint32_t enabledExtensionCount;
	const char* const* ppEnabledExtensionNames;
	ci.enabledExtensionCount = desiredExtensions.size();
	ci.ppEnabledExtensionNames = &desiredExtensions.front();
	VkInstance instance;
	VK_ASSERT(vkCreateInstance(&ci, nullptr, &instance));
	ASSERT(instance);

	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkEnumeratePhysicalDevices);
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkGetPhysicalDeviceProperties2);
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkGetPhysicalDeviceQueueFamilyProperties2);
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkCreateDevice);
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkGetDeviceProcAddr);
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkDestroyInstance);

	//Find Physical Device
	uint32_t physicalDeviceCount;
	VK_ASSERT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));
	ASSERT(physicalDeviceCount > 0);
	auto physicalDevices = new VkPhysicalDevice[physicalDeviceCount];
	VK_ASSERT(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));
	VkPhysicalDevice physicalDevice = physicalDevices[0];
	delete[] physicalDevices;

	//Read Physical Device Properties (optional)
	/*
	VkPhysicalDeviceVulkan12Properties pdpv12;
	pdpv12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
	pdpv12.pNext = nullptr;
	VkPhysicalDeviceVulkan11Properties pdpv11;
	pdpv11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
	pdpv11.pNext = &pdpv12;
	VkPhysicalDeviceProperties2 pdp2{};
	pdp2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	pdp2.pNext = &pdpv11;
	vkGetPhysicalDeviceProperties2(physicalDevice, &pdp2);
	*/

	//Find Queue Family Index for VK_QUEUE_GRAPHICS_BIT
	uint32_t queueFamilyPropertyCount;
	vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertyCount, nullptr);
	ASSERT(queueFamilyPropertyCount > 0);
	std::vector<VkQueueFamilyProperties2> queueFamilyProperties(queueFamilyPropertyCount, {VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2, nullptr});
	vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertyCount, &queueFamilyProperties.front());
	auto qfpIsOk = [](const auto& qfp) { return qfp.queueFamilyProperties.queueCount > 0 && qfp.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT; };
	auto it = std::find_if(queueFamilyProperties.cbegin(), queueFamilyProperties.cend(), qfpIsOk);
	ASSERT(it != queueFamilyProperties.cend() && "Found no queue with VK_QUEUE_GRAPHICS_BIT");
	VkQueueFamilyProperties2 queueFamilyProperty = *it;
	int queueFamilyIndex = it - queueFamilyProperties.begin();
	queueFamilyProperties.resize(0);

	//Create Logical Device
	VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	float queue_priorities = 1.0f;
	qci.queueFamilyIndex = queueFamilyIndex;
	qci.queueCount = 1;
	qci.pQueuePriorities = &queue_priorities;
	VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	VkDevice logicalDevice;
	VK_ASSERT(vkCreateDevice(physicalDevice, &dci, nullptr, &logicalDevice));
	ASSERT(logicalDevice);
	VkQueue deviceQueue;
	VK_LOAD_FROM_VULKAN_DEVICE(logicalDevice, vkGetDeviceQueue);
	VK_LOAD_FROM_VULKAN_DEVICE(logicalDevice, vkDeviceWaitIdle);
	VK_LOAD_FROM_VULKAN_DEVICE(logicalDevice, vkDestroyDevice);
	vkGetDeviceQueue(logicalDevice, queueFamilyIndex, 0, &deviceQueue);
	ASSERT(deviceQueue);

	//Create a win32 window
	auto wnd = CreateWnd(640, 480);

	//Create a win32 surface
	VK_LOAD_FROM_VULKAN_INSTANCE(instance, vkCreateWin32SurfaceKHR);
	VkWin32SurfaceCreateInfoKHR sci = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
	sci.hinstance = wnd.hInstance;
	sci.hwnd = wnd.hWnd;
	VkSurfaceKHR surface;
	VK_ASSERT(vkCreateWin32SurfaceKHR(instance, &sci, nullptr, &surface));
	ASSERT(surface);

	//Destroy
	VK_ASSERT(vkDeviceWaitIdle(logicalDevice));
	vkDestroyDevice(logicalDevice, nullptr);
	vkDestroyInstance(instance, nullptr);
	FreeLibrary(vulkan);
};

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ INT)
{
	VulkanPeek();
	return 0;
}
