// Copyright 2024-2025 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#ifndef __AUDIO_CODEC_H__
#define __AUDIO_CODEC_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    AUDIO_CODEC_STA_IDLE = 0,
    AUDIO_CODEC_STA_RUNNING,
    AUDIO_CODEC_STA_PAUSED,
} audio_codec_sta_t;

typedef enum
{
    AUDIO_CODEC_UNKNOWN = 0,

    AUDIO_CODEC_MP3,
} audio_codec_type_t;

typedef struct audio_info_s
{
    int channel_number;
    int sample_rate;
    int sample_bits;
} audio_frame_info_t;


typedef struct audio_codec audio_codec_t;

typedef int (*codec_out_data_handle)(audio_frame_info_t *frame_info, char *buffer, uint32_t len, void *params);
typedef int (*data_empty_cb)(audio_codec_t *codec);


typedef struct
{
    uint32_t chunk_size;             /*!< the size (unit byte) of ringbuffer pool saved data need to codec */
    uint32_t pool_size;
    codec_out_data_handle data_handle;
    data_empty_cb empty_cb;
    void *usr_data;
} audio_codec_cfg_t;

#define DEFAULT_CHUNK_SIZE      (4608)
#define DEFAULT_POOL_SIZE       (1940*2)

#define DEFAULT_AUDIO_CODEC_CONFIG() {      \
    .chunk_size = DEFAULT_CHUNK_SIZE,       \
    .pool_size = DEFAULT_POOL_SIZE,         \
    .data_handle = NULL,                    \
    .data_empty_cb = NULL,                  \
    .usr_data = NULL,                       \
}


typedef struct
{
    int (*open)(audio_codec_t *codec, audio_codec_cfg_t *config);
    //int (*get_frame_info)(audio_codec_t *codec);
    int (*write)(audio_codec_t *codec, char *buffer, uint32_t len);
    int (*close)(audio_codec_t *codec);
} audio_codec_ops_t;

struct audio_codec
{
    audio_codec_ops_t *ops;

    audio_codec_cfg_t config;

    void *codec_ctx;
};



/**
 * @brief     Create audio play with config
 *
 * This API create audio play handle according to play type and config.
 * This API should be called before other api.
 *
 * @param[in] play_type The type of play
 * @param[in] config    Play config used in audio_play_open api
 *
 * @return
 *    - Not NULL: success
 *    - NULL: failed
 */
audio_codec_t *audio_codec_create(  audio_codec_type_t codec_type, audio_codec_cfg_t *config);

/**
 * @brief      Destroy audio play
 *
 * This API Destroy audio play according to audio play handle.
 *
 *
 * @param[in] play  The audio play handle
 *
 * @return
 *    - BK_OK: success
 *    - NULL: failed
 */
bk_err_t audio_codec_destroy(audio_codec_t *codec);

/**
 * @brief      Open audio play
 *
 * This API open audio play and start play.
 *
 *
 * @param[in] play  The audio play handle
 *
 * @return
 *    - BK_OK: success
 *    - NULL: failed
 */
bk_err_t audio_codec_open(audio_codec_t *codec);

/**
 * @brief      Close audio play
 *
 * This API stop play and close audio play.
 *
 *
 * @param[in] play  The audio play handle
 *
 * @return
 *    - BK_OK: success
 *    - NULL: failed
 */
bk_err_t audio_codec_close(audio_codec_t *codec);

/**
 * @brief      Write speaker data to audio play
 *
 * This API write speaker data to pool.
 * If memory in pool is not enough, wait until the pool has enough memory.
 *
 *
 * @param[in] play      The audio play handle
 * @param[in] buffer    The speaker data buffer
 * @param[in] len       The length (byte) of speaker data
 *
 * @return
 *    - BK_OK: success
 *    - NULL: failed
 */
bk_err_t audio_codec_write_data(audio_codec_t *codec, char *buffer, uint32_t len);

/**
 * @brief      Control audio play
 *
 * This API can control audio play, such as pause, resume and so on.
 *
 *
 * @param[in] play  The audio play handle
 * @param[in] ctl   The control opcode
 *
 * @return
 *    - BK_OK: success
 *    - NULL: failed
 */
//bk_err_t audio_codec_get_frame_info(audio_codec_t *codec);

#ifdef __cplusplus
}
#endif
#endif /* __AUDIO_CODEC_H__ */

