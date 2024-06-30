#include "TQZHftMa.h"
#include <iostream>

namespace hftalphas {

	int32_t MaAlpha::alphaValue(const TicksUPtr& ticks, const uint32_t length, const uint32_t unitValue) {
		if (!ticks->size() || ticks->size() < length)
			return 0;

		double sumValue(0);
		size_t beginIndex(ticks->size() - length);
		for (size_t i(beginIndex); i < static_cast<size_t>(ticks->size()); i++)
			sumValue += ticks->at(i).price();

		double maValue(sumValue / length);
		int32_t alphaValue(0);
		WTSTickData lastTick = ticks->at(ticks->size() - 1);
		if (lastTick.price() > maValue)
			alphaValue = unitValue;
		else if (lastTick.price() < maValue)
			alphaValue = -1 * unitValue;

		return alphaValue;
	}

}