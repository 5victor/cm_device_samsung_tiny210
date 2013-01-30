
#define LOG_TAG "audio_hw_primary"
#define LOG_NDEBUG 0
#define LOG_NDEBUG_FUNCTION
#ifndef LOG_NDEBUG_FUNCTION
#define LOGFUNC(...) ((void)0)
#else
#define LOGFUNC(...) (ALOGV(__VA_ARGS__))
#endif

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <tinyalsa/asoundlib.h>
#include <audio_utils/resampler.h>
#include <audio_utils/echo_reference.h>
#include <hardware/audio_effect.h>
#include <audio_effects/effect_aec.h>

#define DEFAULT_OUT_SAMPLING_RATE 44100

#define MIN_WRITE_SLEEP_US 5000

#define PERIOD_SIZE (24 * 80)

#define PLAYBACK_PERIOD_COUNT 4


struct pcm_config pcm_config_mm = {
	.channels = 2,
	.rate = DEFAULT_OUT_SAMPLING_RATE,
	.period_size = PERIOD_SIZE,
	.period_count = PLAYBACK_PERIOD_COUNT,
	.format = PCM_FORMAT_S16_LE,
};


struct mini210_stream_out {
	struct audio_stream_out stream;
	
	struct pcm_config config;
	int standby;
	pthread_mutex_t lock;
	struct pcm *pcm;
	int write_threshold;

	struct mini210_audio_device *dev;
};

struct mini210_stream_in {
	struct audio_stream_in stream;

	pthread_mutex_t lock;
	struct pcm *pcm;
};

struct mini210_audio_device {
	struct audio_hw_device hw_device;

	pthread_mutex_t lock;
	struct mini210_stream_in *active_input;
	struct mini210_stream_out *active_output;
	struct mixer *mixer;
};

static void set_output_volumes(struct mini210_audio_device *dev)
{
	struct mixer_ctl *right_switch, *left_switch;
	struct mixer_ctl *head_volume;

	LOGFUNC("%s", __FUNCTION__);

	right_switch = mixer_get_ctl_by_name(dev->mixer,
			"Right Output Mixer PCM Playback Switch");
	mixer_ctl_set_value(right_switch, 0, 1);

	left_switch = mixer_get_ctl_by_name(dev->mixer,
			"Left Output Mixer PCM Playback Switch");
	mixer_ctl_set_value(left_switch, 0, 1);

	head_volume = mixer_get_ctl_by_name(dev->mixer,
			"Headphone Playback Volume");
	mixer_ctl_set_value(head_volume, 0, 127);
	mixer_ctl_set_value(head_volume, 1, 127);
}

static int start_output_stream(struct mini210_stream_out *out)
{
	struct mini210_audio_device *adev = out->dev;

	out->write_threshold = PLAYBACK_PERIOD_COUNT * PERIOD_SIZE;
	out->config.start_threshold = PERIOD_SIZE * 2;
	out->config.avail_min = PERIOD_SIZE;

	out->pcm = pcm_open(0, 0, PCM_OUT | PCM_MMAP, &out->config);

	if (!pcm_is_ready(out->pcm)) {
		ALOGE("cannot open pcm_out driver: %s",
				pcm_get_error(out->pcm));
		pcm_close(out->pcm);
		return -ENOMEM;
	}
	

	return 0;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
	LOGFUNC("%s(%p)", __FUNCTION__, stream);

	return DEFAULT_OUT_SAMPLING_RATE;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
	LOGFUNC("%s(%p, %d)", __FUNCTION__, stream, rate);

	return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
	struct mini210_stream_out *out = (struct mini210_stream_out *)stream;
	size_t size;
	
	LOGFUNC("%s(%p)", __FUNCTION__, stream);

	size  = (PERIOD_SIZE * DEFAULT_OUT_SAMPLING_RATE) / out->config.rate;

	/* multiple 16 */
	size = ((size + 15) / 16) * 16;

	return size * audio_stream_frame_size(stream);
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
/*	LOGFUNC("%s(%p)", __FUNCTION__, stream); */

	return AUDIO_CHANNEL_OUT_STEREO;
}

static int out_get_format(const struct audio_stream *stream)
{
/*	LOGFUNC("%s(%p)", __FUNCTION__, stream); */

	return AUDIO_FORMAT_PCM_16_BIT;
}

static int out_set_format(struct audio_stream *stream, int format)
{
	LOGFUNC("%s(%p)", __FUNCTION__, stream);

	return 0;
}

static int out_standby(struct audio_stream *stream)
{
	LOGFUNC("%s(%p)", __FUNCTION__, stream);

	return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
	LOGFUNC("%s(%p, %d)", __FUNCTION__, stream, fd);

	return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
	LOGFUNC("%s(%p, %s)", __FUNCTION__, stream, kvpairs);

	return 0;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
	LOGFUNC("%s(%p, %s)", __FUNCTION__, stream, keys);

	return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
	struct mini210_stream_out *out = (struct mini210_stream_out *)stream;
/*	LOGFUNC("%s(%p)", __FUNCTION__, stream); */

	return (out->config.period_size * out->config.period_count * 1000) / out->config.rate;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
			float right)
{
	LOGFUNC("%s(%p, %f, %f)", __FUNCTION__, stream, left, right);

	return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
			size_t bytes)
{
	struct mini210_stream_out *out = (struct mini210_stream_out *)stream;
	int ret;
	int kernel_frames;

/*	LOGFUNC("%s(%p, %p, %d)", __FUNCTION__, stream, buffer, bytes); */

	pthread_mutex_lock(&out->lock);
	if (out->standby) {
		ret = start_output_stream(out);
		if (ret != 0) {
			pthread_mutex_unlock(&out->lock);
			return ret;
		}
		out->standby = 0;
	}

	do {
		struct timespec time_stamp;

		if (pcm_get_htimestamp(out->pcm,
			(unsigned int *)&kernel_frames, &time_stamp) < 0)
			break;

		kernel_frames = pcm_get_buffer_size(out->pcm) - kernel_frames;
		if (kernel_frames > out->write_threshold) {
			unsigned long time = (unsigned long)(((int64_t)
			(kernel_frames - out->write_threshold) * 1000000) /
			DEFAULT_OUT_SAMPLING_RATE);
			
		if (time < MIN_WRITE_SLEEP_US)
			time = MIN_WRITE_SLEEP_US;
		usleep(time);
		}
	} while (kernel_frames > out->write_threshold);

	ret = pcm_mmap_write(out->pcm, buffer, bytes);

	pthread_mutex_unlock(&out->lock);	
	return ret;
}

static int out_get_render_position(const struct audio_stream_out *stream,
			uint32_t *dsp_frames)
{
	LOGFUNC("%s(%p, %p)", __FUNCTION__, stream, dsp_frames);

	return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
	LOGFUNC("%s(%p, %p)", __FUNCTION__, stream, effect);

	return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
	LOGFUNC("%s(%p, %p)", __FUNCTION__, stream, effect);

	return 0;
}

static int adev_close(hw_device_t *device)
{
	LOGFUNC("%s", __FUNCTION__);

	free(device);
	return 0;
}

static uint32_t adev_get_supported_devices(const struct audio_hw_device *dev)
{
	LOGFUNC("%s", __FUNCTION__);

	return (/* OUT */
		AUDIO_DEVICE_OUT_SPEAKER |
		AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
		AUDIO_DEVICE_OUT_DEFAULT |
		/* IN */
		AUDIO_DEVICE_IN_BUILTIN_MIC |
		AUDIO_DEVICE_IN_WIRED_HEADSET |
		AUDIO_DEVICE_IN_DEFAULT);
}

static int adev_init_check(const struct audio_hw_device *dev)
{
	LOGFUNC("%s", __FUNCTION__);

	return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
	LOGFUNC("%s(%p, %f)", __FUNCTION__, dev, volume);

	return 0;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
	struct mini210_audio_device *adev = (struct mini210_audio_device *)dev;
	LOGFUNC("%s(%p, %f)", __FUNCTION__, dev, volume);

	set_output_volumes(adev);
	return 0;
}

static int adev_set_mode(struct audio_hw_device *dev, int mode)
{
	struct mini210_audio_device *adev = (struct mini210_audio_device *)dev;

	LOGFUNC("%s(%p, %d)", __FUNCTION__, dev, mode);

	return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
	LOGFUNC("%s(%p, %d)", __FUNCTION__, dev, state);

	return 0;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
	LOGFUNC("%s(%p, %p)", __FUNCTION__, dev, state);

	return 0;
}

static int adev_set_parameters(struct audio_hw_device *dev,
		const char *kvpairs)
{
	LOGFUNC("%s(%p, %s)", __FUNCTION__, dev, kvpairs);
	
	return 0;
}

static char *adev_get_parameters(const struct audio_hw_device *dev,
		const char *keys)
{
	LOGFUNC("%s(%p, %s)", __FUNCTION__, dev, keys);

	return strdup("");
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
		const struct audio_config *config)
{
	LOGFUNC("%s(%p, %d, %d, %d)", __FUNCTION__, dev, config->sample_rate,
			config->format, popcount(config->channel_mask));

	return 0;
}

static int adev_open_input_stream(struct audio_hw_device *dev,
		audio_io_handle_t handle,
		audio_devices_t devices,
		struct audio_config *config,
		struct audio_stream_in **stream_in)
{
	LOGFUNC("%s", __FUNCTION__);

	return 0;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
								   struct audio_stream_in *stream)
{
	LOGFUNC("%s", __FUNCTION__);

	return ;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
	LOGFUNC("%s(%p, %d)", __FUNCTION__, device, fd);

	return 0;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
		audio_io_handle_t handle,
		audio_devices_t devices,
		audio_output_flags_t flags,
		struct audio_config *config,
		struct audio_stream_out **stream_out)
{
	struct mini210_stream_out *out;
	int ret;

	LOGFUNC("%s:devices=%d, audio_config:sample_rate=%d,"
		"channel=%d,format=%d",
		__FUNCTION__, devices, config->sample_rate,
		popcount(config->channel_mask), config->format);

	out = calloc(1, sizeof(struct mini210_stream_out));
	if (!out)
		return -ENOMEM;

	out->stream.common.get_sample_rate = out_get_sample_rate;
	out->stream.common.set_sample_rate = out_set_sample_rate;
	out->stream.common.get_buffer_size = out_get_buffer_size;
	out->stream.common.get_channels = out_get_channels;
	out->stream.common.get_format = out_get_format;
	out->stream.common.set_format = out_set_format;
	out->stream.common.standby = out_standby;
	out->stream.common.dump = out_dump;
	out->stream.common.set_parameters = out_set_parameters;
	out->stream.common.get_parameters = out_get_parameters;
	out->stream.common.add_audio_effect = out_add_audio_effect;
	out->stream.common.remove_audio_effect = out_remove_audio_effect;
	out->stream.get_latency = out_get_latency;
	out->stream.set_volume = out_set_volume;
	out->stream.write = out_write;
	out->stream.get_render_position = out_get_render_position;
	
	out->config = pcm_config_mm;
	out->standby = 1;

	config->format = out_get_format(&out->stream.common);
	config->channel_mask = out_get_channels(&out->stream.common);
	config->sample_rate = out_get_sample_rate(&out->stream.common);

	*stream_out = &out->stream;	

	return 0;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
		struct audio_stream_out *stream)
{
	LOGFUNC("%s", __FUNCTION__);

	return ;
}

static int adev_open(const struct hw_module_t *module, const char* id,
	struct hw_device_t ** device)
{
	struct mini210_audio_device *adev;
	int ret;

	LOGFUNC("%s:%s", __FUNCTION__, id);

	if (strcmp(id, AUDIO_HARDWARE_INTERFACE) != 0)
		return -EINVAL;

	adev = calloc(1, sizeof(struct mini210_audio_device));
	if (!adev)
		return -ENOMEM;

	adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
	adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_1_0;
	adev->hw_device.common.module = (struct hw_module_t *) module;
	adev->hw_device.common.close = adev_close;

	adev->hw_device.get_supported_devices = adev_get_supported_devices;
	adev->hw_device.init_check = adev_init_check;
	adev->hw_device.set_voice_volume = adev_set_voice_volume;
	adev->hw_device.set_master_volume = adev_set_master_volume;
	adev->hw_device.set_mode = adev_set_mode;
	adev->hw_device.set_mic_mute = adev_set_mic_mute;
	adev->hw_device.get_mic_mute = adev_get_mic_mute;
	adev->hw_device.set_parameters = adev_set_parameters;
	adev->hw_device.get_parameters = adev_get_parameters;
	adev->hw_device.get_input_buffer_size = adev_get_input_buffer_size;
	adev->hw_device.open_output_stream = adev_open_output_stream;
	adev->hw_device.close_output_stream = adev_close_output_stream;
	adev->hw_device.open_input_stream = adev_open_input_stream;
	adev->hw_device.close_input_stream = adev_close_input_stream;
	adev->hw_device.dump = adev_dump;

	adev->mixer = mixer_open(0);
	if (!adev->mixer) {
		free(adev);
		ALOGE("Unable to open the mixer, aborting.");
		return -EINVAL;
	}

	*device = &adev->hw_device.common;

	return 0;
} 

static struct hw_module_methods_t hal_module_methods = {
	.open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.module_api_version = AUDIO_MODULE_API_VERSION_0_1,
		.hal_api_version = HARDWARE_HAL_API_VERSION,
		.id = AUDIO_HARDWARE_MODULE_ID,
		.name = "tiny210 audio HW HAL",
		.author = "Victor Wen",
		.methods = &hal_module_methods,
	},
};
