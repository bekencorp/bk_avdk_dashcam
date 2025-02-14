// Copyright 2023-2024 Beken
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "fatfs_stream.h"
#include "audio_common.h"
#include "audio_mem.h"
#include "audio_element.h"
#include "ff.h"
#include "diskio.h"


#define TAG  "FTFS_STR"

typedef struct fatfs_stream {
    audio_stream_type_t type;
    int block_size;
    bool is_open;
    void *file;
} fatfs_stream_t;

static bk_err_t fatfs_open(void **fp, char* path, uint8_t mode)
{
	FRESULT fr;

	FIL *fp_open = os_malloc(sizeof(FIL));
	if (!fp_open) {
		BK_LOGE(TAG, "%s calloc fp_open fail, line: %d \n", __func__, __LINE__);
		return BK_FAIL;
	}
	os_memset(fp_open, 0x00, sizeof(FIL));
	*fp = fp_open;
	BK_LOGI(TAG, "[%s] %s fatfs open ok, fp: %p. line: %d \n", tag, __func__, *fp, __LINE__);

	fr = f_open(*fp, path, mode);
	if (fr != BK_OK) {
		BK_LOGE(TAG, "Failed to open. File name: %s, error: %d, line: %d \n", path, fr, __LINE__);
		return BK_FAIL;
	}

	return BK_OK;
}

static bk_err_t fatfs_size(void *fp)
{
	return f_size((FIL *)fp);
}

static bk_err_t fatfs_lseek(void *fp, uint64_t ofs)
{
	int ret = f_lseek((FIL *)fp, ofs);
	if (ret < 0)
		BK_LOGE(TAG, "%s fail seek file. Error: %s, line: %d \n", __func__, f_error((FIL *)fp), __LINE__);
	return ret;
}

static int fatfs_read(void *fp, void* buff, uint64_t len)
{
	FRESULT fr;
	int ret = 0;
	/* use file descriptors to access files */
	uint32 rlen = 0;
	fr = f_read((FIL *)fp, buff, len, &rlen);
	if (fr == BK_OK) {
		ret = rlen;
	}else {
		BK_LOGE(TAG, "%s The error is happened in reading data. Error: %s, line: %d \n", __func__, f_error((FIL *)fp), __LINE__);
		ret = -1;
	}
	return ret;
}

static int fatfs_write(void *fp, void* buff, uint64_t len)
{
	FRESULT fr;
	int ret = 0;
	uint32 wlen = 0;
	fr = f_write((FIL *)fp, buff, len, &wlen);
	fr = BK_OK;
	if (fr == BK_OK) {
		ret = wlen;
		if (ret != len) {
			BK_LOGE(TAG, "%s len: %d, wlen: %d \n", __func__, len, wlen);
		}
	} else {
		BK_LOGE(TAG, "%s writing data error. Error: %s, line: %d \n", __func__, f_error((FIL *)fp), __LINE__);
		ret = -1;
	}
	return ret;
}

static bk_err_t fatfs_close(void *fp)
{
	FRESULT fr = f_close((FIL*)fp);
	if (fr == BK_OK) {
		BK_LOGI(TAG, "[%s] %s fatfs close ok, fp: %p. line: %d \n", tag, __func__, fp, __LINE__);
		os_free((FIL*)fp);
		return BK_OK;
	} else {
		BK_LOGE(TAG, "[%s] %s Failed to fatfs close, ret: %d. line: %d \n", tag, __func__, fr, __LINE__);
		return BK_FAIL;
	}
}

static bk_err_t _fatfs_open(audio_element_handle_t self)
{
	bk_err_t ret = BK_OK;
    fatfs_stream_t *fatfs = (fatfs_stream_t *)audio_element_getdata(self);

    audio_element_info_t info;
    char *uri = audio_element_get_uri(self);
    if (uri == NULL) {
        BK_LOGE(TAG, "Error, uri is not set \n");
        return BK_FAIL;
    }
    char *path = strstr(uri, "1:/");
    BK_LOGI(TAG, "_fatfs_open, uri:%s \n", uri);
    audio_element_getinfo(self, &info);
    if (path == NULL) {
        BK_LOGE(TAG, "Error, need file path to open \n");
        return BK_FAIL;
    }
    if (fatfs->is_open) {
        BK_LOGE(TAG, "already opened \n");
        return BK_FAIL;
    }
    if (fatfs->type == AUDIO_STREAM_READER) {
        ret = fatfs_open(&fatfs->file, path, 0x01);
        if (ret != BK_OK) {
            BK_LOGE(TAG, "Failed to open. File name: %s, error: %d, line: %d \n", path, ret, __LINE__);
            return BK_FAIL;
        }
		info.total_bytes = (int64_t)fatfs_size(fatfs->file);
        BK_LOGI(TAG, "File size: 0x%x%x byte, file position: 0x%x%x \n", (int)(info.total_bytes>>32), (int)info.total_bytes, (int)(info.byte_pos>>32), (int)info.byte_pos);
        if (info.byte_pos > 0) {
            if (fatfs_lseek(fatfs->file, info.byte_pos) < 0) {
                return BK_FAIL;
            }
        }
    } else if (fatfs->type == AUDIO_STREAM_WRITER) {
        ret = fatfs_open(&fatfs->file, path, 0x08 | 0x02);
        if (ret != BK_OK) {
            BK_LOGE(TAG, "[%s] Failed to open: %s, error: %d, line: %d \n", audio_element_get_tag(self), path, ret, __LINE__);
            return BK_FAIL;
        }
    } else {
        BK_LOGE(TAG, "FATFS must be Reader or Writer \n");
        return BK_FAIL;
    }
    fatfs->is_open = true;
    ret = audio_element_set_total_bytes(self, info.total_bytes);
    return ret;
}

static int _fatfs_read(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
	BK_LOGD(TAG, "[%s] _fatfs_read, len: %d \n", audio_element_get_tag(self), len);

	fatfs_stream_t *fatfs = (fatfs_stream_t *)audio_element_getdata(self);
	audio_element_info_t info;
	audio_element_getinfo(self, &info);

	BK_LOGD(TAG, "[%s] read len=%d, pos=%d/%d \n", audio_element_get_tag(self), len, (int)info.byte_pos, (int)info.total_bytes);
	/* use file descriptors to access files */
	int rlen = fatfs_read(fatfs->file, buffer, len);
	if (rlen == 0) {
		BK_LOGW(TAG, "No more data, ret:%d \n", rlen);
	} else {
		audio_element_update_byte_pos(self, rlen);
	}
	return rlen;
}

static int _fatfs_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
	fatfs_stream_t *fatfs = (fatfs_stream_t *)audio_element_getdata(self);
	audio_element_info_t info;
	audio_element_getinfo(self, &info);
	BK_LOGD(TAG, "[%s] _fatfs_write len: %d \n", audio_element_get_tag(self), len);
	int wlen = fatfs_write(fatfs->file, buffer, len);
	if (wlen > 0) {
		audio_element_update_byte_pos(self, wlen);
	}

	return wlen;
}

static int _fatfs_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int r_size = audio_element_input(self, in_buffer, in_len);
    int w_size = 0;
    if (r_size > 0) {
        w_size = audio_element_output(self, in_buffer, r_size);
    } else {
        w_size = r_size;
    }
    return w_size;
}

static bk_err_t _fatfs_close(audio_element_handle_t self)
{
    fatfs_stream_t *fatfs = (fatfs_stream_t *)audio_element_getdata(self);

    if (fatfs->is_open) {
        fatfs_close(fatfs->file);
        fatfs->file = NULL;
        fatfs->is_open = false;
    }
    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
        audio_element_report_info(self);
        audio_element_set_byte_pos(self, 0);
    }
    return BK_OK;
}

static bk_err_t _fatfs_destroy(audio_element_handle_t self)
{
	BK_LOGI(TAG, "[%s] %s \n", audio_element_get_tag(self), __func__);

	fatfs_stream_t *fatfs = (fatfs_stream_t *)audio_element_getdata(self);

	if (fatfs) {
		audio_free(fatfs);
		fatfs = NULL;
	}

	return BK_OK;
}

audio_element_handle_t fatfs_stream_init(fatfs_stream_cfg_t *config)
{
    audio_element_handle_t el;
    fatfs_stream_t *fatfs = audio_calloc(1, sizeof(fatfs_stream_t));

    AUDIO_MEM_CHECK(TAG, fatfs, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = _fatfs_open;
    cfg.close = _fatfs_close;
    cfg.process = _fatfs_process;
    cfg.destroy = _fatfs_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.out_rb_size = config->out_rb_size;
    cfg.buffer_len = config->buf_sz;
    cfg.stack_in_ext = config->ext_stack;

    cfg.tag = "file";
    fatfs->type = config->type;

    if (config->type == AUDIO_STREAM_WRITER) {
        cfg.write = _fatfs_write;
    } else {
        cfg.read = _fatfs_read;
    }
    el = audio_element_init(&cfg);

    AUDIO_MEM_CHECK(TAG, el, goto _fatfs_init_exit);
    audio_element_setdata(el, fatfs);

    return el;
_fatfs_init_exit:
	if (el) {
		audio_element_deinit(el);
	}
	if (fatfs) {
		audio_free(fatfs);
		fatfs = NULL;
	}
    return NULL;
}
