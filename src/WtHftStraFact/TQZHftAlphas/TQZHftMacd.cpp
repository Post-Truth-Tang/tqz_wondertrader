#include "TQZHftMacd.h"
#include <iostream>

namespace hftalphas {

	int32_t MacdAlpha::alphaValue(
		const TicksUPtr& ticks,
		const uint32_t fastParam,
		const uint32_t midParam,
		const uint32_t slowParam,
		const uint32_t unitValue
	) {
		if (!MacdAlpha::__paramIsOK(fastParam, midParam, slowParam))
			return 0;
		if (ticks->size() < slowParam)
			return 0;

		Differs maDiffers;

		/// calculate differ.
		size_t fastBeginIndex = ticks->size() - fastParam;
		for (size_t fastIndex(fastBeginIndex); fastIndex < ticks->size(); fastIndex++)
			maDiffers.push_back(MacdAlpha::__maValue(ticks, fastIndex - midParam, fastIndex) - MacdAlpha::__maValue(ticks, fastIndex - slowParam, fastIndex));
		
		double sumValue(0);
		for (size_t index(0); index < maDiffers.size(); index++)
			sumValue += maDiffers.at(index);

		/// calculate ma(differ) & macd alpha value.
		int32_t macdAlphaValue = static_cast<int32_t>(sumValue / maDiffers.size());
		if (macdAlphaValue > 0)
			macdAlphaValue = unitValue;
		else if (macdAlphaValue < 0)
			macdAlphaValue = -1 * unitValue;
		return macdAlphaValue;
	}

	bool MacdAlpha::__paramIsOK(const uint32_t fastParam, const uint32_t midParam, const uint32_t slowParam) {
		return (slowParam > midParam) && (midParam > fastParam);
	}
	
	double MacdAlpha::__maValue(const TicksUPtr& ticks, uint32_t beginIndex, uint32_t endIndex) {
		if (!(endIndex > beginIndex && ticks->size() > endIndex))
			return 0;

		double sumValue(0);
		for (size_t index(beginIndex); index <= endIndex; index++)
			sumValue += ticks->at(index).price();

		return sumValue / (endIndex - beginIndex);
	}

}