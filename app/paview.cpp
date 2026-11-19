// PAVIEW.CPP - PsiAmp's two windows, drawn from the skin's sprites at the
// places the classic skins were made for (the main window 275 x 116, the
// playlist from pledit.bmp's pieces), at 1x or 2x ("Double size").
#include <e32std.h>
#include <w32std.h>
#include <coemain.h>
#include <eikenv.h>
#include <eikappui.h>
#include <psiamp.rsg>
#include "psiamp.h"

// ---- the main window's places (1x) ------------------------------------------
enum TPaHit
	{
	EHitPrev, EHitPlay, EHitPause, EHitStop, EHitNext, EHitEject,
	EHitShuffle, EHitRepeat, EHitEq, EHitPl,
	EHitOptions, EHitMinimise, EHitShade, EHitClose, EHitAbout,
	EHitTime, EHitVis, EHitPos, EHitVolume, EHitBalance, EHitTitle, EHitCount
	};

struct TPaPlace { TInt16 iX, iY, iW, iH; };

static const TPaPlace KPaPlaces[EHitCount] =
	{
	{ 16, 88, 23, 18 }, { 39, 88, 23, 18 }, { 62, 88, 23, 18 }, { 85, 88, 23, 18 }, { 108, 88, 22, 18 },
	{ 136, 89, 22, 16 },
	{ 164, 89, 47, 15 }, { 210, 89, 28, 15 }, { 219, 58, 23, 12 }, { 242, 58, 23, 12 },
	{ 6, 3, 9, 9 }, { 244, 3, 9, 9 }, { 254, 3, 9, 9 }, { 264, 3, 9, 9 }, { 253, 91, 13, 15 },
	{ 36, 26, 63, 13 }, { 24, 43, 76, 16 }, { 16, 72, 248, 10 }, { 107, 57, 68, 13 }, { 177, 57, 38, 13 },
	{ 0, 0, 275, 14 }
	};

static TRect PlaceRect(TInt aHit)
	{
	const TPaPlace& p = KPaPlaces[aHit];
	return TRect(TPoint(p.iX, p.iY), TSize(p.iW, p.iH));
	}

void PaFormatTime(TDes& aOut, TInt aMs)
	{
	TInt s = aMs / 1000;
	if (s >= 3600)
		aOut.Format(_L("%d:%02d:%02d"), s / 3600, (s / 60) % 60, s % 60);
	else
		aOut.Format(_L("%d:%02d"), s / 60, s % 60);
	}

// ===========================================================================
// The main window
// ===========================================================================

CPaMainWin::~CPaMainWin()
	{
	}

void CPaMainWin::ConstructL()
	{
	CreateWindowL();
	EnableDragEvents();
	LayoutL();
	ActivateL();
	}

void CPaMainWin::LayoutL()
	{
	TInt s = iUi.Scale();
	iFace.SetSizeL(275, 116, s);
	SetSizeL(TSize(275 * s, 116 * s));
	Compose(ETrue);
	}

// the face area changed: to the bitmap and on screen
void CPaMainWin::Changed(const TRect& aFaceRect)
	{
	iFace.Update(aFaceRect);
	TInt s = iFace.Scale();
	TRect r(aFaceRect.iTl.iX * s, aFaceRect.iTl.iY * s, aFaceRect.iBr.iX * s, aFaceRect.iBr.iY * s);
	if (IsReadyToDraw())
		{
		Window().Invalidate(r);
		}
	}

void CPaMainWin::ComposeTitle()
	{
	CPaEngine& e = iUi.Engine();
	TBuf<160> t;
	if (iUi.Count() == 0)
		t = _L("PsiAmp - no files: Ctrl+O opens one");
	else
		{
		TBuf<128> name;
		if (e.IsOpen())
			name = e.Title();
		else
			iUi.EntryName(iUi.Current(), name);
		TBuf<16> len;
		len.Zero();
		if (e.IsOpen() && e.DurationMs() > 0)
			{
			TBuf<12> tt;
			PaFormatTime(tt, e.DurationMs());
			len.Format(_L(" (%S)"), &tt);
			}
		t.Format(_L("%d. %S%S"), iUi.Current() + 1, &name, &len);
		}
	if (t != iMarquee)
		{
		iMarquee = t;
		iScroll = 0;
		}
	}

TInt CPaMainWin::SliderValue(TInt aX) const
	{
	// the volume: the thumb's left edge from 107 to 161
	TInt v = ((aX - 107 - 7) * 100) / (68 - 14);
	return v < 0 ? 0 : v > 100 ? 100 : v;
	}

void CPaMainWin::Compose(TBool aAll)
	{
	const CPaSkin& sk = iUi.Skin();
	CPaEngine& e = iUi.Engine();
	TPaSettings& set = iUi.Settings();
	const TPaSheet& main = sk.Sheet(CPaSkin::EMain);
	CPaEngine::TState st = e.State();
	TBool playing = st == CPaEngine::EPlaying;

	if (aAll)
		{
		iFace.Put(main, 0, 0, 275, 116, 0, 0);
		iFace.Put(sk.Sheet(CPaSkin::ETitle), 27, iActive ? 0 : 15, 275, 14, 0, 0);
		// the clutter bar, down the display's left edge
		iFace.Put(sk.Sheet(CPaSkin::ETitle), 304, 0, 8, 43, 10, 22);
		// the title bar's buttons
		const TPaSheet& tb = sk.Sheet(CPaSkin::ETitle);
		iFace.Put(tb, 0, iPressed == EHitOptions ? 9 : 0, 9, 9, 6, 3);
		iFace.Put(tb, 9, iPressed == EHitMinimise ? 9 : 0, 9, 9, 244, 3);
		iFace.Put(tb, iPressed == EHitShade ? 9 : 0, 18, 9, 9, 254, 3);
		iFace.Put(tb, 18, iPressed == EHitClose ? 9 : 0, 9, 9, 264, 3);
		// the transport
		const TPaSheet& cb = sk.Sheet(CPaSkin::ECButtons);
		static const TInt16 cx[6] = { 0, 23, 46, 69, 92, 114 };
		for (TInt b = EHitPrev; b <= EHitEject; b++)
			{
			const TPaPlace& p = KPaPlaces[b];
			TInt sy = iPressed == b ? (b == EHitEject ? 16 : 18) : 0;
			iFace.Put(cb, cx[b], sy, p.iW, p.iH, p.iX, p.iY);
			}
		// shuffle, repeat, EQ, PL
		const TPaSheet& sr = sk.Sheet(CPaSkin::EShufRep);
		iFace.Put(sr, 28, (set.iShuffle ? 30 : 0) + (iPressed == EHitShuffle ? 15 : 0), 47, 15, 164, 89);
		iFace.Put(sr, 0, (set.iRepeat ? 30 : 0) + (iPressed == EHitRepeat ? 15 : 0), 28, 15, 210, 89);
		iFace.Put(sr, iPressed == EHitEq ? 46 : 0, 61, 23, 12, 219, 58);
		iFace.Put(sr, iPressed == EHitPl ? 69 : 23, set.iPlaylist ? 73 : 61, 23, 12, 242, 58);
		// the balance: its middle (the netBook has one speaker)
		const TPaSheet& bal = sk.Sheet(CPaSkin::EBalance);
		iFace.Put(bal, 9, 0, 38, 13, 177, 57);
		iFace.Put(bal, 15, 422, 14, 11, 177 + 12, 58);
		// mono or stereo, the bitrate and rate
		const TPaSheet& ms = sk.Sheet(CPaSkin::EMonoSter);
		TBool open = e.IsOpen();
		iFace.Put(ms, 29, open && e.Channels() == 1 ? 0 : 12, 27, 12, 212, 41);
		iFace.Put(ms, 0, open && e.Channels() == 2 ? 0 : 12, 29, 12, 239, 41);
		const TPaSheet& font = sk.Sheet(CPaSkin::EText);
		TBuf<8> num;
		if (open && e.Kbps() > 0)
			num.Format(_L("%3d"), e.Kbps() > 999 ? 999 : e.Kbps());
		else
			num = _L("   ");
		iFace.Text(font, num, 111, 43, 15);
		if (open && e.Khz() > 0)
			num.Format(_L("%2d"), e.Khz());
		else
			num = _L("  ");
		iFace.Text(font, num, 156, 43, 10);
		ComposeTitle();
		}

	// the play state
	const TPaSheet& pp = sk.Sheet(CPaSkin::EPlayPaus);
	iFace.Put(pp, playing ? 0 : st == CPaEngine::EPaused ? 9 : 18, 0, 9, 9, 26, 28);
	iFace.Put(pp, playing && !e.Silent() ? 36 : 39, 0, 3, 9, 24, 28);

	// the time
	const TPaSheet& nums = sk.Sheet(set.iRemaining && sk.Sheet(CPaSkin::ENumsEx).iPix ? CPaSkin::ENumsEx : CPaSkin::ENumbers);
	TInt pos = e.PositionMs();
	TBool showTime = e.IsOpen() && st != CPaEngine::EStopped;
	TInt t = pos;
	if (set.iRemaining && e.DurationMs() > 0)
		t = e.DurationMs() - pos;
	if (t < 0)
		t = 0;
	TInt secs = t / 1000;
	TInt mins = secs / 60;
	if (mins > 99)
		mins = 99;
	TInt digit[4] = { mins / 10, mins % 10, (secs % 60) / 10, secs % 10 };
	static const TInt16 dx[4] = { 48, 60, 78, 90 };
	// in pause the time blinks, as it always has
	TBool blinkOff = st == CPaEngine::EPaused && (User::TickCount() / 32) % 2;
	for (TInt d = 0; d < 4; d++)
		iFace.Put(nums, showTime && !blinkOff ? digit[d] * 9 : 90, 0, 9, 13, dx[d], 26);
	if (set.iRemaining && showTime && !blinkOff && nums.iW >= 108)
		iFace.Put(nums, 99, 0, 9, 13, 36, 26);
	else
		iFace.Put(main, 36, 26, 9, 13, 36, 26);

	// the visualiser
	TUint8 bars[19], scope[76];
	TRect vis(24, 43, 100, 59);
	iFace.Fill(24, 43, 76, 16, sk.Vis(0));
	for (TInt gy = 1; gy < 16; gy += 2)
		for (TInt gx = 1; gx < 76; gx += 2)
			iFace.Fill(24 + gx, 43 + gy, 1, 1, sk.Vis(1));
	if (set.iVisMode != 2 && playing)
		{
		if (set.iVisMode == 0)
			{
			e.Vis(bars, NULL);
			for (TInt b = 0; b < 19; b++)
				{
				TInt h = bars[b];
				for (TInt r = 16 - h; r < 16; r++)
					iFace.Fill(24 + b * 4, 43 + r, 3, 1, sk.Vis(2 + r));
				// the peaks fall slowly
				if (h >= iPeak[b])
					iPeak[b] = (TUint8)h;
				else if (iPeak[b] > 0)
					iPeak[b]--;
				if (iPeak[b] > 0)
					iFace.Fill(24 + b * 4, 43 + 16 - iPeak[b], 3, 1, sk.Vis(23));
				}
			}
		else
			{
			e.Vis(NULL, scope);
			for (TInt x = 0; x < 76; x++)
				{
				TInt y = scope[x];
				TInt y0 = x ? scope[x - 1] : y;
				TInt a = Min(y, y0), b = Max(y, y0);
				TInt c = y - 8;
				if (c < 0) c = -c;
				iFace.Fill(24 + x, 43 + a, 1, b - a + 1, sk.Vis(18 + (c > 4 ? 4 : c)));
				}
			}
		}
	else
		Mem::FillZ(iPeak, sizeof(iPeak));

	// the song's title, scrolling when it does not fit
	const TPaSheet& font = sk.Sheet(CPaSkin::EText);
	if (!aAll)
		ComposeTitle();
	TBuf<200> show;
	TInt textW = iMarquee.Length() * 5;
	iFace.Put(main, 111, 27, 154, 6, 111, 27);
	if (textW <= 154)
		iFace.Text(font, iMarquee, 111, 27, 154);
	else
		{
		show = iMarquee;
		show.Append(_L("  ***  "));
		TInt loop = show.Length() * 5;
		if (iScroll >= loop)
			iScroll -= loop;
		iFace.Text(font, show, 111, 27, 154, iScroll);
		iFace.Text(font, show, 111, 27, 154, iScroll - loop);
		if (playing)
			iScroll += 1;
		}

	// the position bar
	const TPaSheet& pb = sk.Sheet(CPaSkin::EPosBar);
	iFace.Put(pb, 0, 0, 248, 10, 16, 72);
	TInt dur = e.DurationMs();
	if (e.IsOpen() && dur > 0)
		{
		TInt p = iDragPos ? iDragPosMs : pos;
		if (p > dur) p = dur;
		TInt x = 16 + (TInt)((p / 100) * (248 - 29) / (dur / 100 + 1));
		iFace.Put(pb, iDragPos ? 278 : 248, 0, 29, 10, x, 72);
		}

	// the volume
	const TPaSheet& vol = sk.Sheet(CPaSkin::EVolume);
	TInt v = set.iVolume;
	TInt frame = (v * 27) / 100;
	iFace.Put(vol, 0, frame * 15, 68, 13, 107, 57);
	iFace.Put(vol, iDragVol ? 0 : 15, 422, 14, 11, 107 + (v * (68 - 14)) / 100, 58);

	if (aAll)
		{
		Changed(TRect(0, 0, 275, 116));
		}
	else
		{
		Changed(TRect(24, 26, 100, 59));        // the play state, the time, the visualiser
		Changed(TRect(111, 27, 265, 33));       // the title
		Changed(TRect(16, 72, 264, 82));        // the position
		Changed(TRect(107, 57, 175, 70));       // the volume
		}
	(void)vis;
	}

void CPaMainWin::Draw(const TRect& aRect) const
	{
	CWindowGc& gc = SystemGc();
	if (!iUi.Skin().Sheet(CPaSkin::EMain).iPix)
		{
		// (0.07) no skin could be read (Coda.wsz not beside the program):
		// said, in place of a black window
		const CFont* f = iEikonEnv->NormalFont();
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(KRgbBlack);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(TRgb(170, 170, 170));
		gc.UseFont(f);
		TRect r(Rect());
		gc.DrawRect(r);
		TInt h = f->HeightInPixels() + 4;
		TRect line(r.iTl.iX + 8, r.iTl.iY + r.Height() / 2 - h, r.iBr.iX - 8, r.iTl.iY + r.Height() / 2);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.DrawText(_L("PsiAmp's skin is missing"), line, f->AscentInPixels() + 2, CGraphicsContext::ECenter);
		line.Move(0, h);
		gc.DrawText(_L("Install PsiAmp again - Menu for its commands"), line, f->AscentInPixels() + 2, CGraphicsContext::ECenter);
		gc.DiscardFont();
		return;
		}
	if (iFace.Bitmap())
		gc.BitBlt(aRect.iTl, iFace.Bitmap(), aRect);
	}

TInt CPaMainWin::HitButton(const TPoint& aFace) const
	{
	for (TInt i = 0; i < EHitCount; i++)
		if (PlaceRect(i).Contains(aFace))
			return i;
	return -1;
	}

void CPaMainWin::HandlePointerEventL(const TPointerEvent& aPointerEvent)
	{
	TInt s = iFace.Scale();
	TPoint f(aPointerEvent.iPosition.iX / s, aPointerEvent.iPosition.iY / s);
	CPaEngine& e = iUi.Engine();
	if (!iUi.InFront())
		{
		// (kept on top over another program: a tap brings PsiAmp forward first)
		iUi.ShowL();
		}
	switch (aPointerEvent.iType)
		{
	case TPointerEvent::EButton1Down:
		{
		TInt hit = HitButton(f);
		if (hit == EHitPos && e.IsOpen() && e.DurationMs() > 0)
			{
			iDragPos = ETrue;
			iDragPosMs = (TInt)(((f.iX - 16 - 14) * (e.DurationMs() / 100)) / (248 - 29)) * 100;
			if (iDragPosMs < 0) iDragPosMs = 0;
			Compose(EFalse);
			}
		else if (hit == EHitVolume)
			{
			iDragVol = ETrue;
			iUi.SetVolume(SliderValue(f.iX));
			Compose(EFalse);
			}
		else if (hit == EHitTitle)
			{
			iDragWin = ETrue;
			iDragFrom = aPointerEvent.iParentPosition;
			}
		else if (hit == EHitTime)
			{
			iUi.Settings().iRemaining = !iUi.Settings().iRemaining;
			Compose(EFalse);
			}
		else if (hit == EHitVis)
			iUi.ToggleVisL();
		else if (hit >= 0 && hit <= EHitAbout)
			{
			iPressed = hit;
			Compose(ETrue);
			}
		break;
		}
	case TPointerEvent::EDrag:
		if (iDragPos)
			{
			iDragPosMs = (TInt)(((f.iX - 16 - 14) * (e.DurationMs() / 100)) / (248 - 29)) * 100;
			if (iDragPosMs < 0) iDragPosMs = 0;
			if (iDragPosMs > e.DurationMs()) iDragPosMs = e.DurationMs();
			Compose(EFalse);
			}
		else if (iDragVol)
			{
			iUi.SetVolume(SliderValue(f.iX));
			Compose(EFalse);
			}
		else if (iDragWin)
			{
			TPoint by = aPointerEvent.iParentPosition - iDragFrom;
			iDragFrom = aPointerEvent.iParentPosition;
			iUi.MoveWindows(this, by);
			}
		break;
	case TPointerEvent::EButton1Up:
		{
		if (iDragPos)
			{
			iDragPos = EFalse;
			e.SeekMs(iDragPosMs);
			Compose(EFalse);
			}
		iDragVol = iDragWin = EFalse;
		TInt was = iPressed;
		iPressed = -1;
		if (was >= 0)
			{
			Compose(ETrue);
			if (HitButton(f) != was)
				break;
			switch (was)
				{
			case EHitPrev: iUi.HandleCommandL(EPaCmdPrev); break;
			case EHitPlay: iUi.HandleCommandL(EPaCmdPlay); break;
			case EHitPause: iUi.HandleCommandL(EPaCmdPause); break;
			case EHitStop: iUi.HandleCommandL(EPaCmdStop); break;
			case EHitNext: iUi.HandleCommandL(EPaCmdNext); break;
			case EHitEject: iUi.HandleCommandL(EPaCmdOpenFile); break;
			case EHitShuffle: iUi.HandleCommandL(EPaCmdShuffle); break;
			case EHitRepeat: iUi.HandleCommandL(EPaCmdRepeat); break;
			case EHitEq: iEikonEnv->InfoMsg(_L("PsiAmp has no equaliser")); break;
			case EHitPl: iUi.HandleCommandL(EPaCmdPlaylist); break;
			case EHitOptions: iUi.ShowMenuL(); break;
			case EHitMinimise: iUi.HideL(); break;
			case EHitShade: iUi.HandleCommandL(EPaCmdDouble); break;
			case EHitClose: iUi.HideL(); break;
			case EHitAbout: iUi.HandleCommandL(EPaCmdAbout); break;
				}
			}
		break;
		}
	default:
		break;
		}
	}

TKeyResponse CPaMainWin::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
	{
	if (aType != EEventKey)
		return EKeyWasNotConsumed;
	TUint code = aKeyEvent.iCode;
	TUint mods = aKeyEvent.iModifiers;
	TBool ctrl = (mods & EModifierCtrl) != 0;
	CPaEngine& e = iUi.Engine();
	TPaSettings& set = iUi.Settings();
	// a key held down repeats only where that makes sense (moving, the
	// volume, the playlist's arrows): not the ones that switch something
	if (aKeyEvent.iRepeats && code != EKeyLeftArrow && code != EKeyRightArrow && code != EKeyUpArrow &&
		code != EKeyDownArrow && code != EKeyPageUp && code != EKeyPageDown && code != '+' && code != '=' && code != '-')
		return EKeyWasConsumed;
	switch (code)
		{
	case EKeyEscape:
		iUi.HideL();
		return EKeyWasConsumed;
	case ' ':
		iUi.PlayPauseL();
		return EKeyWasConsumed;
	case EKeyLeftArrow:
		if (ctrl)
			iUi.HandleCommandL(EPaCmdPrev);
		else if (e.IsOpen())
			e.SeekMs(e.PositionMs() - 5000);
		Compose(EFalse);
		return EKeyWasConsumed;
	case EKeyRightArrow:
		if (ctrl)
			iUi.HandleCommandL(EPaCmdNext);
		else if (e.IsOpen())
			e.SeekMs(e.PositionMs() + 5000);
		Compose(EFalse);
		return EKeyWasConsumed;
	case EKeyUpArrow:
	case EKeyDownArrow:
	case EKeyPageUp:
	case EKeyPageDown:
	case EKeyHome:
	case EKeyEnd:
	case EKeyEnter:
	case EKeyBackspace:
	case EKeyDelete:
		if (set.iPlaylist && !ctrl)
			return iUi.HandleListKeyL(aKeyEvent);
		if (code == EKeyUpArrow || code == EKeyDownArrow)
			{
			iUi.SetVolume(set.iVolume + (code == EKeyUpArrow ? 5 : -5));
			Compose(EFalse);
			return EKeyWasConsumed;
			}
		if (code == EKeyEnter)
			{
			iUi.HandleCommandL(EPaCmdPlay);
			return EKeyWasConsumed;
			}
		return EKeyWasNotConsumed;
	case '+':
	case '=':
		iUi.SetVolume(set.iVolume + 5);
		Compose(EFalse);
		return EKeyWasConsumed;
	case '-':
		iUi.SetVolume(set.iVolume - 5);
		Compose(EFalse);
		return EKeyWasConsumed;
		}
	if (ctrl)
		return EKeyWasNotConsumed;
	// the classic players' keys
	switch (code >= 'A' && code <= 'Z' ? code + 32 : code)
		{
	case 'z': iUi.HandleCommandL(EPaCmdPrev); return EKeyWasConsumed;
	case 'x': iUi.HandleCommandL(EPaCmdPlay); return EKeyWasConsumed;
	case 'c': iUi.HandleCommandL(EPaCmdPause); return EKeyWasConsumed;
	case 'v': iUi.HandleCommandL(EPaCmdStop); return EKeyWasConsumed;
	case 'b': iUi.HandleCommandL(EPaCmdNext); return EKeyWasConsumed;
	case 's': iUi.HandleCommandL(EPaCmdShuffle); return EKeyWasConsumed;
	case 'r': iUi.HandleCommandL(EPaCmdRepeat); return EKeyWasConsumed;
	case 'l': iUi.HandleCommandL(EPaCmdOpenFile); return EKeyWasConsumed;
	case 'j': iUi.HandleCommandL(EPaCmdGoTo); return EKeyWasConsumed;
	case 'k': iUi.NextSkinL(); return EKeyWasConsumed;
		}
	return EKeyWasNotConsumed;
	}

// ===========================================================================
// The playlist
// ===========================================================================

CPaListWin::~CPaListWin()
	{
	}

void CPaListWin::ConstructL()
	{
	CreateWindowL();
	EnableDragEvents();
	LayoutL();
	ActivateL();
	}

void CPaListWin::LayoutL()
	{
	// the frame from the skin's pieces at the player's scale (275 x 116 at
	// 2x is 550 x 232; 275 x 174 at 1x), the names in a screen font
	TInt s = iUi.Scale();
	TInt h = s == 2 ? 116 : 174;
	iFont = s == 2 ? iEikonEnv->NormalFont() : iEikonEnv->AnnotationFont();
	iFace.SetSizeL(275, h, s);
	SetSizeL(TSize(275 * s, h * s));
	Compose();
	}

// the names' area, in the window's pixels
TRect CPaListWin::TextRect() const
	{
	TSize sz = iFace.Size();
	TInt s = iFace.Scale();
	return TRect(12 * s, 20 * s, (sz.iWidth - 20) * s, (sz.iHeight - 38) * s);
	}

TInt CPaListWin::RowHeight() const
	{
	return iFont->HeightInPixels() + 3;
	}

TInt CPaListWin::Rows() const
	{
	TInt n = TextRect().Height() / RowHeight();
	return n < 1 ? 1 : n;
	}

void CPaListWin::SetSelected(TInt aIndex)
	{
	TInt n = iUi.Count();
	if (aIndex >= n) aIndex = n - 1;
	if (aIndex < 0) aIndex = 0;
	iSel = aIndex;
	ShowCurrent();
	}

void CPaListWin::ShowCurrent()
	{
	TInt rows = Rows();
	if (iSel < iTop)
		iTop = iSel;
	else if (iSel >= iTop + rows)
		iTop = iSel - rows + 1;
	if (iTop > iUi.Count() - rows)
		iTop = iUi.Count() - rows;
	if (iTop < 0)
		iTop = 0;
	Compose();
	}

void CPaListWin::Compose()
	{
	const CPaSkin& sk = iUi.Skin();
	const TPaSheet& pl = sk.Sheet(CPaSkin::EPlEdit);
	TSize sz = iFace.Size();
	TInt w = sz.iWidth, h = sz.iHeight;
	TInt ty = iActive ? 0 : 21;
	// the top: corners, the title in the middle, tiles between
	for (TInt x = 25; x < w - 25; x += 25)
		iFace.Put(pl, 127, ty, Min(25, w - 25 - x), 20, x, 0);
	iFace.Put(pl, 26, ty, 100, 20, (w - 100) / 2, 0);
	iFace.Put(pl, 0, ty, 25, 20, 0, 0);
	iFace.Put(pl, 153, ty, 25, 20, w - 25, 0);
	// the sides
	for (TInt y = 20; y < h - 38; y += 29)
		{
		TInt th = Min(29, h - 38 - y);
		iFace.Put(pl, 0, 42, 12, th, 0, y);
		iFace.Put(pl, 31, 42, 20, th, w - 20, y);
		}
	// the bottom
	for (TInt x2 = 125; x2 < w - 150; x2 += 25)
		iFace.Put(pl, 179, 0, Min(25, w - 150 - x2), 38, x2, h - 38);
	iFace.Put(pl, 0, 72, 125, 38, 0, h - 38);
	iFace.Put(pl, 126, 72, 150, 38, w - 150, h - 38);
	// the scroll bar's handle
	TInt n = iUi.Count();
	TInt rows = Rows();
	TInt span = (h - 38 - 20) - 18;
	TInt hy = 20 + (n > rows ? (iTop * span) / (n - rows) : 0);
	iFace.Put(pl, iDragBar ? 61 : 52, 53, 8, 18, w - 15, hy);
	// the running time: the track of the list, and the time
	const TPaSheet& font = sk.Sheet(CPaSkin::EText);
	TBuf<32> t;
	if (n)
		{
		TBuf<12> tm;
		PaFormatTime(tm, iUi.Engine().PositionMs());
		t.Format(_L("%d/%d %S"), iUi.Current() + 1, n, &tm);
		}
	iFace.Put(pl, 126 + 8, 72 + 9, 84, 9, w - 150 + 8, h - 38 + 9);
	iFace.Text(font, t, w - 150 + 10, h - 38 + 11, 82);
	iFace.Update(TRect(0, 0, w, h));
	if (IsReadyToDraw())
		DrawNow();
	}

void CPaListWin::Draw(const TRect& /*aRect*/) const
	{
	CWindowGc& gc = SystemGc();
	if (iFace.Bitmap())
		gc.BitBlt(TPoint(0, 0), iFace.Bitmap());
	TRect tr = TextRect();
	const CPaSkin& sk = iUi.Skin();
	gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetBrushColor(sk.ListRgb(2));
	gc.DrawRect(tr);
	gc.UseFont(iFont);
	TInt rh = RowHeight();
	TInt rows = Rows();
	TInt n = iUi.Count();
	gc.SetClippingRect(tr);
	for (TInt r = 0; r < rows && iTop + r < n; r++)
		{
		TInt i = iTop + r;
		TRect row(tr.iTl.iX, tr.iTl.iY + r * rh, tr.iBr.iX, tr.iTl.iY + (r + 1) * rh);
		if (i == iSel)
			{
			gc.SetBrushColor(sk.ListRgb(3));
			gc.DrawRect(row);
			}
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(i == iSel && sk.HasSelText() ? sk.ListRgb(4) : i == iUi.Current() ? sk.ListRgb(1) : sk.ListRgb(0));
		TBuf<160> text;
		TBuf<128> name;
		iUi.EntryName(i, name);
		text.Format(_L("%d. %S"), i + 1, &name);
		TBuf<12> len;
		if (i == iUi.Current() && iUi.Engine().IsOpen() && iUi.Engine().DurationMs() > 0)
			PaFormatTime(len, iUi.Engine().DurationMs());
		TInt lw = len.Length() ? iFont->TextWidthInPixels(len) + 4 : 0;
		TRect tbox(row.iTl.iX + 2, row.iTl.iY, row.iBr.iX - lw, row.iBr.iY);
		gc.DrawText(text, tbox, iFont->AscentInPixels() + 1, CGraphicsContext::ELeft);
		if (lw)
			gc.DrawText(len, TRect(row.iBr.iX - lw, row.iTl.iY, row.iBr.iX - 2, row.iBr.iY), iFont->AscentInPixels() + 1, CGraphicsContext::ERight);
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		}
	if (n == 0)
		{
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(sk.ListRgb(0));
		gc.DrawText(_L("No files - ADD or Ctrl+O"), TRect(tr.iTl.iX + 2, tr.iTl.iY, tr.iBr.iX, tr.iTl.iY + rh), iFont->AscentInPixels() + 1, CGraphicsContext::ELeft);
		}
	gc.CancelClippingRect();
	gc.DiscardFont();
	}

void CPaListWin::HandlePointerEventL(const TPointerEvent& aPointerEvent)
	{
	TPoint p = aPointerEvent.iPosition;             // the window's pixels: the names
	TInt sc = iFace.Scale();
	TPoint fp(p.iX / sc, p.iY / sc);                // the face's: the frame and its buttons
	TSize sz = iFace.Size();
	TInt w = sz.iWidth, h = sz.iHeight;
	if (!iUi.InFront())
		iUi.ShowL();
	switch (aPointerEvent.iType)
		{
	case TPointerEvent::EButton1Down:
		{
		TRect tr = TextRect();
		if (fp.iY < 20)
			{
			if (fp.iX >= w - 13)
				iUi.HandleCommandL(EPaCmdPlaylist);       // the close button
			else
				{
				iDragWin = ETrue;
				iDragFrom = aPointerEvent.iParentPosition;
				}
			}
		else if (tr.Contains(p))
			{
			TInt i = iTop + (p.iY - tr.iTl.iY) / RowHeight();
			if (i < iUi.Count())
				{
				// one tap chooses; a tap on the chosen one plays it
				if (i == iSel)
					iUi.PlayIndexL(i);
				else
					SetSelected(i);
				}
			}
		else if (fp.iX >= w - 20 && fp.iY < h - 38)
			{
			iDragBar = ETrue;
			Compose();
			}
		else if (fp.iY >= h - 38)
			{
			TPoint at = aPointerEvent.iParentPosition;
			TInt bx = fp.iX, by = fp.iY - (h - 38);
			if (by >= 8 && by <= 26)
				{
				if (bx >= 14 && bx < 37) iUi.PopupL(R_PA_ADD_POPUP, at);
				else if (bx >= 43 && bx < 66) iUi.PopupL(R_PA_REM_POPUP, at);
				else if (bx >= 72 && bx < 95) iUi.PopupL(R_PA_SEL_POPUP, at);
				else if (bx >= 101 && bx < 124) iUi.PopupL(R_PA_MISC_POPUP, at);
				else if (bx >= w - 44 && bx < w - 21) iUi.PopupL(R_PA_LIST_POPUP, at);
				}
			}
		break;
		}
	case TPointerEvent::EDrag:
		if (iDragWin)
			{
			TPoint by = aPointerEvent.iParentPosition - iDragFrom;
			iDragFrom = aPointerEvent.iParentPosition;
			iUi.MoveWindows(this, by);
			}
		else if (iDragBar)
			{
			TInt n = iUi.Count(), rows = Rows();
			if (n > rows)
				{
				TInt span = (h - 38 - 20) - 18;
				TInt top = ((fp.iY - 20 - 9) * (n - rows)) / (span > 0 ? span : 1);
				iTop = top < 0 ? 0 : top > n - rows ? n - rows : top;
				Compose();
				}
			}
		break;
	case TPointerEvent::EButton1Up:
		iDragWin = EFalse;
		if (iDragBar)
			{
			iDragBar = EFalse;
			Compose();
			}
		break;
	default:
		break;
		}
	}

TKeyResponse CPaListWin::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
	{
	if (aType != EEventKey)
		return EKeyWasNotConsumed;
	TInt n = iUi.Count();
	TInt rows = Rows();
	switch (aKeyEvent.iCode)
		{
	case EKeyUpArrow: SetSelected(iSel - 1); return EKeyWasConsumed;
	case EKeyDownArrow: SetSelected(iSel + 1); return EKeyWasConsumed;
	case EKeyPageUp: SetSelected(iSel - rows); return EKeyWasConsumed;
	case EKeyPageDown: SetSelected(iSel + rows); return EKeyWasConsumed;
	case EKeyHome: SetSelected(0); return EKeyWasConsumed;
	case EKeyEnd: SetSelected(n - 1); return EKeyWasConsumed;
	case EKeyEnter:
		if (n)
			iUi.PlayIndexL(iSel);
		return EKeyWasConsumed;
	case EKeyBackspace:
	case EKeyDelete:
		iUi.HandleCommandL(EPaCmdRemove);
		return EKeyWasConsumed;
		}
	return EKeyWasNotConsumed;
	}
