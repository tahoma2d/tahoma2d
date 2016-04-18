

#include "tconvert.h"
//#include "texception.h"
#include "tfilepath.h"

#ifndef TNZCORE_LIGHT
#include <QString>
#endif

#ifdef WIN32
#pragma warning(disable : 4996)
#include "windows.h"
#endif

class TStringConvertException : public TException
{
	string m_string;

public:
	TStringConvertException(const string str) : m_string(str) {}
};

wstring toWideString(string s)
{
#ifdef TNZCORE_LIGHT
	std::wstring ws;
	ws.assign(s.begin(), s.end());
	return ws;
#else

	QString testString = QString::fromStdString(s);
	QString qString = QString::fromUtf8(s.c_str());

	// To detect if 's' is UTF-8 encoded or not
	if (qString != testString && string(qString.toUtf8()) == s)
		return qString.toStdWString();
	else
		return testString.toStdWString();
#endif
}

string toString(wstring ws)
{
#ifdef TNZCORE_LIGHT
	std::string s;
	s.assign(ws.begin(), ws.end());
	return s;
#else

	QString qString = QString::fromStdWString(ws);

// Test if 'ws' is not unicode (UTF-8)
#if 0
  if(qString.toAscii() == qString)
#else
	if (qString.toLatin1() == qString)
#endif
	return qString.toStdString();

	QByteArray a = qString.toUtf8();
	return string(a);
#endif
}

string toString(const TFilePath &fp)
{
	return toString(fp.getWideString());
}

wstring toWideString(int x)
{
	return toWideString(toString(x));
}

string toString(int value)
{
	ostrstream ss;
	ss << value << '\0';
	string s = ss.str();
	ss.freeze(false);
	return s;
}

string toString(unsigned long value)
{
	ostrstream ss;
	ss << value << '\0';
	string s = ss.str();
	ss.freeze(false);
	return s;
}

string toString(unsigned long long value)
{
	ostrstream ss;
	ss << value << '\0';
	string s = ss.str();
	ss.freeze(false);
	return s;
}

/*!
  The  default precision is six decimal places. If the 
  precision is less than of the decimal places in the fractonal
  part, the remainder is not cut off but rounded.
*/

string toString(double value, int prec)
{
	ostrstream ss;
	ss.setf(std::ios_base::fixed, std::ios_base::floatfield);
	if (prec >= 0)
		ss.precision(prec);
	ss << value << '\0';
	string s = ss.str();
	ss.freeze(0);
	return s;
}

string toString(void *p)
{
	ostrstream ss;
	ss << p << '\0';
	string s = ss.str();
	ss.freeze(false);
	return s;
}

int toInt(string str)
{
	int value = 0;
	for (int i = 0; i < (int)str.size(); i++)
		value = value * 10 + str[i] - '0';
	return value;
}

int toInt(wstring str)
{
	return toInt(toString(str));
}

bool isInt(string s)
{
	int i = 0, len = (int)s.size();
	if (len == 0)
		return false;
	if (s[0] == '-') {
		if (len == 1)
			return false;
		else
			i++;
	}

	while (i < len) {
		if (s[i] < '0' || s[i] > '9')
			return false;
		i++;
	}
	return true;
}

bool isDouble(string s)
{
	int i = 0, len = (int)s.size();
	if (len == 0)
		return false;
	if (i < len && s[i] == '-')
		i++;
	while (i < len && s[i] != '.') {
		if (s[i] < '0' || s[i] > '9')
			return false;
		i++;
	}
	if (i >= len)
		return true;
	i++;
	while (i < len) {
		if (s[i] < '0' || s[i] > '9')
			return false;
		i++;
	}
	return true;
}

bool isInt(wstring s) { return isInt(toString(s)); }
bool isDouble(wstring s) { return isDouble(toString(s)); }

double toDouble(string str)
{
	double value;
	istrstream ss(str.c_str(), (std::streamsize)str.length());
	ss >> value;
	return value;
}

double toDouble(wstring str)
{
	return toDouble(toString(str));
}

wstring toWideString(double v, int p)
{
	return toWideString(toString(v, p));
}

string toUpper(string a)
{
#ifdef WIN32
	return _strupr(const_cast<char *>(a.c_str()));
#else
	string ret = a;
	for (int i = 0; i < (int)ret.length(); i++)
		ret[i] = toupper(ret[i]);
	return ret;
#endif
}

string toLower(string a)
{
#ifdef WIN32
	return _strlwr(const_cast<char *>(a.c_str()));
#else
	string ret = a;
	for (int i = 0; i < (int)ret.length(); i++)
		ret[i] = tolower(ret[i]);
	return ret;
#endif
}

wstring toUpper(wstring a)
{
#ifdef WIN32
	return _wcsupr(const_cast<wchar_t *>(a.c_str()));
#else
	wstring ret;
	for (int i = 0; i < (int)a.length(); i++) {
		wchar_t c = towupper(a[i]);
		ret += c;
	}
	return ret;
#endif
}

wstring toLower(wstring a)
{
#ifdef WIN32
	return _wcslwr(const_cast<wchar_t *>(a.c_str()));
#else
	wstring ret;
	for (int i = 0; i < (int)a.length(); i++) {
		wchar_t c = towlower(a[i]);
		ret += c;
	}
	return ret;
#endif
}
