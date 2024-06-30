#pragma once

#include <stdint.h>
//#include <vector>

#include "../../Includes/WTSDataDef.hpp"
USING_NS_OTP;

namespace hftalphas {

typedef unique_ptr<vector<WTSTickData>> TicksUPtr;

class MaAlpha {
public:
	MaAlpha() = default;
	~MaAlpha() = default;

public:
	static int32_t alphaValue(const TicksUPtr& ticks, const uint32_t length, const uint32_t unitValue = 1);

};

}