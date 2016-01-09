#include <algorithm>
#include "IControl.h"
#include "MyKnobControl.h"

bool MyKnobControl::Draw(IGraphics *pGraphics)
{
	static const float shadow_angle = PI / 1.3f;
	int cX = (mRECT.L + mRECT.R) / 2;
	int cY = (mRECT.T + mRECT.B) / 2;
	double angle = mMinAngle + mValue * (mMaxAngle - mMinAngle);
	float full_radius = 0;
	int i = 0;
//	for (i = 0; i < 10; i++) {
		full_radius = std::min((mRECT.R - mRECT.L) + i / 2.0f, (mRECT.B - mRECT.T) + i / 2.0f);
		if (pGraphics->DrawArc(&COLOR_RED, cX, cY, full_radius, mMinAngle, angle) && pGraphics->DrawArc(&COLOR_BLACK, cX, cY, full_radius, angle, mMaxAngle)) {
			return true;
		}
		else {
			return false;
		}
//	}
	return true;
}