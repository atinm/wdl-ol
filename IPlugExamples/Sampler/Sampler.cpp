#include "Sampler.h"
#include "IPlug_include_in_plug_src.h"
#include "resource.h"

#include "IControl.h"
#include "IKeyboardControl.h"


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

  IText text = IText(14);
  IBitmap regular = pGraphics->LoadIBitmap(WHITE_KEY_ID, WHITE_KEY_FN, 6);
  IBitmap sharp   = pGraphics->LoadIBitmap(BLACK_KEY_ID, BLACK_KEY_FN);

  //                    C#     D#          F#      G#      A#
  int coords[12] = { 0, 7, 12, 20, 24, 36, 43, 48, 56, 60, 69, 72 };
  mKeyboard = new IKeyboardControl(this, kKeybX, kKeybY, 21 /* A0 */, 7, &regular, &sharp, coords);

  pGraphics->AttachControl(mKeyboard);

  MyTextControl* textControl = new MyTextControl(this, IRECT(100, 100, 200, 200), &text, "Display text strings");
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
  scrollbarRect.T = 5;
  scrollbarRect.R = 960;
  scrollbarRect.B = 584;
  vscrollbar = new IScrollbar(this, &scrollbarRect, &si, IColor(255, 81, 81, 81), IColor(255, 28, 198, 227), textControl, true);
  pGraphics->AttachControl(vscrollbar);
  scrollbarRect.L = 8;
  scrollbarRect.T = 582;
  scrollbarRect.R = 952;
  scrollbarRect.B = 592;
  si.nMin = 0;
  si.nMax = 10000;
  si.nPage = 1000;
  si.nPos = 0;
  hscrollbar = new IScrollbar(this, &scrollbarRect, &si, IColor(255, 81, 81, 81), IColor(255, 28, 198, 227), NULL, false);
  pGraphics->AttachControl(hscrollbar);

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
