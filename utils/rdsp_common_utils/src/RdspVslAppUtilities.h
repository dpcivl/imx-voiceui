/*
 * Copyright (c) 2021 by Retune DSP. This code is the confidential and
 * proprietary property of Retune DSP and the possession or use of this
 * file requires a written license from Retune DSP.
 *
 * Contact information: info@retune-dsp.com
 *                      www.retune-dsp.com
 */

#ifndef RDSP_VOICESEEKERLIGHT_APP_UTILITIES_H
#define RDSP_VOICESEEKERLIGHT_APP_UTILITIES_H

#include "libVoiceSeekerLight.h"
#include "public/rdsp_voicespot.h"
#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

	/*
	 * VoiceSpot utilities
	 */

	void rdsp_import_voicespot_model(const char* Afilename, uint8_t** Amodel, uint32_t* Amodel_size);
	int32_t rdsp_set_voicespot_params(rdsp_voicespot_control* Avoicespot_control, int32_t Avoicespot_handle, const char* Avoicespot_params);

	/*
	 * Performance log file
	 */

	int32_t rdsp_write_performance_file_header(RETUNE_VOICESEEKERLIGHT_plugin_t* APluginInit, FILE* Afid);
	int32_t rdsp_write_performance_log(int32_t Atrigger, int32_t Atrigger_sample, int32_t Astart_sample, int32_t Astop_sample, int32_t Ascore, FILE* Afid);

	/*
	 * App utilities
	 */

	void csv_to_array(int32_t* Aarray, FILE* Afid);

#ifdef __cplusplus
}
#endif

#endif // RDSP_VOICESEEKERLIGHT_APP_UTILITIES_H
