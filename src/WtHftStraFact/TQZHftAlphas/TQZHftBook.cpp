#include "TQZHftBook.h"
#include <iostream>

namespace hftalphas {

	int32_t BookAlpha::alphaValue(const TicksUPtr& ticks, const uint32_t length, const uint32_t unitValue) {
		if (ticks->size() < length)
			return 0;

		double totalAsk(0);
		double totalBid(0);
		double totalAskSize(0);
		double totalBidSize(0);
		double lastMidPrice(0);
		for (size_t index(ticks->size() - length); index < ticks->size(); index++) {
			WTSTickData tickData = ticks->at(index);

			totalAsk += (tickData.askprice(0) * tickData.askqty(0));
			totalBid += (tickData.bidprice(0) * tickData.bidqty(0));

			totalAskSize += tickData.askqty(0);
			totalBidSize += tickData.bidqty(0);

			lastMidPrice = (tickData.askprice(0) * tickData.askqty(0)) * 0.5;
		}

		/// make sure midPrice is correct.
		if (!(totalAskSize && totalBidSize))
			return 0;
		double midPrice = (totalAsk / totalAskSize) + (totalBid / totalBidSize);

		/// calculate alpha value.
		int32_t alphaValue(0);
		if (midPrice < lastMidPrice)
			alphaValue = unitValue;
		else if (midPrice > lastMidPrice)
			alphaValue = -1 * unitValue;
		return alphaValue;
	}

}