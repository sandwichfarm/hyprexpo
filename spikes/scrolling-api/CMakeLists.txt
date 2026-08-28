cmake_minimum_required(VERSION 3.27)
project(hyprexpo_scrolling_api_probe LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HYPRLAND REQUIRED IMPORTED_TARGET "hyprland=0.56.1")

add_library(hyprexpo-scrolling-api-probe MODULE ScrollingApiProbe.cpp)
target_compile_features(hyprexpo-scrolling-api-probe PRIVATE cxx_std_23)
target_compile_options(hyprexpo-scrolling-api-probe PRIVATE -Wall -Wextra -Werror -Wno-c++11-narrowing -Wno-narrowing)
target_link_libraries(hyprexpo-scrolling-api-probe PRIVATE PkgConfig::HYPRLAND)
set_target_properties(hyprexpo-scrolling-api-probe PROPERTIES PREFIX "" OUTPUT_NAME "hyprexpo-scrolling-api-probe")
