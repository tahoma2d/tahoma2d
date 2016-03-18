

#include "tstreamexception.h"
#include "tfilepath.h"
#include "tconvert.h"
#include "tstream.h"

namespace
{

wstring getLocation(TIStream &is)
{
	return L"File: " +
		   is.getFilePath().getWideString() +
		   L":" + toWideString(is.getLine());
}

wstring message(TIStream &is, wstring msg)
{
	return getLocation(is) + L"\n" + msg;
}

wstring message(TIStream &is, string msg)
{
	return message(is, toWideString(msg));
}

} // namespace

TIStreamException::TIStreamException(TIStream &is)
	: TException(message(is, L"unknown exception"))
{
}

TIStreamException::TIStreamException(TIStream &is, const TException &e)
	: TException(message(is, e.getMessage()))
{
}

TIStreamException::TIStreamException(TIStream &is, wstring msg)
	: TException(message(is, msg))
{
}

TIStreamException::TIStreamException(TIStream &is, string msg)
	: TException(message(is, msg))
{
}

TIStreamException::~TIStreamException()
{
}
