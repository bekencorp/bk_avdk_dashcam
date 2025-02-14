// Copyright 2023-2024 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stdio.h"
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <modules/pm.h>
#include "mp3_play.h"


#define MP3_PLAY_TAG  "mp3_play"
#define LOGI(...) BK_LOGI(MP3_PLAY_TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(MP3_PLAY_TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(MP3_PLAY_TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(MP3_PLAY_TAG, ##__VA_ARGS__)


int mp3_codec_out_data_handle(audio_frame_info_t *frame_info, char *buffer, uint32_t len, void *params)
{
    LOGD("channel_number: %d, sample_rate: %d, sample_bits: %d\n", frame_info->channel_number, frame_info->sample_rate, frame_info->sample_bits);

    mp3_play_t *mp3_play = (mp3_play_t *)params;

    /* check and create audio play */
    if (!mp3_play->play)
    {
        /* create audio play according to frameinfo */
        LOGI("%s, %d, create audio play\n", __func__, __LINE__);
        mp3_play->config.play_cfg.nChans = frame_info->channel_number;
        mp3_play->config.play_cfg.sampRate = frame_info->sample_rate;
        mp3_play->config.play_cfg.bitsPerSample = frame_info->sample_bits;
        mp3_play->play = audio_play_create(AUDIO_PLAY_ONBOARD_SPEAKER, &mp3_play->config.play_cfg);
        if (!mp3_play->play)
        {
            LOGE("%s, %d, create audio play handle fail\n", __func__, __LINE__);
            return BK_FAIL;
        }
        /* open audio play */
        if (BK_OK != audio_play_open(mp3_play->play))
        {
            LOGE("%s, %d, open audio play fail\n", __func__, __LINE__);
            audio_play_destroy(mp3_play->play);
            mp3_play->play = NULL;
            return BK_FAIL;
        }
    }

    uint32_t w_len = 0;
    int ret = 0;
    while (w_len < len)
    {
        ret = audio_play_write_data(mp3_play->play, buffer + w_len, len - w_len);
        if (ret <= 0)
        {
            LOGE("%s, %d, audio_play_write_data fail, ret: %d\n", __func__, __LINE__, ret);
            break;
        }
        w_len += ret;
    }

    return w_len;
}

int play_pool_empty_notify(void *play_ctx, void *params)
{
    LOGD("%s, %d, play_ctx: %p, params: %p\n", __func__, __LINE__, play_ctx, params);

    mp3_play_t *mp3_play = (mp3_play_t *)params;

    if (mp3_play->play_finish_sem)
    {
        rtos_set_semaphore(&mp3_play->play_finish_sem);
    }

    return BK_OK;
}


mp3_play_t *mp3_play_create(  mp3_play_cfg_t *config)
{
    LOGI("%s\n", __func__);

    mp3_play_t *mp3_play = psram_malloc(sizeof(mp3_play_t));
    if (!mp3_play)
    {
        LOGE("%s, %d, malloc mp3 play handle fail\n", __func__, __LINE__);
        return NULL;
    }
    os_memset(mp3_play, 0, sizeof(mp3_play_t));

    if (BK_OK != rtos_init_semaphore(&mp3_play->play_finish_sem, 1))
    {
        LOGE("%s, %d, ceate semaphore fail\n", __func__, __LINE__);
        goto fail;
    }

    config->play_cfg.pool_empty_notify_cb = play_pool_empty_notify;
    config->play_cfg.usr_data = mp3_play;
    config->codec_cfg.data_handle = mp3_codec_out_data_handle;
    config->codec_cfg.usr_data = mp3_play;
    mp3_play->codec = audio_codec_create(AUDIO_CODEC_MP3, &config->codec_cfg);
    if (!mp3_play->codec)
    {
        LOGE("%s, %d, create audio codec handle fail\n", __func__, __LINE__);
        goto fail;
    }

    os_memcpy(&mp3_play->config, config, sizeof(mp3_play_cfg_t));

    return mp3_play;

fail:

    if (mp3_play->play_finish_sem)
    {
        rtos_deinit_semaphore(&mp3_play->play_finish_sem);
        mp3_play->play_finish_sem = NULL;
    }

    if (mp3_play->codec)
    {
        audio_codec_destroy(mp3_play->codec);
        mp3_play->codec = NULL;
    }

    if (mp3_play)
    {
        psram_free(mp3_play);
        mp3_play = NULL;
    }

    return NULL;
}

bk_err_t mp3_play_destroy(mp3_play_t *mp3_play)
{
    LOGI("%s\n", __func__);
    if (!mp3_play)
    {
        LOGW("%s, %d, mp3_play already destroy\n", mp3_play);
        return BK_OK;
    }

    if (mp3_play->codec)
    {
        audio_codec_destroy(mp3_play->codec);
        mp3_play->codec = NULL;
    }

    if (mp3_play->play)
    {
        audio_play_destroy(mp3_play->play);
        mp3_play->play = NULL;
    }

    psram_free(mp3_play);

    return BK_OK;
}

bk_err_t mp3_play_open(mp3_play_t *mp3_play)
{
    if (!mp3_play)
    {
        LOGE("%s, %d, mp3_play is NULL\n");
        return BK_FAIL;
    }

    if (!mp3_play->codec)
    {
        LOGE("%s, %d, mp3_play->codec is NULL\n");
        return BK_FAIL;
    }

    audio_codec_open(mp3_play->codec);

    return BK_OK;
}

bk_err_t mp3_play_close(mp3_play_t *mp3_play, bool wait_play_finish)
{
    LOGI("%s\n", __func__);
    if (!mp3_play)
    {
        LOGE("%s, %d, mp3_play is NULL\n");
        return BK_FAIL;
    }

    if (wait_play_finish)
    {
        if (mp3_play->play_finish_sem)
        {
            rtos_get_semaphore(&mp3_play->play_finish_sem, BEKEN_NEVER_TIMEOUT);
        }
    }

    if (mp3_play->play_finish_sem)
    {
        rtos_deinit_semaphore(&mp3_play->play_finish_sem);
        mp3_play->play_finish_sem = NULL;
    }

    if (mp3_play->codec)
    {
        audio_codec_close(mp3_play->codec);
    }

    if (mp3_play->play)
    {
        audio_play_close(mp3_play->play);
    }

    return BK_OK;
}

bk_err_t mp3_play_write_data(mp3_play_t *mp3_play, char *buffer, uint32_t len)
{
    return audio_codec_write_data(mp3_play->codec, buffer, len);
}

