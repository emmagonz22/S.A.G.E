cmake_minimum_required(VERSION 3.16)
project(S.A.G.A)

set(EXTRA_COMPONENT_DIRS components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

idf_build_process()