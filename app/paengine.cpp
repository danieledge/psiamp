// PAENGINE.CPP - PsiAmp's sound: the decoder (MP3 through Helix, WAV) and
// the engine that feeds the netBook's sound device.
//
// ESound.ldd plays A-law at 8000 samples a second (RDevSound; the SDK:
// "Playback is fixed at 8KHz"). A play request completes as soon as the
// driver has copied the chunk into its own buffer (iAlawBufferSize, which
// must be at least twice the biggest chunk), so the engine keeps one request
// out at a time, 2000 bytes (0.25 s) each, into an 8000-byte buffer.
//
// What is heard is timed by a clock (8000 samples a second from the first
// chunk): it drives the time, the position bar and the visualiser. (The
// driver's BytesPlayed is only logged, after the first requests: in the
// emulator it says 0.) The decoder runs in an idle object above the
// display's tick, a frame at a time, up to 2 s ahead of the device.
//
// The device is held only while playing (pause and stop let it go), so that
// alarms still sound. If it does not take a chunk within 3 s (the emulator's
// netBook sound never completes a play request), PsiAmp plays on silently,
// timed by the clock alone, and says so once.
//
// Starting, a second is decoded before the first chunk goes out: in the
// emulator, a driver run dry after its first chunk stopped the whole machine.

#include <e32std.h>
#include <e32math.h>
#include <eikenv.h>
#include "psiamp.h"
#include "paskinc.h"

// ===========================================================================
// The decoder
// ===========================================================================

CPaDecoder* CPaDecoder::NewL(RFs& aFs)
	{
	CPaDecoder* d = new(ELeave) CPaDecoder(aFs);
	CleanupStack::PushL(d);
	d->iIn = (TUint8*)User::AllocL(KIn);
	d->iPcm = (TInt16*)User::AllocL(1152 * 2 * 2);
	d->iOut = (TInt16*)User::AllocL(1200 * 2);
	CleanupStack::Pop();
	return d;
	}

CPaDecoder::~CPaDecoder()
	{
	Reset();
	User::Free(iIn);
	User::Free(iPcm);
	User::Free(iOut);
	}

void CPaDecoder::Reset()
	{
	if (iFileOpen)
		iFile.Close();
	iFileOpen = EFalse;
	if (iMp3)
		MP3FreeDecoder(iMp3);
	iMp3 = NULL;
	if (iRsOpen)
		pa_rs_close(&iRs);
	iRsOpen = EFalse;
	iInLen = iInPos = iOutLen = iOutPos = 0;
	iEof = EFalse;
	iWav = EFalse;
	iDurationMs = iKbps = iRate = iChans = iFrames = 0;
	iHaveToc = EFalse;
	iTitle.Zero();
	}

// the input topped up to the buffer's size (when it is under half full)
TInt CPaDecoder::Fill()
	{
	TInt left = iInLen - iInPos;
	if (iEof || left >= KIn / 2)
		return KErrNone;
	if (left > 0 && iInPos > 0)
		Mem::Copy(iIn, iIn + iInPos, left);
	iInLen = left;
	iInPos = 0;
	TInt want = KIn - iInLen;
	if (iReadPos + want > iDataEnd)
		want = iDataEnd - iReadPos;
	if (want <= 0)
		{
		iEof = ETrue;
		return KErrNone;
		}
	TPtr8 p(iIn + iInLen, want);
	TInt err = iFile.Read(iReadPos, p, want);
	if (err != KErrNone)
		return err;
	iInLen += p.Length();
	iReadPos += p.Length();
	if (p.Length() < want)
		iEof = ETrue;
	return KErrNone;
	}

static void Latin1(TDes& aOut, const char* aIn)
	{
	aOut.Zero();
	for (TInt i = 0; aIn[i] && aOut.Length() < aOut.MaxLength(); i++)
		aOut.Append(TChar((TUint8)aIn[i]));
	}

TInt CPaDecoder::Open(const TDesC& aName)
	{
	Reset();
	TInt err = iFile.Open(iFs, aName, EFileRead | EFileShareReadersOnly);
	if (err == KErrInUse)
		err = iFile.Open(iFs, aName, EFileRead | EFileShareAny);
	if (err != KErrNone)
		return err;
	iFileOpen = ETrue;
	iName = aName;
	iFile.Size(iFileSize);
	// the title: the file's name, until the tags say better
	TParsePtrC parse(aName);
	iTitle = PaLeft(parse.Name(), iTitle.MaxLength());
	TPtr8 head(iIn, KIn);
	err = iFile.Read(0, head, KIn);
	if (err != KErrNone)
		return err;
	if (head.Length() >= 12 && head[0] == 'R' && head[1] == 'I' && head[2] == 'F' && head[3] == 'F')
		return OpenWav();
	return OpenMp3();
	}

TInt CPaDecoder::OpenWav()
	{
	TInt len = Min(iFileSize, KIn);
	if (pa_wav_parse(iIn, len, &iWavInfo) != 0)
		return KErrNotSupported;
	iWav = ETrue;
	iRate = iWavInfo.rate;
	iChans = iWavInfo.chans;
	TInt block = iWavInfo.chans * iWavInfo.bits / 8;
	iDataStart = iWavInfo.dataStart;
	iDataEnd = iDataStart + iWavInfo.dataLen;
	if (iWavInfo.dataLen <= 0 || iWavInfo.dataLen > iFileSize - iDataStart)
		iDataEnd = iFileSize;
	iDataEnd -= (iDataEnd - iDataStart) % block;
	TInt frames = (iDataEnd - iDataStart) / block;
	iDurationMs = (frames / iRate) * 1000 + ((frames % iRate) * 1000) / iRate;
	iKbps = (iRate * iChans * iWavInfo.bits) / 1000;
	if (pa_rs_open(&iRs, iRate, 1152) != 0)
		return KErrNoMemory;
	iRsOpen = ETrue;
	iReadPos = iDataStart;
	iInLen = iInPos = 0;
	return KErrNone;
	}

TInt CPaDecoder::OpenMp3()
	{
	TInt len = Min(iFileSize, KIn);
	// the tags: ID3v2 at the start (its text from what is here), ID3v1 at the end
	TInt tag = pa_id3v2_len(iIn, len);
	char title[100], artist[100];
	title[0] = artist[0] = 0;
	if (tag)
		pa_id3v2_text(iIn, len, title, artist, sizeof(title));
	iDataStart = tag;
	iDataEnd = iFileSize;
	if (iFileSize >= 128 + tag)
		{
		TBuf8<128> v1;
		if (iFile.Read(iFileSize - 128, v1, 128) == KErrNone && v1.Length() == 128 && v1[0] == 'T' && v1[1] == 'A' && v1[2] == 'G')
			{
			iDataEnd = iFileSize - 128;
			if (!title[0])
				pa_id3v1_text(v1.Ptr(), title, artist, sizeof(title));
			}
		}
	if (title[0])
		{
		TBuf<100> t, a;
		Latin1(t, title);
		Latin1(a, artist);
		if (a.Length())
			{
			// (two long tags are longer than the title's room: USER 11)
			TBuf<210> both;
			both.Format(_L("%S - %S"), &a, &t);
			iTitle = PaLeft(both, iTitle.MaxLength());
			}
		else
			iTitle = t;
		}
	iMp3 = MP3InitDecoder();
	if (!iMp3)
		return KErrNoMemory;
	// nothing above 4.4 kHz decoded: the netBook plays 8,000 samples a second
	// (the result at 8 kHz is the full decode's to 76-84 dB: test/host)
	MP3SetBandLimit(iMp3, 4400);
	iReadPos = iDataStart;
	iInLen = iInPos = 0;
	TInt err = FirstFrame();
	if (err != KErrNone)
		return err;
	if (pa_rs_open(&iRs, iRate, 1152) != 0)
		return KErrNoMemory;
	iRsOpen = ETrue;
	return KErrNone;
	}

// the first frame's details, and a Xing (or Info) header's frame count and
// table for the length and seeking of a VBR file
TInt CPaDecoder::FirstFrame()
	{
	TInt err = Fill();
	if (err != KErrNone)
		return err;
	MP3FrameInfo fi;
	for (TInt tries = 0; tries < 64; tries++)
		{
		TInt left = iInLen - iInPos;
		TInt off = MP3FindSyncWord(iIn + iInPos, left);
		if (off < 0)
			return KErrCorrupt;
		iInPos += off;
		if (MP3GetNextFrameInfo(iMp3, &fi, iIn + iInPos) == ERR_MP3_NONE && fi.layer == 3 && fi.bitrate > 0)
			break;
		iInPos++;
		fi.samprate = 0;
		}
	if (fi.samprate <= 0)
		return KErrCorrupt;
	iRate = fi.samprate;
	iChans = fi.nChans;
	iKbps = fi.bitrate / 1000;
	TInt firstAt = iDataStart + iInPos;
	// Xing: after the header (4) and the side information
	const TUint8* h = iIn + iInPos;
	TInt mpeg1 = (h[1] & 0x18) == 0x18;
	TInt side = mpeg1 ? (iChans == 1 ? 17 : 32) : (iChans == 1 ? 9 : 17);
	const TUint8* x = h + 4 + side;
	if (x + 120 <= iIn + iInLen &&
		((x[0] == 'X' && x[1] == 'i' && x[2] == 'n' && x[3] == 'g') || (x[0] == 'I' && x[1] == 'n' && x[2] == 'f' && x[3] == 'o')))
		{
		TInt flags = x[7];
		const TUint8* p = x + 8;
		if (flags & 1)
			{
			iFrames = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
			p += 4;
			}
		if (flags & 2)
			p += 4;
		if (flags & 4)
			{
			Mem::Copy(iToc, p, 100);
			iHaveToc = ETrue;
			}
		}
	TInt perFrame = mpeg1 ? 1152 : 576;
	if (iFrames > 0)
		{
		// frames * perFrame / rate in ms, without overflow
		TInt samples = iFrames * perFrame;       // (under 2^31 for any real file)
		iDurationMs = (samples / iRate) * 1000 + ((samples % iRate) * 1000) / iRate;
		TInt bytes = iDataEnd - firstAt;
		if (iDurationMs > 0)
			iKbps = (bytes / iDurationMs) * 8 + ((bytes % iDurationMs) * 8) / iDurationMs;
		}
	else if (iKbps > 0)
		iDurationMs = (iDataEnd - firstAt) / (iKbps / 8);
	iDataStart = firstAt;
	return KErrNone;
	}

TInt CPaDecoder::Seek(TInt aMs)
	{
	if (aMs < 0)
		aMs = 0;
	if (iDurationMs > 0 && aMs > iDurationMs)
		aMs = iDurationMs;
	TInt byte;
	if (iWav)
		{
		TInt block = iWavInfo.chans * iWavInfo.bits / 8;
		TInt frame = (aMs / 1000) * iRate + ((aMs % 1000) * iRate) / 1000;
		byte = frame * block;
		}
	else if (iHaveToc && iDurationMs > 0)
		{
		// the table: 100 points, each a 256th of the file
		TInt pc = aMs / (iDurationMs / 100 + 1);
		if (pc > 99)
			pc = 99;
		byte = ((iDataEnd - iDataStart) / 256) * iToc[pc];
		}
	else
		byte = aMs * (iKbps / 8);
	if (byte > iDataEnd - iDataStart)
		byte = iDataEnd - iDataStart;
	iReadPos = iDataStart + byte;
	iInLen = iInPos = iOutLen = iOutPos = 0;
	iEof = EFalse;
	if (iRsOpen)
		pa_rs_reset(&iRs);
	return aMs;
	}

TInt CPaDecoder::DecodeMp3Frame()
	{
	MP3FrameInfo fi;
	for (TInt guard = 0; guard < 1000; guard++)
		{
		TInt err = Fill();
		if (err != KErrNone)
			return err;
		TInt left = iInLen - iInPos;
		if (left <= 4)
			return 0;
		TInt off = MP3FindSyncWord(iIn + iInPos, left);
		if (off < 0)
			{
			if (iEof)
				return 0;
			iInPos = iInLen - 3;
			continue;
			}
		iInPos += off;
		left -= off;
		unsigned char* p = iIn + iInPos;
		int bl = left;
		err = MP3Decode(iMp3, &p, &bl, iPcm, 0);
		TInt used = left - bl;
		iInPos += used;
		if (err == ERR_MP3_INDATA_UNDERFLOW)
			{
			if (iEof)
				return 0;
			if (iInPos > 0 || iInLen < KIn)
				{
				// read more, even if half is here
				TInt rest = iInLen - iInPos;
				Mem::Copy(iIn, iIn + iInPos, rest);
				iInLen = rest;
				iInPos = 0;
				TInt want = Min(KIn - iInLen, iDataEnd - iReadPos);
				if (want <= 0)
					{
					iEof = ETrue;
					return 0;
					}
				TPtr8 q(iIn + iInLen, want);
				if (iFile.Read(iReadPos, q, want) != KErrNone)
					return KErrCorrupt;
				iInLen += q.Length();
				iReadPos += q.Length();
				if (q.Length() == 0)
					iEof = ETrue;
				}
			continue;
			}
		if (err == ERR_MP3_MAINDATA_UNDERFLOW)
			continue;                            // (after a seek: the reservoir fills)
		if (err != ERR_MP3_NONE)
			{
			if (used == 0)
				iInPos++;
			continue;
			}
		MP3GetLastFrameInfo(iMp3, &fi);
		if (fi.samprate != iRate && fi.samprate > 0)
			{
			// a new rate in mid-stream (rare): the resampler made again
			pa_rs_close(&iRs);
			iRsOpen = EFalse;
			iRate = fi.samprate;
			if (pa_rs_open(&iRs, iRate, 1152) != 0)
				return KErrNoMemory;
			iRsOpen = ETrue;
			}
		TInt frames = fi.outputSamps / fi.nChans;
		pa_downmix(iPcm, frames, fi.nChans);
		TInt n = pa_rs_run(&iRs, iPcm, frames, iOut);
		iOutLen = n;
		iOutPos = 0;
		if (n > 0)
			return n;
		}
	return 0;
	}

TInt CPaDecoder::DecodeWav()
	{
	TInt block = iWavInfo.chans * iWavInfo.bits / 8;
	TInt frames = 1152;
	TInt err = Fill();
	if (err != KErrNone)
		return err;
	TInt avail = (iInLen - iInPos) / block;
	if (avail <= 0)
		return 0;
	if (frames > avail)
		frames = avail;
	const TUint8* p = iIn + iInPos;
	TInt k = frames * iWavInfo.chans;
	for (TInt i = 0; i < k; i++)
		{
		TInt s;
		if (iWavInfo.format == 1 && iWavInfo.bits == 16)
			s = (TInt16)(p[2 * i] | (p[2 * i + 1] << 8));
		else if (iWavInfo.format == 1)
			s = (p[i] - 128) << 8;
		else if (iWavInfo.format == 6)
			s = pa_unalaw(p[i]);
		else
			{
			// mu-law
			TInt u = ~p[i] & 0xff;
			TInt t = ((u & 0x0f) << 3) + 0x84;
			t <<= (u & 0x70) >> 4;
			s = (u & 0x80) ? (0x84 - t) : (t - 0x84);
			}
		iPcm[i] = (TInt16)s;
		}
	iInPos += frames * block;
	pa_downmix(iPcm, frames, iWavInfo.chans);
	iOutLen = pa_rs_run(&iRs, iPcm, frames, iOut);
	iOutPos = 0;
	return iOutLen > 0 ? iOutLen : 1;            // (a few samples may give none out yet)
	}

TInt CPaDecoder::Decode(TInt16* aOut, TInt aMax)
	{
	if (!iFileOpen)
		return KErrNotReady;
	while (iOutPos >= iOutLen)
		{
		TInt n = iWav ? DecodeWav() : DecodeMp3Frame();
		if (n <= 0)
			return n;
		}
	TInt n = Min(aMax, iOutLen - iOutPos);
	Mem::Copy(aOut, iOut + iOutPos, n * 2);
	iOutPos += n;
	return n;
	}

// ===========================================================================
// The engine
// ===========================================================================

const TInt KPaWatchdog = 3000000;                // a play request not done in this: no sound

CPaEngine::CPaEngine(RFs& aFs, MPaEngineObserver& aObserver)
	: CActive(EPriorityHigh), iFs(aFs), iObserver(aObserver), iVolume(80)
	{
	}

CPaEngine* CPaEngine::NewL(RFs& aFs, MPaEngineObserver& aObserver)
	{
	CPaEngine* e = new(ELeave) CPaEngine(aFs, aObserver);
	CleanupStack::PushL(e);
	e->ConstructL();
	CleanupStack::Pop();
	return e;
	}

void CPaEngine::ConstructL()
	{
	CActiveScheduler::Add(this);
	iDec = CPaDecoder::NewL(iFs);
	iRingA = (TUint8*)User::AllocL(KRing);
	iRingS = (TInt16*)User::AllocL(KRing * 2);
	Mem::FillZ(iRingS, KRing * 2);
	Mem::Fill(iRingA, KRing, TChar(KAlawSilence));
	iTimer = CPeriodic::NewL(CActive::EPriorityStandard);
	// the decoder: above the display's tick (EPriorityLow), so drawing never
	// starves it; it stops by itself 2 s ahead of the device
	iIdle = CIdle::NewL(CActive::EPriorityStandard - 1);
	}

CPaEngine::~CPaEngine()
	{
	SafeCancel();
	CloseDevice();
	delete iIdle;
	delete iTimer;
	delete iDec;
	User::Free(iRingA);
	User::Free(iRingS);
	}

TInt CPaEngine::OpenDevice()
	{
	if (iDevOpen || iSilent)
		return KErrNone;
	TInt err = iDev.Open();
	TBuf<160> m;
	if (err != KErrNone)
		{
		m.Format(_L("sound device: open %d"), err);
		iObserver.EngineLog(m);
		return err;
		}
	iDevOpen = ETrue;
	TSoundCaps caps;
	iDev.Caps(caps);
	iMaxVolume = (caps().iVolume & KCapsVolumeByValue) ? caps().iMaxVolume : -1;
	SetVolume(iVolume);
	err = iDev.PreparePlayAlawBuffer();
	iDev.ResetBytesPlayed();
	TSoundConfig cfg;
	iDev.Config(cfg);
	m.Format(_L("sound device: open; caps service %x volume %x max volume %d max frequency %d; prepare %d; buffer %d, volume %d/%d"),
		caps().iService, caps().iVolume, caps().iMaxVolume, caps().iMaxFrequency, err, cfg().iAlawBufferSize, cfg().iVolume, cfg().iVolumeValue);
	iObserver.EngineLog(m);
	iRequests = 0;
	return KErrNone;
	}

// the request cancelled without waiting for ever: the emulator's netBook
// sound never completes a play request, not even a cancelled one, and
// CActive::Cancel waits for the completion. The driver is given 50 ms;
// then the request is completed here
void CPaEngine::SafeCancel()
	{
	if (!IsActive())
		return;
	if (iDevOpen)
		{
		iDev.PlayCancel();
		}
	for (TInt i = 0; i < 5 && iStatus == KRequestPending; i++)
		User::After(10000);
	if (iStatus == KRequestPending)
		{
		TRequestStatus* s = &iStatus;
		User::RequestComplete(s, KErrCancel);
		}
	Cancel();
	}

void CPaEngine::CloseDevice()
	{
	if (!iDevOpen)
		return;
	SafeCancel();
	iDev.FlushPlayBuffer();
	iDev.Close();
	iDevOpen = EFalse;
	}

void CPaEngine::SetVolume(TInt aPercent)
	{
	iVolume = aPercent < 0 ? 0 : aPercent > 100 ? 100 : aPercent;
	if (!iDevOpen)
		return;
	TSoundConfig cfg;
	iDev.Config(cfg);
	if (iMaxVolume > 0)
		{
		cfg().iVolume = EVolumeByValue;
		cfg().iVolumeValue = (iVolume * iMaxVolume + 50) / 100;
		}
	else
		cfg().iVolume = iVolume == 0 ? EVolumeNone : iVolume < 34 ? EVolumeSoft : iVolume < 67 ? EVolumeMedium : EVolumeLoud;
	cfg().iAlawBufferSize = KDevBuffer;
	iDev.SetConfig(cfg);
	}

TInt CPaEngine::Open(const TDesC& aName, TInt aStartMs)
	{
	Close();
	TInt err = iDec->Open(aName);
	if (err != KErrNone)
		{
		iDec->Seek(0);
		return err;
		}
	iOpen = ETrue;
	iTitle = iDec->Title();
	iDurationMs = iDec->DurationMs();
	iKbps = iDec->Kbps();
	iRate = iDec->Rate();
	iChans = iDec->Channels();
	TInt ms = aStartMs > 0 ? iDec->Seek(aStartMs) : 0;
	Restart(ms * 8);
	iObserver.EngineChanged();
	return KErrNone;
	}

void CPaEngine::Close()
	{
	Stop();
	iOpen = EFalse;
	iTitle.Zero();
	iDurationMs = iKbps = iRate = iChans = 0;
	}

// decoding and the device from aSample on (the ring emptied)
void CPaEngine::Restart(TInt aSample)
	{
	SafeCancel();
	if (iDevOpen)
		{
		iDev.FlushPlayBuffer();
		iDev.ResetBytesPlayed();
		}
	iWritten = iSubmitted = iClockSample = aSample;
	iClockRunning = EFalse;
	iEnded = EFalse;
	}

void CPaEngine::Play()
	{
	if (!iOpen || iState == EPlaying)
		return;
	if (iEnded && iSubmitted >= iWritten)
		{
		// played to the end: from the start
		iDec->Seek(0);
		Restart(0);
		}
	TInt err = OpenDevice();
	if (err != KErrNone && !iSilent)
		{
		if (err == KErrInUse)
			iObserver.EngineMessage(_L("Sound in use by another program"));
		else
			{
			TBuf<48> m;
			m.Format(_L("Cannot open the sound device (%d)"), err);
			iObserver.EngineMessage(m);
			}
		return;
		}
	// what the device had and what was not heard: again from what was heard
	TInt heard = iClockSample;
	if (heard < iWritten - (KRing - KAhead))
		{
		iDec->Seek(heard / 8);
		Restart(heard);
		}
	else
		{
		iSubmitted = heard;
		iClockRunning = EFalse;
		if (iDevOpen)
			iDev.ResetBytesPlayed();
		}
	iState = EPlaying;
	iTimer->Cancel();
	iTimer->Start(0, 100000, TCallBack(Tick, this));
	if (!iIdle->IsActive())
		iIdle->Start(TCallBack(DecodeStep, this));
	iObserver.EngineChanged();
	}

void CPaEngine::Pause()
	{
	if (iState != EPlaying)
		return;
	iClockSample = Played();
	iClockRunning = EFalse;
	iSubmitted = iClockSample;
	CloseDevice();
	iTimer->Cancel();
	iState = EPaused;
	iObserver.EngineChanged();
	}

void CPaEngine::Stop()
	{
	CloseDevice();
	iTimer->Cancel();
	iIdle->Cancel();
	iState = EStopped;
	if (iOpen)
		{
		iDec->Seek(0);
		Restart(0);
		}
	iObserver.EngineChanged();
	}

void CPaEngine::SeekMs(TInt aMs)
	{
	if (!iOpen)
		return;
	TInt ms = iDec->Seek(aMs);
	Restart(ms * 8);
	if (iState == EPlaying && !iIdle->IsActive())
		iIdle->Start(TCallBack(DecodeStep, this));
	iObserver.EngineChanged();
	}

TInt CPaEngine::Played() const
	{
	if (!iClockRunning)
		return iClockSample;
	TInt p;
	TTime now;
	now.UniversalTime();
	TInt64 us = now.MicroSecondsFrom(iClockTime).Int64();
	TInt64 s = us / TInt64(125);
	p = iClockSample + s.GetTInt();
	return p < iSubmitted ? p : iSubmitted;
	}

TInt CPaEngine::PositionMs() const
	{
	return Played() / 8;
	}

void CPaEngine::Feed()
	{
	if (iState != EPlaying || IsActive() || !iDevOpen || iSilent)
		return;
	TInt avail = iWritten - iSubmitted;
	// starting (or after an underrun): a second decoded first, the driver's
	// buffer, so the first chunks go out together and it does not run dry
	if (!iClockRunning && !iEnded && avail < KDevBuffer)
		return;
	if (avail < KChunk && !(iEnded && avail > 0))
		return;
	TInt n = Min(avail, (TInt)KChunk);
	iChunk.SetLength(n);
	TUint8* d = (TUint8*)iChunk.Ptr();
	TInt at = iSubmitted & (KRing - 1);
	TInt first = Min(n, KRing - at);
	Mem::Copy(d, iRingA + at, first);
	if (first < n)
		Mem::Copy(d + first, iRingA, n - first);
	if (!iClockRunning)
		{
		iClockTime.UniversalTime();
		iClockRunning = ETrue;
		// (BytesPlayed counts from here)
		iClockSample = iSubmitted;
		iDev.ResetBytesPlayed();
		}
	if (iRequests < 8)
		{
		TBuf<80> m;
		m.Format(_L("play %d: %d bytes from %d, written %d"), iRequests, n, iSubmitted, iWritten);
		iObserver.EngineLog(m);
		}
	iRequests++;
	iDev.PlayAlawData(iStatus, iChunk);
	iRequestTime.UniversalTime();
	iSubmitted += n;
	SetActive();
	}

void CPaEngine::RunL()
	{
	if (iRequests <= 8)
		{
		TTime now;
		now.UniversalTime();
		TBuf<80> m;
		m.Format(_L("play %d done: %d after %d ms; bytes played %d"), iRequests - 1, iStatus.Int(),
			(now.MicroSecondsFrom(iRequestTime).Int64() / TInt64(1000)).GetTInt(), iDevOpen ? iDev.BytesPlayed() : -1);
		iObserver.EngineLog(m);
		}
	if (iStatus.Int() != KErrNone && iStatus.Int() != KErrCancel)
		{
		TBuf<48> m;
		m.Format(_L("Sound device: %d"), iStatus.Int());
		iObserver.EngineMessage(m);
		}
	Feed();
	if (iState == EPlaying && !iEnded && !iIdle->IsActive())
		iIdle->Start(TCallBack(DecodeStep, this));
	}

void CPaEngine::DoCancel()
	{
	// (SafeCancel has cancelled it, and seen it complete)
	}

void CPaEngine::GoSilent()
	{
	SafeCancel();
	if (iDevOpen)
		{
		// flushed, and left open: a device that does not take sound may not
		// close either (the emulator's: RDevSound::Close never returns)
		iDev.FlushPlayBuffer();
		iDevOpen = EFalse;
		}
	iSilent = ETrue;
	iObserver.EngineLog(_L("sound device: a play request not done in 3 s - silent from here"));
	if (!iSilentSaid)
		{
		iSilentSaid = ETrue;
		iObserver.EngineMessage(_L("No sound - the sound device does not answer"));
		}
	}

TInt CPaEngine::DecodeStep(TAny* aSelf)
	{
	return ((CPaEngine*)aSelf)->DoDecodeStep();
	}

// one frame's worth into the ring; ETrue while there is room for more
TInt CPaEngine::DoDecodeStep()
	{
	if (!iOpen || iEnded || iState != EPlaying)
		return EFalse;
	if (iWritten - iSubmitted >= KAhead)
		return EFalse;
	TInt16 buf[600];
	TInt n = iDec->Decode(buf, 600);
	if (n <= 0)
		{
		iEnded = ETrue;
		if (n < 0)
			iObserver.EngineMessage(_L("Cannot read the rest of this file"));
		Feed();
		return EFalse;
		}
	TInt at = iWritten & (KRing - 1);
	TInt first = Min(n, KRing - at);
	pa_to_alaw(buf, first, iRingA + at, 256);
	Mem::Copy(iRingS + at, buf, first * 2);
	if (first < n)
		{
		pa_to_alaw(buf + first, n - first, iRingA, 256);
		Mem::Copy(iRingS, buf + first, (n - first) * 2);
		}
	iWritten += n;
	Feed();
	return iWritten - iSubmitted < KAhead;
	}

TInt CPaEngine::Tick(TAny* aSelf)
	{
	((CPaEngine*)aSelf)->DoTick();
	return ETrue;
	}

void CPaEngine::DoTick()
	{
	if (iState != EPlaying)
		return;
	TTime now;
	now.UniversalTime();
	// the watchdog: a request the device does not take
	if (IsActive() && now.MicroSecondsFrom(iRequestTime) > TTimeIntervalMicroSeconds(TInt64(KPaWatchdog)))
		{
		TInt heard = Played();
		GoSilent();
		iSubmitted = heard;
		iClockSample = heard;
		iClockTime = now;
		iClockRunning = ETrue;
		}
	if (iSilent)
		{
		// no device: the clock alone, with a device's second of buffer
		if (!iClockRunning && iWritten > iSubmitted)
			{
			iClockTime = now;
			iClockSample = iSubmitted;
			iClockRunning = ETrue;
			}
		TInt target = Played() + KDevBuffer;
		if (iSubmitted < target)
			iSubmitted = Min(target, iWritten);
		}
	else
		Feed();
	TInt heard = Played();
	if (iClockRunning && heard >= iSubmitted && !iEnded && iWritten <= iSubmitted)
		{
		// an underrun (the decoder fell behind): the clock waits for sound
		iClockSample = heard;
		iClockRunning = EFalse;
		}
	if (!iEnded && !iIdle->IsActive() && iWritten - iSubmitted < KAhead)
		iIdle->Start(TCallBack(DecodeStep, this));
	if (iEnded && iSubmitted >= iWritten && heard >= iWritten)
		{
		// all heard: the end of the track
		iClockSample = iWritten;
		iClockRunning = EFalse;
		CloseDevice();
		iTimer->Cancel();
		iState = EStopped;
		iObserver.EngineTrackEnded();
		}
	}

void CPaEngine::Vis(TUint8* aBars, TUint8* aScope) const
	{
	TInt16 s[128];
	TInt heard = Played();
	if (heard > iWritten)
		heard = iWritten;
	for (TInt i = 0; i < 128; i++)
		{
		TInt k = heard - 128 + i;
		s[i] = (k >= 0 && k >= iWritten - KRing) ? iRingS[k & (KRing - 1)] : 0;
		}
	if (aBars)
		pa_vis_bars(s, aBars);
	if (aScope)
		pa_vis_scope(s, aScope);
	}

TInt CPaEngine::SpeedTest(TInt aMaxMs, TInt& aSoundMs, TInt& aTakenMs)
	{
	aSoundMs = aTakenMs = 0;
	if (!iOpen)
		return KErrNotReady;
	CPaDecoder* d = NULL;
	TRAPD(err, d = CPaDecoder::NewL(iFs));
	if (err != KErrNone)
		return err;
	TFileName name;
	name = iDec->iName;
	err = d->Open(name);
	if (err != KErrNone)
		{
		delete d;
		return err;
		}
	TInt16 buf[600];
	TUint8 al[600];
	TInt samples = 0;
	TUint t0 = User::TickCount();
	TTime start;
	start.UniversalTime();
	while (samples < aMaxMs * 8)
		{
		TInt n = d->Decode(buf, 600);
		if (n <= 0)
			break;
		pa_to_alaw(buf, n, al, 256);
		samples += n;
		}
	TTime end;
	end.UniversalTime();
	(void)t0;
	aSoundMs = samples / 8;
	aTakenMs = (end.MicroSecondsFrom(start).Int64() / TInt64(1000)).GetTInt();
	delete d;
	return KErrNone;
	}
