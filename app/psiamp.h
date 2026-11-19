// PSIAMP.H - PsiAmp: an MP3 and WAV player for Coda (the Psion netBook),
// in the look of the classic skinned players of 1999.
//
// The sound: EPOC R5 on the netBook plays only A-law at 8000 samples a
// second, mono (ESound.ldd through RDevSound; Record does the same), so
// every file is decoded (Helix's fixed-point MP3 decoder, helix/), mixed
// down to mono, resampled to 8000 a second and turned into A-law
// (engine/pacore.c) before it goes to the sound device.
//
// The look: a classic Winamp 2 skin (.wsz, a zip of BMPs), drawn from its
// sprites; PsiAmp's own skin, Coda (pkg/mkskin.py), is one, so a user's own
// skins (in \System\Apps\PsiAmp\Skins\ or \Skins\ on any disk) draw the same
// way. The player's two windows float over the program in use, as PsiHub's
// panel does; PsiAmp plays on when they are put away (Esc).
#ifndef __PSIAMP_H__
#define __PSIAMP_H__

#include <e32base.h>
#include <f32file.h>
#include <d32snd.h>
#include <coecntrl.h>
#include <eikappui.h>
#include <eikapp.h>
#include <eikdoc.h>
#include <badesca.h>
#include <t32alm.h>
#include "psiamp.hrh"
#include "mp3dec.h"
#include "pacore.h"

const TUid KUidPsiAmp = { 0x01000AFA };
_LIT(KPaVersion, "0.07");
_LIT(KPaDir, "C:\\System\\Apps\\PsiAmp\\");
_LIT(KPaIni, "C:\\System\\Apps\\PsiAmp\\PsiAmp.ini");
_LIT(KPaListFile, "C:\\System\\Apps\\PsiAmp\\PsiAmp.m3u");
_LIT(KPaLog, "C:\\System\\Apps\\PsiAmp\\PsiAmp.log");
_LIT(KPaSkinName, "Coda.wsz");                  // beside PsiAmp.app
_LIT(KPaEpocName, "Epoc.wsz");                  // EPOC: in key colours, given the theme's (paskin.cpp)
_LIT(KPaEpocSetting, "*EPOC");                  // TPaSettings::iSkin for it
_LIT(KPaSample, "Coda.mp3");                    // the sample (pkg/mksample.py), beside the program: the first playlist

// ===========================================================================
// The sound
// ===========================================================================

class MPaEngineObserver
	{
public:
	virtual void EngineChanged() = 0;            // playing, paused, stopped, a new track's details
	virtual void EngineTrackEnded() = 0;         // played to the end
	virtual void EngineMessage(const TDesC& aText) = 0;   // an infoprint
	virtual void EngineLog(const TDesC& aText) = 0;       // a line in PsiAmp.log
	};

class CPaDecoder;

class CPaEngine : public CActive
	{
public:
	enum TState { EStopped, EPlaying, EPaused };
	static CPaEngine* NewL(RFs& aFs, MPaEngineObserver& aObserver);
	~CPaEngine();
	// a file: its details read and decoding ready at aStartMs
	TInt Open(const TDesC& aName, TInt aStartMs = 0);
	void Close();
	TBool IsOpen() const { return iOpen; }
	void Play();
	void Pause();
	void Stop();                                 // back to the start
	void SeekMs(TInt aMs);
	void SetVolume(TInt aPercent);
	TState State() const { return iState; }
	TInt PositionMs() const;
	TInt DurationMs() const { return iDurationMs; }
	TInt Kbps() const { return iKbps; }
	TInt Khz() const { return iRate / 1000; }
	TInt Channels() const { return iChans; }
	const TDesC& Title() const { return iTitle; }
	TBool Silent() const { return iSilent; }
	// the visualiser at what is being heard: 19 bars 0..16, 76 points 0..15
	void Vis(TUint8* aBars, TUint8* aScope) const;
	// decodes the open file from its start as fast as it can (aMaxMs of
	// sound at most): the sound's length and the time taken, in ms
	TInt SpeedTest(TInt aMaxMs, TInt& aSoundMs, TInt& aTakenMs);
	TBool Ended() const { return iEnded; }
private:
	CPaEngine(RFs& aFs, MPaEngineObserver& aObserver);
	void ConstructL();
	// CActive: a play request done
	void RunL();
	void DoCancel();
	TInt OpenDevice();
	void CloseDevice();
	void Feed();                                 // a chunk to the device, if there is one
	static TInt DecodeStep(TAny* aSelf);
	TInt DoDecodeStep();
	static TInt Tick(TAny* aSelf);
	void DoTick();
	void Restart(TInt aSample);                  // the ring and the device from aSample
	TInt Played() const;                         // samples heard, from the track's start
	void GoSilent();
	void SafeCancel();
public:
	// the ring: A-law and the 16-bit sound for the visualiser, indexed by
	// the sample's number from the track's start, modulo the size
	enum { KRing = 32768, KAhead = 16000, KChunk = 2000, KDevBuffer = 8000 };
private:
	RFs& iFs;
	MPaEngineObserver& iObserver;
	CPaDecoder* iDec;
	TBool iOpen;
	TState iState;
	TBuf<128> iTitle;
	TInt iDurationMs, iKbps, iRate, iChans;
	TUint8* iRingA;
	TInt16* iRingS;
	TInt iWritten;                               // samples decoded (from the track's start)
	TInt iSubmitted;                             // given to the device
	TBool iEnded;                                // the file is decoded to its end
	// what is heard: iClockSample at iClockTime, on at 8000 a second while playing
	TInt iClockSample;
	TTime iClockTime;
	TBool iClockRunning;
	TInt iBusyFrom;                              // iSubmitted when the request went out
	// the device
	RDevSound iDev;
	TBool iDevOpen;
	TBool iSilent;                               // no sound device: timed by the clock alone
	TBool iSilentSaid;
	TBool iDevHeld;                              // (0.07) open but not in use after a stall: tried again at the next play
	TInt iRequestTicks;                          // (0.07) the timer's ticks since the request went out
	TInt iVolume;                                // 0..100
	TInt iMaxVolume;
	TBuf8<KChunk> iChunk;
	TInt iRequests;                              // play requests made (the first few are logged)
	TTime iRequestTime;
	CPeriodic* iTimer;                           // the decoder's pump, the end, the watchdog
	CIdle* iIdle;
	};

// The decoding: an MP3 (Helix) or a WAV file to mono 16-bit at 8000 a second
class CPaDecoder : public CBase
	{
public:
	static CPaDecoder* NewL(RFs& aFs);
	~CPaDecoder();
	TInt Open(const TDesC& aName);
	TInt Seek(TInt aMs);                         // -> the ms it went to
	// some sound: up to aMax samples into aOut -> how many; 0 at the end, < 0 failed
	TInt Decode(TInt16* aOut, TInt aMax);
	TInt DurationMs() const { return iDurationMs; }
	TInt Kbps() const { return iKbps; }
	TInt Rate() const { return iRate; }
	TInt Channels() const { return iChans; }
	const TDesC& Title() const { return iTitle; }
private:
	friend class CPaEngine;
	CPaDecoder(RFs& aFs) : iFs(aFs) {}
	void Reset();
	TInt Fill();                                 // the input buffer topped up
	TInt OpenMp3();
	TInt OpenWav();
	TInt FirstFrame();
	TInt DecodeMp3Frame();                       // -> samples into iOut, 0 at the end
	TInt DecodeWav();
private:
	RFs& iFs;
	RFile iFile;
	TFileName iName;
	TBool iFileOpen;
	TBool iWav;
	PaWavInfo iWavInfo;
	HMP3Decoder iMp3;
	TInt iFileSize, iDataStart, iDataEnd;
	TInt iReadPos;                               // the file's next byte to read
	enum { KIn = 8192 };
	TUint8* iIn;
	TInt iInLen, iInPos;
	TInt16* iPcm;                                // a frame as decoded (1152 x 2)
	TInt16* iOut;                                // at 8000 a second
	TInt iOutLen, iOutPos;
	PaResampler iRs;
	TBool iRsOpen;
	TInt iDurationMs, iKbps, iRate, iChans;
	TInt iFrames;                                // Xing: frames in the file (0: none)
	TUint8 iToc[100];
	TBool iHaveToc;
	TBuf<128> iTitle;
	TBool iEof;
	};

// ===========================================================================
// The skin
// ===========================================================================

// a sprite sheet: one of the skin's BMPs, in EPOC's 256 colours
struct TPaSheet
	{
	TInt iW, iH;
	TUint8* iPix;
	};

class CPaSkin : public CBase
	{
public:
	enum TSheet { EMain, ETitle, ECButtons, ENumbers, ENumsEx, EText, EPlayPaus, EMonoSter,
		EPosBar, EVolume, EBalance, EShufRep, EPlEdit, ESheets };
	static CPaSkin* NewL();
	~CPaSkin();
	// a .wsz file; what it lacks comes from the standard skin (aStandard)
	TInt Load(RFs& aFs, const TDesC& aWsz, const CPaSkin* aStandard, TBool aThemed = EFalse);
	const TPaSheet& Sheet(TInt aSheet) const { return iSheet[aSheet]; }
	TUint8 Vis(TInt aIndex) const { return iVis[aIndex]; }
	TRgb ListRgb(TInt aIndex) const { return iListRgb[aIndex]; }   // normal, current, background, selected, selected text
	TBool HasSelText() const { return iHasSelText; }
	TUint8 Index(TInt aR, TInt aG, TInt aB) const { return iLut[((aR >> 4) << 8) | ((aG >> 4) << 4) | (aB >> 4)]; }
	const TDesC& Name() const { return iName; }
private:
	void Free();
	TInt LoadSheet(const TUint8* aZip, TInt aLen, const char* aName, TInt aSheet, TInt aW, TInt aH);
	void CopyFrom(const CPaSkin& aOther, TInt aSheet, TInt aTo);
private:
	TPaSheet iSheet[ESheets];
	TUint8 iVis[24];
	TRgb iListRgb[5];
	TBool iHasSelText;                           // the selection's own text colour (EPOC: EIKON's)
	TBool iThemed;
	TRgb iKeyTo[12];                             // EPOC: what each key colour becomes
	TUint8 iLut[4096];                           // RGB 4:4:4 -> EPOC's 256 colours
	TBuf<64> iName;
	};

// the face: the skin's sprites put together, a byte (colour) a pixel, and
// shown at 1x or 2x through a 256-colour bitmap
class CPaFace : public CBase
	{
public:
	~CPaFace();
	void SetSizeL(TInt aW, TInt aH, TInt aScale);
	void Free();                                 // (0.07) the pixels and the bitmap let go (SetSizeL makes them again)
	void Put(const TPaSheet& aSheet, TInt aSx, TInt aSy, TInt aW, TInt aH, TInt aDx, TInt aDy);
	void Fill(TInt aX, TInt aY, TInt aW, TInt aH, TUint8 aColour);
	void Text(const TPaSheet& aFont, const TDesC& aText, TInt aX, TInt aY, TInt aW, TInt aOffset = 0);
	// the area (in face pixels) copied to the bitmap, scaled
	void Update(const TRect& aRect);
	CFbsBitmap* Bitmap() const { return iBmp; }
	TInt Scale() const { return iScale; }
	TSize Size() const { return TSize(iW, iH); }
private:
	TUint8* iPix;
	TInt iW, iH, iScale;
	CFbsBitmap* iBmp;
	};

// ===========================================================================
// The windows
// ===========================================================================

class CPaAppUi;

class CPaMainWin : public CCoeControl
	{
public:
	CPaMainWin(CPaAppUi& aUi) : iUi(aUi), iPressed(-1) {}
	~CPaMainWin();
	void ConstructL();
	void LayoutL();                              // the size from the scale; the face again
	void Compose(TBool aAll);                   // the changing parts (or all) into the face and on screen
	void LetGo() { iFace.Free(); }               // (0.07) out of sight: LayoutL again before it is seen
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
	void SetActive(TBool aActive) { iActive = aActive; }
private:
	void Draw(const TRect& aRect) const;
	void HandlePointerEventL(const TPointerEvent& aPointerEvent);
	TInt HitButton(const TPoint& aFace) const;
	void Changed(const TRect& aFaceRect);
	void ComposeTitle();
	TInt SliderValue(TInt aX) const;
private:
	CPaAppUi& iUi;
	CPaFace iFace;
	TInt iPressed;                               // the button held down (-1: none)
	TBool iDragPos, iDragVol, iDragWin;
	TInt iDragPosMs;
	TPoint iDragFrom;
	TInt iScroll;                                // the title's scroll, in pixels
	TBuf<160> iMarquee;
	TBool iActive;
	TUint8 iPeak[19];
	};

class CPaListWin : public CCoeControl
	{
public:
	CPaListWin(CPaAppUi& aUi) : iUi(aUi) {}
	~CPaListWin();
	void ConstructL();
	void LayoutL();
	void Compose();
	void LetGo() { iFace.Free(); }
	void ShowCurrent();                          // the selection in view
	TInt Selected() const { return iSel; }
	void SetSelected(TInt aIndex);
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
	void SetActive(TBool aActive) { iActive = aActive; }
private:
	void Draw(const TRect& aRect) const;
	void HandlePointerEventL(const TPointerEvent& aPointerEvent);
	TRect TextRect() const;
	TInt Rows() const;
	TInt RowHeight() const;
private:
	CPaAppUi& iUi;
	CPaFace iFace;
	const CFont* iFont;
	TInt iSel, iTop;
	TBool iDragWin, iDragBar;
	TPoint iDragFrom;
	TBool iActive;
	};

// ===========================================================================
// The program
// ===========================================================================

struct TPaSettings
	{
	TInt32 iVersion;              // 1
	TInt32 iVolume;               // 0..100
	TInt32 iShuffle, iRepeat;
	TInt32 iDouble;               // 2x
	TInt32 iPlaylist;             // shown
	TInt32 iVisMode;              // 0 spectrum, 1 oscilloscope, 2 off
	TInt32 iRemaining;            // the time shown: 0 elapsed, 1 remaining
	TInt32 iKeys;                 // the keys in other programs
	TInt32 iOnTop;
	TInt32 iTrack;                // the playlist's current entry
	TInt32 iPosMs;                // where it was
	TInt32 iMainX, iMainY, iListX, iListY;   // the windows' places (-1: standard)
	TInt32 iSpare[8];
	TBuf<128> iSkin;              // the .wsz in use ("": Coda)
	};

class CPaAppUi : public CEikAppUi, public MPaEngineObserver
	{
public:
	~CPaAppUi();
	void ConstructL();
	// for the windows
	CPaEngine& Engine() { return *iEngine; }
	const CPaSkin& Skin() const { return *iSkin; }
	TPaSettings& Settings() { return iSet; }
	TInt Scale() const { return iSet.iDouble ? 2 : 1; }
	TInt Count() const { return iList->Count(); }
	TPtrC Entry(TInt aIndex) const { return (*iList)[aIndex]; }
	void EntryName(TInt aIndex, TDes& aName) const;
	TInt Current() const { return iCurrent; }
	void PlayIndexL(TInt aIndex, TInt aStartMs = 0);
	void SetVolume(TInt aPercent);
	void HandleCommandL(TInt aCommand);
	void MoveWindows(CCoeControl* aWin, const TPoint& aBy);
	void ShowMenuL();
	void PopupL(TInt aResource, const TPoint& aScreenPos);
	void HideL();
	void ShowL();
	void OpenFileL(const TDesC& aFileName);     // from the System screen
	void ToggleVisL();
	void PlayPauseL();                           // Space, Ctrl+Fn+X
	TBool TooSoon();
	void NextSkinL();                            // K: Coda, EPOC, then the skins found
	TKeyResponse HandleListKeyL(const TKeyEvent& aKeyEvent);
	TBool InFront() const { return iInFront; }
	// MPaEngineObserver
	void EngineChanged();
	void EngineTrackEnded();
	void EngineMessage(const TDesC& aText);
	void EngineLog(const TDesC& aText) { Log(aText); }
	void Log(const TDesC& aText);
private:
	void DynInitMenuPaneL(TInt aMenuId, CEikMenuPane* aMenuPane);
	void HandleWsEventL(const TWsEvent& aEvent, CCoeControl* aDestination);
	void HandleForegroundEventL(TBool aForeground);
	TBool ProcessCommandParametersL(TApaCommand aCommand, TFileName& aDocumentName, const TDesC8& aTail);
	void LoadSettings();
	void SaveSettings();
	void LoadListL(const TDesC& aM3u, TBool aAppend);
	void ListLineL(TPtrC8 aLine, const TDesC& aM3u);
	void SaveListL(const TDesC& aM3u);
	void WriteListL(RFile& aFile);
	void AddFileL(const TDesC& aName);
	void AddFolderL(const TDesC& aFolder);
	TBool StartsWell(const TDesC& aName);
	TBool AskFileL(TFileName& aName, TInt aTitle);
	void NextL(TBool aUser);
	void PrevL();
	void GoToL();
	void PrefsL();
	void SpeedTestL();
	void AboutL();
	void SetSkinL(const TDesC& aWsz, TBool aRedraw);
	CPaSkin* LoadCodaL();
	void FindSkinsL();
	void CaptureKeys();
	void ReleaseKeys();
	void LayoutWindowsL();
	void SetOnTop(TBool aOn);
	static TInt Tick(TAny* aSelf);
	void DoTick();
	void StartTicks();
	static TInt Keep(TAny* aSelf);
	void DoKeep();
	void LetGo();
	TBool TakeBackL();
	static TBool IsAudio(const TDesC& aName);
	static TBool IsList(const TDesC& aName);
private:
	CPaEngine* iEngine;
	CPaSkin* iSkin;                              // the one in use (only one is kept)
	CDesCArrayFlat* iList;                       // the playlist's files
	CDesCArrayFlat* iTitles;                     // their titles, once played ("": not known)
	CDesCArrayFlat* iSkins;                      // skins found
	TInt iCurrent;
	CPaMainWin* iMain;
	CPaListWin* iListWin;
	TPaSettings iSet;
	CPeriodic* iTicker;
	TBool iInFront;
	TBool iPinned;                               // on top while another program has the keys
	TInt32 iCap[8];
	TInt iCapCount;
	TInt64 iSeed;
	TBool iReady;                                // (0.06) ConstructL finished: only then is there anything to save
	TBool iHidden;                               // (0.07) put away by the user: not kept on top until shown
	TBool iListOk;                               // (0.07) the playlist was read whole (or made here): saved at the end
	TBool iAway;                                 // (0.07) out of sight: the skin and the pictures let go
	CPeriodic* iKeeper;                          // (0.07) while playing: the machine kept on, alarms watched
	RAlarmServer iAlarms;
	TBool iAlarmsOpen;
	TInt iAlarmPaused;                           // the alarm last paused for
	TTime iAlarmPausedAt;
	TInt iTickCount;
	TTime iLastToggle;
	};

class CPaDocument : public CEikDocument
	{
public:
	CPaDocument(CEikApplication& aApp) : CEikDocument(aApp) {}
	CFileStore* OpenFileL(TBool aDoOpen, const TDesC& aFilename, RFs& aFs);
	TFileName iStartFile;
private:
	CEikAppUi* CreateAppUiL() { return new(ELeave) CPaAppUi; }
	};

class CPaApplication : public CEikApplication
	{
private:
	CApaDocument* CreateDocumentL() { return new(ELeave) CPaDocument(*this); }
	TUid AppDllUid() const { return KUidPsiAmp; }
	};

void PaFormatTime(TDes& aOut, TInt aMs);
// the first aMax characters, or all of a shorter text (ER5's Left panics,
// USER 22, past the end)
inline TPtrC PaLeft(const TDesC& aText, TInt aMax) { return aText.Left(aText.Length() < aMax ? aText.Length() : aMax); }

#endif
