set(incs include)
set(srcs "")

if (CONFIG_MP3_PLAY)
	list(APPEND srcs
		src/audio_codec.c
		src/mp3_codec.c
		src/mp3_play.c
	)
endif()

if (CONFIG_MP3_PLAY_TEST)
	list(APPEND srcs
		src/mp3_play_test.c
	)
endif()

armino_component_register(
    SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_common include avdk_utils audio_play
)

