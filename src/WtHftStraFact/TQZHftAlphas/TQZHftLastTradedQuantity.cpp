#include "TQZHftLastTradedQuantity.h"

namespace hftalphas {

	int32_t LastTradedQuantityAlpha::alphaValue(const TicksUPtr& ticks, const uint32_t length, const uint32_t unitValue) {
		if (ticks->size() < length || !length)
			return 0;

		double sumSizeValue(0);
		for (size_t index(ticks->size() - length); index < ticks->size(); index++) {
			double midSizeValue = (ticks->at(index).bidqty(0) + ticks->at(index).askqty(0) * -1) * 0.5;

			sumSizeValue += midSizeValue;
		}

		/// calculate alpha value.
		int32_t alphaValue(0);
		if (sumSizeValue > 0)
			alphaValue = unitValue;
		else if (sumSizeValue < 0)
			alphaValue = -1 * unitValue;
		return alphaValue;
	}
}