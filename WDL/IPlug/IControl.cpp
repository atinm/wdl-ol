#include "IControl.h"
#include "math.h"
#include "Log.h"

const float GRAYED_ALPHA = 0.25f;

void IControl::SetValueFromPlug(double value)
{
  if (mDefaultValue < 0.0)
  {
    mDefaultValue = mValue = value;
  }

  if (mValue != value)
  {
    mValue = value;
    SetDirty(false);
    Redraw();
  }
}

void IControl::SetValueFromUserInput(double value)
{
  if (mValue != value)
  {
    mValue = value;
    SetDirty();
    Redraw();
  }
}

void IControl::SetDirty(bool pushParamToPlug)
{
  mValue = BOUNDED(mValue, mClampLo, mClampHi);
  mDirty = true;
  if (pushParamToPlug && mPlug && mParamIdx >= 0)
  {
    mPlug->SetParameterFromGUI(mParamIdx, mValue);
    IParam* pParam = mPlug->GetParam(mParamIdx);
    
    if (mValDisplayControl) 
    {
      WDL_String plusLabel;
      char str[32];
      pParam->GetDisplayForHost(str);
      plusLabel.Set(str, 32);
      plusLabel.Append(" ", 32);
      plusLabel.Append(pParam->GetLabelForHost(), 32);
      
      ((ITextControl*)mValDisplayControl)->SetTextFromPlug(plusLabel.Get());
    }
    
    if (mNameDisplayControl) 
    {
      ((ITextControl*)mNameDisplayControl)->SetTextFromPlug((char*) pParam->GetNameForHost());
    }
  }
}

void IControl::SetClean()
{
  mDirty = mRedraw;
  mRedraw = false;
}

void IControl::Hide(bool hide)
{
  mHide = hide;
  mRedraw = true;
  SetDirty(false);
}

void IControl::GrayOut(bool gray)
{
  mGrayed = gray;
  mBlend.mWeight = (gray ? GRAYED_ALPHA : 1.0f);
  SetDirty(false);
}

void IControl::OnMouseDown(int x, int y, IMouseMod* pMod)
{
  #ifdef PROTOOLS
  if (pMod->A && mDefaultValue >= 0.0)
  {
    mValue = mDefaultValue;
    SetDirty();
  }
  #endif
  
  if (pMod->R) {
		PromptUserInput();
	}
}

void IControl::OnMouseDblClick(int x, int y, IMouseMod* pMod)
{
  #ifdef PROTOOLS
  PromptUserInput();
  #else
  if (mDefaultValue >= 0.0)
  {
    mValue = mDefaultValue;
    SetDirty();
  }
  #endif
}

#define PARAM_EDIT_W 40
#define PARAM_EDIT_H 16

void IControl::PromptUserInput()
{
  if (mParamIdx >= 0 && !mDisablePrompt)
  {
    if (mPlug->GetParam(mParamIdx)->GetNDisplayTexts()) // popup menu
    {
      mPlug->GetGUI()->PromptUserInput(this, mPlug->GetParam(mParamIdx), &mRECT );
    }
    else // text entry
    {
      int cX = (int) mRECT.MW();
      int cY = (int) mRECT.MH();
      int halfW = int(float(PARAM_EDIT_W)/2.f);
      int halfH = int(float(PARAM_EDIT_H)/2.f);

      IRECT txtRECT = IRECT(cX - halfW, cY - halfH, cX + halfW,cY + halfH);
      mPlug->GetGUI()->PromptUserInput(this, mPlug->GetParam(mParamIdx), &txtRECT );
    }

    Redraw();
  }
}

void IControl::PromptUserInput(IRECT* pTextRect)
{
  if (mParamIdx >= 0 && !mDisablePrompt)
  {
    mPlug->GetGUI()->PromptUserInput(this, mPlug->GetParam(mParamIdx), pTextRect);
    Redraw();
  }
}

IControl::AuxParam* IControl::GetAuxParam(int idx)
{
  assert(idx > -1 && idx < mAuxParams.GetSize());
  return mAuxParams.Get() + idx;
}

int IControl::AuxParamIdx(int paramIdx)
{
  for (int i=0;i<mAuxParams.GetSize();i++)
  {
    if(GetAuxParam(i)->mParamIdx == paramIdx)
      return i;
  }
  
  return -1;
}

void IControl::AddAuxParam(int paramIdx)
{
  mAuxParams.Add(AuxParam(paramIdx));
}

void IControl::SetAuxParamValueFromPlug(int auxParamIdx, double value)
{
  AuxParam* auxParam = GetAuxParam(auxParamIdx);
  
  if (auxParam->mValue != value)
  {
    auxParam->mValue = value;
    SetDirty(false);
    Redraw();
  }
}

void IControl::SetAllAuxParamsFromGUI()
{
  for (int i=0;i<mAuxParams.GetSize();i++)
  {
    AuxParam* auxParam = GetAuxParam(i);
    mPlug->SetParameterFromGUI(auxParam->mParamIdx, auxParam->mValue);
  }
}

bool IPanelControl::Draw(IGraphics* pGraphics)
{
  pGraphics->FillIRect(&mColor, &mRECT, &mBlend);
  return true;
}

bool IBitmapControl::Draw(IGraphics* pGraphics)
{
  int i = 1;
  if (mBitmap.N > 1)
  {
    i = 1 + int(0.5 + mValue * (double) (mBitmap.N - 1));
    i = BOUNDED(i, 1, mBitmap.N);
  }
  return pGraphics->DrawBitmap(&mBitmap, &mRECT, i, &mBlend);
}

void ISwitchControl::OnMouseDown(int x, int y, IMouseMod* pMod)
{
  if (mBitmap.N > 1)
  {
    mValue += 1.0 / (double) (mBitmap.N - 1);
  }
  else
  {
    mValue += 1.0;
  }

  if (mValue > 1.001)
  {
    mValue = 0.0;
  }
  SetDirty();
}

void ISwitchControl::OnMouseDblClick(int x, int y, IMouseMod* pMod)
{
  OnMouseDown(x, y, pMod);
}

void ISwitchPopUpControl::OnMouseDown(int x, int y, IMouseMod* pMod)
{
  PromptUserInput();

  SetDirty();
}

ISwitchFramesControl::ISwitchFramesControl(IPlugBase* pPlug, int x, int y, int paramIdx, IBitmap* pBitmap, bool imagesAreHorizontal, IChannelBlend::EBlendMethod blendMethod)
  : ISwitchControl(pPlug, x, y, paramIdx, pBitmap, blendMethod)
{
  mDisablePrompt = false;
  
  for(int i = 0; i < pBitmap->N; i++)
  {
    if (imagesAreHorizontal)
      mRECTs.Add(mRECT.SubRectHorizontal(pBitmap->N, i)); 
    else
      mRECTs.Add(mRECT.SubRectVertical(pBitmap->N, i)); 
  }
}

void ISwitchFramesControl::OnMouseDown(int x, int y, IMouseMod* pMod)
{
  int n = mRECTs.GetSize();
  
  for (int i = 0; i < n; i++) 
  {
    if (mRECTs.Get()[i].Contains(x, y)) 
    {
      mValue = (double) i / (double) (n - 1);
      break;
    }
  }
  
  SetDirty();
}

IInvisibleSwitchControl::IInvisibleSwitchControl(IPlugBase* pPlug, IRECT pR, int paramIdx)
  :   IControl(pPlug, pR, paramIdx, IChannelBlend::kBlendClobber)
{
  mDisablePrompt = true;
}

void IInvisibleSwitchControl::OnMouseDown(int x, int y, IMouseMod* pMod)
{
  if (mValue < 0.5)
  {
    mValue = 1.0;
  }
  else
  {
    mValue = 0.0;
  }
  SetDirty();
}

IRadioButtonsControl::IRadioButtonsControl(IPlugBase* pPlug, IRECT pR, int paramIdx, int nButtons,
    IBitmap* pBitmap, EDirection direction, bool reverse)
  :   IControl(pPlug, pR, paramIdx), mBitmap(*pBitmap)
{
  mRECTs.Resize(nButtons);
  int h = int((double) pBitmap->H / (double) pBitmap->N);
  
  if (reverse) 
  {
    if (direction == kHorizontal)
    {
      int dX = int((double) (pR.W() - nButtons * pBitmap->W) / (double) (nButtons - 1));
      int x = mRECT.R - pBitmap->W - dX;
      int y = mRECT.T;
      
      for (int i = 0; i < nButtons; ++i)
      {
        mRECTs.Get()[i] = IRECT(x, y, x + pBitmap->W, y + h);
        x -= pBitmap->W + dX;
      }
    }
    else
    {
      int dY = int((double) (pR.H() - nButtons * h) /  (double) (nButtons - 1));
      int x = mRECT.L;
      int y = mRECT.B - h - dY;
      
      for (int i = 0; i < nButtons; ++i)
      {
        mRECTs.Get()[i] = IRECT(x, y, x + pBitmap->W, y + h);
        y -= h + dY;
      }
    }
    
  }
  else
  {
    int x = mRECT.L, y = mRECT.T;
    
    if (direction == kHorizontal)
    {
      int dX = int((double) (pR.W() - nButtons * pBitmap->W) / (double) (nButtons - 1));
      for (int i = 0; i < nButtons; ++i)
      {
        mRECTs.Get()[i] = IRECT(x, y, x + pBitmap->W, y + h);
        x += pBitmap->W + dX;
      }
    }
    else
    {
      int dY = int((double) (pR.H() - nButtons * h) /  (double) (nButtons - 1));
      for (int i = 0; i < nButtons; ++i)
      {
        mRECTs.Get()[i] = IRECT(x, y, x + pBitmap->W, y + h);
        y += h + dY;
      }
    }
  }
}

void IRadioButtonsControl::OnMouseDown(int x, int y, IMouseMod* pMod)
{
  #ifdef PROTOOLS
  if (pMod->A) 
  {
    if (mDefaultValue >= 0.0)
    {
      mValue = mDefaultValue;
      SetDirty();
      return;
    }
  }
  else
  #endif
  if (pMod->R)
  {
    PromptUserInput();
    return;
  }

  int i, n = mRECTs.GetSize();

  for (i = 0; i < n; ++i)
  {
    if (mRECTs.Get()[i].Contains(x, y))
    {
      mValue = (double) i / (double) (n - 1);
      break;
    }
  }

  SetDirty();
}

bool IRadioButtonsControl::Draw(IGraphics* pGraphics)
{
  int i, n = mRECTs.GetSize();
  int active = int(0.5 + mValue * (double) (n - 1));
  active = BOUNDED(active, 0, n - 1);
  for (i = 0; i < n; ++i)
  {
    if (i == active)
    {
      pGraphics->DrawBitmap(&mBitmap, &mRECTs.Get()[i], 2, &mBlend);
    }
    else
    {
      pGraphics->DrawBitmap(&mBitmap, &mRECTs.Get()[i], 1, &mBlend);
    }
  }
  return true;
}

void IContactControl::OnMouseUp(int x, int y, IMouseMod* pMod)
{
  mValue = 0.0;
  SetDirty();
}

IFaderControl::IFaderControl(IPlugBase* pPlug, int x, int y, int len, int paramIdx, IBitmap* pBitmap, EDirection direction, bool onlyHandle)
  : IControl(pPlug, IRECT(), paramIdx), mLen(len), mBitmap(*pBitmap), mDirection(direction), mOnlyHandle(onlyHandle)
{
  if (direction == kVertical)
  {
    mHandleHeadroom = mBitmap.H;
    mRECT = mTargetRECT = IRECT(x, y, x + mBitmap.W, y + len);
  }
  else
  {
    mHandleHeadroom = mBitmap.W;
    mRECT = mTargetRECT = IRECT(x, y, x + len, y + mBitmap.H);
  }
}

IRECT IFaderControl::GetHandleRECT(double value) const
{
  if (value < 0.0)
  {
    value = mValue;
  }
  IRECT r(mRECT.L, mRECT.T, mRECT.L + mBitmap.W, mRECT.T + mBitmap.H);
  if (mDirection == kVertical)
  {
    int offs = int((1.0 - value) * (double) (mLen - mHandleHeadroom));
    r.T += offs;
    r.B += offs;
  }
  else
  {
    int offs = int(value * (double) (mLen - mHandleHeadroom));
    r.L += offs;
    r.R += offs;
  }
  return r;
}

void IFaderControl::OnMouseDown(int x, int y, IMouseMod* pMod)
{
  #ifdef PROTOOLS
  if (pMod->A) 
  {
    if (mDefaultValue >= 0.0)
    {
      mValue = mDefaultValue;
      SetDirty();
      return;
    }
  }
  else
  #endif
  if (pMod->R)
  {
    PromptUserInput();
    return;
  }

  return SnapToMouse(x, y);
}

void IFaderControl::OnMouseDrag(int x, int y, int dX, int dY, IMouseMod* pMod)
{
  return SnapToMouse(x, y);
}

void IFaderControl::OnMouseWheel(int x, int y, IMouseMod* pMod, int d)
{
#ifdef PROTOOLS
  if (pMod->C)
  {
    mValue += 0.001 * d;
  }
#else
  if (pMod->C || pMod->S)
  {
    mValue += 0.001 * d;
  }
#endif
  else
  {
    mValue += 0.01 * d;
  }
  
  SetDirty();
}

void IFaderControl::SnapToMouse(int x, int y)
{
  if (mDirection == kVertical)
  {
    mValue = 1.0 - (double) (y - mRECT.T - mHandleHeadroom / 2) / (double) (mLen - mHandleHeadroom);
  }
  else
  {
    mValue = (double) (x - mRECT.L - mHandleHeadroom / 2) / (double) (mLen - mHandleHeadroom);
  }
  SetDirty();
}

bool IFaderControl::Draw(IGraphics* pGraphics)
{
  IRECT r = GetHandleRECT();
  return pGraphics->DrawBitmap(&mBitmap, &r, 1, &mBlend);
}

bool IFaderControl::IsHit(int x, int y) 
{
  if(mOnlyHandle)
  {
    IRECT r = GetHandleRECT();
    return r.Contains(x, y); 
  }
  else 
  {
    return mTargetRECT.Contains(x, y); 
  }
}

void IKnobControl::OnMouseDrag(int x, int y, int dX, int dY, IMouseMod* pMod)
{
  double gearing = mGearing;
  
  #ifdef PROTOOLS
    #ifdef OS_WIN
      if (pMod->C) gearing *= 10.0;
    #else
      if (pMod->R) gearing *= 10.0;
    #endif
  #else
    if (pMod->C || pMod->S) gearing *= 10.0;
  #endif
  
  if (mDirection == kVertical)
  {
    mValue += (double) dY / (double) (mRECT.T - mRECT.B) / gearing;
  }
  else
  {
    mValue += (double) dX / (double) (mRECT.R - mRECT.L) / gearing;
  }

  SetDirty();
}

void IKnobControl::OnMouseWheel(int x, int y, IMouseMod* pMod, int d)
{
#ifdef PROTOOLS
  if (pMod->C)
  {
    mValue += 0.001 * d;
  }
#else
  if (pMod->C || pMod->S)
  {
    mValue += 0.001 * d;
  }
#endif
  else
  {
    mValue += 0.01 * d;
  }
  
  SetDirty();
}

IKnobLineControl::IKnobLineControl(IPlugBase* pPlug, IRECT pR, int paramIdx,
                                   const IColor* pColor, double innerRadius, double outerRadius,
                                   double minAngle, double maxAngle, EDirection direction, double gearing)
  :   IKnobControl(pPlug, pR, paramIdx, direction, gearing),
      mColor(*pColor)
{
  mMinAngle = (float) minAngle;
  mMaxAngle = (float) maxAngle;
  mInnerRadius = (float) innerRadius;
  mOuterRadius = (float) outerRadius;
  if (mOuterRadius == 0.0f)
  {
    mOuterRadius = 0.5f * (float) pR.W();
  }
  mBlend = IChannelBlend(IChannelBlend::kBlendClobber);
}

bool IKnobLineControl::Draw(IGraphics* pGraphics)
{
  double v = mMinAngle + mValue * (mMaxAngle - mMinAngle);
  float sinV = (float) sin(v);
  float cosV = (float) cos(v);
  float cx = mRECT.MW(), cy = mRECT.MH();
  float x1 = cx + mInnerRadius * sinV, y1 = cy - mInnerRadius * cosV;
  float x2 = cx + mOuterRadius * sinV, y2 = cy - mOuterRadius * cosV;
  return pGraphics->DrawLine(&mColor, x1, y1, x2, y2, &mBlend, true);
}

bool IKnobRotaterControl::Draw(IGraphics* pGraphics)
{
  int cX = (mRECT.L + mRECT.R) / 2;
  int cY = (mRECT.T + mRECT.B) / 2;
  double angle = mMinAngle + mValue * (mMaxAngle - mMinAngle);
  return pGraphics->DrawRotatedBitmap(&mBitmap, cX, cY, angle, mYOffset, &mBlend);
}

// Same as IBitmapControl::Draw.
bool IKnobMultiControl::Draw(IGraphics* pGraphics)
{
  int i = 1 + int(0.5 + mValue * (double) (mBitmap.N - 1));
  i = BOUNDED(i, 1, mBitmap.N);
  return pGraphics->DrawBitmap(&mBitmap, &mRECT, i, &mBlend);
}

bool IKnobRotatingMaskControl::Draw(IGraphics* pGraphics)
{
  double angle = mMinAngle + mValue * (mMaxAngle - mMinAngle);
  return pGraphics->DrawRotatedMask(&mBase, &mMask, &mTop, mRECT.L, mRECT.T, angle, &mBlend);
}

bool IBitmapOverlayControl::Draw(IGraphics* pGraphics)
{
  if (mValue < 0.5)
  {
    mTargetRECT = mTargetArea;
    return true;  // Don't draw anything.
  }
  else
  {
    mTargetRECT = mRECT;
    return IBitmapControl::Draw(pGraphics);
  }
}

void ITextControl::SetTextFromPlug(char* str)
{
  if (strcmp(mStr.Get(), str))
  {
    SetDirty(false);
    mStr.Set(str);
  }
}

bool ITextControl::Draw(IGraphics* pGraphics)
{
  char* cStr = mStr.Get();
  if (CSTR_NOT_EMPTY(cStr))
  {
    return pGraphics->DrawIText(&mText, cStr, &mRECT);
  }
  return true;
}

ICaptionControl::ICaptionControl(IPlugBase* pPlug, IRECT pR, int paramIdx, IText* pText, bool showParamLabel)
  :   ITextControl(pPlug, pR, pText), mShowParamLabel(showParamLabel)
{
  mParamIdx = paramIdx;
}

void ICaptionControl::OnMouseDown(int x, int y, IMouseMod* pMod)
{
  if (pMod->L || pMod->R)
  {
    PromptUserInput();
  }
}

void ICaptionControl::OnMouseDblClick(int x, int y, IMouseMod* pMod)
{
  PromptUserInput();
}

bool ICaptionControl::Draw(IGraphics* pGraphics)
{
  IParam* pParam = mPlug->GetParam(mParamIdx);
  char cStr[32];
  pParam->GetDisplayForHost(cStr);
  mStr.Set(cStr);

  if (mShowParamLabel)
  {
    mStr.Append(" ");
    mStr.Append(pParam->GetLabelForHost());
  }

  return ITextControl::Draw(pGraphics);
}

IURLControl::IURLControl(IPlugBase* pPlug, IRECT pR, const char* url, const char* backupURL, const char* errMsgOnFailure)
  : IControl(pPlug, pR)
{
  memset(mURL, 0, MAX_URL_LEN);
  memset(mBackupURL, 0, MAX_URL_LEN);
  memset(mErrMsg, 0, MAX_NET_ERR_MSG_LEN);

  if (CSTR_NOT_EMPTY(url))
  {
    strcpy(mURL, url);
  }
  if (CSTR_NOT_EMPTY(backupURL))
  {
    strcpy(mBackupURL, backupURL);
  }
  if (CSTR_NOT_EMPTY(errMsgOnFailure))
  {
    strcpy(mErrMsg, errMsgOnFailure);
  }
}

void IURLControl::OnMouseDown(int x, int y, IMouseMod* pMod)
{
  bool opened = false;

  if (CSTR_NOT_EMPTY(mURL))
  {
    opened = mPlug->GetGUI()->OpenURL(mURL, mErrMsg);
  }

  if (!opened && CSTR_NOT_EMPTY(mBackupURL))
  {
    opened = mPlug->GetGUI()->OpenURL(mBackupURL, mErrMsg);
  }
}

void IFileSelectorControl::OnMouseDown(int x, int y, IMouseMod* pMod)
{
  if (mPlug && mPlug->GetGUI())
  {
    mState = kFSSelecting;
    SetDirty(false);

    mPlug->GetGUI()->PromptForFile(&mFile, mFileAction, &mDir, mExtensions.Get());
    mValue += 1.0;
    if (mValue > 1.0)
    {
      mValue = 0.0;
    }
    mState = kFSDone;
    SetDirty();
  }
}

bool IFileSelectorControl::Draw(IGraphics* pGraphics)
{
  if (mState == kFSSelecting)
  {
    pGraphics->DrawBitmap(&mBitmap, &mRECT, 0, 0);
  }
  return true;
}

void IFileSelectorControl::GetLastSelectedFileForPlug(WDL_String* pStr)
{
  pStr->Set(mFile.Get());
}

void IFileSelectorControl::SetLastSelectedFileFromPlug(char* file)
{
  mFile.Set(file);
}

bool IFileSelectorControl::IsDirty()
{
  if (mDirty)
  {
    return true;
  }

  if (mState == kFSDone)
  {
    mState = kFSNone;
    return true;
  }
  return false;
}

IScrollbar::IScrollbar(IPlugBase *pPlug, IRECT *pR, IScrollInfo *si, IColor fg, IColor bg, IScrollbarListener* listener, bool isVertical)
	: IControl(pPlug, *pR), mListener(listener), fg(fg), bg(bg), vertical(isVertical)
{
	if (si)
		memcpy((void *)&this->si, si, sizeof(*si));
	else
		memset((void *)&this->si, 0, sizeof(*si));
	if (pR)
		memcpy((void *)&this->mRECT, pR, sizeof(*pR));
	else
		memset((void *)&this->mRECT, 0, sizeof(*pR));

	mTargetRECT = mRECT;
	if (vertical) {
		mTargetRECT.T += mRECT.W();
		mTargetRECT.B -= mRECT.W();
	}
	else {
		mTargetRECT.L += mRECT.H();
		mTargetRECT.R -= mRECT.H();
	}

	mTopThumb.L = mRECT.L + 1;
	mTopThumb.R = mRECT.R - 1;
	mTopThumb.T = mRECT.T + 1;
	mTopThumb.B = mRECT.T + mRECT.W();
	mBottomThumb.L = mTopThumb.L;
	mBottomThumb.R = mTopThumb.R;
	mBottomThumb.T = mRECT.B - mRECT.W();
	mBottomThumb.B = mRECT.B - 1;
	mLeftThumb.L = mRECT.L + 1;
	mLeftThumb.R = mRECT.L + mRECT.H();
	mLeftThumb.T = mRECT.B - mRECT.H();
	mLeftThumb.B = mRECT.B - 1;
	mRightThumb.L = mRECT.R - mRECT.H();
	mRightThumb.R = mRECT.R - 1;
	mRightThumb.T = mLeftThumb.T;
	mRightThumb.B = mLeftThumb.B;
}

IScrollbar::~IScrollbar()
{
}

bool IScrollbar::SetScrollInfo(IScrollInfo * si)
{
	if (si)
		memcpy(&this->si, si, sizeof(*si));
	else
		return false;

	return true;
}

bool IScrollbar::GetScrollInfo(IScrollInfo *si) {
	if (si)
		*si = this->si;
	else
		return false;
	return true;
}

bool IScrollbar::Draw(IGraphics * pGraphics)
{
	IRECT handleRECT;

	// background
	if (!pGraphics->FillIRect(&bg, &mRECT))
		return false;
	
	// thumb buttons
	int x1, x2, x3, y1, y2, y3;
	if (vertical) {
		// top triangle
		x1 = mTopThumb.L; x2 = mTopThumb.L + ((double)mTopThumb.W() / 2.0f); x3 = mTopThumb.R;
		y1 = mTopThumb.T + mTopThumb.W(); y2 = mTopThumb.T; y3 = y1;
		if (!pGraphics->FillTriangle(&fg, x1, y1, x2, y2, x3, y3, NULL))
			return false;

		// bottom triangle
		x1 = mBottomThumb.L; x2 = mBottomThumb.L + ((double)mBottomThumb.W() / 2.0f); x3 = mBottomThumb.R;
		y1 = mBottomThumb.B - mBottomThumb.W(); y2 = mBottomThumb.B; y3 = y1;
		if (!pGraphics->FillTriangle(&fg, x1, y1, x2, y2, x3, y3, NULL))
			return false;
	}
	else {
		// left triangle
		x1 = mLeftThumb.L; x2 = mLeftThumb.R; x3 = x2;
		y1 = mLeftThumb.B - ((double)mLeftThumb.H() / 2.0f); y2 = mLeftThumb.B; y3 = mLeftThumb.T;
		if (!pGraphics->FillTriangle(&fg, x1, y1, x2, y2, x3, y3, NULL))
			return false;
		// right triangle
		x1 = mRightThumb.R - mRightThumb.H(); x2 = mRightThumb.R; x3 = x1;
		y1 = mRightThumb.B; y2 = mRightThumb.B - ((double)mRECT.H() / 2.0f); y3 = mRightThumb.T;
		if (!pGraphics->FillTriangle(&fg, x1, y1, x2, y2, x3, y3, NULL))
			return false;
	}

	// thumb slider
	GetThumbSliderRect(handleRECT);
	if (!pGraphics->FillIRect(&fg, &handleRECT))
		return false;

	handleRECT.B -= 2;
	handleRECT.T += 1;
	handleRECT.L += 1;
	handleRECT.R -= 2;
	return pGraphics->DrawRect(&bg, &handleRECT);
}
	

double IScrollbar::GetCurrentPosPercent()
{
	double   thePosPercent = 0.0f;
	long    theCurPos = si.nPos - si.nMin;

	// Compute the current position percent scrolled
	double   theRange = (double)(si.nMax - si.nMin);
	thePosPercent = theCurPos / theRange;
	return (thePosPercent);
}

void IScrollbar::GetThumbSliderRect(IRECT &thumbSliderRect)
{
	long    l;
	float   pctScroll;
	long    thumbSliderLength;

	thumbSliderRect = mTargetRECT;

	// Get the percentage of the scrollbar position
	pctScroll = GetCurrentPosPercent();

	thumbSliderLength = GetThumbSliderLength();

	if (vertical)
	{
		// vertical scrollbar
		l = thumbSliderRect.H() - thumbSliderLength;
		thumbSliderRect.T += (long)((float)l * pctScroll);
		thumbSliderRect.B = thumbSliderRect.T + thumbSliderLength;
	}
	else
	{
		// horizontal scrollbar
		l = thumbSliderRect.W() - thumbSliderLength;
		thumbSliderRect.L += (long)((float)l * pctScroll);
		thumbSliderRect.R = thumbSliderRect.L + thumbSliderLength;
	}


}

void IScrollbar::SetScrollPos(int pos, bool dirty)
{
	si.nPos = pos;
	// bounds checking:
	if (si.nPos < si.nMin)
		si.nPos = si.nMin;
	else if (si.nPos > si.nMax)
		si.nPos = si.nMax;
	if (dirty)
		SetDirty();
	if (mListener)
		mListener->NotifyParentScrollPosChange(this, si.nPos);
}

int IScrollbar::GetThumbSliderLength()
{
	return (int)(((double)si.nPage / (double)si.nMax) * (vertical ? (double)mTargetRECT.H() : (double)mTargetRECT.W()));
}

void IScrollbar::OnMouseDown(int x, int y, IMouseMod * pMod)
{
	if (vertical) {
		if (mTopThumb.Contains(x, y)) {
			SetScrollPos(si.nPos - 1, true);
		}
		else if (mBottomThumb.Contains(x, y)) {
			SetScrollPos(si.nPos + 1, true);
		}
		else {
			SetScrollPos(((double)(y - mTargetRECT.T) / (double)mTargetRECT.H()) * ((double)si.nMax - (double)si.nMin), true);
		}
	}
	else {
		if (mLeftThumb.Contains(x, y)) {
			SetScrollPos(si.nPos - 1, true);
		}
		else if (mRightThumb.Contains(x, y)) {
			SetScrollPos(si.nPos + 1, true);
		}
		else {
			SetScrollPos(((double)(x - mTargetRECT.L) / (double)mTargetRECT.W()) * ((double)si.nMax - (double)si.nMin), true);
		}
	}
}

void IScrollbar::OnMouseDrag(int x, int y, int dX, int dY, IMouseMod* pMod)
{
	SetScrollPos(si.nPos + 
		(vertical ? 
			((double)si.nMax / (double)mTargetRECT.H()) * (double)dY :
			((double)si.nMax / (double)mTargetRECT.W()) * (double)dX),
		true);
}

void IScrollbar::OnMouseWheel(int x, int y, IMouseMod* pMod, int d)
{
	SetScrollPos(si.nPos + ((d < 0) ? si.nPage : -si.nPage), true);
}

bool IScrollbar::OnKeyDown(int x, int y, int key)
{
	switch (key) {
	case KEY_LEFTARROW:
	case KEY_UPARROW:
		SetScrollPos(si.nPos - 1, true);
		break;
	case KEY_RIGHTARROW:
	case KEY_DOWNARROW:
		SetScrollPos(si.nPos + 1, true);
		break;
	case KEY_HOME:
		SetScrollPos(si.nMin, true);
		break;
	case KEY_END:
		SetScrollPos(si.nMax, true);
		break;
	case KEY_PRIOR:
		SetScrollPos(si.nPos - si.nPage, true);
		break;
	case KEY_NEXT:
		SetScrollPos(si.nPos + si.nPage, true);
		break;
	default:
		return false;
	}
	return true;
}
