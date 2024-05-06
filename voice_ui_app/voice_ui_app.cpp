/*----------------------------------------------------------------------------
	Copyright 2020-2021 NXP
	SPDX-License-Identifier: BSD-3-Clause
----------------------------------------------------------------------------*/

#include <cstring>
#include <AudioStream.h>
#include <dlfcn.h>

#include "AFEConfigState.h"
#include "RdspAppUtilities.h"
#include "VoiceProcessorImplementation.h"
#include "SignalProcessor_VIT.h"
#include "RdspBuffer.h"

#define VOICESEEKER_OUT_NHOP 200
#define VSLOUTBUFFERSIZE (VOICESEEKER_OUT_NHOP * sizeof(float))

std::string commandUsageStr =
    "Invalid input arguments!\n" \
    "Refer to the following command:\n" \
    "./voicespot <-notify>\n";

#define CHECK(x) \
        do { \
                if (!(x)) { \
                        fprintf(stderr, "%s:%d: ", __func__, __LINE__); \
                        perror(#x); \
                        exit(-1); \
                } \
        } while (0) \

using namespace SignalProcessor;
using namespace AudioStreamWrapper;

static const char * captureOutputName = "default";
static const int captureOutputChannels = 1;
static snd_pcm_format_t format = SND_PCM_FORMAT_S32_LE;
static int period_size = 128;
static int buffer_size = period_size * 4;
static int rate = 16000;

typedef void * (*creator)(void);
typedef void * (*destructor)(SignalProcessor::VoiceProcessorImplementation * impl);

//Define structure for circular buffer
typedef struct {
	char* samples;
	int32_t head, tail;
	size_t size, num_entries;
} queue;

void initQueue(queue* q, size_t samples_delay, size_t maxSize) {
	q->size = maxSize;
	q->samples = (char*)malloc(q->size);
	q->num_entries = 0;
	q->head = 0;
	q->tail = 0;

	//Initialize buffer to zeros
	memset(q->samples, 0, q->size);
}

void queueDestroy(queue* q) {
	free(q->samples);
}

void enqueue(queue* q, const char* samples_ref, size_t sizeBuff) {
	int32_t avail_bytes = q->size - q->tail;

	if (avail_bytes >= sizeBuff) {
		memcpy(q->samples + q->tail, (char*)samples_ref, sizeBuff);
		q->tail = (q->tail + sizeBuff) % q->size;
		q->num_entries += sizeBuff;
	} else {
		memcpy((q->samples) + q->tail, (char*)samples_ref, avail_bytes);
		q->tail = 0;
		int32_t rest_of_samples = sizeBuff - avail_bytes;
		memcpy((q->samples) + q->tail, (char*)samples_ref + avail_bytes, rest_of_samples);
		q->tail = (q->tail + rest_of_samples) % q->size;
		q->num_entries += sizeBuff;
	}
}

const char* dequeue(queue* q, size_t sizeBuff) {
	char* sample_buff = q->samples + q->head;
	q->head = (q->head + sizeBuff) % q->size;

	q->num_entries -= sizeBuff;

	return sample_buff;
}

int match(char *src, int src_len, queue* q, int *index) {

	int found = 0;
	int idx = 0;
	int i,j,a;
	int *dst = (int *)q->samples;
	int *src_tmp = (int *)src;
	int dst_len = q->num_entries;

	/* Samplesize is 4 bytes */
	for (i = 0; i < dst_len/4; i++) {
		for (j = 0; j < src_len/4; j++) {
			a = (j + idx + q->head/4) % (q->size/4);

			if (dst[a] != src_tmp[j])
				break;
		}

		if (j == src_len/4) {
			found = 1;
			*index = idx*4;
			break;
		}
		idx++;
	}

	return found;
}

/**
* @brief Transform buffer from VoiceSpot to VIT process
*
* This function is used for VIT process
*
* @param VIT            SignalProcessor_VIT class
* @param buffer         buffer from VoiceSpot
* @param vit_frame_buf  VIT frame buffer
* @param vit_frame_siz  VIT frame size
*
* @return true if VIT has detection
*/
static bool VoiceSpotToVITProcess(SignalProcessor_VIT &VIT, void *buffer, rdsp_buffer *vit_frame_buf, int vit_frame_size, int *start_offset, bool notify, int32_t iteration) {
	/* Since the frame size is different between VoiceSpot and VIT, a frame buffer is needed for VIT input audio */
	int16_t vit_frame_buffer_lin[VOICESEEKER_OUT_NHOP];
	float* frame_buffer_float = (float *)buffer;
	rdsp_float_to_pcm((char *)vit_frame_buffer_lin, &frame_buffer_float, VOICESEEKER_OUT_NHOP, 1, 2);

	/* Write buffered VoiceSpot audio frame to VIT frame buffer */
	RdspBuffer_WriteInputBlocks(vit_frame_buf, VOICESEEKER_OUT_NHOP, (uint8_t*)vit_frame_buffer_lin);
	bool command_found = false;
	int16_t cmd_id = 0;
	while (RdspBuffer_NumBlocksAvailable(vit_frame_buf, 0) >= (int32_t)vit_frame_size) {
		/* Get vit_frame_buffer_lin samples for voice trigger */
		RdspBuffer_ReadInputBlocks(vit_frame_buf, 0, vit_frame_size, (uint8_t*)vit_frame_buffer_lin);

		/* Run VIT processing */
		command_found = VIT.VIT_Process_Phase(VIT.VIT_Handle, vit_frame_buffer_lin, &cmd_id, start_offset, notify, iteration);
		/* VIT command recognition phase is finalized */
		/* command_found triggered when targeted Voice command is recognized or VIT detection timeout is reached */
		if (command_found) {
			/* Close VIT model */
			return true;
		}
	}
	return false;
}

//VoiceSpot's main
int main(int argc, char *argv[]) {
	ssize_t bytes_read;
	char buffer[VSLOUTBUFFERSIZE];
	int32_t iterations;
	int32_t enable_triggering;
	int32_t keyword_start_offset_samples;
	bool wakewordnotify = false;
	bool micSamplesReady = false;
	bool voice_ww_detect = false;
	std::string libraryDir = "/usr/lib/nxp-afe/";
	std::string libraryName = libraryDir + "libvoicespot.so";

	struct streamSettings captureOutputSettings =
	{
		captureOutputName,
		format,
		StreamType::eInterleaved,
		StreamDirection::eInput,
		captureOutputChannels,
		rate,
		buffer_size,
		period_size,
	};

	int sampleSize = snd_pcm_format_width(format) / 8;
	char* captureBuffer = (char *)calloc(period_size * captureOutputChannels, sampleSize);
	char* tmp_buf = (char*)malloc(VOICESEEKER_OUT_NHOP * sampleSize);
	float* float_buffer = (float*)malloc(sizeof(float) * period_size * captureOutputChannels);
	int tmp_pos = 0;
	int capture_pos = period_size;
	int err;
	int queue_size = VSLOUTBUFFERSIZE * 40;
	queue seekeroutput;
	int index;
	int framenum = 0;
	int frameoffset = 0;
	/* VIT uses a frame size of 480 samples */
	int vit_frame_size = VIT_SAMPLES_PER_30MS_FRAME;
	int vit_frame_count = 3 * 80;  /* 3 seconds*/
	rdsp_buffer vit_frame_buf;
	VIT_Handle_t VITHandle = PL_NULL;
	char *message;
	void *library;
	VoiceProcessorImplementation *impl;
	creator createFce;
	destructor destroyFce;
	struct mq_attr attr;
	mqd_t mqVslOut;
	mqd_t mqIter;
	mqd_t mqTrigg;
	mqd_t mqOffset;
	bool use_voicespot;

	initQueue(&seekeroutput, 0, queue_size);

	if (argc == 2 && !strcmp(argv[1], "-notify"))
		wakewordnotify = true;
	else if (argc > 1) {
		std::cout << commandUsageStr << std::endl;
		exit(1);
	}

	/* vit_frame_buf stores vit input frames */
	/* The size shoud be larger than input frames size */
	RdspBuffer_Create(&vit_frame_buf, 1, sizeof(int16_t), 6 * VOICESEEKER_OUT_NHOP);
	vit_frame_buf.assume_full = 0;

	AFEConfig::AFEConfigState configState;
	std::string WakeWordEngine = configState.isConfigurationEnable("WakeWordEngine", "VoiceSpot");

	if (WakeWordEngine == "VoiceSpot")
		use_voicespot = true;
	else
		use_voicespot = false;

	if (use_voicespot) {
		library = dlopen(libraryName.c_str(), RTLD_NOW);
		message = dlerror();
		if (nullptr != message) {
			std::cout << "Opening library failed: " << message << std::endl;
			use_voicespot = false;
			goto start_detect;
		}

		createFce = (void * (*)())dlsym(library, "createProcessor");
		message = dlerror();
		if (nullptr != message) {
			std::cout << "createProcessor load: " << message << std::endl;
			use_voicespot = false;
			dlclose(library);
			goto start_detect;
		}
		destroyFce = (void *(*)(SignalProcessor::VoiceProcessorImplementation * impl))dlsym(library, "destroyProcessor");
		message = dlerror();
		if (nullptr != message) {
			std::cout << "destroyProcessor load: " << message << std::endl;
			use_voicespot = false;
			dlclose(library);
			goto start_detect;
		}

		impl = (SignalProcessor::VoiceProcessorImplementation *) createFce();
		if (impl == NULL) {
			use_voicespot = false;
			destroyFce(impl);
			dlclose(library);
			goto start_detect;
		}
	}

start_detect:

	SignalProcessor_VIT VIT{};
	VITHandle = VIT.VIT_open_model(!use_voicespot);
	VIT.VIT_Handle = VITHandle;

	//initialize the queue attributes
	attr.mq_flags = 0;
	attr.mq_maxmsg = 10;
	attr.mq_msgsize = VSLOUTBUFFERSIZE;
	attr.mq_curmsgs = 0;

	//create the message queue
	mqVslOut = mq_open("/voicespot_vslout", O_CREAT | O_RDONLY, 0644, &attr);
	attr.mq_msgsize = sizeof(int32_t);
	mqIter = mq_open("/voiceseeker_iterations", O_CREAT | O_RDONLY, 0644, &attr);
	mqTrigg = mq_open("/voiceseeker_trigger", O_CREAT | O_RDONLY, 0644, &attr);
	mqOffset = mq_open("/voicespot_offset", O_CREAT | O_WRONLY, 0644, &attr);

	AudioStream captureOutput;
	captureOutput.open(captureOutputSettings);
	captureOutput.start();

	while (true) {
		while (tmp_pos < VOICESEEKER_OUT_NHOP) {
			if (capture_pos == period_size) {
				err = captureOutput.readFrames(captureBuffer, period_size * captureOutputChannels * sampleSize);
				if (err < 0)
					throw AudioStreamException(snd_strerror(err), "readFrames", __FILE__, __LINE__, err);

				if (period_size == err)
					capture_pos = 0;
				else {
					usleep(10000);
				}
			}

			if (VOICESEEKER_OUT_NHOP - tmp_pos > period_size - capture_pos) {
				memcpy(tmp_buf + tmp_pos* sampleSize,
				       captureBuffer + capture_pos * sampleSize,
				       (period_size - capture_pos)* sampleSize);
				tmp_pos += period_size - capture_pos;
				capture_pos = period_size;
			} else {
				memcpy(tmp_buf + tmp_pos* sampleSize,
				       captureBuffer + capture_pos * sampleSize,
				       (VOICESEEKER_OUT_NHOP - tmp_pos)* sampleSize);
				capture_pos += VOICESEEKER_OUT_NHOP - tmp_pos;
				tmp_pos = VOICESEEKER_OUT_NHOP;
			}
		}

		rdsp_pcm_to_float(tmp_buf, &float_buffer, VOICESEEKER_OUT_NHOP, 1, sampleSize);
		tmp_pos = 0;

		bytes_read = mq_receive(mqVslOut, (char*)buffer, VSLOUTBUFFERSIZE, NULL);
		bytes_read = mq_receive(mqIter, (char*)&iterations, sizeof(int32_t), NULL);
		bytes_read = mq_receive(mqTrigg, (char*)&enable_triggering, sizeof(int32_t), NULL);
		framenum++;

		if (seekeroutput.num_entries >= (queue_size - VSLOUTBUFFERSIZE))
			dequeue(&seekeroutput, VSLOUTBUFFERSIZE);

		enqueue(&seekeroutput, buffer, VSLOUTBUFFERSIZE);

		/* 200 * 100 / 16000 = 1.25s, check the offset between voiceseeker and voicespot */
		if (framenum > 100) {
			if (seekeroutput.num_entries >= 2*VSLOUTBUFFERSIZE) {
				int found = 0;
				found = match(tmp_buf, VSLOUTBUFFERSIZE, &seekeroutput, &index);
				if (found) {
					dequeue(&seekeroutput, index + VSLOUTBUFFERSIZE);
					frameoffset = seekeroutput.num_entries;
					framenum = 0;
				}
			}
		}

		keyword_start_offset_samples = 0;
		if (!use_voicespot) {
			bool VIT_Result = VoiceSpotToVITProcess(VIT, buffer,
								&vit_frame_buf, vit_frame_size,
								&keyword_start_offset_samples,
								wakewordnotify, iterations);
		}
		else if (!voice_ww_detect) {
			keyword_start_offset_samples = impl->processSignal(buffer, wakewordnotify, iterations, enable_triggering);
			if (keyword_start_offset_samples){
				voice_ww_detect = true;
				vit_frame_count = 3* 80;
			}
		}

		keyword_start_offset_samples += frameoffset / sampleSize;
		CHECK(0 <= mq_send(mqOffset, (char*)&keyword_start_offset_samples, sizeof(int32_t), 0));

		if (voice_ww_detect) {
			voice_ww_detect = !VoiceSpotToVITProcess(VIT, buffer,
								 &vit_frame_buf, vit_frame_size,
								 &keyword_start_offset_samples,
								 wakewordnotify, iterations);
			vit_frame_count--;
			if (!vit_frame_count)
				voice_ww_detect = false;
		}
	}

	mq_close(mqVslOut);
	mq_close(mqIter);
	mq_close(mqTrigg);
	mq_close(mqOffset);

	if (use_voicespot){
		impl->closeProcessor();
		destroyFce(impl);
		dlclose(library);
	}

	/* Close VIT model */
	VIT.VIT_close_model(VITHandle);
	RdspBuffer_Destroy(&vit_frame_buf);
	free(captureBuffer);
	free(tmp_buf);
	free(float_buffer);

	return 0;
}

