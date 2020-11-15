#include "mali_profiler.h"

#include "memory.h"
#include "hwcpipe_log.h"
#include "hwc.hpp"

#include <errno.h>
#include <string.h>

using mali_userspace::MALI_NAME_BLOCK_JM;
using mali_userspace::MALI_NAME_BLOCK_MMU;
using mali_userspace::MALI_NAME_BLOCK_SHADER;
using mali_userspace::MALI_NAME_BLOCK_TILER;

namespace hwcpipe
{

// ---------------------------------------------

struct mali_hardware_info_t
{
	unsigned int mp_count;
	unsigned int gpu_id;
	unsigned int r_value;
	unsigned int p_value;
	unsigned int core_mask;
	unsigned int l2_slices;
};

struct runtime_hardware_info_t
{
	int device_fd;
	int hwc_fd;
	int gpu_id;
	int num_cores;
	int num_l2_slices;

	runtime_hardware_info_t()
		: device_fd(-1)
		, hwc_fd(-1)
		, gpu_id(-1)
		, num_cores(-1)
		, num_l2_slices(-1)
	{ }
};

struct profile_info_t
{
	const uint32_t k_buffer_count;
	uint32_t hw_version;
	size_t buffer_size;
	size_t num_core_index_remap;
	uint64_t time_stamp;

	uint8_t* sample_data;
	const char* const* names_lut;
	uint32_t* raw_counter_data;
	unsigned int* core_index_remap;

	profile_info_t()
		: k_buffer_count(16)
		, hw_version(0)
		, buffer_size(0)
		, num_core_index_remap(0)
		, time_stamp(0)

		, sample_data(nullptr)
		, names_lut(nullptr)
		, raw_counter_data(nullptr)
		, core_index_remap(nullptr)
	{ }
};

typedef uint64_t (*read_func_t)(mali_userspace::MaliCounterBlockName i_block, const int i_index);
struct counter_index_pairs_t
{
	mali_userspace::MaliCounterBlockName blockName;
	read_func_t readFunc;
	int index;
	uint64_t value;
};

struct counter_mapping_t
{
	mali_userspace::MaliCounterBlockName		blockName;
	const char*									counterName;
	read_func_t									readFunc;
};

// ---------------------------------------------

static bool s_gpuProfilerReady = false;
static const char* k_maliDevicePath = "/dev/mali0";
static runtime_hardware_info_t s_hwInfo;
static profile_info_t s_profInfo;

static counter_index_pairs_t s_enabledCounters[(size_t)gpu_counter_e::count];

// ---------------------------------------------

static constexpr int k_invalidIndex = -1;

// ---------------------------------------------

int find_counter_index_by_name(mali_userspace::MaliCounterBlockName i_block, const char* i_name);

// ---------------------------------------------
// readers

uint64_t get_counter_value(mali_userspace::MaliCounterBlockName i_block, const int i_index);

inline uint64_t default_read(mali_userspace::MaliCounterBlockName i_block, const int i_index)
{
	return get_counter_value(i_block, i_index);
}

inline uint64_t read_beats_to_bytes(mali_userspace::MaliCounterBlockName i_block, const int i_index)
{
	return 16 * get_counter_value(i_block, i_index);
}

// ---------------------------------------------

static counter_mapping_t k_bifrostMapping[] =
{
	{ MALI_NAME_BLOCK_JM,						"GPU_ACTIVE", &default_read },		// gpu_cycles
	{ MALI_NAME_BLOCK_JM,						"JS0_ACTIVE", &default_read },		// fragment_cycles
	{ MALI_NAME_BLOCK_TILER,					"TILER_ACTIVE", &default_read },	// tiler_cycles
	{ MALI_NAME_BLOCK_SHADER,					"FRAG_TRANS_ELIM", &default_read },	// frag_elim

	{ MALI_NAME_BLOCK_SHADER,					"EXEC_CORE_ACTIVE", &default_read },	// shader_cycles
	{ MALI_NAME_BLOCK_SHADER,					"EXEC_INSTR_COUNT", &default_read },	// shader_arithmetic_cycles
	{ MALI_NAME_BLOCK_SHADER,					"TEX_FILT_NUM_OPERATIONS", &default_read },		// shader_texture_cycles
	{ MALI_NAME_BLOCK_SHADER,					"VARY_SLOT_16", &default_read },	// varying_16_bits
	{ MALI_NAME_BLOCK_SHADER,					"VARY_SLOT_32", &default_read },	// varying_32_bits

	{ MALI_NAME_BLOCK_MMU,						"L2_EXT_READ_BEATS", &read_beats_to_bytes },	// external_memory_read_bytes
	{ MALI_NAME_BLOCK_MMU,						"L2_EXT_WRITE_BEATS", &read_beats_to_bytes },	// external_memory_write_bytes
};
static counter_mapping_t k_midgardMapping[] =
{
	{ MALI_NAME_BLOCK_JM,						"GPU_ACTIVE", &default_read },		// gpu_cycles
	{ MALI_NAME_BLOCK_JM,						"JS0_ACTIVE", &default_read },		// fragment_cycles
	{ MALI_NAME_BLOCK_TILER,					"TILER_ACTIVE", &default_read },	// tiler_cycles
	{ MALI_NAME_BLOCK_SHADER,					"FRAG_TRANS_ELIM", &default_read },	// frag_elim

	{ MALI_NAME_BLOCK_SHADER,					"<none>", nullptr },		// shader_cycles
	{ MALI_NAME_BLOCK_SHADER,					"<none>", nullptr },		// shader_arithmetic_cycles
	{ MALI_NAME_BLOCK_SHADER,					"TEX_ISSUES", &default_read },		// shader_texture_cycles
	{ MALI_NAME_BLOCK_SHADER,					"VARY_SLOT_16", &default_read },	// varying_16_bits
	{ MALI_NAME_BLOCK_SHADER,					"VARY_SLOT_32", &default_read },	// varying_32_bits

	{ MALI_NAME_BLOCK_MMU,						"L2_EXT_READ_BEATS", &read_beats_to_bytes },	// external_memory_read_bytes
	{ MALI_NAME_BLOCK_MMU,						"L2_EXT_WRITE_BEATS", &read_beats_to_bytes },	// external_memory_write_bytes
};
counter_mapping_t* s_counterMapping = nullptr;

// ---------------------------------------------

mali_hardware_info_t get_mali_hardware_info(const char* i_path)
{
	mali_hardware_info_t retInfo;
	memset(&retInfo, 0, sizeof(mali_hardware_info_t));

	int fd = open(i_path, O_RDWR);
	if (fd < 0)
	{
		HWCPIPE_LOG("Failed to get HW info.");
		return retInfo;
	}

	// driver version
	{
		mali_userspace::kbase_uk_hwcnt_reader_version_check_args version_check_args;
		version_check_args.header.id = mali_userspace::UKP_FUNC_ID_CHECK_VERSION;
		version_check_args.major     = 10;
		version_check_args.minor     = 2;

		if (mali_userspace::mali_ioctl(fd, version_check_args) != 0)
		{
			mali_userspace::kbase_ioctl_version_check _version_check_args = {0, 0};
			if (ioctl(fd, KBASE_IOCTL_VERSION_CHECK, &_version_check_args) < 0)
			{
				close(fd);
				HWCPIPE_LOG("Failed to check version.");
				return retInfo;
			}
		}
	}

	{
		mali_userspace::kbase_uk_hwcnt_reader_set_flags flags;
		memset(&flags, 0, sizeof(flags));
		flags.header.id    = mali_userspace::KBASE_FUNC_SET_FLAGS;
		flags.create_flags = mali_userspace::BASE_CONTEXT_CREATE_KERNEL_FLAGS;

		if (mali_userspace::mali_ioctl(fd, flags) != 0)
		{
			mali_userspace::kbase_ioctl_set_flags _flags = {1u << 1};
			if (ioctl(fd, KBASE_IOCTL_SET_FLAGS, &_flags) < 0)
			{
				close(fd);
				HWCPIPE_LOG("Failed settings flags ioctl.");
				return retInfo;
			}
		}
	}

	// gpu properties
	{
		mali_userspace::kbase_uk_gpuprops props = {};
		props.header.id                         = mali_userspace::KBASE_FUNC_GPU_PROPS_REG_DUMP;
		if (mali_ioctl(fd, props) == 0)
		{
			retInfo.gpu_id  = props.props.core_props.product_id;
			retInfo.r_value = props.props.core_props.major_revision;
			retInfo.p_value = props.props.core_props.minor_revision;
			for (uint32_t i = 0; i < props.props.coherency_info.num_core_groups; i++)
				retInfo.core_mask |= props.props.coherency_info.group[i].core_mask;
			retInfo.mp_count  = __builtin_popcountll(retInfo.core_mask);
			retInfo.l2_slices = props.props.l2_props.num_l2_slices;

			close(fd);
		}
		else
		{
			mali_userspace::kbase_ioctl_get_gpuprops get_props = {};
			int                                      ret;
			if ((ret = ioctl(fd, KBASE_IOCTL_GET_GPUPROPS, &get_props)) < 0)
			{
				HWCPIPE_LOG("Failed getting GPU properties.");
				close(fd);
				return retInfo;
			}

			get_props.size = ret;
			uint8_t buffer[4096];
			if (ret > sizeof(buffer))
			{
				HWCPIPE_LOG("Not enough buffer for getting GPU properties");
				return retInfo;
			}
			get_props.buffer.value = buffer;
			ret                    = ioctl(fd, KBASE_IOCTL_GET_GPUPROPS, &get_props);
			if (ret < 0)
			{
				HWCPIPE_LOG("Failed getting GPU properties.");
				close(fd);
				return retInfo;
			}

#define READ_U8(p) ((p)[0])
#define READ_U16(p) (READ_U8((p)) | (uint16_t(READ_U8((p) + 1)) << 8))
#define READ_U32(p) (READ_U16((p)) | (uint32_t(READ_U16((p) + 2)) << 16))
#define READ_U64(p) (READ_U32((p)) | (uint64_t(READ_U32((p) + 4)) << 32))

			mali_userspace::gpu_props props = {};

			const auto *ptr  = buffer;
			int         size = ret;
			while (size > 0)
			{
				uint32_t type       = READ_U32(ptr);
				uint32_t value_type = type & 3;
				uint64_t value;

				ptr += 4;
				size -= 4;

				switch (value_type)
				{
					case KBASE_GPUPROP_VALUE_SIZE_U8:
						value = READ_U8(ptr);
						ptr++;
						size--;
						break;
					case KBASE_GPUPROP_VALUE_SIZE_U16:
						value = READ_U16(ptr);
						ptr += 2;
						size -= 2;
						break;
					case KBASE_GPUPROP_VALUE_SIZE_U32:
						value = READ_U32(ptr);
						ptr += 4;
						size -= 4;
						break;
					case KBASE_GPUPROP_VALUE_SIZE_U64:
						value = READ_U64(ptr);
						ptr += 8;
						size -= 8;
						break;
				}

				for (unsigned i = 0; mali_userspace::gpu_property_mapping[i].type; i++)
				{
					if (mali_userspace::gpu_property_mapping[i].type == (type >> 2))
					{
						auto  offset = mali_userspace::gpu_property_mapping[i].offset;
						void *p      = reinterpret_cast<uint8_t *>(&props) + offset;
						switch (mali_userspace::gpu_property_mapping[i].size)
						{
							case 1:
								*reinterpret_cast<uint8_t *>(p) = value;
								break;
							case 2:
								*reinterpret_cast<uint16_t *>(p) = value;
								break;
							case 4:
								*reinterpret_cast<uint32_t *>(p) = value;
								break;
							case 8:
								*reinterpret_cast<uint64_t *>(p) = value;
								break;
							default:
								HWCPIPE_LOG("Invalid property size.");
								close(fd);
								// TODO: should we return?
						}
						break;
					}
				}
			}

			retInfo.gpu_id  = props.product_id;
			retInfo.r_value = props.major_revision;
			retInfo.p_value = props.minor_revision;
			for (uint32_t i = 0; i < props.num_core_groups; i++)
				retInfo.core_mask |= props.core_mask[i];
			retInfo.mp_count  = __builtin_popcountll(retInfo.core_mask);
			retInfo.l2_slices = props.l2_slices;

			close(fd);
		}

		return retInfo;
	}
}

void find_products_and_create_mapping(gpu_counter_e* i_enabledCounters, const size_t i_numCounters)
{
	const mali_userspace::CounterMapping* mapping = nullptr;
	for (size_t i = 0; i < mali_userspace::NUM_PRODUCTS; i++)
	{
		if ((mali_userspace::products[i].product_mask & s_hwInfo.gpu_id) == mali_userspace::products[i].product_id)
		{
			mapping = &mali_userspace::products[i];
			break;
		}
	}

	if (mapping != nullptr)
	{
		switch (mapping->product_id)
		{
			case mali_userspace::PRODUCT_ID_T60X:
			case mali_userspace::PRODUCT_ID_T62X:
			case mali_userspace::PRODUCT_ID_T72X:
				//mappings_                     = midgard_mappings;
				//mappings_[GpuCounter::Pixels] = [this]() { return get_counter_value(MALI_NAME_BLOCK_JM, "JS0_TASKS") * 256; };
				//counterMappings = k_midgardMappings;
				s_counterMapping = k_midgardMapping;
				break;
			case mali_userspace::PRODUCT_ID_T76X:
			case mali_userspace::PRODUCT_ID_T82X:
			case mali_userspace::PRODUCT_ID_T83X:
			case mali_userspace::PRODUCT_ID_T86X:
			case mali_userspace::PRODUCT_ID_TFRX:
				//mappings_ = midgard_mappings;
				s_counterMapping = k_midgardMapping;
				break;
			case mali_userspace::PRODUCT_ID_TMIX:
			case mali_userspace::PRODUCT_ID_THEX:
				//mappings_                                  = bifrost_mappings;
				//mappings_[GpuCounter::ShaderTextureCycles] = [this] { return get_counter_value(MALI_NAME_BLOCK_SHADER, "TEX_COORD_ISSUE"); };
				s_counterMapping = k_bifrostMapping;
				break;
			case mali_userspace::PRODUCT_ID_TSIX:
			case mali_userspace::PRODUCT_ID_TNOX:
			default:
				s_counterMapping = k_bifrostMapping;
				//mappings_ = bifrost_mappings;
				break;
		}
	}
	else
	{
		HWCPIPE_LOG("Mali counters initialization failed: Failed to identify GPU");
		return;
	}

	for (int i = 0; i < (int)gpu_counter_e::count; i++)
	{
		s_enabledCounters[i].blockName = MALI_NAME_BLOCK_JM;
		s_enabledCounters[i].readFunc = nullptr;
		s_enabledCounters[i].index = k_invalidIndex;
		s_enabledCounters[i].value = 0;
	}

	// fill index map using mappings
	for (int i = 0; i < i_numCounters; i++)
	{
		int cIdx = (int)i_enabledCounters[i];
		s_enabledCounters[cIdx].blockName = s_counterMapping[cIdx].blockName;
		s_enabledCounters[cIdx].readFunc = s_counterMapping[cIdx].readFunc;
		s_enabledCounters[cIdx].index = find_counter_index_by_name(
				s_counterMapping[cIdx].blockName, s_counterMapping[cIdx].counterName);
		if (s_enabledCounters[cIdx].index > 0)
		{
			HWCPIPE_INFO("Enabled counter: %s(%d) @id %d",
					s_counterMapping[cIdx].counterName,
					cIdx, s_enabledCounters[cIdx].index);
		}
		else
		{
			HWCPIPE_LOG("Cannot enable counter: %s(%d) (not available)",
					s_counterMapping[cIdx].counterName, cIdx);
		}
	}
}

const bool initialize_mali_profiler(gpu_counter_e* i_enabledCounters, const size_t i_numCounters)
{
	mali_hardware_info_t hwInfo = get_mali_hardware_info(k_maliDevicePath);

	s_hwInfo.gpu_id = hwInfo.gpu_id;
	s_hwInfo.num_cores = hwInfo.mp_count;
	s_hwInfo.num_l2_slices = hwInfo.l2_slices;

	HWCPIPE_INFO("GPU ID: %x", s_hwInfo.gpu_id);
	HWCPIPE_INFO("Number of cores: %d", s_hwInfo.num_cores);
	HWCPIPE_INFO("Number of L2 slices: %d", s_hwInfo.num_l2_slices);

	s_hwInfo.device_fd = open(k_maliDevicePath, O_RDWR | O_CLOEXEC | O_NONBLOCK);

	if (s_hwInfo.device_fd < 0)
	{
		HWCPIPE_LOG("Failed to open /dev/mali0.");
		return false;
	}

	{
		mali_userspace::kbase_uk_hwcnt_reader_version_check_args check;
		memset(&check, 0, sizeof(check));

		if (mali_userspace::mali_ioctl(s_hwInfo.device_fd, check) != 0)
		{
			mali_userspace::kbase_ioctl_version_check _check = {0, 0};
			if (ioctl(s_hwInfo.device_fd, KBASE_IOCTL_VERSION_CHECK, &_check) < 0)
			{
				HWCPIPE_LOG("Failed to get ABI version.");
				return false;
			}
		}
		else if (check.major < 10)
		{
			HWCPIPE_LOG("Unsupported ABI version 10.");
			return false;
		}
	}

	{
		mali_userspace::kbase_uk_hwcnt_reader_set_flags flags;
		memset(&flags, 0, sizeof(flags));
		flags.header.id    = mali_userspace::KBASE_FUNC_SET_FLAGS;
		flags.create_flags = mali_userspace::BASE_CONTEXT_CREATE_KERNEL_FLAGS;

		if (mali_userspace::mali_ioctl(s_hwInfo.device_fd, flags) != 0)
		{
			mali_userspace::kbase_ioctl_set_flags _flags = {1u << 1};
			if (ioctl(s_hwInfo.device_fd, KBASE_IOCTL_SET_FLAGS, &_flags) < 0)
			{
				HWCPIPE_LOG("Failed settings flags ioctl.");
				return false;
			}
		}
	}

	{
		mali_userspace::kbase_uk_hwcnt_reader_setup setup;
		memset(&setup, 0, sizeof(setup));
		setup.header.id    = mali_userspace::KBASE_FUNC_HWCNT_READER_SETUP;
		setup.buffer_count = s_profInfo.k_buffer_count;
		setup.jm_bm        = -1;
		setup.shader_bm    = -1;
		setup.tiler_bm     = -1;
		setup.mmu_l2_bm    = -1;
		setup.fd           = -1;

		if (mali_userspace::mali_ioctl(s_hwInfo.device_fd, setup) != 0)
		{
			mali_userspace::kbase_ioctl_hwcnt_reader_setup _setup = {};
			_setup.buffer_count                                   = s_profInfo.k_buffer_count;
			_setup.jm_bm                                          = -1;
			_setup.shader_bm                                      = -1;
			_setup.tiler_bm                                       = -1;
			_setup.mmu_l2_bm                                      = -1;

			int ret;
			if ((ret = ioctl(s_hwInfo.device_fd, KBASE_IOCTL_HWCNT_READER_SETUP, &_setup)) < 0)
			{
				HWCPIPE_LOG("Failed setting hwcnt reader ioctl. Error: {%d, %s}", errno, strerror(errno));
				return false;
			}
			s_hwInfo.hwc_fd = ret;
		}
		else
		{
			s_hwInfo.hwc_fd = setup.fd;
		}
	}

	{
		uint32_t api_version = ~mali_userspace::HWCNT_READER_API;

		if (ioctl(s_hwInfo.hwc_fd, mali_userspace::KBASE_HWCNT_READER_GET_API_VERSION, &api_version) != 0)
		{
			HWCPIPE_LOG("Could not determine hwcnt reader API.");
			return false;
		}
		else if (api_version != mali_userspace::HWCNT_READER_API)
		{
			HWCPIPE_LOG("Invalid API version.");
			return false;
		}
	}

	if (ioctl(s_hwInfo.hwc_fd, static_cast<int>(mali_userspace::KBASE_HWCNT_READER_GET_BUFFER_SIZE), &s_profInfo.buffer_size) != 0)
	{
		HWCPIPE_LOG("Failed to get buffer size.");
		return false;
	}

	if (ioctl(s_hwInfo.hwc_fd, static_cast<int>(mali_userspace::KBASE_HWCNT_READER_GET_HWVER), &s_profInfo.hw_version) != 0)
	{
		HWCPIPE_LOG("Could not determine HW version.");
		return false;
	}

	if (s_profInfo.hw_version < 5)
	{
		HWCPIPE_LOG("Unsupported HW version.");
		return false;
	}

	s_profInfo.sample_data = static_cast<uint8_t *>(mmap(nullptr, s_profInfo.k_buffer_count * s_profInfo.buffer_size, PROT_READ, MAP_PRIVATE, s_hwInfo.hwc_fd, 0));

	if (s_profInfo.sample_data == MAP_FAILED)
	{
		HWCPIPE_LOG("Failed to map sample data.");
		return false;
	}

	{
		const mali_userspace::CounterMapping* mapping = nullptr;
		for (size_t i = 0; i < mali_userspace::NUM_PRODUCTS; i++)
		{
			if ((mali_userspace::products[i].product_mask & s_hwInfo.gpu_id) == mali_userspace::products[i].product_id)
			{
				mapping = &mali_userspace::products[i];
				break;
			}
		}

		if (mapping)
		{
			s_profInfo.names_lut = mapping->names_lut;
		}
		else
		{
			HWCPIPE_LOG("Could not identify GPU.");
			return false;
		}
	}

	s_profInfo.raw_counter_data = (uint32_t*)memory::allocate(s_profInfo.buffer_size);

	// Build core remap table.
	s_profInfo.num_core_index_remap = hwInfo.mp_count;
	s_profInfo.core_index_remap = (unsigned int*)memory::allocate(s_profInfo.num_core_index_remap * sizeof(unsigned int));

	unsigned int mask = hwInfo.core_mask;

	size_t cidx = 0;
	while (mask != 0)
	{
		unsigned int bit = __builtin_ctz(mask);
		s_profInfo.core_index_remap[cidx] = bit;
		mask &= ~(1u << bit);
		cidx++;
	}

	find_products_and_create_mapping(i_enabledCounters, i_numCounters);
	s_gpuProfilerReady = true;
	return true;
}

// ---------------------------------------------

void wait_next_event()
{
	pollfd poolFd;
	poolFd.fd     = s_hwInfo.hwc_fd;
	poolFd.events = POLLIN;

	const int count = poll(&poolFd, 1, -1);

	if (count < 0)
	{
		HWCPIPE_LOG("poll() failed.");
	}

	if ((poolFd.revents & POLLIN) != 0)
	{
		mali_userspace::kbase_hwcnt_reader_metadata meta;

		if (ioctl(s_hwInfo.hwc_fd, static_cast<int>(mali_userspace::KBASE_HWCNT_READER_GET_BUFFER), &meta) != 0)
		{
			HWCPIPE_LOG("Failed READER_GET_BUFFER.");
		}

		memcpy(s_profInfo.raw_counter_data, s_profInfo.sample_data + s_profInfo.buffer_size * meta.buffer_idx, s_profInfo.buffer_size);
		s_profInfo.time_stamp = meta.timestamp;

		if (ioctl(s_hwInfo.hwc_fd, mali_userspace::KBASE_HWCNT_READER_PUT_BUFFER, &meta) != 0)
		{
			HWCPIPE_LOG("Failed READER_PUT_BUFFER.");
		}
	}
	else if ((poolFd.revents & POLLHUP) != 0)
	{
		HWCPIPE_LOG("HWC hung up.");
	}
}

void sample_counters()
{
	if (ioctl(s_hwInfo.hwc_fd, mali_userspace::KBASE_HWCNT_READER_DUMP, 0) != 0)
	{
		HWCPIPE_LOG("Could not sample hardware counters.");
	}
}

const uint32_t* get_counters_block_base_address(mali_userspace::MaliCounterBlockName i_block, int i_index)
{
	switch (i_block)
	{
		case mali_userspace::MALI_NAME_BLOCK_JM:
			return s_profInfo.raw_counter_data + mali_userspace::MALI_NAME_BLOCK_SIZE * 0;
		case mali_userspace::MALI_NAME_BLOCK_MMU:
			if (i_index < 0 || i_index >= s_hwInfo.num_l2_slices)
			{
				HWCPIPE_LOG("Invalid slice number.");
				return nullptr;
			}

			// If an MMU counter is selected, index refers to the MMU slice
			return s_profInfo.raw_counter_data + mali_userspace::MALI_NAME_BLOCK_SIZE * (2 + i_index);
		case mali_userspace::MALI_NAME_BLOCK_TILER:
			return s_profInfo.raw_counter_data + mali_userspace::MALI_NAME_BLOCK_SIZE * 1;
		default:
			if (i_index < 0 || i_index >= s_hwInfo.num_cores)
			{
				HWCPIPE_LOG("Invalid core number.");
			}

			// If a shader core counter is selected, index refers to the core index
			return s_profInfo.raw_counter_data + mali_userspace::MALI_NAME_BLOCK_SIZE * (2 + s_hwInfo.num_l2_slices + s_profInfo.core_index_remap[i_index]);
	}
}

int find_counter_index_by_name(mali_userspace::MaliCounterBlockName i_block, const char* i_name)
{
	const char* const* names = &s_profInfo.names_lut[mali_userspace::MALI_NAME_BLOCK_SIZE * i_block];
	for (int i = 0; i < mali_userspace::MALI_NAME_BLOCK_SIZE; ++i)
	{
		if (strstr(names[i], i_name) != nullptr)
		{
			return i;
		}
	}

	return -1;
}

uint64_t get_counter_value(mali_userspace::MaliCounterBlockName i_block, const char* i_name)
{
	uint64_t sum = 0;
	switch (i_block)
	{
		case mali_userspace::MALI_NAME_BLOCK_MMU:
			// If an MMU counter is selected, sum the values over MMU slices
			for (int i = 0; i < s_hwInfo.num_l2_slices; i++)
			{
				sum += get_counters_block_base_address(i_block, i)[find_counter_index_by_name(i_block, i_name)];
			}
			return sum;

		case mali_userspace::MALI_NAME_BLOCK_SHADER:
			// If a shader core counter is selected, sum the values over shader cores
			for (int i = 0; i < s_hwInfo.num_cores; i++)
			{
				sum += get_counters_block_base_address(i_block, i)[find_counter_index_by_name(i_block, i_name)];
			}
			return sum;

		case mali_userspace::MALI_NAME_BLOCK_JM:
		case mali_userspace::MALI_NAME_BLOCK_TILER:
		default:
			return static_cast<uint64_t>(get_counters_block_base_address(i_block, 0)[find_counter_index_by_name(i_block, i_name)]);
	}
}

uint64_t get_counter_value(mali_userspace::MaliCounterBlockName i_block, const int i_index)
{
	uint64_t sum = 0;
	switch (i_block)
	{
		case mali_userspace::MALI_NAME_BLOCK_MMU:
			// If an MMU counter is selected, sum the values over MMU slices
			for (int i = 0; i < s_hwInfo.num_l2_slices; i++)
			{
				sum += get_counters_block_base_address(i_block, i)[i_index];
			}
			return sum;

		case mali_userspace::MALI_NAME_BLOCK_SHADER:
			// If a shader core counter is selected, sum the values over shader cores
			for (int i = 0; i < s_hwInfo.num_cores; i++)
			{
				sum += get_counters_block_base_address(i_block, i)[i_index];
			}
			return sum;

		case mali_userspace::MALI_NAME_BLOCK_JM:
		case mali_userspace::MALI_NAME_BLOCK_TILER:
		default:
			return static_cast<uint64_t>(get_counters_block_base_address(i_block, 0)[i_index]);
	}
}

// ---------------------------------------------

void start_mali_profiler()
{
	if (!s_gpuProfilerReady)
	{
		return;
	}

	sample_counters();
	wait_next_event();
}

void stop_mali_profiler()
{
	if (s_profInfo.core_index_remap)
	{
		memory::free(s_profInfo.core_index_remap);
		s_profInfo.core_index_remap = nullptr;
	}
	if (s_profInfo.raw_counter_data)
	{
		memory::free(s_profInfo.raw_counter_data);
		s_profInfo.raw_counter_data = nullptr;
	}
}

void sample_mali_profiler()
{
	if (!s_gpuProfilerReady)
	{
		return;
	}

	sample_counters();
	wait_next_event();

	// fill values
	for (int i = 0; i < (int)gpu_counter_e::count; i++)
	{
		if (s_enabledCounters[i].index >= 0)
		{
			if (s_enabledCounters[i].readFunc)
			{
				s_enabledCounters[i].value = s_enabledCounters[i].readFunc(s_enabledCounters[i].blockName, s_enabledCounters[i].index);
			}
			else
			{
				s_enabledCounters[i].value = 0;
			}
		}
	}
}

uint64_t get_counter_value(const gpu_counter_e i_counter)
{
	if (!s_gpuProfilerReady)
	{
		return 0;
	}

	return s_enabledCounters[(int)i_counter].value;
}

}
