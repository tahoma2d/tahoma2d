

#ifndef AVI_CODEC_RESTRICTIONS
#define AVI_CODEC_RESTRICTIONS

#include "tcommon.h"
#include "tgeometry.h"
#include <QString>
#include <QMap>
#include <QObject>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace AviCodecRestrictions
{

//! Returns a text specifing resolution restricions for the given codec name.
void DVAPI getRestrictions(const wstring &codecName, QString &restrictions);
//! Returns true if the ginven codec can work in the given resolution.
bool DVAPI canWriteMovie(const wstring &codecName, const TDimension &resolution);
//! Return a mapping containing the same codecs in \b codecNames, specifing which codec can work in the given resolution.
QMap<wstring, bool> DVAPI getUsableCodecs(const TDimension &resolution);
//! Returns true if the specifiedcodec can be configured.
bool DVAPI canBeConfigured(const wstring &codecName);
//! Oopen the configuration popup for the specified codec
void DVAPI openConfiguration(const wstring &codecName, void *winId);
}

#endif; //AVI_CODEC_RESTRICTIONS
