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
#include <string.h>
#include <stdlib.h>

#include <os/os.h>
#include <os/mem.h>
#include <components/log.h>

#include "lcd_act.h"
#include "lcd_decode.h"
#include "lcd_rotate.h"
#include "yuv_encode.h"
#include "frame_buffer.h"

#define TAG "sw_jdec"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

typedef struct
{
	uint8_t enable : 1;
	uint8_t dec_init : 1;
	uint8_t rot_init: 1;
    uint8_t decode_result;
	media_rotate_t rotate;
	beken_semaphore_t sem;
	beken_semaphore_t decode_sem;
	beken_thread_t thread;
} lcd_dec_handle_t;

static lcd_dec_handle_t *s_lcd_decode_handle = NULL;
media_software_decode_info_t sw_decode_info = {0};

void jpeg_software_decode_callback(uint8_t ret)
{
    if (s_lcd_decode_handle && s_lcd_decode_handle->decode_sem)
    {
        s_lcd_decode_handle->decode_result = ret;
        rtos_set_semaphore(s_lcd_decode_handle->decode_sem);
    }
    else
    {
        LOGE("%s %d sw decode has problem\n", __func__, __LINE__);
    }
}

#if CONFIG_LVGL
uint8_t lvgl_video_switch = 0;
#endif

static void software_decoder_task_entry(beken_thread_arg_t data)
{
	int ret = BK_OK;
	frame_buffer_t *jpeg_frame = NULL, *dec_frame = NULL, *rot_frame = NULL;

	lcd_dec_handle_t *handle = (lcd_dec_handle_t *)data;
	handle->enable = true;
	frame_buffer_fb_register(MODULE_DECODER_CP1, FB_INDEX_SMALL_JPEG);
	rtos_set_semaphore(&handle->sem);

	while (handle->enable)
	{
		jpeg_frame = frame_buffer_fb_read(MODULE_DECODER_CP1);
		if (jpeg_frame == NULL)
		{
			LOGD("%s read jpeg frame NULL\n", __func__);
			continue;
		}

		dec_frame = frame_buffer_display_malloc(jpeg_frame->width * jpeg_frame->height * 2);
		if (dec_frame == NULL)
		{
			LOGW("%s malloc jdec frame fail...\n", __func__);
			frame_buffer_fb_free(jpeg_frame, MODULE_DECODER_CP1);
			continue;
		}

		dec_frame->type = jpeg_frame->type;
		dec_frame->fmt = PIXEL_FMT_YUYV;
		dec_frame->sequence = jpeg_frame->sequence;
		dec_frame->width = jpeg_frame->width;
		dec_frame->height = jpeg_frame->height;
        sw_decode_info.in_frame = jpeg_frame;
        sw_decode_info.out_frame = dec_frame;
        sw_decode_info.cb = jpeg_software_decode_callback;
        s_lcd_decode_handle->decode_result = BK_OK;
        ret = software_decode_task_send_msg(JPEGDEC_START, (uint32_t)&sw_decode_info);
        if (ret == BK_OK)
        {
            ret = rtos_get_semaphore(&handle->decode_sem, 2000);
            if (ret != BK_OK)
            {
                LOGE("%s %d sw decode has problem\n", __func__, __LINE__);
                frame_buffer_fb_free(jpeg_frame, MODULE_DECODER_CP1);
                frame_buffer_display_free(dec_frame);
                continue;
            }
            if (s_lcd_decode_handle->decode_result != BK_OK)
            {
                frame_buffer_fb_free(jpeg_frame, MODULE_DECODER_CP1);
                frame_buffer_display_free(dec_frame);
                LOGE("%s %d sw decode failed ret %d\n", __func__, __LINE__, s_lcd_decode_handle->decode_result);
                continue;
            }
        }
        else
        {
            LOGE("%s %d send msg failed\n", __func__, __LINE__);
            frame_buffer_fb_free(jpeg_frame, MODULE_DECODER_CP1);
            frame_buffer_display_free(dec_frame);
            continue;
        }

		frame_buffer_fb_free(jpeg_frame, MODULE_DECODER_CP1);

		if (ret != BK_OK)
		{
			frame_buffer_display_free(dec_frame);
			continue;
		}

		if (handle->rot_init)
		{
			rot_frame = frame_buffer_display_malloc(jpeg_frame->width * jpeg_frame->height * 2);
			if (rot_frame == NULL)
			{
				LOGW("%s malloc rot frame fail...\n", __func__);
				frame_buffer_display_free(dec_frame);
				continue;
			}

			rot_frame->type = dec_frame->type;
			rot_frame->fmt = PIXEL_FMT_RGB565_LE;
			rot_frame->sequence = dec_frame->sequence;
			rot_frame->width = dec_frame->height;
			rot_frame->height = dec_frame->width;

#if CONFIG_HW_ROTATE_PFC
			ret = lcd_hw_rotate_yuv2rgb565(dec_frame, rot_frame, handle->rotate);
#endif
			frame_buffer_display_free(dec_frame);
			if (ret != BK_OK)
			{
				frame_buffer_display_free(rot_frame);
			}
			else
			{
				if (check_lcd_task_is_open())
				{
				#if CONFIG_LVGL
					if (lvgl_video_switch) {
						frame_buffer_display_free(rot_frame);
					}
					else
				#endif
					{
						lcd_display_frame_request(rot_frame);
					}
				}
				else
				{
					frame_buffer_display_free(rot_frame);
				}
			}
		}
		else
		{
			if (check_lcd_task_is_open())
			{
				#if CONFIG_LVGL
				if (lvgl_video_switch) {
					frame_buffer_display_free(dec_frame);
				}
				else
				#endif
				{
					lcd_display_frame_request(dec_frame);
				}
			}
			else
			{
				frame_buffer_display_free(dec_frame);
			}
		}
	}

	handle->thread = NULL;
	rtos_set_semaphore(&handle->sem);
	rtos_delete_thread(NULL);
}

bool check_lcd_sw_decode_act_is_open(void)
{
	if (s_lcd_decode_handle == NULL)
	{
		return false;
	}
	else
	{
		return s_lcd_decode_handle->enable;
	}
}

bk_err_t lcd_open_jpeg_decode_frame_handle(media_mailbox_msg_t *msg, media_rotate_t rotate)
{
	int ret = BK_FAIL;

#ifndef CONFIG_LCD_SW_DECODE
	LOGW("%s, CONFIG_LCD_SW_DECODE not open...\n", __func__);
	return ret;
#endif

	lcd_dec_handle_t *handle = s_lcd_decode_handle;

	if (handle && handle->enable)
	{
		LOGW("%s, already open\n", __func__);
		return ret;
	}

	if (handle != NULL)
	{
		LOGW("%s, not free complete\n", __func__);
		return ret;
	}

	handle = (lcd_dec_handle_t *)os_malloc(sizeof(lcd_dec_handle_t));
	if (handle == NULL)
	{
		LOGW("%s, malloc handle failed...\n", __func__);
		return ret;
	}

	os_memset(handle, 0, sizeof(lcd_dec_handle_t));

#if CONFIG_LCD_SW_DECODE
	ret = lcd_sw_decode_init(JPEGDEC_SW_MODE);
	if (ret != BK_OK)
	{
		LOGW("%s, soft jdec init fail...\n", __func__);
		goto out;
	}

	handle->dec_init = true;
#else
	LOGW("%s, CONFIG_LCD_SW_DECODE not open\n", __func__);
	ret = BK_FAIL;
	goto out;
#endif

	handle->rotate = rotate;
	if (handle->rotate != ROTATE_NONE)
	{
#if CONFIG_HW_ROTATE_PFC
		ret = lcd_rotate_init(HW_ROTATE);
		if (ret != BK_OK)
		{
			LOGW("%s, hardware rotate init fail...\n", __func__);
			goto out;
		}

		handle->rot_init = true;
#else
		LOGW("%s, CONFIG_HW_ROTATE_PFC not open\n", __func__);
		ret = BK_FAIL;
		goto out;
#endif
	}

	ret = rtos_init_semaphore(&handle->sem, 1);
	if (ret != BK_OK)
	{
		LOGW("%s, malloc sem failed...\n", __func__);
		goto out;
	}

	ret = rtos_init_semaphore(&handle->decode_sem, 1);
	if (ret != BK_OK)
	{
		LOGW("%s, malloc sem failed...\n", __func__);
		goto out;
	}

    software_decode_task_open();

	ret = rtos_create_thread(&handle->thread,
	                         BEKEN_APPLICATION_PRIORITY,
	                         "sw_jdec_thread",
	                         (beken_thread_function_t)software_decoder_task_entry,
	                         3 * 1024,
	                         (beken_thread_arg_t)handle);

	if (BK_OK != ret)
	{
		LOGE("%s lcd decoder task init failed\n", __func__);
		goto out;
	}

	rtos_get_semaphore(&handle->sem, BEKEN_NEVER_TIMEOUT);

	s_lcd_decode_handle = handle;

	return ret;

out:
	if(check_software_decode_task_is_open())
	{
		software_decode_task_close();
	}

	if (handle)
	{
#if CONFIG_LCD_SW_DECODE
		if (handle->dec_init)
		{
			handle->dec_init = false;
			lcd_sw_decode_deinit(JPEGDEC_SW_MODE);
		}
#endif

		if (handle->rot_init)
		{
			handle->rot_init = false;
#ifdef CONFIG_HW_ROTATE_PFC
			lcd_rotate_deinit();
#endif
		}

		if (handle->sem)
		{
			rtos_deinit_semaphore(&handle->sem);
		}

		if (handle->decode_sem)
		{
			rtos_deinit_semaphore(&handle->decode_sem);
		}

		os_free(handle);
		handle = NULL;
	}

	s_lcd_decode_handle = NULL;

	return ret;
}

bk_err_t lcd_close_jpeg_decode_frame_handle(media_mailbox_msg_t *msg)
{
	lcd_dec_handle_t *handle = s_lcd_decode_handle;

	if (handle == NULL || handle->enable == false)
	{
		LOGW("%s, already closed\n", __func__);
		return BK_OK;
	}

	if(check_software_decode_task_is_open())
	{
		software_decode_task_close();
	}

	handle->enable = false;
	frame_buffer_fb_deregister(MODULE_DECODER, FB_INDEX_JPEG);
	rtos_get_semaphore(&handle->sem, BEKEN_NEVER_TIMEOUT);

#if CONFIG_LCD_SW_DECODE
	if (handle->dec_init)
	{
		handle->dec_init = false;
		lcd_sw_decode_deinit(JPEGDEC_SW_MODE);
	}
#endif

	if (handle->rot_init)
	{
		handle->rot_init = false;
#ifdef CONFIG_HW_ROTATE_PFC
		lcd_rotate_deinit();
#endif
	}

	if (handle->sem)
	{
		rtos_deinit_semaphore(&handle->sem);
	}

	if (handle->decode_sem)
	{
		rtos_deinit_semaphore(&handle->decode_sem);
	}

	os_free(handle);
	s_lcd_decode_handle = NULL;

	return BK_OK;
}

