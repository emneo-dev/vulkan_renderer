#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/mat4.h>
#include <cglm/vec4.h>

#include "array_helper_macros.h"
#include "log.h"

static const int WIDTH = 800;
static const int HEIGHT = 600;

static const char *const VALIDATION_LAYERS[] = {
    "VK_LAYER_KHRONOS_validation",
};

#ifdef NDEBUG
const bool ENABLE_VALIDATION_LAYERS = false;
#else
const bool ENABLE_VALIDATION_LAYERS = true;
#endif

typedef struct {
    GLFWwindow *window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkSurfaceKHR surface;
    VkQueue present_queue;
} global_ctx;

static global_ctx CTX = { 0 };

static void init_window(void)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    CTX.window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", NULL, NULL);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data
)
{
    switch (message_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        log_trace("vl: %s", callback_data->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        log_info("vl: %s", callback_data->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        log_warn("vl: %s", callback_data->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        log_error("vl: %s", callback_data->pMessage);
        break;
    default:
        break;
    }
    return VK_FALSE;
}

void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT *create_info)
{
    create_info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info->pfnUserCallback = debug_callback;
    create_info->pUserData = NULL;
}

static bool check_validation_layer_support(void)
{
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);

    VkLayerProperties *available_layers = calloc(sizeof(VkLayerProperties), layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
    log_trace("Found %u available validation layers", layer_count);
    for (size_t i = 0; i < layer_count; i++) {
        log_trace("validation layer %lu: %s", i, available_layers[i].layerName);
    }

    for (size_t i = 0; i < LENGTH_OF(VALIDATION_LAYERS); i++) {
        bool layer_found = false;
        log_debug("Testing for validation layer: %s", VALIDATION_LAYERS[i]);

        for (size_t y = 0; y < layer_count; y++) {
            if (!strcmp(VALIDATION_LAYERS[i], available_layers[y].layerName)) {
                log_debug("Found wanted validation layer: %s", VALIDATION_LAYERS[i]);
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            log_fatal("Did not find wanted validation layer: %s", VALIDATION_LAYERS[i]);
            free(available_layers);
            return false;
        }
    }

    free(available_layers);
    return true;
}

static const char **get_required_extensions(uint32_t *glfw_extension_count)
{
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(glfw_extension_count);
    if (ENABLE_VALIDATION_LAYERS)
        *glfw_extension_count += 1;

    const char **required_extensions = calloc(sizeof *required_extensions, *glfw_extension_count);
    assert(required_extensions);

    for (size_t i = 0; i < (ENABLE_VALIDATION_LAYERS ? *glfw_extension_count - 1 : *glfw_extension_count); i++)
        required_extensions[i] = glfw_extensions[i];

    if (ENABLE_VALIDATION_LAYERS)
        required_extensions[*glfw_extension_count - 1] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    return required_extensions;
}

static void create_instance(void)
{
    if (ENABLE_VALIDATION_LAYERS)
        assert(check_validation_layer_support());

    VkApplicationInfo app_info = { 0 };
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Hello Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;
    log_trace("Created VkApplicationInfo");

    log_trace("Creating VkInstanceCreateInfo");
    VkInstanceCreateInfo create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    uint32_t glfw_extension_count = 0;
    const char **glfw_extensions;
    glfw_extensions = get_required_extensions(&glfw_extension_count);
    create_info.enabledExtensionCount = glfw_extension_count;
    create_info.ppEnabledExtensionNames = glfw_extensions;
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = { 0 };
    if (ENABLE_VALIDATION_LAYERS) {
        create_info.enabledLayerCount = LENGTH_OF(VALIDATION_LAYERS);
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
        populate_debug_messenger_create_info(&debug_create_info);
        create_info.pNext = &debug_create_info;
    } else {
        create_info.enabledLayerCount = 0;
    }
    log_debug("Created VkInstanceCreateInfo with %u extensions", glfw_extension_count);

    log_trace("Creating VkInstance");
    VkResult result = vkCreateInstance(&create_info, NULL, &CTX.instance);
    assert(result == VK_SUCCESS);
    log_trace("Created VkInstance");
    free(glfw_extensions);
}

static VkResult vk_create_debug_utils_messenger_ext(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *p_create_info,
    const VkAllocationCallbacks *p_allocator, VkDebugUtilsMessengerEXT *p_debug_messenger
)
{
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT
    ) vkGetInstanceProcAddr(CTX.instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL)
        return func(instance, p_create_info, p_allocator, p_debug_messenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void vk_destroy_debug_utils_messenger_ext(
    VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks *p_allocator
)
{
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT
    ) vkGetInstanceProcAddr(CTX.instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL)
        return func(instance, debug_messenger, p_allocator);
}

static void setup_debug_messenger(void)
{
    if (!ENABLE_VALIDATION_LAYERS)
        return;

    VkDebugUtilsMessengerCreateInfoEXT create_info = { 0 };
    populate_debug_messenger_create_info(&create_info);

    VkResult result = vk_create_debug_utils_messenger_ext(CTX.instance, &create_info, NULL, &CTX.debug_messenger);
    assert(result == VK_SUCCESS);
}

typedef struct {
    uint32_t graphics_family;
    bool has_graphics_family;
    uint32_t present_family;
    bool has_present_family;
} queue_family_indices;

static bool is_queue_family_indices_complete(queue_family_indices qfi)
{
    return qfi.has_graphics_family && qfi.has_present_family;
}

static queue_family_indices find_queue_families(VkPhysicalDevice device)
{
    queue_family_indices indices = { 0 };

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_families = calloc(sizeof *queue_families, queue_family_count);
    assert(queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    for (uint32_t i = 0; i < queue_family_count; i++) {
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, CTX.surface, &present_support);
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.has_graphics_family = true;
        }
        if (present_support) {
            indices.present_family = i;
            indices.has_present_family = true;
        }
        if (is_queue_family_indices_complete(indices))
            break;
    }

    free(queue_families);
    return indices;
}

static bool is_device_suitable(VkPhysicalDevice device)
{
    queue_family_indices indices = find_queue_families(device);

    return is_queue_family_indices_complete(indices);
}

static void pick_physical_device(void)
{
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(CTX.instance, &device_count, NULL);
    assert(device_count != 0);

    VkPhysicalDevice *devices = calloc(sizeof *devices, device_count);
    vkEnumeratePhysicalDevices(CTX.instance, &device_count, devices);

    for (uint32_t i = 0; i < device_count; i++) {
        if (is_device_suitable(devices[i])) {
            physical_device = devices[i];
            break;
        }
    }

    assert(physical_device != VK_NULL_HANDLE);
    free(devices);
    CTX.physical_device = physical_device;
}

static void create_logical_device(void)
{
    queue_family_indices indices = find_queue_families(CTX.physical_device);
    float queue_priority = 1.0;

    VkDeviceQueueCreateInfo queue_create_infos[2] = { 0 };
    queue_create_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_infos[0].queueFamilyIndex = indices.graphics_family;
    queue_create_infos[0].queueCount = 1;
    queue_create_infos[0].pQueuePriorities = &queue_priority;
    bool is_same_queue = indices.present_family == indices.graphics_family;
    if (!is_same_queue) {
        queue_create_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[1].queueFamilyIndex = indices.present_family;
        queue_create_infos[1].queueCount = 1;
        queue_create_infos[1].pQueuePriorities = &queue_priority;
    }

    VkPhysicalDeviceFeatures device_features = { 0 };

    VkDeviceCreateInfo create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.queueCreateInfoCount = is_same_queue ? 1 : 2;
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = 0;
    if (ENABLE_VALIDATION_LAYERS) {
        create_info.enabledLayerCount = LENGTH_OF(VALIDATION_LAYERS);
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    } else {
        create_info.enabledLayerCount = 0;
    }

    VkResult result = vkCreateDevice(CTX.physical_device, &create_info, NULL, &CTX.device);
    assert(result == VK_SUCCESS);
    vkGetDeviceQueue(CTX.device, indices.graphics_family, 0, &CTX.graphics_queue);
    vkGetDeviceQueue(CTX.device, indices.present_family, 0, &CTX.present_queue);
}

static void create_surface(void)
{
    VkResult result = glfwCreateWindowSurface(CTX.instance, CTX.window, NULL, &CTX.surface);
    assert(result == VK_SUCCESS);
}

static void init_vulkan(void)
{
    create_instance();
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
}

static void main_loop(void)
{
    while (!glfwWindowShouldClose(CTX.window)) {
        glfwPollEvents();
    }
}

static void cleanup(void)
{
    vkDestroyDevice(CTX.device, NULL);
    if (ENABLE_VALIDATION_LAYERS) {
        vk_destroy_debug_utils_messenger_ext(CTX.instance, CTX.debug_messenger, NULL);
    }
    vkDestroySurfaceKHR(CTX.instance, CTX.surface, NULL);
    vkDestroyInstance(CTX.instance, NULL);

    glfwDestroyWindow(CTX.window);
    glfwTerminate();
}

int main(void)
{
    init_window();
    init_vulkan();
    main_loop();
    cleanup();
    return 0;
}
