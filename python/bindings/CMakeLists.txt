# Optional: cmake -DMERCURY_BUILD_PYTHON=ON
find_package(Python 3.10 COMPONENTS Interpreter Development.Module REQUIRED)

include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG v2.13.6
)
FetchContent_MakeAvailable(pybind11)

pybind11_add_module(mercury_engine mercury_py.cpp)
target_link_libraries(mercury_engine PRIVATE mercury)
mercury_set_warnings(mercury_engine)

# Avoid depending on MinGW runtime DLLs next to the .pyd on Windows.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_link_options(mercury_engine PRIVATE -static-libgcc -static-libstdc++)
endif()

# Drop next to mercury_sim for easy `PYTHONPATH=python` imports.
set(_mercury_py_out "${CMAKE_SOURCE_DIR}/python")
set_target_properties(mercury_engine PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${_mercury_py_out}"
  RUNTIME_OUTPUT_DIRECTORY "${_mercury_py_out}"
  LIBRARY_OUTPUT_DIRECTORY_DEBUG "${_mercury_py_out}"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${_mercury_py_out}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${_mercury_py_out}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_mercury_py_out}"
)

# pybind11 headers can trip -Wpedantic on some toolchains; keep local code strict.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(mercury_engine PRIVATE -Wno-pedantic)
endif()
