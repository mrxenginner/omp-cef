project(ServerOmp LANGUAGES CXX)

# You can change this path for development
#set(DLL_DEPLOY_DIR "${CMAKE_SOURCE_DIR}/tests/omp/components")
set(DLL_DEPLOY_DIR "E:/multiplayer/aurora-mp/rpg/env/components")
#set(INC_DEPLOY_DIR "${CMAKE_SOURCE_DIR}/tests/omp/qawno/include")
set(INC_DEPLOY_DIR "E:/multiplayer/aurora-mp/rpg/env/qawno/include")

file(GLOB_RECURSE SERVER_OMP_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_library(${PROJECT_NAME} 
    MODULE 
        ${SERVER_OMP_FILES}
        $<TARGET_OBJECTS:ServerCommon>
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        #${CMAKE_SOURCE_DIR}/deps
        ${CMAKE_SOURCE_DIR}/deps/omp-sdk/include
        ${CMAKE_SOURCE_DIR}/deps/pawn/source
        ${CMAKE_SOURCE_DIR}/deps/pawn/source/linux
        #${CMAKE_SOURCE_DIR}/deps/pawn-natives
        # Ajoutez ici le chemin de pawn-natives si différent :
        # "${CMAKE_SOURCE_DIR}/deps/pawn-natives/include"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        OMP-SDK
        Shared
        ServerCommon
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        HAVE_STDINT_H=1
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "Cef"
    PREFIX ""
)

if(NOT DEFINED ENV{CI})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/server/cef.inc"
            "${INC_DEPLOY_DIR}/cef.inc"
        COMMENT "Deploying cef.inc to ${INC_DEPLOY_DIR}"
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${PROJECT_NAME}>
            "${DLL_DEPLOY_DIR}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        COMMENT "Deploying Cef.dll to ${DLL_DEPLOY_DIR}"
    )
endif()