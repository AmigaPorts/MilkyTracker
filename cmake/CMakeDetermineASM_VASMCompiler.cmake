set(CMAKE_ASM_VASM_COMPILER_LIST vasm vasmm68k_mot)
set(CMAKE_ASM_VASM_COMPILER_ID vasm)

set(ASM_DIALECT "_VASM")
include(CMakeDetermineASMCompiler)
set(ASM_DIALECT)
