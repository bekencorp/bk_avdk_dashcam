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
#include "ff.h"
#include "diskio.h"
#include "mp3_play.h"
#include "cli.h"


#define TAG  "AUD_PLAY_SDCARD_MP3"


#define SD_READ_BUFF_SIZE       (1940)

typedef struct {
    FIL mp3file;
    char mp3_file_name[50];

    char *sd_read_buf;

    mp3_play_t *mp3_play;
} audio_play_info_t;


static audio_play_info_t *audio_play_info = NULL;
static FATFS *pfs = NULL;
static bool audio_play_running = false;
static beken_thread_t audio_play_task_hdl = NULL;
static beken_semaphore_t audio_play_sem = NULL;


static bk_err_t tf_mount(void)
{
	FRESULT fr;

	if (pfs != NULL)
	{
		os_free(pfs);
	}

	pfs = psram_malloc(sizeof(FATFS));
	if(NULL == pfs)
	{
		BK_LOGI(TAG, "f_mount malloc failed!\r\n");
		return BK_FAIL;
	}

	fr = f_mount(pfs, "1:", 1);
	if (fr != FR_OK)
	{
		BK_LOGE(TAG, "f_mount failed:%d\r\n", fr);
		return BK_FAIL;
	}
	else
	{
		BK_LOGI(TAG, "f_mount OK!\r\n");
	}

	return BK_OK;
}

static bk_err_t tf_unmount(void)
{
	FRESULT fr;
	fr = f_unmount(DISK_NUMBER_SDIO_SD, "1:", 1);
	if (fr != FR_OK)
	{
		BK_LOGE(TAG, "f_unmount failed:%d\r\n", fr);
		return BK_FAIL;
	}
	else
	{
		BK_LOGI(TAG, "f_unmount OK!\r\n");
	}

	if (pfs)
	{
		psram_free(pfs);
		pfs = NULL;
	}

	return BK_OK;
}


static bk_err_t audio_play_sdcard_mp3_music_exit(void)
{
    if (!audio_play_info) {
        return BK_OK;
    }

    if (audio_play_info->mp3_play)
    {
        mp3_play_close(audio_play_info->mp3_play, true);
        mp3_play_destroy(audio_play_info->mp3_play);
        audio_play_info->mp3_play = NULL;
    }

	f_close(&audio_play_info->mp3file);

    if (audio_play_info->sd_read_buf) {
        psram_free(audio_play_info->sd_read_buf);
        audio_play_info->sd_read_buf = NULL;
    }

    if (audio_play_info) {
        psram_free(audio_play_info);
        audio_play_info = NULL;
    }

    tf_unmount();

    return BK_OK;
}


static void audio_play_sdcard_mp3_music_main(beken_thread_arg_t param_data)
{
	bk_err_t ret = BK_OK;
    uint32 uiTemp = 0;
    char *file_name = (char *)param_data;

    mp3_play_cfg_t config = DEFAULT_MP3_PLAY_CONFIG();

	if (!file_name) {
		BK_LOGE(TAG, "file_name is NULL\n");
		goto fail;
	}

    ret = tf_mount();
    if (ret != BK_OK) {
        BK_LOGE(TAG, "mount sdcard fail\n");
        goto fail;
    }

    audio_play_info = (audio_play_info_t *)psram_malloc(sizeof(audio_play_info_t));
    if (!audio_play_info) {
        BK_LOGE(TAG, "mount sdcard fail\n");
        goto fail;
    }
    os_memset(audio_play_info, 0, sizeof(audio_play_info_t));

	/*open file to read mp3 data */
    os_memset(audio_play_info->mp3_file_name, 0, sizeof(audio_play_info->mp3_file_name)/sizeof(audio_play_info->mp3_file_name[0]));
	sprintf(audio_play_info->mp3_file_name, "%d:/%s", DISK_NUMBER_SDIO_SD, file_name);
	FRESULT fr = f_open(&audio_play_info->mp3file, audio_play_info->mp3_file_name, FA_OPEN_EXISTING | FA_READ);
	if (fr != FR_OK) {
		BK_LOGE(TAG, "open %s fail\n", audio_play_info->mp3_file_name);
		goto fail;
	}
	BK_LOGI(TAG, "mp3 file: %s open successful\n", audio_play_info->mp3_file_name);

    audio_play_info->mp3_play = mp3_play_create(&config);
    if (!audio_play_info->mp3_play) {
        BK_LOGE(TAG, "create mp3_play fail\n");
        goto fail;
    }

    ret = mp3_play_open(audio_play_info->mp3_play);
    if (ret != BK_OK) {
        BK_LOGE(TAG, "open mp3_play fail\n");
        goto fail;
    }

    audio_play_info->sd_read_buf = psram_malloc(SD_READ_BUFF_SIZE);
    if (!audio_play_info->sd_read_buf)
    {
        BK_LOGE(TAG, "malloc sd_read_buf fail\n");
        goto fail;
    }
    os_memset(audio_play_info->sd_read_buf, 0, SD_READ_BUFF_SIZE);

    rtos_set_semaphore(&audio_play_sem);

    audio_play_running = true;

    while (audio_play_running)
    {
        //BK_LOGE(TAG, "read+++++++\n");
        fr = f_read(&audio_play_info->mp3file, (void *)audio_play_info->sd_read_buf, 1940, &uiTemp);
        if (fr != FR_OK)
        {
            BK_LOGE(TAG, "read %s fail\n", audio_play_info->mp3_file_name);
            goto fail;
        }

        if (uiTemp > 0)
        {
            //BK_LOGE(TAG, "write----------\n");
            mp3_play_write_data(audio_play_info->mp3_play, audio_play_info->sd_read_buf, uiTemp);
        }

        if (uiTemp == 0)
        {
            //rtos_delay_milliseconds(1000);
            BK_LOGE(TAG, "%s is empty, and stop play\n", audio_play_info->mp3_file_name);
            break;
        }
    }

fail:
    audio_play_running = false;

    audio_play_sdcard_mp3_music_exit();

    rtos_set_semaphore(&audio_play_sem);

    audio_play_task_hdl = NULL;

    rtos_delete_thread(NULL);
}


bk_err_t audio_play_sdcard_mp3_music_start(char *file_name)
{
    bk_err_t ret = BK_OK;

    ret = rtos_init_semaphore(&audio_play_sem, 1);
    if (ret != BK_OK)
    {
        BK_LOGE(TAG, "%s, %d, ceate semaphore fail\n", __func__, __LINE__);
        return BK_FAIL;
    }

    ret = rtos_create_thread(&audio_play_task_hdl,
                             (BEKEN_DEFAULT_WORKER_PRIORITY - 1),
                             "audio_play",
                             (beken_thread_function_t)audio_play_sdcard_mp3_music_main,
                             2048 * 2,
                             (beken_thread_arg_t)file_name);
    if (ret != BK_OK)
    {
        BK_LOGE(TAG, "%s, %d, create mp3 codec task fail\n", __func__, __LINE__);
        goto fail;
    }

    rtos_get_semaphore(&audio_play_sem, BEKEN_NEVER_TIMEOUT);

    return BK_OK;

fail:
    if (audio_play_sem)
    {
        rtos_deinit_semaphore(&audio_play_sem);
        audio_play_sem = NULL;
    }

    return BK_FAIL;
}

bk_err_t audio_play_sdcard_mp3_music_stop(void)
{
    if (!audio_play_running)
    {
        BK_LOGW(TAG, "%s, %d, mp3 not play\n", __func__, __LINE__);
        return BK_OK;
    }

    audio_play_running = false;

    rtos_get_semaphore(&audio_play_sem, BEKEN_NEVER_TIMEOUT);

    rtos_deinit_semaphore(&audio_play_sem);
    audio_play_sem = NULL;

    return BK_OK;
}

static void cli_audio_play_sdcard_mp3_music_help(void)
{
	BK_LOGI(TAG, "audio_play_sdcard_mp3_music {start|stop file_name}\n");
}

void cli_audio_play_sdcard_mp3_music_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	if (argc != 2 && argc != 3) {
		cli_audio_play_sdcard_mp3_music_help();
		return;
	}

	if (os_strcmp(argv[1], "start") == 0) {
		if (BK_OK != audio_play_sdcard_mp3_music_start(argv[2]))
        {      
			BK_LOGI(TAG, "start audio play sdcard mp3 music fail\n");
		}
        else
        {
			BK_LOGI(TAG, "start audio play sdcard mp3 music ok\n");
        }
	}
    else if (os_strcmp(argv[1], "stop") == 0)
    {
		audio_play_sdcard_mp3_music_stop();
	}
    else
    {
		cli_audio_play_sdcard_mp3_music_help();
	}
}


#define AUDIO_PLAY_SDCARD_MP3_MUSIC_CMD_CNT  (sizeof(s_audio_play_sdcard_mp3_music_commands) / sizeof(struct cli_command))
static const struct cli_command s_audio_play_sdcard_mp3_music_commands[] =
{
	{"audio_play_sdcard_mp3_music", "audio_play_sdcard_mp3_music {start|stop file_name}", cli_audio_play_sdcard_mp3_music_cmd},
};

int cli_audio_play_sdcard_mp3_music_init(void)
{
	BK_LOGI(TAG, "cli_audio_play_sdcard_mp3_music_init \n");

	return cli_register_commands(s_audio_play_sdcard_mp3_music_commands, AUDIO_PLAY_SDCARD_MP3_MUSIC_CMD_CNT);
}


