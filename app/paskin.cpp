// PASKIN.CPP - a classic skin (.wsz) in EPOC's 256 colours, and the face the
// windows are drawn from (a byte a pixel, shown at 1x or 2x).
#include <e32std.h>
#include <f32file.h>
#include <fbs.h>
#include "psiamp.h"
#include "paskinc.h"
#include <eikenv.h>
#include <eikcolor.h>

// the files, in TSheet's order; balance.bmp falls back to volume.bmp, as in
// the players these skins were made for
static const char* const KPaSheetName[CPaSkin::ESheets] =
	{
	"main.bmp", "titlebar.bmp", "cbuttons.bmp", "numbers.bmp", "nums_ex.bmp", "text.bmp",
	"playpaus.bmp", "monoster.bmp", "posbar.bmp", "volume.bmp", "balance.bmp", "shufrep.bmp", "pledit.bmp"
	};

// EPOC's key colours (pkg/mkskin.py's K: (1, 2, 10 * (n + 1))), each a role
// taken from EIKON's logical colours, which the Coda theme in use sets
static TRgb Mix(TRgb aA, TRgb aB, TInt aNum, TInt aDen)
	{
	return TRgb((aA.Red() * (aDen - aNum) + aB.Red() * aNum) / aDen,
		(aA.Green() * (aDen - aNum) + aB.Green() * aNum) / aDen,
		(aA.Blue() * (aDen - aNum) + aB.Blue() * aNum) / aDen);
	}

static void ThemeKeys(TRgb* aTo)
	{
	CEikonEnv* env = CEikonEnv::Static();
	TRgb face = env->Color(EEikColorDialogBackground);
	aTo[0] = face;
	aTo[1] = Mix(face, KRgbWhite, 2, 3);                    // light
	aTo[2] = Mix(face, KRgbBlack, 1, 3);                    // shadow
	aTo[3] = env->Color(EEikColorButtonText);               // the edge
	aTo[4] = env->Color(EEikColorDialogTitle);
	aTo[5] = env->Color(EEikColorDialogTitleText);
	aTo[6] = env->Color(EEikColorButtonFaceClear);
	aTo[7] = env->Color(EEikColorButtonText);
	aTo[8] = env->Color(EEikColorButtonFaceSetPressed);
	aTo[9] = env->Color(EEikColorDialogText);
	aTo[10] = env->Color(EEikColorControlHighlightBackground);
	aTo[11] = env->Color(EEikColorControlBackground);
	}

CPaSkin* CPaSkin::NewL()
	{
	CPaSkin* s = new(ELeave) CPaSkin;
	for (TInt i = 0; i < 4096; i++)
		{
		TInt r = (i >> 8) & 15, g = (i >> 4) & 15, b = i & 15;
		s->iLut[i] = (TUint8)TRgb(r * 17, g * 17, b * 17).Color256();
		}
	// the standard colours, if the skin has no text files
	static const TUint8 vis[24 * 3] = {
		0, 0, 0, 51, 51, 51, 255, 51, 0, 255, 51, 0, 255, 102, 0, 255, 102, 0, 255, 204, 0, 255, 204, 0,
		204, 255, 0, 204, 255, 0, 153, 255, 0, 102, 255, 0, 51, 255, 0, 0, 255, 0, 0, 204, 0, 0, 204, 0,
		0, 153, 0, 0, 153, 0, 0, 255, 0, 0, 204, 0, 0, 153, 0, 0, 102, 0, 0, 51, 0, 255, 255, 255 };
	for (TInt v = 0; v < 24; v++)
		s->iVis[v] = s->Index(vis[v * 3], vis[v * 3 + 1], vis[v * 3 + 2]);
	s->iListRgb[0] = TRgb(0, 255, 0);
	s->iListRgb[1] = TRgb(255, 255, 255);
	s->iListRgb[2] = TRgb(0, 0, 0);
	s->iListRgb[3] = TRgb(0, 102, 102);
	return s;
	}

CPaSkin::~CPaSkin()
	{
	Free();
	}

void CPaSkin::Free()
	{
	for (TInt i = 0; i < ESheets; i++)
		{
		User::Free(iSheet[i].iPix);
		iSheet[i].iPix = NULL;
		iSheet[i].iW = iSheet[i].iH = 0;
		}
	}

void CPaSkin::CopyFrom(const CPaSkin& aOther, TInt aSheet, TInt aTo)
	{
	const TPaSheet& o = aOther.iSheet[aSheet];
	if (!o.iPix)
		return;
	TUint8* p = (TUint8*)User::Alloc(o.iW * o.iH);
	if (!p)
		return;
	Mem::Copy(p, o.iPix, o.iW * o.iH);
	User::Free(iSheet[aTo].iPix);
	iSheet[aTo].iPix = p;
	iSheet[aTo].iW = o.iW;
	iSheet[aTo].iH = o.iH;
	}

TInt CPaSkin::LoadSheet(const TUint8* aZip, TInt aLen, const char* aName, TInt aSheet, TInt /*aW*/, TInt /*aH*/)
	{
	PaZipEntry e;
	if (pa_zip_find(aZip, aLen, aName, &e) != 0)
		return KErrNotFound;
	unsigned char* bmp = pa_zip_read(aZip, aLen, &e);
	if (!bmp)
		return KErrCorrupt;
	int w, h;
	TInt err = KErrCorrupt;
	if (pa_bmp_size(bmp, e.usize, &w, &h) == 0)
		{
		unsigned char* rgb = (unsigned char*)User::Alloc(w * h * 3);
		TUint8* pix = (TUint8*)User::Alloc(w * h);
		if (rgb && pix && pa_bmp_decode(bmp, e.usize, rgb) == 0)
			{
			for (TInt i = 0; i < w * h; i++)
				{
				const unsigned char* c = rgb + 3 * i;
				if (iThemed && c[0] == 1 && c[1] == 2 && c[2] % 10 == 0 && c[2] >= 10 && c[2] <= 120)
					{
					TRgb t = iKeyTo[c[2] / 10 - 1];
					pix[i] = Index(t.Red(), t.Green(), t.Blue());
					}
				else
					pix[i] = Index(c[0], c[1], c[2]);
				}
			User::Free(iSheet[aSheet].iPix);
			iSheet[aSheet].iPix = pix;
			iSheet[aSheet].iW = w;
			iSheet[aSheet].iH = h;
			pix = NULL;
			err = KErrNone;
			}
		else if (!rgb || !pix)
			err = KErrNoMemory;
		User::Free(rgb);
		User::Free(pix);
		}
	pa_free(bmp);
	return err;
	}

TInt CPaSkin::Load(RFs& aFs, const TDesC& aWsz, const CPaSkin* aStandard, TBool aThemed)
	{
	iThemed = aThemed;
	if (aThemed)
		ThemeKeys(iKeyTo);
	RFile f;
	TInt err = f.Open(aFs, aWsz, EFileRead | EFileShareReadersOnly);
	if (err != KErrNone)
		return err;
	TInt size = 0;
	f.Size(size);
	if (size <= 22 || size > (2 << 20))
		{
		f.Close();
		return KErrCorrupt;
		}
	HBufC8* buf = HBufC8::New(size);
	if (!buf)
		{
		f.Close();
		return KErrNoMemory;
		}
	TPtr8 p = buf->Des();
	err = f.Read(p);
	f.Close();
	if (err == KErrNone)
		{
		Free();
		const TUint8* z = p.Ptr();
		TInt found = 0;
		for (TInt i = 0; i < ESheets; i++)
			{
			TInt e = LoadSheet(z, p.Length(), KPaSheetName[i], i, 0, 0);
			if (e == KErrNone)
				found++;
			else if (e == KErrNoMemory)
				err = e;
			}
		if (!iSheet[EBalance].iPix)
			LoadSheet(z, p.Length(), "volume.bmp", EBalance, 0, 0);
		// the digits: nums_ex.bmp has them too (some skins have only it); a
		// skin without nums_ex.bmp shows no minus sign rather than another
		// skin's
		if (!iSheet[ENumbers].iPix && iSheet[ENumsEx].iPix)
			CopyFrom(*this, ENumsEx, ENumbers);
		if (aStandard)
			for (TInt i = 0; i < ESheets; i++)
				if (!iSheet[i].iPix && i != ENumsEx)
					CopyFrom(*aStandard, i, i);
		if (!found && err == KErrNone)
			err = KErrCorrupt;
		// viscolor.txt and pledit.txt
		PaZipEntry e;
		if (pa_zip_find(z, p.Length(), "viscolor.txt", &e) == 0)
			{
			unsigned char* t = pa_zip_read(z, p.Length(), &e);
			if (t)
				{
				TUint8 rgb[72];
				TInt n = pa_viscolor_parse((const char*)t, e.usize, rgb);
				for (TInt v = 0; v < n; v++)
					iVis[v] = Index(rgb[v * 3], rgb[v * 3 + 1], rgb[v * 3 + 2]);
				pa_free(t);
				}
			}
		if (pa_zip_find(z, p.Length(), "pledit.txt", &e) == 0)
			{
			unsigned char* t = pa_zip_read(z, p.Length(), &e);
			if (t)
				{
				TUint8 rgb[12], have[12];
				for (TInt k = 0; k < 4; k++)
					{
					have[k * 3] = (TUint8)iListRgb[k].Red();
					have[k * 3 + 1] = (TUint8)iListRgb[k].Green();
					have[k * 3 + 2] = (TUint8)iListRgb[k].Blue();
					}
				Mem::Copy(rgb, have, 12);
				pa_pledit_parse((const char*)t, e.usize, rgb);
				for (TInt k2 = 0; k2 < 4; k2++)
					iListRgb[k2] = TRgb(rgb[k2 * 3], rgb[k2 * 3 + 1], rgb[k2 * 3 + 2]);
				pa_free(t);
				}
			}
		if (aThemed)
			{
			// the list as EIKON's lists are: its text, the background, the highlight
			CEikonEnv* env = CEikonEnv::Static();
			iListRgb[0] = env->Color(EEikColorControlText);
			iListRgb[1] = env->Color(EEikColorDialogTitle);
			iListRgb[2] = env->Color(EEikColorControlBackground);
			iListRgb[3] = env->Color(EEikColorControlHighlightBackground);
			iListRgb[4] = env->Color(EEikColorControlHighlightText);
			iHasSelText = ETrue;
			}
		TParsePtrC parse(aWsz);
		iName = PaLeft(parse.Name(), iName.MaxLength());
		}
	delete buf;
	return err;
	}

// ===========================================================================
// The face
// ===========================================================================

CPaFace::~CPaFace()
	{
	User::Free(iPix);
	delete iBmp;
	}

void CPaFace::SetSizeL(TInt aW, TInt aH, TInt aScale)
	{
	if (iPix && aW == iW && aH == iH && aScale == iScale)
		return;
	User::Free(iPix);
	iPix = NULL;
	delete iBmp;
	iBmp = NULL;
	iPix = (TUint8*)User::AllocL(aW * aH);
	Mem::FillZ(iPix, aW * aH);
	iW = aW;
	iH = aH;
	iScale = aScale;
	iBmp = new(ELeave) CFbsBitmap;
	User::LeaveIfError(iBmp->Create(TSize(aW * aScale, aH * aScale), EColor256));
	}

void CPaFace::Put(const TPaSheet& aSheet, TInt aSx, TInt aSy, TInt aW, TInt aH, TInt aDx, TInt aDy)
	{
	if (!aSheet.iPix || !iPix)
		return;
	for (TInt y = 0; y < aH; y++)
		{
		TInt sy = aSy + y, dy = aDy + y;
		if (sy < 0 || sy >= aSheet.iH || dy < 0 || dy >= iH)
			continue;
		const TUint8* s = aSheet.iPix + sy * aSheet.iW;
		TUint8* d = iPix + dy * iW;
		for (TInt x = 0; x < aW; x++)
			{
			TInt sx = aSx + x, dx = aDx + x;
			if (sx >= 0 && sx < aSheet.iW && dx >= 0 && dx < iW)
				d[dx] = s[sx];
			}
		}
	}

void CPaFace::Fill(TInt aX, TInt aY, TInt aW, TInt aH, TUint8 aColour)
	{
	for (TInt y = aY; y < aY + aH; y++)
		if (y >= 0 && y < iH)
			for (TInt x = aX; x < aX + aW; x++)
				if (x >= 0 && x < iW)
					iPix[y * iW + x] = aColour;
	}

// the skin's 5 x 6 letters (text.bmp: three rows of 31)
static void FontCell(TUint aCh, TInt& aCol, TInt& aRow)
	{
	static const char row1[] = "0123456789\x85.:()-'!_+\\/[]^&%,=$#";
	aRow = 0;
	aCol = 30;                                   // a space
	if (aCh >= 'a' && aCh <= 'z')
		aCh -= 32;
	if (aCh >= 'A' && aCh <= 'Z') { aCol = aCh - 'A'; return; }
	if (aCh == '"') { aCol = 26; return; }
	if (aCh == '@') { aCol = 27; return; }
	if (aCh == ' ') return;
	for (TInt i = 0; row1[i]; i++)
		if ((TUint8)row1[i] == aCh) { aRow = 1; aCol = i; return; }
	switch (aCh)
		{
	case 0xc5: case 0xe5: aRow = 2; aCol = 0; return;      // A ring
	case 0xd6: case 0xf6: aRow = 2; aCol = 1; return;      // O diaeresis
	case 0xc4: case 0xe4: aRow = 2; aCol = 2; return;      // A diaeresis
	case '?': aRow = 2; aCol = 3; return;
	case '*': aRow = 2; aCol = 4; return;
	case '<': aRow = 1; aCol = 13; return;                  // ( for <
	case '>': aRow = 1; aCol = 14; return;
	case '~': aRow = 1; aCol = 15; return;
		}
	if (aCh >= 0xc0 && aCh <= 0xff)
		{
		// accented letters as their plain ones
		static const char plain[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPSAAAAAAACEEEEIIIIDNOOOOO/OUUUUYPY";
		TUint c = (TUint8)plain[aCh - 0xc0];
		if (c >= 'A' && c <= 'Z') { aCol = c - 'A'; return; }
		}
	}

void CPaFace::Text(const TPaSheet& aFont, const TDesC& aText, TInt aX, TInt aY, TInt aW, TInt aOffset)
	{
	for (TInt i = 0; i < aText.Length(); i++)
		{
		TInt x = aX + i * 5 - aOffset;
		if (x + 5 <= aX || x >= aX + aW)
			continue;
		TInt col, row;
		FontCell(aText[i], col, row);
		// clipped to the box
		TInt sx = col * 5, w = 5, dx = x;
		if (dx < aX) { sx += aX - dx; w -= aX - dx; dx = aX; }
		if (dx + w > aX + aW)
			w = aX + aW - dx;
		Put(aFont, sx, row * 6, w, 6, dx, aY);
		}
	}

void CPaFace::Update(const TRect& aRect)
	{
	if (!iBmp || !iPix)
		return;
	TRect r(aRect);
	r.Intersection(TRect(0, 0, iW, iH));
	if (r.IsEmpty())
		return;
	TInt stride = CFbsBitmap::ScanLineLength(iW * iScale, EColor256);
	TUint8* base = (TUint8*)iBmp->DataAddress();
	for (TInt y = r.iTl.iY; y < r.iBr.iY; y++)
		{
		const TUint8* s = iPix + y * iW;
		if (iScale == 1)
			Mem::Copy(base + y * stride + r.iTl.iX, s + r.iTl.iX, r.Width());
		else
			{
			TUint8* d = base + (y * 2) * stride + r.iTl.iX * 2;
			for (TInt x = r.iTl.iX; x < r.iBr.iX; x++)
				{
				d[0] = d[1] = s[x];
				d += 2;
				}
			Mem::Copy(base + (y * 2 + 1) * stride + r.iTl.iX * 2, base + (y * 2) * stride + r.iTl.iX * 2, r.Width() * 2);
			}
		}
	}
