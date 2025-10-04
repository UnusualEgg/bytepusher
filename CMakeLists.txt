cmake_minimum_required(VERSION 3.10.0)
project(bytepusher VERSION 1.0.0 LANGUAGES C)

# Create an option to switch between a system sdl library and a vendored SDL library
option(BUILD_SDL "Build SDL" OFF)
option(BUILD_SDL_TTF "Build SDL_TTF" OFF)

if(BUILD_SDL)
    # This assumes you have added SDL as a submodule in vendored/SDL
    add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
    add_subdirectory(vendored/SDL_ttf EXCLUDE_FROM_ALL)

else()
    # 1. Look for a SDL3 package,
    # 2. look for the SDL3-shared component, and
    # 3. fail if the shared component cannot be found.
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
    find_package(SDL3_ttf REQUIRED CONFIG REQUIRED COMPONENTS SDL3_ttf-shared)
endif()

add_executable(bytepusher main.c)

# Link to the actual SDL3 library.
target_link_libraries(bytepusher PRIVATE SDL3_ttf::SDL3_ttf SDL3::SDL3)

