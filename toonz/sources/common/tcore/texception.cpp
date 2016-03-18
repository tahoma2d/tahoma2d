

#include "texception.h"
#include "tconvert.h"

TException::TException(const string &msg)
{
	m_msg = toWideString(msg);
}
/*
ostream& operator<<(ostream &out, const TException &e)
{
  return out<<e.getMessage().c_str();
}
*/
