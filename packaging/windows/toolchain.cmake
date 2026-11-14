# Cross-compiles for 64-bit Windows with llvm-mingw, the compiler Qt's own
# Windows builds (win64_llvm_mingw) are made with. LLVM_MINGW is its folder.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(NOT LLVM_MINGW)
  set(LLVM_MINGW "$ENV{LLVM_MINGW}")
endif()
# Try-compiles configure a project of their own and need it too.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LLVM_MINGW)

set(triple x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${LLVM_MINGW}/bin/${triple}-clang)
set(CMAKE_CXX_COMPILER ${LLVM_MINGW}/bin/${triple}-clang++)
set(CMAKE_RC_COMPILER ${LLVM_MINGW}/bin/${triple}-windres)

# Libraries and headers come from the Windows side only; programs that run
# during the build (moc, rcc, the QML compiler) come from this machine.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
