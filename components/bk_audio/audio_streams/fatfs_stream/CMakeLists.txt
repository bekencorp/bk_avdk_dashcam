set(incs .)

set(srcs)

if (CONFIG_ASDF_FATFS_STREAM)
if ((CONFIG_SYS_CPU0 AND CONFIG_ASDF_WORK_CPU0) OR (CONFIG_SYS_CPU1 AND CONFIG_ASDF_WORK_CPU1))
list(APPEND srcs
		fatfs_stream.c
		fatfs_act.c
	)

if (CONFIG_ASDF_FATFS_STREAM_TEST)
	list(APPEND srcs
		fatfs_stream_test.c
	)
endif()
endif()#((CONFIG_SYS_CPU0 AND CONFIG_ASDF_WORK_CPU0) OR (CONFIG_SYS_CPU1 AND CONFIG_ASDF_WORK_CPU1))
endif()#(CONFIG_ASDF_FATFS_STREAM)

set(priv_req bk_common audio_pipeline)

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES "${priv_req}")