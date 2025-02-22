/*!
 * \file WtHelper.cpp
 * \project	WonderTrader
 *
 * \author Wesley
 * \date 2020/03/30
 * 
 * \brief 
 */
#include "WtHelper.h"

#include "../Share/StrUtil.hpp"
#include "../Share/BoostFile.hpp"

#ifdef _WIN32
#include <direct.h>
#else	//UNIX
#include <stdio.h>
#endif

std::string WtHelper::_bin_dir;

const char* WtHelper::get_cwd()
{
	static std::string _cwd;
	if(_cwd.empty())
	{
		char   buffer[255];
#ifdef _WIN32
		_getcwd(buffer, 255);
#else	//UNIX
		getcwd(buffer, 255);
#endif
		_cwd = buffer;
		_cwd = StrUtil::standardisePath(_cwd);
	}	
	return _cwd.c_str();
}