/*----------------------------------------------------------------------------
    Copyright 2024 NXP
    SPDX-License-Identifier: BSD-3-Clause
----------------------------------------------------------------------------*/
#include <mqueue.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

#ifndef __VoiceProcessorImplementation_h__
#define __VoiceProcessorImplementation_h__

namespace SignalProcessor
{
	#define GET_MAJOR_VERSION(version)      (uint32_t)((0xFF000000u & version) >> 24u)
	#define GET_MINOR_VERSION(version)      (uint32_t)((0x00FF0000u & version) >> 16u)
	#define GET_PATCH_VERSION(version)      (uint32_t)((0x0000FF00u & version) >> 8u)

	class VoiceProcessorImplementation
	{
		public:
			virtual ~VoiceProcessorImplementation() {};
			/**
			 * Returns the implementation version number
			 * @details The version number should be stored in a 32 bit long variable.
			 * Bits 0 - 7 are reserved
			 * Bits 8 - 15 hold the patch version
			 * Bits 16 - 23 hold the minor version
			 * Bits 24 - 31 hold the major version
			 */
			virtual uint32_t getVersionNumber(void) const = 0;
			/**
			 * @details Responsibility of this function is to allocate all resources
			 * required by the signal processor and initialize the signal processor
			 * according to input settings.\n
			 * The input settings may differ from vendor to vendor, thus an unordered map
			 * is provided for flexibility.\n
			 * Its also possible not to provide the settings. In such case default settings
			 * are used as defined by the specialized implementing class.
			 *
			 * @param[in] settings Signal processor configuration
			 *
			 * @remark To have as generic as possible interface, we need to define the settings
			 * as unorderd map. The configuration will be provided as a key:value pair. From
			 * this perspective it may be unclear, what can be set by the user, thus it's
			 * mandatory to implement the getSettingsDefinition function which should
			 * print out the required key:value pairs.
			 *
			 * @see setDefaultSettings, processSignal, closeProcessor, getJsonConfigurations
			 */
			virtual int openProcessor(const std::unordered_map<std::string, std::string> * settings = nullptr) = 0;
			/**
			 * @details This function should deallocate all resources initialized
			 * during signal processor opening, revert settings back to default and close
			 * the signal processor.\n
			 * If there is nothing to be closed (processor has not been opened yet or so),
			 * silently assume closing was successful returning ok status.
			 */
			virtual int closeProcessor(void) = 0;
			/**
			 * @details This function should handle the data from voiceseeker
			 */
			virtual int
			processSignal(void* vsl_out, bool notify, int32_t iteration, int32_t enable_triggering) = 0;

		protected:
	};
}
#endif
