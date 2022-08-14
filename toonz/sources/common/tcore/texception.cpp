

#include "texception.h"
#include "tconvert.h"

static TString s_lastMsg;

TException::TException(const std::string &msg) {
  m_msg     = ::to_wstring(msg);
  s_lastMsg = getMessage();
}

TString TException::getLastMessage() { return s_lastMsg; }

/*
ostream& operator<<(ostream &out, const TException &e)
{
  return out<<e.getMessage().c_str();
}
*/
