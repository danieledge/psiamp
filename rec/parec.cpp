// PAREC.CPP - PsiAmp.mdl: says which files are MP3 and WAV sound and M3U
// playlists, so that a tap on one on the System screen opens it in PsiAmp
// (whose .aif takes these types). In \System\Recogs, where the recogniser
// server loads every .mdl at start-up.
//
// MP3 by an ID3v2 tag or a frame header at the start (certain with a second
// frame header where the first says it ends), else by its ending (.mp3:
// probable: an MP3 may start with anything); WAV by its RIFF/WAVE header;
// M3U by "#EXTM3U" or its ending; a skin (.wsz, a zip) by its ending and
// the zip's header.

#include <e32std.h>
#include <apmrec.h>
#include <apmstd.h>
#include <f32file.h>

const TUid KUidPaRec = { 0x01000AFB };

_LIT8(KPaMp3, "audio/mpeg");
_LIT8(KPaWav, "audio/x-wav");
_LIT8(KPaM3u, "audio/x-mpegurl");
_LIT8(KPaWsz, "interface/x-winamp-skin");

class CPaRecognizer : public CApaDataRecognizerType
	{
public:
	CPaRecognizer() : CApaDataRecognizerType(KUidPaRec, ENormal) { iCountDataTypes = 4; }
	TUint PreferredBufSize() { return 4096; }
	TDataType SupportedDataTypeL(TInt aIndex) const;
private:
	void DoRecognizeL(const TDesC& aName, const TDesC8& aBuffer);
	void Found(const TDesC8& aType, TInt aConfidence);
	};

TDataType CPaRecognizer::SupportedDataTypeL(TInt aIndex) const
	{
	switch (aIndex)
		{
	case 0: return TDataType(KPaMp3);
	case 1: return TDataType(KPaWav);
	case 2: return TDataType(KPaM3u);
	default: return TDataType(KPaWsz);
		}
	}

void CPaRecognizer::Found(const TDesC8& aType, TInt aConfidence)
	{
	iDataType = TDataType(aType);
	iConfidence = aConfidence;
	}

// the length of the MPEG audio layer 3 frame whose header is at h, else 0
static TInt FrameLen(const TUint8* h)
	{
	if (h[0] != 0xff || (h[1] & 0xe0) != 0xe0)
		return 0;
	TInt ver = (h[1] >> 3) & 3;            // 3 MPEG1, 2 MPEG2, 0 MPEG2.5
	TInt layer = (h[1] >> 1) & 3;          // 1: layer 3
	TInt bri = h[2] >> 4, sri = (h[2] >> 2) & 3, pad = (h[2] >> 1) & 1;
	if (ver == 1 || layer != 1 || bri == 0 || bri == 15 || sri == 3)
		return 0;
	static const TInt16 br1[15] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
	static const TInt16 br2[15] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 };
	static const TInt sr[3] = { 44100, 48000, 32000 };
	TInt rate = sr[sri] >> (ver == 3 ? 0 : ver == 2 ? 1 : 2);
	TInt kbps = ver == 3 ? br1[bri] : br2[bri];
	return (ver == 3 ? 144000 : 72000) * kbps / rate + pad;
	}

void CPaRecognizer::DoRecognizeL(const TDesC& aName, const TDesC8& aBuffer)
	{
	iConfidence = ENotRecognized;
	const TUint8* h = aBuffer.Ptr();
	TInt n = aBuffer.Length();
	TParsePtrC p(aName);
	TPtrC ext = p.Ext();
	TBool mp3Name = !ext.CompareF(_L(".mp3"));
	// a classic player skin: a zip named .wsz
	if (!ext.CompareF(_L(".wsz")) && (n < 4 || (h[0] == 'P' && h[1] == 'K' && h[2] == 3 && h[3] == 4)))
		{ Found(KPaWsz, n < 4 ? EProbable : ECertain); return; }
	if (n >= 12 && h[0] == 'R' && h[1] == 'I' && h[2] == 'F' && h[3] == 'F' && h[8] == 'W' && h[9] == 'A' && h[10] == 'V' && h[11] == 'E')
		{ Found(KPaWav, ECertain); return; }
	if (n >= 7 && h[0] == '#' && h[1] == 'E' && h[2] == 'X' && h[3] == 'T' && h[4] == 'M' && h[5] == '3' && h[6] == 'U')
		{ Found(KPaM3u, ECertain); return; }
	if (n >= 10 && h[0] == 'I' && h[1] == 'D' && h[2] == '3' && h[3] < 5 && !((h[6] | h[7] | h[8] | h[9]) & 0x80))
		{ Found(KPaMp3, mp3Name ? ECertain : EProbable); return; }
	if (n >= 4)
		{
		TInt len = FrameLen(h);
		if (len > 0 && len + 4 <= n && FrameLen(h + len) > 0)
			{ Found(KPaMp3, ECertain); return; }
		if (len > 0 && mp3Name)
			{ Found(KPaMp3, ECertain); return; }
		}
	// the name, with no bytes to go on (or bytes that say nothing)
	if (mp3Name)
		Found(KPaMp3, EProbable);
	else if (!ext.CompareF(_L(".wav")) && n < 12)
		Found(KPaWav, EProbable);
	else if (!ext.CompareF(_L(".m3u")))
		Found(KPaM3u, EProbable);
	}

// the recogniser server's entry point: ordinal 1
EXPORT_C CApaDataRecognizerType* CreateRecognizer()
	{
	return new CPaRecognizer;
	}

GLDEF_C TInt E32Dll(TDllReason)
	{
	return KErrNone;
	}
