#ifndef SAMSUNG_DP_DMA_H
#define SAMSUNG_DP_DMA_H

#define AUDIO_DISABLE 0
#define AUDIO_ENABLE 1
#define AUDIO_WAIT_BUF_FULL 2
#define AUDIO_DMA_REQ_HIGH 3

#define DP_AUDIO_COUNT 2
#define DP_AUDIO_PARAM_CHANNEL_OFFSET 0
#define DP_AUDIO_PARAM_WIDTH_OFFSET 16
#define DP_AUDIO_PARAM_RATE_OFFSET 19

enum audio_sampling_frequency {
	FS_32KHZ	= 0,
	FS_44KHZ	= 1,
	FS_48KHZ	= 2,
	FS_88KHZ	= 3,
	FS_96KHZ	= 4,
	FS_176KHZ	= 5,
	FS_192KHZ	= 6,
};

enum audio_bit_per_channel {
	AUDIO_16_BIT = 0,
	AUDIO_20_BIT,
	AUDIO_24_BIT,
};

enum audio_16bit_dma_mode {
	NORMAL_MODE = 0,
	PACKED_MODE = 1,
	PACKED_MODE2 = 2,
};

enum audio_dma_word_length {
	WORD_LENGTH_1 = 0,
	WORD_LENGTH_2,
	WORD_LENGTH_3,
	WORD_LENGTH_4,
	WORD_LENGTH_5,
	WORD_LENGTH_6,
	WORD_LENGTH_7,
	WORD_LENGTH_8,
};

enum audio_param {
	DPADO_RATE,
	DPADO_WIDTH,
	DPADO_CHANNEL,
	DPADO_PARAM_COUNT,
};

struct displayport_audio_config_data {
	u32 audio_enable;
	u32 audio_channel_cnt;
	enum audio_sampling_frequency audio_fs;
	enum audio_bit_per_channel audio_bit;
	enum audio_16bit_dma_mode audio_packed_mode;
	enum audio_dma_word_length audio_word_length;
};

struct dp_audio_pdata {
	u32 id;
	u32 fifo_addr;
	u32 rate;
	u32 width;
	u32 channel;
};

#if IS_ENABLED(CONFIG_EXYNOS_MIPI_DISPLAYPORT) || IS_ENABLED(CONFIG_EXYNOS_DISPLAYPORT)
extern int displayport_audio_config(u32 sst_id, struct displayport_audio_config_data *audio_config_data);
#else
int displayport_audio_config(u32 sst_id, struct displayport_audio_config_data *audio_config_data)
{
	return -ENODEV;
}
#endif

#endif /* SAMSUNG_DP_DMA_H */
