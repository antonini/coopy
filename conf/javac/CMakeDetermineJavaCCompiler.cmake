SET(CMAKE_JavaC_COMPILER "/usr/bin/gcj" CACHE PATH "JavaC Compiler")
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/conf/javac/CMakeJavaCInformation.cmake
  ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeJavaCCompiler.cmake 
  IMMEDIATE @ONLY)

SET(CMAKE_JavaC_COMPILER_ENV_VAR "JavaC_COMPILER")
