#pragma once

#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.hpp>  // A modern C++ API for Vulkan. Beware 14K lines of code

#include <stdint.h>
#include <memory>

// Imgui
#define GUI
#ifdef GUI
#include "backends/imgui_impl_glfw.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#endif

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>

#include "shaders/shared_structs.h"
#include "ImageWrap.h"
#include "DescriptorWrap.h"
#include "BufferWrap.h"
#include "Util.h"
#include "RenderPass.h"

class Window;
class Camera;

class Graphics
{
	friend class ImageWrap;
private:
	Window* p_parent_window;
	Camera* p_active_cam;

	std::vector<const char*> instance_layers = {
		"VK_LAYER_KHRONOS_validation"
	};
	std::vector<const char*> instance_extensions = {
		"VK_EXT_debug_utils"
	};
	std::vector<const char*> device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,				  // Presentation engine; draws to screen
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,	  // Ray tracing extension
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,		  // Ray tracing extension
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME }; // Required by ray tracing pipeline;
	
	vk::Instance m_instance;
	vk::PhysicalDevice m_physical_device;
	uint32_t m_graphics_queue_index{ VK_QUEUE_FAMILY_IGNORED };
	vk::Device m_device;
	vk::Queue m_queue;
	vk::SurfaceKHR m_surface;
	vk::CommandPool m_cmd_pool;
	vk::CommandBuffer m_cmd_buffer;
	vk::SwapchainKHR m_swapchain;
	uint32_t m_swapchain_index = 0;
	uint32_t       m_image_count = 0;
	std::vector<vk::Image>     m_swapchain_images;  // from vkGetSwapchainImagesKHR
	std::vector<vk::ImageView> m_image_views;
	std::vector<vk::ImageMemoryBarrier> m_barriers;  // Filled in  VkImageMemoryBarrier objects
	vk::Fence m_waitfence;
	vk::Semaphore m_read_semaphore;
	vk::Semaphore m_written_semaphore;
	vk::Extent2D window_size{ 0, 0 }; // Size of the window
	ImageWrap m_depth_image;
	
	//Resources required for the post processing render pass
	vk::RenderPass m_post_proc_render_pass;
	std::vector<vk::Framebuffer> m_framebuffers;
	vk::Pipeline m_post_proc_pipeline;
	vk::PipelineLayout m_post_proc_pipeline_layout;
	DescriptorWrap m_post_proc_desc;
	
	//Resources required for the scanline render pass

	glm::mat4 m_prior_viewproj;

	std::vector<std::unique_ptr<RenderPass>> render_passes;

	bool do_post_process;
private:
	//Creates and intializes the vk::Instance
	void CreateInstance(bool api_dump);

	//Chooses and intializes the vk::PhysicalDevice
	void CreatePhysicalDevice();

	//Chooses the Queueindex
	void ChooseQueueIndex();

	//Create and intializes the vk::Device
	void CreateDevice();

	//Gets the command queue from the device
	void GetCommandQueue();

	vk::Format GetSupportedDepthFormat();

	//Loads the vulkan extensions
	void LoadExtensions();

	//Create and initialize the vk::SurfaceKHR
	void GetSurface();

	//Create and initialize the vk::CommandPool and vk::CommandBuffer
	void CreateCommandPool();

	//Create and initialize the vk::SwapchainKHR
	void CreateSwapchain();
	//Destroy the created vk::SwapchainKHR
	void DestroySwapchain();

	//Create a temporary command buffer
	vk::CommandBuffer CreateTempCommandBuffer();

	//Submit a command through a temporary command buffer
	void SubmitTempCommandBuffer(vk::CommandBuffer cmd_buffer);

	//Create the depth image wrap (aka the depth buffer)
	void CreateDepthResource();

	//Create a vk::RenderPass for post processing
	void CreatePostProcessRenderPass();
	
	//Create a vector<vk::FrameBuffer> for post processing
	void CreatePostFrameBuffers();
	
	//Create a Descriptor for the post processing.
	void CreatePostDescriptor(const ImageWrap& scanline_buffer);

	//Create the post processing pipeline
	void CreatePostPipeline();

	//Post processing render pass
	void PostProcess();

	void CreateObjDescriptionBuffer();

	void CreateMatrixBuffer();

	void LoadModel(const std::string& filename, glm::mat4 transform);

	//Required for ImGUI
	VkDescriptorPool m_imgui_descpool{ VK_NULL_HANDLE };
	void InitGUI();

	void PrepareFrame();

	void SubmitFrame();

	void UpdateCameraBuffer();
public:
	// Arrays of objects instances and textures in the scene
	std::vector<ObjData>  m_objData;  // Obj data in Vulkan Buffers
	std::vector<ObjDesc>  m_objDesc;  // Device-addresses of those buffers
	std::vector<ImageWrap>  m_objText;  // All textures of the scene
	std::vector<ObjInst>  m_objInst;  // Instances paring an object and a transform

	BufferWrap m_objDescriptionBW;  // Device buffer of the OBJ descriptions
	BufferWrap m_matrixBW;  // Device-Host of the camera matrices
	BufferWrap m_lightBW; //BufferWrap for the emitter data

	void DestroyUniformData();
public:
	Graphics(Window* _p_parent_window, bool api_dump=true);

	void DrawFrame();
	//Clean up all the created vulkan related resources
	void Teardown();

	void DrawGUI();

	void SetActiveCamPtr(Camera* p_cam);

	vec2 GetWindowSize() const;
	const vk::Extent2D& GetWindowExtent() const;

	uint32_t GetCurrentSwapchainIndex() const;
	const std::vector<vk::ImageView>& GetSwapChainImageViews() const;
	const ImageWrap& GetDepthBuffer() const;

	const vk::Device& GetDeviceRef() const { return m_device; }
	const vk::PhysicalDevice& GetPhysicalDeviceRef() const { return m_physical_device; }

	void EnablePostProcess();
	void DisablePostProcess();

	//Helper function to find required memory type
	uint8_t FindMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags properties) const;

	vk::Sampler CreateTextureSampler();
	ImageWrap CreateTextureImage(std::string fileName);

	void TransitionImageLayout(vk::Image image,
		vk::Format format,
		vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout,
		uint32_t mipLevels = 1);

	void GenerateMipmaps(vk::Image image, vk::Format imageFormat,
		int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	void ImageLayoutBarrier(vk::CommandBuffer cmdbuffer,
		vk::Image image,
		vk::ImageLayout oldImageLayout,
		vk::ImageLayout newImageLayout,
		vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor) const;

	vk::ShaderModule CreateShaderModule(std::string code);

	BufferWrap CreateStagedBufferWrap(const vk::CommandBuffer& cmdBuf,
		const vk::DeviceSize& size,
		const void* data,
		vk::BufferUsageFlags     usage);
	template <typename T>
	BufferWrap CreateStagedBufferWrap(const vk::CommandBuffer& cmdBuf,
		const std::vector<T>& data,
		vk::BufferUsageFlags     usage)
	{
		return CreateStagedBufferWrap(cmdBuf, sizeof(T) * data.size(), data.data(), usage);
	}


	BufferWrap CreateBufferWrap(vk::DeviceSize size, vk::BufferUsageFlags usage,
		vk::MemoryPropertyFlags properties);

	void CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

	const vk::CommandBuffer& GetCommandBuffer() const;

	void CommandCopyImage(const ImageWrap& src, const ImageWrap& dst) const;
};

vk::AccessFlags AccessFlagsForImageLayout(vk::ImageLayout layout);
vk::PipelineStageFlags PipelineStageForLayout(vk::ImageLayout layout);