#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static const char *const DEVICE_EXTENSIONS[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
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
    VkSwapchainKHR swap_chain;
    VkImage *swap_chain_images;
    uint32_t swap_chain_images_nb;
    VkFormat swap_chain_image_format;
    VkExtent2D swap_chain_extent;
    VkImageView *swap_chain_image_views;
    uint32_t swap_chain_image_views_nb;
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
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

static bool check_device_extension_support(VkPhysicalDevice device)
{
    uint32_t extensions_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensions_count, NULL);

    VkExtensionProperties *available_extensions = calloc(sizeof *available_extensions, extensions_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensions_count, available_extensions);
    log_trace("Found %u available device extensions", extensions_count);
    for (size_t i = 0; i < extensions_count; i++) {
        log_trace("device extension %lu: %s", i, available_extensions[i].extensionName);
    }

    for (size_t i = 0; i < LENGTH_OF(DEVICE_EXTENSIONS); i++) {
        bool extension_found = false;
        log_debug("Testing for device extension: %s", DEVICE_EXTENSIONS[i]);

        for (size_t y = 0; y < extensions_count; y++) {
            if (!strcmp(DEVICE_EXTENSIONS[i], available_extensions[y].extensionName)) {
                log_debug("Found wanted device extension: %s", DEVICE_EXTENSIONS[i]);
                extension_found = true;
                break;
            }
        }

        if (!extension_found) {
            log_fatal("Did not find wanted device extension: %s", DEVICE_EXTENSIONS[i]);
            free(available_extensions);
            return false;
        }
    }

    free(available_extensions);
    return true;
}

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    uint32_t formats_nb;
    VkPresentModeKHR *present_modes;
    uint32_t present_modes_nb;
} swap_chain_support_details;

swap_chain_support_details query_swap_chain_support(VkPhysicalDevice device)
{
    swap_chain_support_details details = { 0 };

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, CTX.surface, &details.capabilities);

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, CTX.surface, &details.present_modes_nb, NULL);
    if (details.present_modes_nb != 0) {
        details.present_modes = calloc(sizeof *details.present_modes, details.present_modes_nb);
        assert(details.present_modes);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, CTX.surface, &details.present_modes_nb, details.present_modes
        );
    }

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, CTX.surface, &details.formats_nb, NULL);
    if (details.formats_nb != 0) {
        details.formats = calloc(sizeof *details.formats, details.formats_nb);
        assert(details.formats);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, CTX.surface, &details.formats_nb, details.formats);
    }

    return details;
}

static VkSurfaceFormatKHR choose_swap_surface_format(
    const VkSurfaceFormatKHR *available_formats, uint32_t nb_available_formats
)
{
    for (uint32_t i = 0; i < nb_available_formats; i++) {
        if (available_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB
            && available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_formats[i];
        }
    }
    // If you get there you will have a bad time :3
    return available_formats[0];
}

static VkPresentModeKHR choose_swap_present_mode(
    const VkPresentModeKHR *available_present_modes, uint32_t nb_available_present_modes
)
{
    for (uint32_t i = 0; i < nb_available_present_modes; i++) {
        if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available_present_modes[i];
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR *capabilities)
{
    if (capabilities->currentExtent.width != UINT32_MAX)
        return capabilities->currentExtent;
    int width;
    int height;
    glfwGetFramebufferSize(CTX.window, &width, &height);
    VkExtent2D actual_extent = {
        .width = (uint32_t) width,
        .height = (uint32_t) height,
    };

    // TODO: Change this, this is fucking horrendous
    actual_extent.width = actual_extent.width > capabilities->maxImageExtent.width ? capabilities->maxImageExtent.width
                                                                                   : actual_extent.width;
    actual_extent.width = actual_extent.width < capabilities->minImageExtent.width ? capabilities->minImageExtent.width
                                                                                   : actual_extent.width;
    actual_extent.height = actual_extent.height > capabilities->maxImageExtent.height
        ? capabilities->maxImageExtent.height
        : actual_extent.height;
    actual_extent.height = actual_extent.height < capabilities->minImageExtent.height
        ? capabilities->minImageExtent.height
        : actual_extent.height;

    return actual_extent;
}

static bool is_device_suitable(VkPhysicalDevice device)
{
    queue_family_indices indices = find_queue_families(device);

    bool extensions_supported = check_device_extension_support(device);
    bool swap_chain_adequate = false;
    if (extensions_supported) {
        swap_chain_support_details swap_chain_support = query_swap_chain_support(device);
        swap_chain_adequate = swap_chain_support.formats_nb != 0 && swap_chain_support.present_modes_nb != 0;
    }

    return is_queue_family_indices_complete(indices) && extensions_supported && swap_chain_adequate;
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
    if (ENABLE_VALIDATION_LAYERS) {
        create_info.enabledLayerCount = LENGTH_OF(VALIDATION_LAYERS);
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    } else {
        create_info.enabledLayerCount = 0;
    }
    create_info.enabledExtensionCount = LENGTH_OF(DEVICE_EXTENSIONS);
    create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS;

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

static void create_swap_chain(void)
{
    swap_chain_support_details swap_chain_support = query_swap_chain_support(CTX.physical_device);

    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(
        swap_chain_support.formats, swap_chain_support.formats_nb
    );
    VkPresentModeKHR present_mode = choose_swap_present_mode(
        swap_chain_support.present_modes, swap_chain_support.present_modes_nb
    );
    VkExtent2D extent = choose_swap_extent(&swap_chain_support.capabilities);
    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;

    if (swap_chain_support.capabilities.maxImageCount > 0
        && image_count > swap_chain_support.capabilities.maxImageCount)
        image_count = swap_chain_support.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = CTX.surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    queue_family_indices indices = find_queue_families(CTX.physical_device);
    uint32_t idx[] = { indices.graphics_family, indices.present_family };
    if (indices.graphics_family != indices.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = idx;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(CTX.device, &create_info, NULL, &CTX.swap_chain);
    assert(result == VK_SUCCESS);

    vkGetSwapchainImagesKHR(CTX.device, CTX.swap_chain, &CTX.swap_chain_images_nb, NULL);
    CTX.swap_chain_images = calloc(sizeof *CTX.swap_chain_images, CTX.swap_chain_images_nb);
    assert(CTX.swap_chain_images);
    vkGetSwapchainImagesKHR(CTX.device, CTX.swap_chain, &CTX.swap_chain_images_nb, CTX.swap_chain_images);

    CTX.swap_chain_image_format = surface_format.format;
    CTX.swap_chain_extent = extent;
}

static void create_image_views(void)
{
    CTX.swap_chain_image_views = calloc(sizeof *CTX.swap_chain_image_views, CTX.swap_chain_images_nb);
    assert(CTX.swap_chain_image_views);
    CTX.swap_chain_image_views_nb = CTX.swap_chain_images_nb;

    for (uint32_t i = 0; i < CTX.swap_chain_image_views_nb; i++) {
        VkImageViewCreateInfo create_info = { 0 };
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = CTX.swap_chain_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = CTX.swap_chain_image_format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(CTX.device, &create_info, NULL, &CTX.swap_chain_image_views[i]);
        assert(result == VK_SUCCESS);
    }
}

static char *read_file(const char *filename, size_t *size)
{
    int fd = open(filename, O_RDONLY);
    assert(fd > 0);

    struct stat st;
    stat(filename, &st);
    *size = (size_t) st.st_size;

    char *data = malloc(*size);
    read(fd, data, *size);
    close(fd);
    return data;
}

static VkShaderModule create_shader_module(const char *code, size_t code_size)
{
    VkShaderModuleCreateInfo create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code_size;
    // Hope this doesn't crash for some alignement reason :/
    create_info.pCode = (const uint32_t *) code;

    VkShaderModule shader_module = { 0 };
    VkResult result = vkCreateShaderModule(CTX.device, &create_info, NULL, &shader_module);
    assert(result == VK_SUCCESS);
    return shader_module;
}

static void create_graphics_pipeline(void)
{
    size_t vert_shader_code_size;
    size_t frag_shader_code_size;
    char *vert_shader_code = read_file("shaders/vert.spv", &vert_shader_code_size);
    log_debug("Loaded vertex shader bytecode with size %lu", vert_shader_code_size);
    char *frag_shader_code = read_file("shaders/frag.spv", &frag_shader_code_size);
    log_debug("Loaded frament shader bytecode with size %lu", frag_shader_code_size);

    VkShaderModule vert_shader_module = create_shader_module(vert_shader_code, vert_shader_code_size);
    VkShaderModule frag_shader_module = create_shader_module(frag_shader_code, frag_shader_code_size);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = { 0 };
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = { 0 };
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vert_shader_stage_info,
        frag_shader_stage_info,
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = { 0 };
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = { 0 };
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = { 0 };
    viewport.x = 0.0;
    viewport.y = 0.0;
    viewport.width = (float) CTX.swap_chain_extent.width;
    viewport.height = (float) CTX.swap_chain_extent.height;
    viewport.minDepth = 0.0;
    viewport.maxDepth = 1.0;

    VkRect2D scissor = { 0 };
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = CTX.swap_chain_extent;

    VkPipelineViewportStateCreateInfo viewport_state = { 0 };
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = { 0 };
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = { 0 };
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachement = { 0 };
    color_blend_attachement.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachement.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending = { 0 };
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachement;

    VkPipelineLayoutCreateInfo pipeline_layout_info = { 0 };
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkResult result = vkCreatePipelineLayout(CTX.device, &pipeline_layout_info, NULL, &CTX.pipeline_layout);
    assert(result == VK_SUCCESS);

    vkDestroyShaderModule(CTX.device, vert_shader_module, NULL);
    vkDestroyShaderModule(CTX.device, frag_shader_module, NULL);
}

static void create_render_pass(void)
{
    VkAttachmentDescription color_attachement = { 0 };
    color_attachement.format = CTX.swap_chain_image_format;
    color_attachement.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachement.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachement.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachement.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = { 0 };
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = { 0 };
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_info = { 0 };
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachement;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VkResult result = vkCreateRenderPass(CTX.device, &render_pass_info, NULL, &CTX.render_pass);
    assert(result == VK_SUCCESS);
}

static void init_vulkan(void)
{
    create_instance();
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swap_chain();
    create_image_views();
    create_render_pass();
    create_graphics_pipeline();
}

static void main_loop(void)
{
    while (!glfwWindowShouldClose(CTX.window)) {
        glfwPollEvents();
    }
}

static void cleanup(void)
{
    vkDestroyPipelineLayout(CTX.device, CTX.pipeline_layout, NULL);
    vkDestroyRenderPass(CTX.device, CTX.render_pass, NULL);
    for (uint32_t i = 0; i < CTX.swap_chain_image_views_nb; i++)
        vkDestroyImageView(CTX.device, CTX.swap_chain_image_views[i], NULL);
    vkDestroySwapchainKHR(CTX.device, CTX.swap_chain, NULL);
    vkDestroyDevice(CTX.device, NULL);
    if (ENABLE_VALIDATION_LAYERS)
        vk_destroy_debug_utils_messenger_ext(CTX.instance, CTX.debug_messenger, NULL);
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
