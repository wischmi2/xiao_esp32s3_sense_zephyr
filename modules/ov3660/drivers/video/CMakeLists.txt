zephyr_library()
zephyr_library_sources_ifdef(CONFIG_VIDEO_OV3660 ov3660.c)
zephyr_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include)
