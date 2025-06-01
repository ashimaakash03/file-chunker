cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

project(file-manager-service VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL CONFIG REQUIRED COMPONENTS Crypto SSL)
find_package(Crow CONFIG REQUIRED)

set(SOURCES
    src/cid_utility.cpp
    src/chunk_config.cpp
    src/chunk.cpp
    src/file_metadata.cpp
    src/chunk_reference_manager.cpp
    src/thread_pool.cpp
    src/file_manager.cpp
    main.cpp
)

add_executable(file-manager-service ${SOURCES})

target_include_directories(file-manager-service PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(file-manager-service PRIVATE
    nlohmann_json::nlohmann_json
    OpenSSL::Crypto
    OpenSSL::SSL
    Crow::Crow
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")