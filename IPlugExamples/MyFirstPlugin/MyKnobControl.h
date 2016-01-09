#pragma once

#include "IControl.h"

class MyKnobControl : public IKnobControl
{
public:
	MyKnobControl(IPlugBase* pPlug, int x, int y, int paramIdx, IBitmap* pBitmap,
		double minAngle = -0.75 * PI, double maxAngle = 0.75 * PI, int yOffsetZeroDeg = 0,
		EDirection direction = kVertical, double gearing = DEFAULT_GEARING)
		: IKnobControl(pPlug, IRECT(x, y, pBitmap), paramIdx, direction, gearing),
		mBitmap(*pBitmap), mMinAngle(minAngle), mMaxAngle(maxAngle), mYOffset(yOffsetZeroDeg) {}
	~MyKnobControl() {}

	bool Draw(IGraphics* pGraphics);

protected:
	IBitmap mBitmap;
	double mMinAngle, mMaxAngle;
	int mYOffset;
};