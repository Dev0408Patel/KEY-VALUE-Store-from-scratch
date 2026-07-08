# Create the fastkv library target
add_library(fastkv STATIC
    fastkv.cpp
    slab_allocator.cpp
    hazard_pointer.cpp
    mutex_kv_store.cpp
    lock_free_kv_store.cpp
)

# Configure public include directories
target_include_directories(fastkv PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

# Configure the CLI/Server entry point
add_executable(fastkv_server
    main.cpp
)

# Link the executable against the library
target_link_libraries(fastkv_server PRIVATE fastkv)
