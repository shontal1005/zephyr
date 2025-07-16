zephyr_syscall_header(${ZEPHYR_BASE}/include/zephyr/drivers/radio_gecko.h)

zephyr_library()
zephyr_include_directories(${CONFIG_BOARD})
zephyr_library_sources_ifdef(CONFIG_RADIO_EFR32 efr32_radio_gecko.c ${CONFIG_BOARD}/rail_config.c)
