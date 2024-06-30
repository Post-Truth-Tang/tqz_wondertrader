#pragma once

#include "../../Includes/WTSDataDef.hpp"
USING_NS_OTP;


namespace hftalphas {

typedef unique_ptr<vector<WTSTickData>> TicksUPtr;
typedef vector<double> Differs;

class MacdAlpha {

	MacdAlpha() = default;
	~MacdAlpha() = default;

public:
	static int32_t alphaValue(
		const TicksUPtr& ticks,
		const uint32_t fastParam, 
		const uint32_t midParam,
		const uint32_t slowParam,
		const uint32_t unitValue = 1
	);


private:
	static bool __paramIsOK(const uint32_t fastParam, const uint32_t midParam, const uint32_t slowParam);

	static double __maValue(const TicksUPtr& ticks, uint32_t beginIndex, uint32_t endIndex);
};

}