

#ifndef TCONVERT_INCLUDED
#define TCONVERT_INCLUDED

#include "tcommon.h"
class TFilePath;

//
// Nota: il file tconvert.cpp esiste gia' in rop.
// l'implementazione di queste funzioni si trova in tstring.cpp
//

#undef DVAPI
#ifdef TNZCORE_EXPORTS //TNZCORE_DLL
#define DVAPI DV_EXPORT_API
#else
#define DVAPI DV_IMPORT_API
#endif

DVAPI bool isInt(string s);
DVAPI bool isDouble(string s);

DVAPI string toString(int v);
DVAPI string toString(unsigned long v);
DVAPI string toString(unsigned long long v);
DVAPI string toString(double v, int prec = -1);
DVAPI string toString(wstring s);
DVAPI string toString(const TFilePath &fp);
DVAPI string toString(void *p);

DVAPI int toInt(string s);
DVAPI double toDouble(string s);

DVAPI bool isInt(wstring s);
DVAPI bool isDouble(wstring s);

DVAPI wstring toWideString(string s);
DVAPI wstring toWideString(int v);
DVAPI wstring toWideString(double v, int prec = -1);

DVAPI int toInt(wstring s);
DVAPI double toDouble(wstring s);

inline bool fromStr(int &v, string s)
{
	if (isInt(s)) {
		v = toInt(s);
		return true;
	} else
		return false;
}

inline bool fromStr(double &v, string s)
{
	if (isDouble(s)) {
		v = toDouble(s);
		return true;
	} else
		return false;
}

inline bool fromStr(string &out, string s)
{
	out = s;
	return true;
}

DVAPI string toUpper(string a);
DVAPI string toLower(string a);

DVAPI wstring toUpper(wstring a);
DVAPI wstring toLower(wstring a);

#ifndef TNZCORE_LIGHT
#include <QString>

inline bool fromStr(int &v, QString s)
{
	bool ret;
	v = s.toInt(&ret);
	return ret;
}

inline bool fromStr(double &v, QString s)
{
	bool ret;
	v = s.toDouble(&ret);
	return ret;
}

#endif

#endif
