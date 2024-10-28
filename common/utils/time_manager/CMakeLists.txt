add_library(time_management STATIC
            time_manager.c)

add_library(time_management_core OBJECT
            time_source.c
            time_server.c
            time_client.c)

target_link_libraries(time_management PUBLIC time_management_core)
target_link_libraries(time_management_core PRIVATE ${T_LIB})

add_subdirectory(tests)
