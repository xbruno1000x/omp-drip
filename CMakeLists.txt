cmake_minimum_required(VERSION 3.19)
project(omp-drip LANGUAGES C CXX VERSION 1.0.0)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "omp-drip currently targets Windows x86. Configure with -A Win32.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(OMP_DRIP_OPENMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.deps/open.mp" CACHE PATH "open.mp source checkout")
set(OMP_DRIP_MINHOOK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.deps/minhook" CACHE PATH "MinHook checkout")
set(OMP_DRIP_DEPLOY_COMPONENT_DIR "" CACHE PATH "Optional open.mp components directory")

if(NOT EXISTS "${OMP_DRIP_OPENMP_ROOT}/SDK/CMakeLists.txt")
    message(FATAL_ERROR "Missing open.mp dependencies. Run scripts/bootstrap-native.ps1 first.")
endif()
if(NOT EXISTS "${OMP_DRIP_MINHOOK_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "Missing MinHook dependency. Run scripts/bootstrap-native.ps1 first.")
endif()

add_subdirectory("${OMP_DRIP_OPENMP_ROOT}/SDK" "${CMAKE_CURRENT_BINARY_DIR}/omp-sdk")
add_subdirectory("${OMP_DRIP_MINHOOK_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/minhook")

add_library(omp-drip-server SHARED src/server/component.cpp)
set_target_properties(omp-drip-server PROPERTIES OUTPUT_NAME "omp-drip")
target_include_directories(omp-drip-server PRIVATE
    src/shared
    src/generated
    "${OMP_DRIP_OPENMP_ROOT}/Shared/Network"
    "${OMP_DRIP_OPENMP_ROOT}/lib/pawn/source"
    "${OMP_DRIP_OPENMP_ROOT}/lib/pawn/source/linux"
)
target_compile_definitions(omp-drip-server PRIVATE
    HAVE_STDINT_H=1
    PAWN_CELL_SIZE=32
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_link_libraries(omp-drip-server PRIVATE OMP-SDK)

if(OMP_DRIP_DEPLOY_COMPONENT_DIR)
    add_custom_command(TARGET omp-drip-server POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OMP_DRIP_DEPLOY_COMPONENT_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:omp-drip-server>"
            "${OMP_DRIP_DEPLOY_COMPONENT_DIR}/omp-drip.dll"
        COMMENT "Deploying omp-drip.dll"
    )
endif()

add_library(omp-drip-client SHARED
    src/client/client.cpp
    src/client/catalog.cpp
    src/client/conflict_detector.cpp
    src/client/file_hash.cpp
)
set_target_properties(omp-drip-client PROPERTIES OUTPUT_NAME "omp-drip" SUFFIX ".asi")
target_include_directories(omp-drip-client PRIVATE
    src/shared
    src/generated
    "${OMP_DRIP_MINHOOK_ROOT}/include"
    "${OMP_DRIP_OPENMP_ROOT}/lib/RakNet/Include/raknet"
)
target_compile_definitions(omp-drip-client PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(omp-drip-client PRIVATE minhook bcrypt psapi)

add_executable(omp-drip-protocol-tests tests/protocol_tests.cpp)
target_include_directories(omp-drip-protocol-tests PRIVATE src/shared)

add_executable(omp-drip-catalog-tests tests/catalog_tests.cpp src/client/catalog.cpp)
target_include_directories(omp-drip-catalog-tests PRIVATE src/client)

add_executable(omp-drip-conflict-tests tests/conflict_tests.cpp src/client/conflict_detector.cpp)
target_include_directories(omp-drip-conflict-tests PRIVATE src/client)
target_link_libraries(omp-drip-conflict-tests PRIVATE psapi)

enable_testing()
add_test(NAME protocol COMMAND omp-drip-protocol-tests)
add_test(NAME catalog COMMAND omp-drip-catalog-tests)
add_test(NAME conflict COMMAND omp-drip-conflict-tests)
