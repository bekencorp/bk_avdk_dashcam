// Copyright 2020-2021 Beken
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

#ifndef __MP3_PLAY_H__
#define __MP3_PLAY_H__


#ifdef __cplusplus
extern "C" {
#endif

#include "audio_play.h"
#include "audio_codec.h"


typedef struct
{
    audio_play_cfg_t play_cfg;
    audio_codec_cfg_t codec_cfg;
} mp3_play_cfg_t;

typedef struct
{
    audio_play_t *play;
    audio_codec_t *codec;
    mp3_play_cfg_t config;
    beken_semaphore_t play_finish_sem;
} mp3_play_t;

#define DEFAULT_MP3_PLAY_CONFIG() {             \
    .play_cfg = {                               \
        .port = 0,                              \
        .nChans = 1,                            \
        .sampRate = 44100,                      \
        .bitsPerSample = 16,                    \
        .volume = 0x2d,                         \
        .play_mode = AUDIO_PLAY_MODE_DIFFEN,    \
        .frame_size = 4608,                     \
        .pool_size = 9216,                      \
        .pool_empty_notify_cb = NULL,           \
        .usr_data = NULL,                       \
    },                                          \
    .codec_cfg = {                              \
        .chunk_size = DEFAULT_CHUNK_SIZE,       \
        .pool_size = DEFAULT_POOL_SIZE,         \
        .data_handle = NULL,                    \
        .empty_cb = NULL,                       \
        .usr_data = NULL,                       \
    },                                          \
}

/**
 * @brief     Create mp3 player with config
 *
 * This API create mp3 play handle according to config.
 * This API should be called before other api.
 *
 * @param[in] config    Mp3 play config
 *
 * @return
 *    - Not NULL: success
 *    - NULL: failed
 */
mp3_play_t *mp3_play_create(  mp3_play_cfg_t *config);

/**
 * @brief      Destroy mp3 play
 *
 * This API Destroy mp3 play according to mp3 play handle.
 *
 *
 * @param[in] mp3_play  The mp3 play handle
 *
 * @return
 *    - BK_OK: success
 *    - Others: failed
 */
bk_err_t mp3_play_destroy(mp3_play_t *mp3_play);

/**
 * @brief      Open mp3 play
 *
 * This API open mp3 play and start decode speaker data to play.
 *
 *
 * @param[in] mp3_play  The mp3 play handle
 *
 * @return
 *    - BK_OK: success
 *    - Others: failed
 */
bk_err_t mp3_play_open(mp3_play_t *mp3_play);

/**
 * @brief      Close mp3 play
 *
 * This API close mp3 play
 *
 *
 * @param[in] mp3_play  The mp3 play handle
 * @param[in] wait_play_finish  The flag to declare whether waiting play pool data finish
 *
 * @return
 *    - BK_OK: success
 *    - Others: failed
 */
bk_err_t mp3_play_close(mp3_play_t *mp3_play, bool wait_play_finish);

/**
 * @brief      Write mp3 data to mp3 play
 *
 * This API write mp3 data to decoder pool.
 * If memory in pool is not enough, wait until the pool has enough memory.
 *
 *
 * @param[in] mp3_play      The mp3 play handle
 * @param[in] buffer    The mp3 data buffer
 * @param[in] len       The length (byte) of mp3 data
 *
 * @return
 *    - len: length of the written data
 *    - Others: failed
 */
bk_err_t mp3_play_write_data(mp3_play_t *mp3_play, char *buffer, uint32_t len);


#ifdef __cplusplus
}
#endif
#endif /* __MP3_PLAY_H__ */

