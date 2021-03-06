# setup build on CI

if (FG_CI_BUILD)
	message( STATUS "configured CI build" )

	set( FG_ENABLE_OPENVR ON CACHE INTERNAL "" FORCE )
	set( FG_ENABLE_SIMPLE_COMPILER_OPTIONS ON CACHE INTERNAL "" FORCE )
	set( FG_ENABLE_TESTS ON CACHE INTERNAL "" FORCE )
	set( FG_ENABLE_ASSIMP ON CACHE INTERNAL "" FORCE )
	set( FG_ENABLE_GLM ON CACHE INTERNAL "" FORCE )
	set( FG_ENABLE_DEVIL ON CACHE INTERNAL "" FORCE )
	set( FG_ENABLE_FREEIMAGE OFF CACHE INTERNAL "" FORCE )

	if (UNIX)
		set( ENABLE_OPT OFF CACHE INTERNAL "" FORCE )
		set( FG_ENABLE_OPENVR OFF CACHE INTERNAL "" FORCE )
		set( FG_ENABLE_DEVIL OFF CACHE INTERNAL "" FORCE )
		set( FG_ENABLE_ASSIMP OFF CACHE INTERNAL "" FORCE )
		set( FG_ENABLE_GLM OFF CACHE INTERNAL "" FORCE )
	endif ()

endif ()
