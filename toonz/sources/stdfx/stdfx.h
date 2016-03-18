

#ifndef STDFX_INCLUDED
#define STDFX_INCLUDED

// TnzBase includes
#include "trasterfx.h"
#include "tzeraryfx.h"
#include "tdoubleparam.h"

//==================================================================

class PaletteFilterFxRenderData;

static const string PLUGIN_PREFIX("STD");

#define FX_PLUGIN_DECLARATION(T) \
public:                          \
	const TPersistDeclaration *getDeclaration() const;

#ifdef WIN32

#ifdef TNZSTDFX_EXPORTS

#define FX_PLUGIN_IDENTIFIER(T, I)                                       \
	template class DV_EXPORT_API TFxDeclarationT<T>;                     \
	namespace                                                            \
	{                                                                    \
	TFxDeclarationT<T> info##T(TFxInfo(PLUGIN_PREFIX + "_" + I, false)); \
	}                                                                    \
	const TPersistDeclaration *T::getDeclaration() const { return &info##T; }
#else
#define FX_PLUGIN_IDENTIFIER(T, I)                                       \
	template class DV_IMPORT_API TFxDeclarationT<T>;                     \
	namespace                                                            \
	{                                                                    \
	TFxDeclarationT<T> info##T(TFxInfo(PLUGIN_PREFIX + "_" + I, false)); \
	}                                                                    \
	const TPersistDeclaration *T::getDeclaration() const { return &info##T; }
#endif
#else
#define FX_PLUGIN_IDENTIFIER(T, I)                                       \
	namespace                                                            \
	{                                                                    \
	TFxDeclarationT<T> info##T(TFxInfo(PLUGIN_PREFIX + "_" + I, false)); \
	}                                                                    \
	const TPersistDeclaration *T::getDeclaration() const { return &info##T; }
#endif

class TStandardRasterFx : public TRasterFx
{
public:
	string getPluginId() const { return PLUGIN_PREFIX; }
};

//-------------------------------------------------------------------

class TStandardZeraryFx : public TZeraryFx
{
public:
	string getPluginId() const { return PLUGIN_PREFIX; }
};

//-------------------------------------------------------------------

bool isAlmostIsotropic(const TAffine &aff);

#endif
