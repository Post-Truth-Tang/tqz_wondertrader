
#1. 确定CMake的最低版本需求
CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0)

#2. 确定工程名
PROJECT(ParserCTP LANGUAGES CXX)
SET(CMAKE_CXX_STANDARD 11)


IF (WIN32)
	IF("${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
		SET(INCS $ENV{MyDepends141}/include)
		SET(LNKS $ENV{MyDepends141}/lib/x64)
	ELSE()
		SET(INCS $ENV{MyDepends141}/include)
		SET(LNKS $ENV{MyDepends141}/lib/x86)
	ENDIF()
ELSE (UNIX)
	SET (MyDeps "/home/mydeps")
	SET(INCS ${MyDeps}/include)
	SET(LNKS ${MyDeps}/lib)
ENDIF ()

SET(SRC  
	${PROJECT_SOURCE_DIR}/ParserCTP.cpp
	${PROJECT_SOURCE_DIR}/ParserCTP.h
	
	${PROJECT_SOURCE_DIR}/ThostTraderApi/ThostFtdcMdApi.h
	${PROJECT_SOURCE_DIR}/ThostTraderApi/ThostFtdcUserApiDataType.h
	${PROJECT_SOURCE_DIR}/ThostTraderApi/ThostFtdcUserApiStruct.h
)

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../build_${CMAKE_BUILD_TYPE}/bin)

INCLUDE_DIRECTORIES(${INCS})
LINK_DIRECTORIES(${LNKS})
ADD_LIBRARY(ParserCTP SHARED ${SRC})

SET(LIBS
	boost_thread
	boost_filesystem
)
TARGET_LINK_LIBRARIES(ParserCTP ${LIBS})

IF (WIN32)
ELSE (UNIX)
	SET_TARGET_PROPERTIES(ParserCTP PROPERTIES
		CXX_VISIBILITY_PRESET hidden
		C_VISIBILITY_PRESET hidden
		VISIBILITY_INLINES_HIDDEN 1)
ENDIF ()

