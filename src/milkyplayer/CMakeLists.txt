project(MilkyPlayer C CXX ASM)

set(SOURCES
		main.cpp
		borrowed-code/AslRequester.cpp
		borrowed-code/PlayerMaster.cpp
		borrowed-code/PlayerController.cpp
		../tracker/ResamplerHelper.cpp
		)

set(HEADERS
		amigaversion.h
		project.h
		borrowed-code/AslRequester.h
		borrowed-code/PlayerMaster.h
		borrowed-code/PlayerController.h
		borrowed-code/BasicTypes.h
		../tracker/ResamplerHelper.h
		../tracker/PlayerCriticalSection.h
		)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
		PRIVATE
		${PROJECT_SOURCE_DIR}
		${PROJECT_SOURCE_DIR}/borrowed-code
		${PROJECT_SOURCE_DIR}/..
		# Include the CMake-generated version header from the build directory
		${PROJECT_BINARY_DIR}
		${SDL_INCLUDE_DIR}
		)

find_package(SDL REQUIRED)

target_link_libraries(${PROJECT_NAME}
		milkyplay
		${SDL_LIBRARY}
		)

# Version number in format X.YY.ZZ
set(VER_X  0)
set(VER_YY 01)
set(VER_ZZ 00)
set(VER_FULL "${VER_X}.${VER_YY}.${VER_ZZ}")


configure_file(
		${PROJECT_SOURCE_DIR}/amigaversion.h.in
		${PROJECT_BINARY_DIR}/amigaversion.h
)