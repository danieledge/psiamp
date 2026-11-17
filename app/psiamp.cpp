// PSIAMP.CPP - PsiAmp's program: the playlist, the commands, the keys in
// other programs, the windows' places, the settings.
#include <e32std.h>
#include <e32math.h>
#include <e32keys.h>
#include <f32file.h>
#include <apgtask.h>
#include <apgwgnam.h>
#include <coemain.h>
#include <eikenv.h>
#include <eikmenup.h>
#include <eikmenub.h>
#include <eikdialg.h>
#include <eikcfdlg.h>
#include <eiklabel.h>
#include <eikchlst.h>
#include <eikmfne.h>
#include <eikmenu.hrh>
#include <eikcmds.hrh>
#include <eikon.rsg>
#include <psiamp.rsg>
#include "psiamp.h"

// the keys PsiAmp takes in every program (with Ctrl and Fn held, as Coda's
// screenshot key, Ctrl+Fn+P): show or put away, play or pause, previous, next
struct TPaKey { TUint iScan; TUint iCode; };
static const TPaKey KPaKeys[4] = { { 'A', 'A' - '@' }, { 'X', 'X' - '@' }, { 'Z', 'Z' - '@' }, { 'B', 'B' - '@' } };
const TUint KPaKeyMods = EModifierCtrl | EModifierFunc;
const TUint KPaKeyMask = EModifierCtrl | EModifierFunc | EModifierShift;
const TInt KPaMaxSkins = 6;

// ===========================================================================
// Dialogs
// ===========================================================================

class CPaAboutDialog : public CEikDialog
	{
private:
	void PreLayoutDynInitL()
		{
		TBuf<24> name;
		name.Format(_L("PsiAmp %S"), &KPaVersion);
		((CEikLabel*)Control(EPaDlgAboutName))->SetTextL(name);
		((CEikLabel*)Control(EPaDlgAboutCopy))->SetTextL(_L("\xa9 Dan Edge 2026 - Coda for the netBook"));
		// the RealNetworks Public Source License's object code notice
		((CEikLabel*)Control(EPaDlgAboutHelix))->SetTextL(_L("Helix DNA Client technology included."));
		((CEikLabel*)Control(EPaDlgAboutHelix2))->SetTextL(_L("Copyright (c) RealNetworks, Inc., 1995-2002. All rights reserved."));
		// and where the source is (RPSL 2.1(e)): PsiAmp's own code under MIT,
		// Helix's under the RPSL
		((CEikLabel*)Control(EPaDlgAboutSource))->SetTextL(_L("Source: MIT licence; Helix under the RPSL, at"));
		((CEikLabel*)Control(EPaDlgAboutSource2))->SetTextL(_L("https://github.com/danieledge/psiamp"));
		}
	};

class CPaGoToDialog : public CEikDialog
	{
public:
	CPaGoToDialog(TInt& aMs, TInt aMax) : iMs(aMs), iMax(aMax) {}
private:
	void PreLayoutDynInitL()
		{
		CEikDurationEditor* d = (CEikDurationEditor*)Control(EPaDlgTime);
		TInt max = iMax / 1000;
		if (max < 1)
			max = 1;
		d->SetMinimumAndMaximum(TTimeIntervalSeconds(0), TTimeIntervalSeconds(max));
		d->SetDuration(TTimeIntervalSeconds(iMs / 1000));
		}
	TBool OkToExitL(TInt /*aButtonId*/)
		{
		CEikDurationEditor* d = (CEikDurationEditor*)Control(EPaDlgTime);
		iMs = d->Duration().Int() * 1000;
		return ETrue;
		}
	TInt& iMs;
	TInt iMax;
	};

class CPaPrefsDialog : public CEikDialog
	{
public:
	CPaPrefsDialog(TPaSettings& aSet) : iSet(aSet) {}
private:
	void PreLayoutDynInitL()
		{
		((CEikChoiceList*)Control(EPaDlgKeys))->SetCurrentItem(iSet.iKeys ? 1 : 0);
		((CEikChoiceList*)Control(EPaDlgVis))->SetCurrentItem(iSet.iVisMode);
		((CEikChoiceList*)Control(EPaDlgTimeShown))->SetCurrentItem(iSet.iRemaining ? 1 : 0);
		}
	TBool OkToExitL(TInt /*aButtonId*/)
		{
		iSet.iKeys = ((CEikChoiceList*)Control(EPaDlgKeys))->CurrentItem();
		iSet.iVisMode = ((CEikChoiceList*)Control(EPaDlgVis))->CurrentItem();
		iSet.iRemaining = ((CEikChoiceList*)Control(EPaDlgTimeShown))->CurrentItem();
		return ETrue;
		}
	TPaSettings& iSet;
	};

// ===========================================================================
// The document: no file of its own; a file it is given is played
// ===========================================================================

CFileStore* CPaDocument::OpenFileL(TBool aDoOpen, const TDesC& aFilename, RFs& /*aFs*/)
	{
	if (aDoOpen)
		{
		if (iAppUi)
			((CPaAppUi*)iAppUi)->OpenFileL(aFilename);
		else
			iStartFile = aFilename;
		}
	return NULL;
	}

// ===========================================================================
// The program
// ===========================================================================

CPaAppUi::~CPaAppUi()
	{
	if (iEngine)
		{
		iSet.iTrack = iCurrent;
		iSet.iPosMs = iEngine->IsOpen() ? iEngine->PositionMs() : 0;
		}
	SaveSettings();
	TRAPD(err, SaveListL(KPaListFile));
	(void)err;
	ReleaseKeys();
	if (iTicker)
		iTicker->Cancel();
	delete iTicker;
	if (iMain)
		{
		RemoveFromStack(iMain);
		delete iMain;
		}
	delete iListWin;
	delete iEngine;
	delete iSkin;
	delete iList;
	delete iTitles;
	delete iSkins;
	}

void CPaAppUi::ConstructL()
	{
	BaseConstructL();
	TTime now;
	now.HomeTime();
	iSeed = now.Int64();
	iList = new(ELeave) CDesCArrayFlat(16);
	iTitles = new(ELeave) CDesCArrayFlat(16);
	iSkins = new(ELeave) CDesCArrayFlat(4);
	LoadSettings();
	iEngine = CPaEngine::NewL(iCoeEnv->FsSession(), *this);
	iEngine->SetVolume(iSet.iVolume);
	// the skin: the one chosen, else Coda (beside the program). Only one is
	// kept: Coda is opened again while another skin is read, for the parts
	// it lacks, and let go after (two skins are about 400 KB)
	TInt err = KErrNone;
	TFileName chosen(iSet.iSkin);
	TRAP(err, SetSkinL(chosen, EFalse));
	if (!iSkin)
		User::Leave(KErrNoMemory);
	TRAP(err, LoadListL(KPaListFile, EFalse));
	if (err == KErrNotFound || err == KErrPathNotFound)
		{
		// the first start: the sample, Coda, beside the program, in the
		// playlist, so there is something to play. (Not in \System\Samples:
		// the Shell copies that folder into C:\Documents on a new C:)
		TParsePtrC app(Application()->AppFullName());
		TFileName sample(app.DriveAndPath());
		sample.Append(KPaSample);
		TEntry e;
		if (iCoeEnv->FsSession().Entry(sample, e) == KErrNone)
			AddFileL(sample);
		}
	iCurrent = iSet.iTrack >= 0 && iSet.iTrack < iList->Count() ? iSet.iTrack : 0;
	iMain = new(ELeave) CPaMainWin(*this);
	iMain->ConstructL();
	iListWin = new(ELeave) CPaListWin(*this);
	iListWin->ConstructL();
	AddToStackL(iMain);
	LayoutWindowsL();
	iTicker = CPeriodic::NewL(CActive::EPriorityLow);
	CaptureKeys();
	CPaDocument* doc = (CPaDocument*)Document();
	if (doc->iStartFile.Length())
		OpenFileL(doc->iStartFile);
	else if (iList->Count())
		{
		// where it was: the track, stopped at its place
		if (iEngine->Open((*iList)[iCurrent], iSet.iPosMs) == KErrNone)
			{
			iTitles->Delete(iCurrent);
			iTitles->InsertL(iCurrent, iEngine->Title());
			}
		}
	iListWin->SetSelected(iCurrent);
	iMain->Compose(ETrue);
	}

// Started with no file (the Extras bar, the System screen's program list): no
// document is made. With a file: it is played (OpenFileL)
TBool CPaAppUi::ProcessCommandParametersL(TApaCommand aCommand, TFileName& aDocumentName, const TDesC8& /*aTail*/)
	{
	if (aCommand == EApaCommandOpen && aDocumentName.Length())
		{
		TEntry e;
		if (iCoeEnv->FsSession().Entry(aDocumentName, e) == KErrNone)
			return ETrue;
		}
	aDocumentName.Zero();
	return EFalse;
	}

void CPaAppUi::Log(const TDesC& aText)
	{
	RFs& fs = iCoeEnv->FsSession();
	fs.MkDirAll(KPaDir);
	RFile f;
	TInt err = f.Open(fs, KPaLog, EFileWrite | EFileShareAny);
	if (err == KErrNotFound)
		err = f.Create(fs, KPaLog, EFileWrite | EFileShareAny);
	if (err != KErrNone)
		return;
	TInt size = 0;
	f.Size(size);
	if (size > 16384)
		{
		f.SetSize(0);
		size = 0;
		}
	TTime now;
	now.HomeTime();
	TBuf<32> stamp;
	TRAPD(e2, now.FormatL(stamp, _L("%H:%T:%S ")));
	(void)e2;
	TBuf8<256> line;
	line.Copy(stamp);
	TInt room = line.MaxLength() - line.Length() - 2;
	TBuf8<256> text;
	text.Copy(PaLeft(aText, room));
	line.Append(text);
	line.Append(_L8("\r\n"));
	f.Write(size, line);
	f.Close();
	}

// ---- settings -------------------------------------------------------------

void CPaAppUi::LoadSettings()
	{
	Mem::FillZ(&iSet, sizeof(iSet));
	iSet.iVersion = 1;
	iSet.iVolume = 80;
	iSet.iDouble = 1;
	iSet.iPlaylist = 1;
	iSet.iKeys = 1;
	iSet.iMainX = iSet.iMainY = iSet.iListX = iSet.iListY = -1;
	RFile f;
	if (f.Open(iCoeEnv->FsSession(), KPaIni, EFileRead) != KErrNone)
		return;
	TPaSettings s;
	TPckg<TPaSettings> p(s);
	// (a damaged file's values are not taken: a text longer than its room
	// or a choice that is not in its list panics later)
	if (f.Read(p) == KErrNone && p.Length() == sizeof(s) && s.iVersion == 1 &&
		s.iSkin.MaxLength() == iSet.iSkin.MaxLength() && s.iSkin.Length() <= s.iSkin.MaxLength() &&
		s.iVisMode >= 0 && s.iVisMode <= 2)
		iSet = s;
	f.Close();
	}

void CPaAppUi::SaveSettings()
	{
	RFs& fs = iCoeEnv->FsSession();
	fs.MkDirAll(KPaDir);
	RFile f;
	if (f.Replace(fs, KPaIni, EFileWrite) != KErrNone)
		return;
	f.Write(TPckgC<TPaSettings>(iSet));
	f.Close();
	}

// ---- the playlist ---------------------------------------------------------

TBool CPaAppUi::IsAudio(const TDesC& aName)
	{
	TParsePtrC p(aName);
	TPtrC x = p.Ext();
	return !x.CompareF(_L(".mp3")) || !x.CompareF(_L(".wav"));
	}

TBool CPaAppUi::IsList(const TDesC& aName)
	{
	TParsePtrC p(aName);
	return !p.Ext().CompareF(_L(".m3u"));
	}

void CPaAppUi::EntryName(TInt aIndex, TDes& aName) const
	{
	aName.Zero();
	if (aIndex < 0 || aIndex >= iList->Count())
		return;
	TPtrC t = (*iTitles)[aIndex];
	if (t.Length())
		{
		aName = PaLeft(t, aName.MaxLength());
		return;
		}
	TParsePtrC p((*iList)[aIndex]);
	aName = PaLeft(p.Name(), aName.MaxLength());
	}

void CPaAppUi::AddFileL(const TDesC& aName)
	{
	iList->AppendL(aName);
	iTitles->AppendL(KNullDesC);
	}

// the sound files in a folder, by name
void CPaAppUi::AddFolderL(const TDesC& aFolder)
	{
	CDir* dir = NULL;
	TFileName spec(aFolder);
	if (spec.Length() && spec[spec.Length() - 1] != '\\')
		spec.Append('\\');
	TFileName folder(spec);
	spec.Append('*');
	User::LeaveIfError(iCoeEnv->FsSession().GetDir(spec, KEntryAttNormal, ESortByName, dir));
	CleanupStack::PushL(dir);
	TInt added = 0;
	for (TInt i = 0; i < dir->Count(); i++)
		{
		const TEntry& e = (*dir)[i];
		if (e.IsDir() || !IsAudio(e.iName))
			continue;
		TFileName f(folder);
		f.Append(e.iName);
		AddFileL(f);
		added++;
		}
	CleanupStack::PopAndDestroy();
	TBuf<40> m;
	if (added == 1)
		m = _L("1 file added");
	else if (added)
		m.Format(_L("%d files added"), added);
	else
		m = _L("No MP3 or WAV files in this folder");
	iEikonEnv->InfoMsg(m);
	}

void CPaAppUi::LoadListL(const TDesC& aM3u, TBool aAppend)
	{
	RFile f;
	User::LeaveIfError(f.Open(iCoeEnv->FsSession(), aM3u, EFileRead | EFileShareReadersOnly));
	CleanupClosePushL(f);
	TInt size = 0;
	f.Size(size);
	if (size > 256 * 1024)
		User::Leave(KErrTooBig);
	HBufC8* buf = HBufC8::NewLC(size);
	TPtr8 p = buf->Des();
	User::LeaveIfError(f.Read(p));
	if (!aAppend)
		{
		iList->Reset();
		iTitles->Reset();
		iCurrent = 0;
		}
	TParsePtrC base(aM3u);
	TInt at = 0;
	while (at < p.Length())
		{
		TInt end = at;
		while (end < p.Length() && p[end] != '\n' && p[end] != '\r')
			end++;
		TPtrC8 line = p.Mid(at, end - at);
		at = end + 1;
		// (a UTF-8 mark at the start, spaces round the name)
		if (line.Length() >= 3 && line[0] == 0xef && line[1] == 0xbb && line[2] == 0xbf)
			line.Set(line.Mid(3));
		while (line.Length() && (line[0] == ' ' || line[0] == '\t'))
			line.Set(line.Mid(1));
		while (line.Length() && (line[line.Length() - 1] == ' ' || line[line.Length() - 1] == '\t'))
			line.Set(line.Left(line.Length() - 1));
		if (!line.Length() || line[0] == '#' || line.Length() > 200)
			continue;
		TFileName name;
		name.Copy(line);
		for (TInt i = 0; i < name.Length(); i++)
			if (name[i] == '/')
				name[i] = '\\';
		if (!(name.Length() > 2 && name[1] == ':'))
			{
			// relative to the list's folder
			TFileName full(base.DriveAndPath());
			if (name.Length() && name[0] == '\\')
				full = base.Drive();
			if (full.Length() + name.Length() > full.MaxLength())
				continue;                        // (too long to be a file's name)
			full.Append(name);
			name = full;
			}
		AddFileL(name);
		}
	CleanupStack::PopAndDestroy(2);
	}

void CPaAppUi::SaveListL(const TDesC& aM3u)
	{
	RFs& fs = iCoeEnv->FsSession();
	TParsePtrC p(aM3u);
	fs.MkDirAll(p.DriveAndPath());
	// written beside it and put in its place when whole, so a full disk
	// does not leave half a playlist
	TFileName tmp(aM3u);
	if (tmp.Length() + 4 > tmp.MaxLength())
		User::Leave(KErrBadName);
	tmp.Append(_L(".tmp"));
	RFile f;
	User::LeaveIfError(f.Replace(fs, tmp, EFileWrite));
	CleanupClosePushL(f);
	TRAPD(err, WriteListL(f));
	CleanupStack::PopAndDestroy();
	if (err == KErrNone)
		err = fs.Replace(tmp, aM3u);
	if (err != KErrNone)
		{
		fs.Delete(tmp);
		User::Leave(err);
		}
	}

void CPaAppUi::WriteListL(RFile& aFile)
	{
	RFile& f = aFile;
	User::LeaveIfError(f.Write(_L8("#EXTM3U\r\n")));
	for (TInt i = 0; i < iList->Count(); i++)
		{
		TBuf8<260> line;
		line.Copy((*iList)[i]);
		line.Append(_L8("\r\n"));
		User::LeaveIfError(f.Write(line));
		}
	User::LeaveIfError(f.Flush());
	}

// a tapped file: an .m3u becomes the playlist; a skin (.wsz) is put on; a
// sound file is played, alone
void CPaAppUi::OpenFileL(const TDesC& aFileName)
	{
	TParsePtrC px(aFileName);
	if (!px.Ext().CompareF(_L(".wsz")))
		{
		SetSkinL(aFileName, ETrue);
		ShowL();
		return;
		}
	if (IsList(aFileName))
		{
		TRAPD(err, LoadListL(aFileName, EFalse));
		if (err != KErrNone)
			{
			iEikonEnv->InfoMsg(_L("Cannot read this playlist"));
			return;
			}
		}
	else
		{
		iList->Reset();
		iTitles->Reset();
		iCurrent = 0;
		AddFileL(aFileName);
		}
	iListWin->SetSelected(0);
	if (iList->Count())
		PlayIndexL(0);
	ShowL();
	}

TBool CPaAppUi::AskFileL(TFileName& aName, TInt aTitle)
	{
	if (aName.Length() == 0)
		{
		aName = iList->Count() ? (*iList)[iCurrent] : TPtrC(_L("C:\\Documents\\"));
		}
	CEikFileOpenDialog* d = new(ELeave) CEikFileOpenDialog(&aName, aTitle);
	return d->ExecuteLD(R_EIK_DIALOG_FILE_OPEN);
	}

// ---- playing --------------------------------------------------------------

void CPaAppUi::PlayIndexL(TInt aIndex, TInt aStartMs)
	{
	TInt n = iList->Count();
	if (n == 0)
		{
		iEikonEnv->InfoMsg(_L("Nothing to play - Ctrl+O opens a file"));
		return;
		}
	// a file that does not open: the next (each tried once)
	for (TInt tries = 0; tries < n; tries++)
		{
		if (aIndex < 0 || aIndex >= n)
			aIndex = 0;
		iCurrent = aIndex;
		TInt err = iEngine->Open((*iList)[aIndex], aStartMs);
		if (err == KErrNone)
			{
			TBuf<200> m;
			TBuf<128> t(iEngine->Title());
			m.Format(_L("play \"%S\": %d kbps, %d kHz, %d ch, %d ms"), &t, iEngine->Kbps(), iEngine->Khz(), iEngine->Channels(), iEngine->DurationMs());
			Log(m);
			iTitles->Delete(aIndex);
			iTitles->InsertL(aIndex, iEngine->Title());
			iEngine->Play();
			iListWin->SetSelected(aIndex);
			iMain->Compose(ETrue);
			StartTicks();
			return;
			}
		TBuf<128> m;
		TParsePtrC p((*iList)[aIndex]);
		TPtrC nm = PaLeft(p.NameAndExt(), 60);
		if (err == KErrNotFound || err == KErrPathNotFound)
			m.Format(_L("File not found - %S"), &nm);
		else if (err == KErrNotSupported || err == KErrCorrupt)
			m.Format(_L("Not an MP3 or WAV file PsiAmp plays - %S"), &nm);
		else
			m.Format(_L("Cannot open %S (%d)"), &nm, err);
		iEikonEnv->InfoMsg(m);
		Log(m);
		aIndex++;
		aStartMs = 0;
		if (aIndex >= n && !iSet.iRepeat)
			break;
		}
	iEngine->Close();
	iMain->Compose(ETrue);
	}

void CPaAppUi::NextL(TBool aUser)
	{
	TInt n = iList->Count();
	if (n == 0)
		return;
	TInt next;
	if (iSet.iShuffle && n > 1)
		{
		do
			next = Math::Rand(iSeed) % n;
		while (next == iCurrent);
		}
	else
		next = iCurrent + 1;
	if (next >= n)
		{
		if (!iSet.iRepeat && !aUser)
			{
			// the end of the list: stopped, at its start
			iEngine->Stop();
			iMain->Compose(ETrue);
			iListWin->Compose();
			return;
			}
		next = 0;
		}
	PlayIndexL(next);
	}

void CPaAppUi::PrevL()
	{
	TInt n = iList->Count();
	if (n == 0)
		return;
	// (more than 3 s in: the start of this track, as the players do)
	if (iEngine->IsOpen() && iEngine->PositionMs() > 3000)
		{
		iEngine->SeekMs(0);
		if (iEngine->State() != CPaEngine::EPlaying)
			iEngine->Play();
		return;
		}
	PlayIndexL(iCurrent > 0 ? iCurrent - 1 : n - 1);
	}

// a second play/pause within 0.4 s is not meant (nobody toggles that fast;
// the emulator sometimes delivers a key twice)
TBool CPaAppUi::TooSoon()
	{
	TTime now;
	now.UniversalTime();
	TBool soon = now.MicroSecondsFrom(iLastToggle) < TTimeIntervalMicroSeconds(TInt64(400000));
	if (!soon)
		iLastToggle = now;
	return soon;
	}

void CPaAppUi::PlayPauseL()
	{
	if (TooSoon())
		return;
	if (iEngine->State() == CPaEngine::EPlaying)
		iEngine->Pause();
	else if (iEngine->IsOpen())
		{
		iEngine->Play();
		StartTicks();
		}
	else
		PlayIndexL(iCurrent);
	}

void CPaAppUi::SetVolume(TInt aPercent)
	{
	iSet.iVolume = aPercent < 0 ? 0 : aPercent > 100 ? 100 : aPercent;
	iEngine->SetVolume(iSet.iVolume);
	}

// ---- MPaEngineObserver ----------------------------------------------------

void CPaAppUi::EngineChanged()
	{
	if (iMain)
		iMain->Compose(ETrue);
	if (iListWin)
		iListWin->Compose();
	}

void CPaAppUi::EngineTrackEnded()
	{
	TRAPD(err, NextL(EFalse));
	(void)err;
	}

void CPaAppUi::EngineMessage(const TDesC& aText)
	{
	iEikonEnv->InfoMsg(aText);
	Log(aText);
	}

// ---- the windows ----------------------------------------------------------

void CPaAppUi::LayoutWindowsL()
	{
	TInt s = Scale();
	iMain->LayoutL();
	iListWin->LayoutL();
	TSize scr = iEikonEnv->ScreenDevice()->SizeInPixels();
	TSize ms = iMain->Size(), ls = iListWin->Size();
	TPoint m(iSet.iMainX, iSet.iMainY), l(iSet.iListX, iSet.iListY);
	if (m.iX < 0 || m.iY < 0 || m.iX + ms.iWidth > scr.iWidth || m.iY + ms.iHeight > scr.iHeight)
		{
		// the standard places: the two together, in the middle, the list under the player
		TInt total = ms.iHeight + (iSet.iPlaylist ? ls.iHeight : 0);
		m.SetXY((scr.iWidth - ms.iWidth) / 2, Max(0, (scr.iHeight - total) / 2));
		l.SetXY(m.iX, m.iY + ms.iHeight);
		}
	if (l.iX < 0 || l.iY < 0 || l.iX + ls.iWidth > scr.iWidth || l.iY + ls.iHeight > scr.iHeight)
		l.SetXY(m.iX, Min(m.iY + ms.iHeight, scr.iHeight - ls.iHeight));
	iMain->SetPosition(m);
	iListWin->SetPosition(l);
	iSet.iMainX = m.iX; iSet.iMainY = m.iY;
	iSet.iListX = l.iX; iSet.iListY = l.iY;
	iListWin->MakeVisible(iSet.iPlaylist != 0);
	iMain->DrawNow();
	if (iSet.iPlaylist)
		iListWin->DrawNow();
	(void)s;
	}

// a window dragged by its title bar; the playlist goes with the player when
// it is docked under it
void CPaAppUi::MoveWindows(CCoeControl* aWin, const TPoint& aBy)
	{
	TSize scr = iEikonEnv->ScreenDevice()->SizeInPixels();
	TPoint mp = iMain->Position(), lp = iListWin->Position();
	TBool docked = lp.iX == mp.iX && lp.iY == mp.iY + iMain->Size().iHeight;
	TPoint to = aWin->Position() + aBy;
	TSize sz = aWin->Size();
	if (to.iX < -sz.iWidth + 40) to.iX = -sz.iWidth + 40;
	if (to.iX > scr.iWidth - 40) to.iX = scr.iWidth - 40;
	if (to.iY < 0) to.iY = 0;
	if (to.iY > scr.iHeight - 20) to.iY = scr.iHeight - 20;
	TPoint by = to - aWin->Position();
	aWin->SetPosition(to);
	if (aWin == iMain && docked)
		iListWin->SetPosition(lp + by);
	else if (aWin == iListWin)
		{
		// snaps under the player when near it
		TPoint want(mp.iX, mp.iY + iMain->Size().iHeight);
		TPoint d = to - want;
		if (d.iX > -10 && d.iX < 10 && d.iY > -10 && d.iY < 10)
			iListWin->SetPosition(want);
		}
	iSet.iMainX = iMain->Position().iX; iSet.iMainY = iMain->Position().iY;
	iSet.iListX = iListWin->Position().iX; iSet.iListY = iListWin->Position().iY;
	}

void CPaAppUi::ShowMenuL()
	{
	if (MenuBar())
		MenuBar()->TryDisplayMenuBarL();
	}

void CPaAppUi::PopupL(TInt aResource, const TPoint& aScreenPos)
	{
	LaunchPopupMenuL(aResource, aScreenPos, EPopupTargetBottomLeft);
	}

TKeyResponse CPaAppUi::HandleListKeyL(const TKeyEvent& aKeyEvent)
	{
	return iListWin->OfferKeyEventL(aKeyEvent, EEventKey);
	}

// K in the player: the next skin (Coda, EPOC, then those found), named
void CPaAppUi::NextSkinL()
	{
	FindSkinsL();
	TInt at = 0;                                     // 0 Coda, 1 EPOC, 2.. the skins found
	if (iSet.iSkin == KPaEpocSetting)
		at = 1;
	else
		for (TInt k = 0; k < iSkins->Count(); k++)
			if (!(*iSkins)[k].CompareF(iSet.iSkin))
				at = k + 2;
	at = (at + 1) % (iSkins->Count() + 2);
	TBuf<64> name;
	if (at == 0)
		{
		SetSkinL(KNullDesC, ETrue);
		name = _L("Coda");
		}
	else if (at == 1)
		{
		SetSkinL(KPaEpocSetting, ETrue);
		name = _L("EPOC");
		}
	else
		{
		TFileName f((*iSkins)[at - 2]);
		SetSkinL(f, ETrue);
		TParsePtrC p(f);
		name = PaLeft(p.Name(), 50);
		}
	TBuf<80> m;
	m.Format(_L("Skin: %S"), &name);
	iEikonEnv->InfoMsg(m);
	SaveSettings();
	}

void CPaAppUi::ToggleVisL()
	{
	iSet.iVisMode = (iSet.iVisMode + 1) % 3;
	iMain->Compose(EFalse);
	}

// ---- in front, put away, kept on top ----------------------------------------

void CPaAppUi::SetOnTop(TBool aOn)
	{
	RWindowGroup& wg = iCoeEnv->RootWin();
	if (aOn == iPinned)
		return;
	iPinned = aOn;
	if (aOn)
		{
		// over every program, without taking the keys from the one in use
		wg.EnableReceiptOfFocus(EFalse);
		wg.SetOrdinalPosition(0, ECoeWinPriorityAlwaysAtFront);
		iListWin->MakeVisible(EFalse);
		}
	else
		{
		wg.SetOrdinalPosition(wg.OrdinalPosition(), ECoeWinPriorityNormal);
		wg.EnableReceiptOfFocus(ETrue);
		iListWin->MakeVisible(iSet.iPlaylist != 0);
		}
	}

void CPaAppUi::ShowL()
	{
	SetOnTop(EFalse);
	TApaTask me(iCoeEnv->WsSession());
	me.SetWgId(iCoeEnv->RootWin().Identifier());
	me.BringToForeground();
	}

void CPaAppUi::HideL()
	{
	iHiding = ETrue;
	SetOnTop(EFalse);
	TApaTask me(iCoeEnv->WsSession());
	me.SetWgId(iCoeEnv->RootWin().Identifier());
	me.SendToBackground();
	iHiding = EFalse;
	}

void CPaAppUi::HandleForegroundEventL(TBool aForeground)
	{
	iInFront = aForeground;
	iMain->SetActive(aForeground);
	iListWin->SetActive(aForeground);
	if (aForeground)
		SetOnTop(EFalse);
	else if (iSet.iOnTop && !iHiding)
		SetOnTop(ETrue);
	iMain->Compose(ETrue);
	iListWin->Compose();
	StartTicks();
	}

void CPaAppUi::StartTicks()
	{
	iTicker->Cancel();
	if (iInFront || iPinned)
		iTicker->Start(100000, 100000, TCallBack(Tick, this));
	}

TInt CPaAppUi::Tick(TAny* aSelf)
	{
	((CPaAppUi*)aSelf)->DoTick();
	return ETrue;
	}

void CPaAppUi::DoTick()
	{
	iMain->Compose(EFalse);
	if (++iTickCount % 10 == 0 && iSet.iPlaylist && !iPinned)
		iListWin->Compose();
	}

// ---- the keys in other programs -------------------------------------------

void CPaAppUi::CaptureKeys()
	{
	ReleaseKeys();
	if (!iSet.iKeys)
		return;
	RWindowGroup& wg = iCoeEnv->RootWin();
	TInt bad = 0;
	for (TInt i = 0; i < 4; i++)
		{
		TInt32 a = wg.CaptureKeyUpAndDowns(KPaKeys[i].iScan, KPaKeyMask, KPaKeyMods);
		TInt32 b = wg.CaptureKey(KPaKeys[i].iCode, KPaKeyMask, KPaKeyMods);
		iCap[iCapCount++] = a;
		iCap[iCapCount++] = b;
		if (a <= 0 || b <= 0)
			bad++;
		}
	if (bad)
		{
		Log(_L("some of Ctrl+Fn+A, X, Z, B are taken by another program"));
		iEikonEnv->InfoMsg(_L("Some PsiAmp keys are in use by another program"));
		}
	}

void CPaAppUi::ReleaseKeys()
	{
	RWindowGroup& wg = iCoeEnv->RootWin();
	for (TInt i = 0; i < iCapCount; i++)
		{
		if (iCap[i] <= 0)
			continue;
		if (i % 2 == 0)
			wg.CancelCaptureKeyUpAndDowns(iCap[i]);
		else
			wg.CancelCaptureKey(iCap[i]);
		}
	iCapCount = 0;
	}

void CPaAppUi::HandleWsEventL(const TWsEvent& aEvent, CCoeControl* aDestination)
	{
	TInt type = aEvent.Type();
	if (iSet.iKeys && (type == EEventKey || type == EEventKeyDown || type == EEventKeyUp))
		{
		const TKeyEvent* k = aEvent.Key();
		if ((k->iModifiers & KPaKeyMask) == KPaKeyMods)
			{
			for (TInt i = 0; i < 4; i++)
				{
				TBool mine = type == EEventKey ? k->iCode == KPaKeys[i].iCode : (TUint)k->iScanCode == KPaKeys[i].iScan;
				if (!mine)
					continue;
				if (type == EEventKey && k->iRepeats)
					return;                      // (held down: once is enough)
				if (type == EEventKey)
					{
					switch (i)
						{
					case 0:
						if (iInFront)
							HideL();
						else
							ShowL();
						break;
					case 1: PlayPauseL(); break;
					case 2: PrevL(); break;
					case 3: NextL(ETrue); break;
						}
					if (i && !iInFront)
						{
						// what happened, over the program in use
						TBuf<140> m;
						if (iEngine->State() == CPaEngine::EPlaying)
							{
							TBuf<128> t(iEngine->Title());
							m.Format(_L("Playing %S"), &t);
							}
						else
							m = iEngine->State() == CPaEngine::EPaused ? _L("Paused") : _L("Stopped");
						iEikonEnv->InfoMsg(m);
						}
					}
				return;
				}
			}
		}
	CEikAppUi::HandleWsEventL(aEvent, aDestination);
	}

// ---- skins ----------------------------------------------------------------

void CPaAppUi::FindSkinsL()
	{
	iSkins->Reset();
	RFs& fs = iCoeEnv->FsSession();
	static const TText* const KDirs[] = { _S("C:\\System\\Apps\\PsiAmp\\Skins\\"), _S("D:\\System\\Apps\\PsiAmp\\Skins\\"), _S("C:\\Skins\\"), _S("D:\\Skins\\") };
	for (TInt d = 0; d < 4 && iSkins->Count() < KPaMaxSkins; d++)
		{
		TPtrC dir(KDirs[d]);
		TFileName spec(dir);
		spec.Append(_L("*.wsz"));
		CDir* list = NULL;
		if (fs.GetDir(spec, KEntryAttNormal, ESortByName, list) != KErrNone)
			continue;
		for (TInt i = 0; i < list->Count() && iSkins->Count() < KPaMaxSkins; i++)
			{
			TFileName f(dir);
			f.Append((*list)[i].iName);
			iSkins->AppendL(f);
			}
		delete list;
		}
	}

// Coda, as it is beside the program (NULL if it cannot be read)
CPaSkin* CPaAppUi::LoadCodaL()
	{
	CPaSkin* coda = CPaSkin::NewL();
	TParsePtrC app(Application()->AppFullName());
	TFileName name(app.DriveAndPath());
	name.Append(KPaSkinName);
	TInt err = coda->Load(iCoeEnv->FsSession(), name, NULL);
	if (err != KErrNone)
		{
		TBuf<64> m;
		m.Format(_L("Coda skin not found (%d)"), err);
		Log(m);
		}
	return coda;
	}

// a skin: "" Coda, KPaEpocSetting EPOC, else a .wsz; Coda's parts fill in
// what it lacks. On failure the skin in use stays (Coda at start-up)
void CPaAppUi::SetSkinL(const TDesC& aWsz, TBool aRedraw)
	{
	CPaSkin* coda = LoadCodaL();
	CPaSkin* chosen = coda;
	if (aWsz.Length())
		{
		CleanupStack::PushL(coda);
		CPaSkin* s = CPaSkin::NewL();
		CleanupStack::PushL(s);
		TBool epoc = aWsz == KPaEpocSetting;
		TFileName wsz(aWsz);
		if (epoc)
			{
			TParsePtrC app(Application()->AppFullName());
			wsz = app.DriveAndPath();
			wsz.Append(KPaEpocName);
			}
		if (aRedraw)
			iEikonEnv->BusyMsgL(_L("Opening skin…"), EHLeftVBottom, TTimeIntervalMicroSeconds32(300000));
		TInt err = s->Load(iCoeEnv->FsSession(), wsz, coda, epoc);
		if (aRedraw)
			iEikonEnv->BusyMsgCancel();
		CleanupStack::Pop(2);
		if (err != KErrNone)
			{
			delete s;
			if (aRedraw)
				{
				delete coda;
				iEikonEnv->InfoMsg(err == KErrNoMemory ? _L("Not enough memory for this skin") : _L("Not a skin PsiAmp can read"));
				return;
				}
			iSet.iSkin.Zero();               // (at start-up: Coda)
			}
		else
			{
			delete coda;
			chosen = s;
			}
		}
	delete iSkin;
	iSkin = chosen;
	iSet.iSkin = chosen == coda ? TPtrC() : PaLeft(aWsz, iSet.iSkin.MaxLength());
	if (aRedraw)
		{
		iMain->Compose(ETrue);
		iListWin->Compose();
		SaveSettings();
		}
	}

// ---- menus and commands ---------------------------------------------------

void CPaAppUi::DynInitMenuPaneL(TInt aMenuId, CEikMenuPane* aPane)
	{
	switch (aMenuId)
		{
	case R_PA_VIEW_MENU:
		aPane->SetItemButtonState(EPaCmdPlaylist, iSet.iPlaylist ? EEikMenuItemSymbolOn : EEikMenuItemSymbolIndeterminate);
		aPane->SetItemButtonState(EPaCmdDouble, iSet.iDouble ? EEikMenuItemSymbolOn : EEikMenuItemSymbolIndeterminate);
		aPane->SetItemButtonState(EPaCmdVisualiser, iSet.iVisMode != 2 ? EEikMenuItemSymbolOn : EEikMenuItemSymbolIndeterminate);
		aPane->SetItemButtonState(EPaCmdOnTop, iSet.iOnTop ? EEikMenuItemSymbolOn : EEikMenuItemSymbolIndeterminate);
		break;
	case R_PA_PLAY_MENU:
	case R_PA_MISC_POPUP:
		aPane->SetItemButtonState(EPaCmdShuffle, iSet.iShuffle ? EEikMenuItemSymbolOn : EEikMenuItemSymbolIndeterminate);
		aPane->SetItemButtonState(EPaCmdRepeat, iSet.iRepeat ? EEikMenuItemSymbolOn : EEikMenuItemSymbolIndeterminate);
		break;
	case R_PA_SKIN_MENU:
		{
		// Coda, the skins found, then Other skin (the pane only adds at its end)
		FindSkinsL();
		CEikMenuPane::TItem::SData other = aPane->ItemData(EPaCmdSkinOther);
		aPane->DeleteMenuItem(EPaCmdSkinOther);
		TInt n = iSkins->Count();
		aPane->ItemData(EPaCmdSkinStandard).iFlags = EEikMenuItemRadioStart;
		aPane->ItemData(EPaCmdSkinEpoc).iFlags = n ? EEikMenuItemRadioMiddle : EEikMenuItemRadioEnd | EEikMenuItemSeparatorAfter;
		for (TInt i = 0; i < n; i++)
			{
			CEikMenuPane::TItem::SData d;
			d.iCommandId = EPaCmdSkin0 + i;
			d.iCascadeId = 0;
			d.iFlags = i == n - 1 ? EEikMenuItemRadioEnd | EEikMenuItemSeparatorAfter : EEikMenuItemRadioMiddle;
			TParsePtrC p((*iSkins)[i]);
			d.iText = PaLeft(p.Name(), 40);
			d.iExtraText.Zero();
			aPane->AddMenuItemL(d);
			}
		aPane->AddMenuItemL(other);
		TInt on = iSet.iSkin == KPaEpocSetting ? EPaCmdSkinEpoc : EPaCmdSkinStandard;
		for (TInt k = 0; k < n; k++)
			if (!(*iSkins)[k].CompareF(iSet.iSkin))
				on = EPaCmdSkin0 + k;
		aPane->SetItemButtonState(on, EEikMenuItemSymbolOn);
		break;
		}
	default:
		break;
		}
	}

void CPaAppUi::HandleCommandL(TInt aCommand)
	{
	switch (aCommand)
		{
	case EPaCmdOpenFile:
	case EPaCmdAddFile:
		{
		TFileName name;
		if (!AskFileL(name, aCommand == EPaCmdOpenFile ? R_PA_OPEN_TITLE : R_PA_ADD_TITLE))
			break;
		if (aCommand == EPaCmdOpenFile)
			OpenFileL(name);
		else
			{
			if (IsList(name))
				LoadListL(name, ETrue);
			else
				AddFileL(name);
			iListWin->SetSelected(iList->Count() - 1);
			iEikonEnv->InfoMsg(_L("Added"));
			}
		break;
		}
	case EPaCmdOpenFolder:
	case EPaCmdAddFolder:
		{
		TFileName name;
		if (!AskFileL(name, aCommand == EPaCmdOpenFolder ? R_PA_OPEN_FOLDER_TITLE : R_PA_ADD_FOLDER_TITLE))
			break;
		TParsePtrC p(name);
		TInt was = iList->Count();
		if (aCommand == EPaCmdOpenFolder)
			{
			iEngine->Close();
			iList->Reset();
			iTitles->Reset();
			iCurrent = 0;                        // (a folder with no sound files leaves an empty list)
			was = 0;
			}
		AddFolderL(p.DriveAndPath());
		if (aCommand == EPaCmdOpenFolder && iList->Count())
			{
			// from the file chosen, if it is a sound file
			TInt start = 0;
			for (TInt i = 0; i < iList->Count(); i++)
				if (!(*iList)[i].CompareF(name))
					start = i;
			iListWin->SetSelected(start);
			PlayIndexL(start);
			}
		else
			iListWin->SetSelected(was);
		iListWin->Compose();
		break;
		}
	case EPaCmdSavePlaylist:
		{
		if (iList->Count() == 0)
			{
			iEikonEnv->InfoMsg(_L("Nothing to save"));
			break;
			}
		TFileName name(_L("C:\\Documents\\Playlist.m3u"));
		CEikFileSaveAsDialog* d = new(ELeave) CEikFileSaveAsDialog(&name, NULL, R_PA_SAVE_TITLE);
		if (!d->ExecuteLD(R_EIK_DIALOG_FILE_SAVEAS))
			break;
		TParsePtrC p(name);
		if (p.Ext().Length() == 0)
			name.Append(_L(".m3u"));
		TRAPD(err, SaveListL(name));
		iEikonEnv->InfoMsg(err == KErrNone ? _L("Saved") : _L("Cannot save the playlist"));
		break;
		}
	case EPaCmdRemove:
		{
		TInt i = iListWin->Selected();
		if (iList->Count() == 0)
			{
			iEikonEnv->InfoMsg(_L("Nothing to remove"));
			break;
			}
		if (i == iCurrent)
			iEngine->Close();
		iList->Delete(i);
		iTitles->Delete(i);
		if (iCurrent > i)
			iCurrent--;
		if (iCurrent >= iList->Count())
			iCurrent = iList->Count() ? iList->Count() - 1 : 0;
		iListWin->SetSelected(i);
		iMain->Compose(ETrue);
		break;
		}
	case EPaCmdClear:
		if (iList->Count() == 0)
			{
			iEikonEnv->InfoMsg(_L("Nothing to clear"));
			break;
			}
		if (!iEikonEnv->QueryWinL(_L("Every file will be taken off the list"), _L("Clear the playlist?")))
			break;
		iEngine->Close();
		iList->Reset();
		iTitles->Reset();
		iCurrent = 0;
		iListWin->SetSelected(0);
		iMain->Compose(ETrue);
		break;
	case EPaCmdSort:
		{
		if (iList->Count() < 2)
			{
			iEikonEnv->InfoMsg(_L("Nothing to sort"));
			break;
			}
		// by name (a simple insertion sort: playlists are short), the playing one followed
		TFileName playing(iList->Count() ? (*iList)[iCurrent] : TPtrC());
		for (TInt i = 1; i < iList->Count(); i++)
			{
			TBuf<128> ni;
			EntryName(i, ni);
			TInt j = i;
			while (j > 0)
				{
				TBuf<128> nj;
				EntryName(j - 1, nj);
				if (nj.CompareF(ni) <= 0)
					break;
				j--;
				}
			if (j != i)
				{
				TFileName f((*iList)[i]);
				TBuf<128> t((*iTitles)[i]);
				iList->Delete(i);
				iTitles->Delete(i);
				iList->InsertL(j, f);
				iTitles->InsertL(j, t);
				}
			}
		for (TInt k = 0; k < iList->Count(); k++)
			if (!(*iList)[k].CompareF(playing))
				iCurrent = k;
		iListWin->SetSelected(iCurrent);
		iMain->Compose(ETrue);
		break;
		}
	case EPaCmdPlaylist:
		iSet.iPlaylist = !iSet.iPlaylist;
		iListWin->MakeVisible(iSet.iPlaylist && !iPinned);
		iMain->Compose(ETrue);
		if (iSet.iPlaylist)
			iListWin->Compose();
		break;
	case EEikCmdZoomIn:                          // the Zoom icons beside the screen:
	case EEikCmdZoomOut:                         // the two sizes, round
	case EPaCmdDouble:
		iSet.iDouble = !iSet.iDouble;
		iSet.iMainX = iSet.iMainY = iSet.iListX = iSet.iListY = -1;     // (the standard places again)
		LayoutWindowsL();
		break;
	case EPaCmdVisualiser:
		iSet.iVisMode = iSet.iVisMode == 2 ? 0 : 2;
		iMain->Compose(EFalse);
		break;
	case EPaCmdOnTop:
		iSet.iOnTop = !iSet.iOnTop;
		iEikonEnv->InfoMsg(iSet.iOnTop ? _L("PsiAmp stays over other programs") : _L("PsiAmp goes behind other programs"));
		break;
	case EPaCmdSkinStandard:
		SetSkinL(KNullDesC, ETrue);
		break;
	case EPaCmdSkinEpoc:
		SetSkinL(KPaEpocSetting, ETrue);
		break;
	case EPaCmdSkinOther:
		{
		TFileName name(iSet.iSkin.Length() ? TPtrC(iSet.iSkin) : TPtrC(_L("D:\\")));
		if (!AskFileL(name, R_PA_SKIN_TITLE))
			break;
		SetSkinL(name, ETrue);
		break;
		}
	case EPaCmdPlay:
		if (iEngine->IsOpen())
			{
			iEngine->Play();
			StartTicks();
			}
		else
			PlayIndexL(iListWin->Selected());
		break;
	case EPaCmdPause:
		if (TooSoon())
			break;
		if (iEngine->State() == CPaEngine::EPlaying)
			iEngine->Pause();
		else if (iEngine->State() == CPaEngine::EPaused)
			{
			iEngine->Play();
			StartTicks();
			}
		break;
	case EPaCmdStop:
		iEngine->Stop();
		break;
	case EPaCmdPrev:
		PrevL();
		break;
	case EPaCmdNext:
		NextL(ETrue);
		break;
	case EPaCmdPlaySelected:
		PlayIndexL(iListWin->Selected());
		break;
	case EPaCmdGoTo:
		GoToL();
		break;
	case EPaCmdShuffle:
		iSet.iShuffle = !iSet.iShuffle;
		iMain->Compose(ETrue);
		iEikonEnv->InfoMsg(iSet.iShuffle ? _L("Shuffle on") : _L("Shuffle off"));
		break;
	case EPaCmdRepeat:
		iSet.iRepeat = !iSet.iRepeat;
		iMain->Compose(ETrue);
		iEikonEnv->InfoMsg(iSet.iRepeat ? _L("Repeat on") : _L("Repeat off"));
		break;
	case EPaCmdPrefs:
		PrefsL();
		break;
	case EPaCmdSpeedTest:
		SpeedTestL();
		break;
	case EPaCmdAbout:
		AboutL();
		break;
	case EPaCmdHide:
		HideL();
		break;
	case EEikCmdExit:
		Exit();
		break;
	default:
		if (aCommand >= EPaCmdSkin0 && aCommand < EPaCmdSkin0 + iSkins->Count())
			{
			TFileName f((*iSkins)[aCommand - EPaCmdSkin0]);
			SetSkinL(f, ETrue);
			}
		break;
		}
	SaveSettings();
	}

void CPaAppUi::GoToL()
	{
	if (!iEngine->IsOpen() || iEngine->DurationMs() <= 0)
		{
		iEikonEnv->InfoMsg(_L("Nothing to go to"));
		return;
		}
	TInt ms = iEngine->PositionMs();
	CEikDialog* d = new(ELeave) CPaGoToDialog(ms, iEngine->DurationMs());
	if (d->ExecuteLD(R_PA_GOTO_DIALOG))
		iEngine->SeekMs(ms);
	}

void CPaAppUi::PrefsL()
	{
	TPaSettings s = iSet;
	CEikDialog* d = new(ELeave) CPaPrefsDialog(s);
	if (!d->ExecuteLD(R_PA_PREFS_DIALOG))
		return;
	TBool keys = s.iKeys != iSet.iKeys;
	iSet = s;
	if (keys)
		CaptureKeys();
	iMain->Compose(ETrue);
	}

// decodes the file in use (a minute of it at most) as fast as it can, and
// says how much of the processor playing it takes; also in the log
void CPaAppUi::SpeedTestL()
	{
	if (!iEngine->IsOpen())
		{
		iEikonEnv->InfoMsg(_L("Nothing to test - open a file first"));
		return;
		}
	TBool was = iEngine->State() == CPaEngine::EPlaying;
	if (was)
		iEngine->Pause();
	iEikonEnv->BusyMsgL(_L("Decoding…"), EHLeftVBottom, TTimeIntervalMicroSeconds32(0));
	TInt sound = 0, taken = 0;
	TInt err = iEngine->SpeedTest(60000, sound, taken);
	iEikonEnv->BusyMsgCancel();
	if (err != KErrNone || sound <= 0)
		{
		iEikonEnv->InfoMsg(_L("Cannot decode this file"));
		return;
		}
	TInt pc = (taken * 100 + sound / 2) / sound;
	TBuf<128> line;
	line.Format(_L("%d.%d s of sound in %d.%d s - %d%% of the processor"),
		sound / 1000, (sound % 1000) / 100, taken / 1000, (taken % 1000) / 100, pc);
	TBuf<320> log;
	TBuf<128> title(iEngine->Title());
	log.Format(_L("speed test: %S, %d kbps, %d kHz, %d ch: %S"), &title, iEngine->Kbps(), iEngine->Khz(), iEngine->Channels(), &line);
	Log(log);
	iEikonEnv->InfoWinL(_L("Speed test"), line);
	if (was)
		{
		iEngine->Play();
		StartTicks();
		}
	}

void CPaAppUi::AboutL()
	{
	CEikDialog* d = new(ELeave) CPaAboutDialog;
	d->ExecuteLD(R_PA_ABOUT_DIALOG);
	}

// ===========================================================================

EXPORT_C CApaApplication* NewApplication()
	{
	return new CPaApplication;
	}

GLDEF_C TInt E32Dll(TDllReason)
	{
	return KErrNone;
	}
