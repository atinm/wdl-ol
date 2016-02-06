#include "Sampler.h"
#include "IPlug_include_in_plug_src.h"
#include "resource.h"

#include "IGraphics.h"
#include "IControl.h"
#include "IKeyboardControl.h"
#include "IAutoGUI.h"
#include <list>

const int kNumPrograms = 8;

#define PITCH 440.
#define TABLE_SIZE 512

#ifndef M_PI
#define M_PI 3.14159265
#endif

#define GAIN_FACTOR 0.2;

enum EParams
{
  kAttack = 0,
  kDecay,
  kSustain,
  kRelease,
  kSelection,
  kSample,
  kNumParams
};

class MyTextControl : public ITextControl, public IScrollbarListener {
public:
	MyTextControl(IPlugBase* pPlug, IRECT pR, IText* pText, const char* str = "")
		: ITextControl(pPlug, pR, pText, str) {};
	virtual ~MyTextControl() {};
	//virtual bool Draw(IGraphics* pGraphics);
	virtual void NotifyParentScrollPosChange(IScrollbar *sb, int pos) {
		char buffer[1024];
		snprintf(buffer, sizeof(buffer), "pos = %d", pos);
		this->SetTextFromPlug(buffer);
		((ITextControl *)this)->SetDirty(true);
	}
};

class ListItem : public IListItem, public ITextControl {
public:
	ListItem(IPlugBase* pPlug, IRECT pR, IText* pText,
		const IColor *pOnFG, const IColor *pOnBG, const IColor *pOffFG, const IColor *pOffBG,
		const char* str = "")
		: ITextControl(pPlug, pR, pText, str),
		mOnFG(*pOnFG), mOnBG(*pOnBG), mOffFG(*pOffFG), mOffBG(*pOffBG) {};
	virtual ~ListItem() {};

	virtual bool Draw(IGraphics* pGraphics) override
	{
		IText *text = this->GetText();
		if (this->GetSelected())
		{
			pGraphics->FillIRect(&mOnBG, GetRECT());
			text->mTextEntryFGColor = mOnFG;
		}
		else
		{
			pGraphics->FillIRect(&mOffBG, GetRECT());
			text->mTextEntryFGColor = mOffFG;
		}

		return ITextControl::Draw(pGraphics);
	}

	virtual void SetItemRECT(IRECT pR) override
	{
		this->SetRECT(pR);
	}

	virtual bool IsItemHit(int x, int y) override
	{
		return this->IsHit(x, y);
	}

private:
	IColor mOnFG, mOnBG, mOffFG, mOffBG;
};

Sampler::Sampler(IPlugInstanceInfo instanceInfo)
  : IPLUG_CTOR(kNumParams, kNumPrograms, instanceInfo),
    mSampleRate(44100.),
    mNumHeldKeys(0),
    mKey(-1),
    mActiveVoices(0)

{
  TRACE;

  mTable = new double[TABLE_SIZE];

  for (int i = 0; i < TABLE_SIZE; i++)
  {
    mTable[i] = sin( i/double(TABLE_SIZE) * 2. * M_PI);
    //printf("mTable[%i] %f\n", i, mTable[i]);
  }

  mOsc = new CWTOsc(mTable, TABLE_SIZE);
  mEnv = new CADSREnvL();

  memset(mKeyStatus, 0, 128 * sizeof(bool));

  //arguments are: name, defaultVal, minVal, maxVal, step, label
  GetParam(kAttack)->InitDouble("Amp Attack", ATTACK_DEFAULT, TIME_MIN, TIME_MAX, 0.001);
  GetParam(kDecay)->InitDouble("Amp Decay", DECAY_DEFAULT, TIME_MIN, TIME_MAX, 0.001);
  GetParam(kSustain)->InitDouble("Amp Sustain", 1., 0., 1., 0.001);
  GetParam(kRelease)->InitDouble("Amp Release", RELEASE_DEFAULT, TIME_MIN, TIME_MAX, 0.001);
  GetParam(kSelection)->InitInt("Selection", 1, 1, 3, "button");

  IGraphics* pGraphics = MakeGraphics(this, kWidth, kHeight);
  pGraphics->AttachBackground(BG_ID, BG_FN);

  IBitmap knob = pGraphics->LoadIBitmap(KNOB_ID, KNOB_FN, kKnobFrames);
  IBitmap top_buttons = pGraphics->LoadIBitmap(TOP_BUTTONS_ID, TOP_BUTTONS_FN, kTopButtonsControls);
  pGraphics->AttachControl(new IRadioButtonsControl(this, IRECT(kTopButtonsControl_X, kTopButtonsControl_Y, kTopButtonsControl_X + (kTopButtonsControl_W*kTopButtonsControl_N), kTopButtonsControl_Y + (kTopButtonsControl_H*kTopButtonsControl_N)), kSelection, kTopButtonsControl_N, &top_buttons, kHorizontal));

  IBitmap regular = pGraphics->LoadIBitmap(WHITE_KEY_ID, WHITE_KEY_FN, 6);
  IBitmap sharp   = pGraphics->LoadIBitmap(BLACK_KEY_ID, BLACK_KEY_FN);

  //                    C#     D#          F#      G#      A#
  int coords[12] = { 0, 7, 12, 20, 24, 36, 43, 48, 56, 60, 69, 72 };
  mKeyboard = new IKeyboardControl(this, kKeybX, kKeybY, 21 /* A0 */, 7, &regular, &sharp, coords);

  pGraphics->AttachControl(mKeyboard);

#if 0
  MyTextControl* textControl = new MyTextControl(this, IRECT(100, 100, 200, 200), &text, &itemBg, &selected, &itemFg, &itemBg, "items");
  //Attach ITextControl
  pGraphics->AttachControl((ITextControl *)textControl);
  IBitmap about = pGraphics->LoadIBitmap(ABOUTBOX_ID, ABOUTBOX_FN);
  mAboutBox = new IBitmapOverlayControl(this, 100, 100, &about, IRECT(540, 250, 680, 290));
  pGraphics->AttachControl(mAboutBox);
  IScrollInfo si;
  si.nMin = 0;
  si.nMax = 5000;
  si.nPage = 500;
  si.nPos = 0;
  IRECT scrollbarRect;
  scrollbarRect.L = 950;
  scrollbarRect.T = 20;
  scrollbarRect.R = 960;
  scrollbarRect.B = 584;
  vscrollbar = new IScrollbar(this, &scrollbarRect, &si, IColor(255, 81, 81, 81), IColor(255, 28, 198, 227), textControl, true);
  pGraphics->AttachControl(vscrollbar);
  scrollbarRect.L = 8;
  scrollbarRect.T = 582;
  scrollbarRect.R = 952;
  scrollbarRect.B = 592;
  si.nMin = 0;
  si.nMax = 5000;
  si.nPage = 500;
  si.nPos = 0;
  hscrollbar = new IScrollbar(this, &scrollbarRect, &si, IColor(255, 81, 81, 81), IColor(255, 28, 198, 227), NULL, false);
  pGraphics->AttachControl(hscrollbar);

#endif // 0

  int yoffs = 100;
  IPanelTabs* pTabsControl = 0;
  IRECT tabsRect = IRECT(2, yoffs, 958, 590);
  const IColor fg(0xff, 0x8c, 0x90, 0x96);
  const IColor bg(0xff, 0x2c, 0x31, 0x3d);
  const IColor on(0xff, 0x44, 0x48, 0x5c);
  const IColor itemFg(0xff, 0xf6, 0xff, 0xff);
  const IColor itemBg(0xff, 0x2c, 0x31, 0x3d);
  const IColor selected(0xff, 0xe8, 0xba, 0x3c);
  IText tabText = IText(14, &COLOR_WHITE, &bg);
  pTabsControl = new IPanelTabs(this, tabsRect, &tabText, &bg, &COLOR_WHITE, &on);
  pGraphics->AttachControl(pTabsControl);
  yoffs += 20;

  WDL_PtrList<const char> groupNames;
  WDL_String thisGroup("");
  groupNames.Add("Browser");
  groupNames.Add("Sampler");
  groupNames.Add("Settings");
  ITab* pTab[3];
  IRECT thisTabRect = tabsRect;
  IRECT tabTargetArea = tabsRect;
  tabTargetArea.T += 22;
  tabTargetArea.B -= 2;
  int groupIdx = 0;
  thisTabRect = tabsRect.SubRectHorizontal(groupNames.GetSize(), groupIdx);
  thisTabRect.B = thisTabRect.T + 20;
  pTab[groupIdx] = new ITab(thisTabRect, groupNames.Get(groupIdx));
  pTabsControl->AddTab(pTab[groupIdx]);

  IListBoxControl *list = new IListBoxControl(this, tabTargetArea, -1, NULL, 100, 14, 12, false, true, &bg, &fg);
  pTab[0]->Add(list);
  pGraphics->AttachControl(list);
  groupIdx++;
  thisTabRect = tabsRect.SubRectHorizontal(groupNames.GetSize(), groupIdx);
  thisTabRect.B = thisTabRect.T + 20;
  pTab[groupIdx] = new ITab(thisTabRect, groupNames.Get(groupIdx));
  pTabsControl->AddTab(pTab[groupIdx]);

  IListBoxControl *mlist = new IListBoxControl(this, tabTargetArea, -1, NULL, 652, 14, 12, true, false, &bg, &fg);
  pTab[1]->Add(mlist);
  pGraphics->AttachControl(mlist);
  mlist->Hide(true);
  groupIdx++;
  thisTabRect = tabsRect.SubRectHorizontal(groupNames.GetSize(), groupIdx);
  thisTabRect.B = thisTabRect.T + 20;
  pTab[groupIdx] = new ITab(thisTabRect, groupNames.Get(groupIdx));
  pTabsControl->AddTab(pTab[groupIdx]);
  groupIdx++;
  IText text = IText(DEFAULT_TEXT_SIZE, &COLOR_WHITE, &bg, TRANSPARENT, 0, IText::kStyleNormal, IText::kAlignNear);

  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 1"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 2"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 3"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 4"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 5"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 6"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 7"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 8"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 9"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 10"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 11"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 12"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 13"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 14"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 15"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 16"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 17"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 18"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 19"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 20"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 21"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 22"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 23"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 24"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 25"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 26"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 27"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 28"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 29"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 30"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 31"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 32"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 33"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 34"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 35"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 36"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 37"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 38"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 39"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 40"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 41"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 42"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 43"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 44"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 45"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 46"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 47"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 48"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 49"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 50"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 51"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 52"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 53"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 54"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 55"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 56"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 57"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 58"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 59"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 60"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 61"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 62"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 63"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 64"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 65"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 66"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 67"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 68"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 69"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 70"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 71"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 72"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 73"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 74"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 75"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 76"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 77"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 78"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 79"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 80"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 81"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 82"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 83"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 84"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 85"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 86"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 87"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 88"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 89"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 90"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 91"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 92"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 93"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 94"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 95"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 96"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 97"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 98"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 99"));
  list->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 100"));

  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 1"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 2"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 3"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 4"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 5"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 6"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 7"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 8"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 9"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 10"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 11"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 12"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 13"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 14"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 15"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 16"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 17"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 18"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 19"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 20"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 21"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 22"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 23"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 24"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 25"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 26"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 27"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 28"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 29"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 30"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 31"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 32"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 33"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 34"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 35"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 36"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 37"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 38"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 39"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 40"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 41"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 42"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 43"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 44"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 45"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 46"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 47"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 48"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 49"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 50"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 51"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 52"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 53"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 54"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 55"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 56"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 57"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 58"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 59"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 60"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 61"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 62"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 63"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 64"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 65"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 66"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 67"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 68"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 69"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 70"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 71"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 72"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 73"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 74"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 75"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 76"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 77"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 78"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 79"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 80"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 81"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 82"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 83"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 84"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 85"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 86"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 87"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 88"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 89"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 90"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 91"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 92"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 93"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 94"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 95"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 96"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 97"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 98"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 99"));
  mlist->AddItem(new ListItem(this, IRECT(0, 0, 0, 0), &text, &itemBg, &selected, &itemFg, &itemBg, "item 100"));

  AttachGraphics(pGraphics);

  //MakePreset("preset 1", ... );
  MakeDefaultPreset((char *) "-", kNumPrograms);
}

Sampler::~Sampler()
{
  delete mOsc;
  delete mEnv;
  delete [] mTable;
}

int Sampler::FindFreeVoice()
{
  int v;

  for(v = 0; v < MAX_VOICES; v++)
  {
    if(!mVS[v].GetBusy())
      return v;
  }

  int quietestVoice = 0;
  double level = 2.;

  for(v = 0; v < MAX_VOICES; v++)
  {
    double summed = mVS[v].mEnv_ctx.mPrev;

    if (summed < level)
    {
      level = summed;
      quietestVoice = v;
    }

  }

  DBGMSG("stealing voice %i\n", quietestVoice);
  return quietestVoice;
}

void Sampler::NoteOnOff(IMidiMsg* pMsg)
{
  int v;

  int status = pMsg->StatusMsg();
  int velocity = pMsg->Velocity();
  int note = pMsg->NoteNumber();

  if (status == IMidiMsg::kNoteOn && velocity) // Note on
  {
    v = FindFreeVoice(); // or quietest
    mVS[v].mKey = note;
    mVS[v].mOsc_ctx.mPhaseIncr = (1./mSampleRate) * midi2CPS(note);
    mVS[v].mEnv_ctx.mLevel = (double) velocity / 127.;
    mVS[v].mEnv_ctx.mStage = kStageAttack;

    mActiveVoices++;
  }
  else  // Note off
  {
    for (v = 0; v < MAX_VOICES; v++)
    {
      if (mVS[v].mKey == note)
      {
        if (mVS[v].GetBusy())
        {
          mVS[v].mKey = -1;
          mVS[v].mEnv_ctx.mStage = kStageRelease;
          mVS[v].mEnv_ctx.mReleaseLevel = mVS[v].mEnv_ctx.mPrev;

          return;
        }
      }
    }
  }

}

void Sampler::ProcessDoubleReplacing(double** inputs, double** outputs, int nFrames)
{
  // Mutex is already locked for us
  IKeyboardControl* pKeyboard = (IKeyboardControl*) mKeyboard;

  if (pKeyboard->GetKey() != mKey)
  {
    IMidiMsg msg;

    if (mKey >= 0)
    {
      msg.MakeNoteOffMsg(mKey + 48, 0, 0);
      mMidiQueue.Add(&msg);
    }

    mKey = pKeyboard->GetKey();

    if (mKey >= 0)
    {
      msg.MakeNoteOnMsg(mKey + 48, pKeyboard->GetVelocity(), 0, 0);
      mMidiQueue.Add(&msg);
    }
  }

  if (mActiveVoices > 0 || !mMidiQueue.Empty()) // block not empty
  {
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double output;
    CVoiceState* vs;

    for (int s = 0; s < nFrames; ++s)
    {
      while (!mMidiQueue.Empty())
      {
        IMidiMsg* pMsg = mMidiQueue.Peek();

        if (pMsg->mOffset > s) break;

        int status = pMsg->StatusMsg(); // get the MIDI status byte

        switch (status)
        {
          case IMidiMsg::kNoteOn:
          case IMidiMsg::kNoteOff:
          {
            NoteOnOff(pMsg);
            break;
          }
          case IMidiMsg::kPitchWheel:
          {
            //TODO
            break;
          }
        }

        mMidiQueue.Remove();
      }

      output = 0.;

      for(int v = 0; v < MAX_VOICES; v++) // for each vs
      {
        vs = &mVS[v];

        if (vs->GetBusy())
        {
          output += mOsc->process(&vs->mOsc_ctx) * mEnv->process(&vs->mEnv_ctx);
        }
      }

      output *= GAIN_FACTOR;

      *out1++ = output;
      *out2++ = output;
    }

    mMidiQueue.Flush(nFrames);
  }
//  else // empty block
//  {
//  }
}

void Sampler::Reset()
{
  TRACE;
  IMutexLock lock(this);

  mSampleRate = GetSampleRate();
  mMidiQueue.Resize(GetBlockSize());
  mEnv->setSampleRate(mSampleRate);
}

void Sampler::setSelection(int selection)
{
	mSelection = (ESelections)selection;
}

void Sampler::OnParamChange(int paramIdx)
{
  IMutexLock lock(this);

  switch (paramIdx)
  {
    case kAttack:
      mEnv->setStageTime(kStageAttack, GetParam(kAttack)->Value());
      break;
    case kDecay:
      mEnv->setStageTime(kStageDecay, GetParam(kDecay)->Value());
      break;
    case kSustain:
      mEnv->setSustainLevel( GetParam(kSustain)->Value() );
      break;
    case kRelease:
      mEnv->setStageTime(kStageRelease, GetParam(kRelease)->Value());
      break;
	case kSelection:
		this->setSelection(GetParam(kSelection)->Value());
		break;
    default:
      break;
  }
}

void Sampler::ProcessMidiMsg(IMidiMsg* pMsg)
{
  int status = pMsg->StatusMsg();
  int velocity = pMsg->Velocity();
  
  switch (status)
  {
    case IMidiMsg::kNoteOn:
    case IMidiMsg::kNoteOff:
      // filter only note messages
      if (status == IMidiMsg::kNoteOn && velocity)
      {
        mKeyStatus[pMsg->NoteNumber()] = true;
        mNumHeldKeys += 1;
      }
      else
      {
        mKeyStatus[pMsg->NoteNumber()] = false;
        mNumHeldKeys -= 1;
      }
      break;
    default:
      return; // if !note message, nothing gets added to the queue
  }
  

  mKeyboard->SetDirty();
  mMidiQueue.Add(pMsg);
}

// Should return non-zero if one or more keys are playing.
int Sampler::GetNumKeys()
{
  IMutexLock lock(this);
  return mNumHeldKeys;
}

// Should return true if the specified key is playing.
bool Sampler::GetKeyStatus(int key)
{
  IMutexLock lock(this);
  return mKeyStatus[key];
}

//Called by the standalone wrapper if someone clicks about
bool Sampler::HostRequestingAboutBox()
{
  IMutexLock lock(this);
  if(GetGUI())
  {
    // get the IBitmapOverlay to show
    mAboutBox->SetValueFromPlug(1.);
    mAboutBox->Hide(false);
  }
  return true;
}
