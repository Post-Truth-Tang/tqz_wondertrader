
#include "TQZHftDiff.h"


namespace hftalphas {

	int32_t DiffAlpha::alphaValue(const TicksUPtr& ticks, const uint32_t length, const uint32_t unitValue) {
		if (ticks->size() <= length || !length)
			return 0;

		double sumDifferValue(0);
		double lastDifferValue(0);
		for (size_t index(ticks->size() - length); index < ticks->size(); index++) {
			WTSTickData pre_tickData = ticks->at(index-1);
			WTSTickData tickData = ticks->at(index);

			double pre_midPrice = (pre_tickData.askprice(0) + pre_tickData.bidprice(0)) * 0.5;
			double midPrice = (tickData.askprice(0) + tickData.bidprice(0)) * 0.5;

			sumDifferValue += (midPrice - pre_midPrice);
			lastDifferValue = (midPrice - pre_midPrice);
		}
		double averageDifferValue(sumDifferValue / length);

		/// calculate alpha value.
		int32_t differAlphaValue(0);
		if (lastDifferValue > averageDifferValue)
			differAlphaValue = unitValue;
		else if (lastDifferValue < averageDifferValue)
			differAlphaValue = (-1 * unitValue);
		return differAlphaValue;
	}

}