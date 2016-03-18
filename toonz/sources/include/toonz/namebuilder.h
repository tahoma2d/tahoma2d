

#ifndef NAMEBUILDER_INCLUDED
#define NAMEBUILDER_INCLUDED

#include "tcommon.h"

//-------------------------------------------------------------------

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------

class DVAPI NameBuilder
{
public:
	virtual ~NameBuilder() {}
	virtual wstring getNext() = 0;

	static NameBuilder *getBuilder(wstring levelName = L"");

	// NameBuilder::getBuilder() restituisce un NameCreator
	// NameBuilder::getBuilder("pippo") restituisce un NameModifier
};

//-------------------------------------------------------------------

// NameCreator genera la sequenza 'A', 'B', ...
class DVAPI NameCreator : public NameBuilder
{
	std::vector<int> m_s;

public:
	NameCreator() {}
	wstring getNext();
};

//-------------------------------------------------------------------

class DVAPI NameModifier : public NameBuilder
{
	wstring m_nameBase;
	int m_index;

public:
	NameModifier(wstring name);
	wstring getNext();
};

//-------------------------------------------------------------------

#endif
