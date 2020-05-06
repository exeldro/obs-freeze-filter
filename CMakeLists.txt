project(freeze-filter)

set(freeze-filter_HEADERS
	freeze-filter.h)
set(freeze-filter_SOURCES
	freeze-filter.c)

if(WIN32)
	list(APPEND freeze-filter_SOURCES
		freeze-filter.rc)
endif()

add_library(freeze-filter MODULE
	${freeze-filter_HEADERS}
	${freeze-filter_SOURCES})
target_link_libraries(freeze-filter
	libobs)

install_obs_plugin_with_data(freeze-filter data)
