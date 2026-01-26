find_package(miniz CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

if (NOT TARGET tiny-aes)
    add_subdirectory(${CMAKE_SOURCE_DIR}/deps/tiny-aes ${CMAKE_BINARY_DIR}/_deps_tinyaes)
endif()

add_subdirectory(common)

if (DEV_ALL_TARGETS OR BUILD_SERVER_OMP)
    if (NOT TARGET OMP-SDK)
        add_subdirectory(${CMAKE_SOURCE_DIR}/deps/omp-sdk ${CMAKE_BINARY_DIR}/_deps_omp_sdk)
    endif()
    add_subdirectory(omp)
endif()