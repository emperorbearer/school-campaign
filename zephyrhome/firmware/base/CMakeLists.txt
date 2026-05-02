# ZephyrHome base framework CMakeLists.txt
# Included by generated project CMakeLists via add_subdirectory().

zephyr_library_named(zh_base)

zephyr_library_sources(
    src/zh_component.c
    src/zh_coap_server.c
    src/zh_ot_init.c
    src/zh_main.c
)

zephyr_library_include_directories(include)

# Expose headers to the application and component libraries
zephyr_include_directories(include)
