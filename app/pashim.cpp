// PASHIM.CPP - the C library functions PsiAmp's C (Helix, zlib, engine/) uses,
// over EUSER (memcpy and memset are EUSER's own), and pacore.h's allocator:
// all on the thread's heap.
#include <e32std.h>

extern "C" {

static void* AllocZ(TInt aBytes)
	{
	void* p = User::Alloc(aBytes > 0 ? aBytes : 1);
	if (p)
		Mem::FillZ(p, aBytes > 0 ? aBytes : 1);
	return p;
	}

void* pa_alloc(int aBytes)
	{
	return AllocZ(aBytes);
	}

void pa_free(void* aPtr)
	{
	User::Free(aPtr);
	}

void* malloc(unsigned int aSize)
	{
	return User::Alloc(aSize ? aSize : 1);
	}

void* calloc(unsigned int aN, unsigned int aSize)
	{
	if (aSize && aN > 0x7fffffffU / aSize)
		return 0;
	return AllocZ((TInt)(aN * aSize));
	}

void free(void* aPtr)
	{
	User::Free(aPtr);
	}

void* memmove(void* aDst, const void* aSrc, unsigned int aN)
	{
	Mem::Copy(aDst, aSrc, aN);          // (Mem::Copy copes with overlap)
	return aDst;
	}

int memcmp(const void* aA, const void* aB, unsigned int aN)
	{
	const unsigned char* a = (const unsigned char*)aA;
	const unsigned char* b = (const unsigned char*)aB;
	for (unsigned int i = 0; i < aN; i++)
		if (a[i] != b[i])
			return a[i] - b[i];
	return 0;
	}

unsigned int strlen(const char* aS)
	{
	unsigned int n = 0;
	while (aS[n])
		n++;
	return n;
	}

}

// gcc's 64-bit division helpers, over EUSER's TInt64 (libgcc's return with
// BX, which the SA-1100 lacks; only zlib's adler32_combine, which nothing
// here calls, needs them)
static TInt64 To64(long long aV)
	{
	union { long long v; TUint32 w[2]; } u;
	u.v = aV;
	return TInt64((TInt)u.w[1], u.w[0]);
	}

static long long From64(const TInt64& aV)
	{
	union { long long v; TUint32 w[2]; } u;
	u.w[0] = aV.Low();
	u.w[1] = (TUint32)aV.High();
	return u.v;
	}

extern "C" long long __moddi3(long long a, long long b) { return From64(To64(a) % To64(b)); }
extern "C" long long __divdi3(long long a, long long b) { return From64(To64(a) / To64(b)); }
