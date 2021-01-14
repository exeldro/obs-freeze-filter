project(freeze-filter VERSION 0.3.2)
set(PROJECT_FULL_NAME "Freeze Filter")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

set(freeze-filter_HEADERS
	freeze-filter.h
	version.h)
set(freeze-filter_SOURCES
	freeze-filter.c)

if(WIN32)
	get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
	get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
	get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
	configure_file("installer.iss.in"
			"${PROJECT_BINARY_DIR}/installer.iss"
		)
	configure_file(resource.rc.in freeze-filter.rc)
	list(APPEND freeze-filter_SOURCES
		freeze-filter.rc)
endif()

add_library(freeze-filter MODULE
	${freeze-filter_HEADERS}
	${freeze-filter_SOURCES})
target_link_libraries(freeze-filter
	libobs)

set_target_properties(freeze-filter PROPERTIES FOLDER "plugins/exeldro")
install_obs_plugin_with_data(freeze-filter data)
