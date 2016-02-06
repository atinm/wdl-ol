#ifndef __SAMPLER__
#define __SAMPLER__

#include "IPlug_include_in_plug_hdr.h"
#include "IMidiQueue.h"
#include "IControl.h"
#include "SamplerDSP.h"

#define MAX_VOICES 16
#define ATTACK_DEFAULT 5.
#define DECAY_DEFAULT 20.
#define RELEASE_DEFAULT 500.
#define TIME_MIN 2.
#define TIME_MAX 5000.

class Sampler : public IPlug
{
public:

  Sampler(IPlugInstanceInfo instanceInfo);
  ~Sampler();

  void Reset();
  void OnParamChange(int paramIdx);

  void ProcessDoubleReplacing(double** inputs, double** outputs, int nFrames);
  bool HostRequestingAboutBox();

  int GetNumKeys();
  bool GetKeyStatus(int key);
  void ProcessMidiMsg(IMidiMsg* pMsg);
  void NoteOnOff(IMidiMsg* pMsg);

private:

  void NoteOnOffPoly(IMidiMsg* pMsg);
  int FindFreeVoice();
  void setSelection(int selection);

  enum ESelections {
	  kBrowser = 1,
	  kSampler = 2,
	  kOptions = 3
  };
  ESelections mSelection;

  IBitmapOverlayControl* mAboutBox;
  IControl* mKeyboard;

  IMidiQueue mMidiQueue;

  int mActiveVoices;
  int mKey;
  int mNumHeldKeys;
  bool mKeyStatus[128]; // array of on/off for each key

  double mSampleRate;

  CVoiceState mVS[MAX_VOICES];
  CWTOsc* mOsc;
  CADSREnvL* mEnv;
  double* mTable;

  IScrollbar* vscrollbar;
  IScrollbar* hscrollbar;
};

enum ELayout
{
  kWidth = GUI_WIDTH,  // width of plugin window
  kHeight = GUI_HEIGHT, // height of plugin window

  kKeybX = 263,
  kKeybY = 529,

  kGainX = 100,
  kGainY = 100,
  kKnobFrames = 60,
  kTopButtonsControls = 2,
  kTopButtonsControl_N = 3,
  kTopButtonsControl_X = 645,
  kTopButtonsControl_Y = 8,
  kTopButtonsControl_W = 98,
  kTopButtonsControl_H = 24
	
};

#endif //__SAMPLER__
