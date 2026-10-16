add_library(tmac_ds STATIC tmac_ds.c)

target_include_directories(tmac_ds PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(tmac_ds PUBLIC
    pico_stdlib
    hardware_spi
)
