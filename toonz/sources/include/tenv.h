

#ifndef TENV_INCLUDED
#define TENV_INCLUDED

//#include "texception.h"
#include "tgeometry.h"
#include "tfilepath.h"

//===================================================================

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================
namespace TEnv
{

//-------------------------------------------------------

class DVAPI Variable
{
public:
	class Imp;
	Imp *m_imp;

public:
	Variable(string name, string defaultValue);
	Variable(string name);
	virtual ~Variable();

	string getName() const;
	string getValue() const;

	void assignValue(string str);
};

class DVAPI IntVar : public Variable
{
public:
	IntVar(string name, int defValue);
	IntVar(string name);
	operator int() const;
	void operator=(int v);
};

class DVAPI DoubleVar : public Variable
{
public:
	DoubleVar(string name, double defValue);
	DoubleVar(string name);
	operator double() const;
	void operator=(double v);
};

class DVAPI StringVar : public Variable
{
public:
	StringVar(string name, const string &defValue);
	StringVar(string name);
	operator string() const;
	void operator=(const string &v);
};

class DVAPI FilePathVar : public Variable
{
public:
	FilePathVar(string name, const TFilePath &defValue);
	FilePathVar(string name);
	operator TFilePath() const;
	void operator=(const TFilePath &v);
};

class DVAPI RectVar : public Variable
{
public:
	RectVar(string name, const TRect &defValue);
	RectVar(string name);
	operator TRect() const;
	void operator=(const TRect &v);
};

//-------------------------------------------------------

// NOTA BENE: bisogna chiamare setApplication() il prima possibile
// questa operazione inizializza il registry root (su Windows) e definisce il file dove
// vengono lette e scritte le variabili di environment
//
// es.:  TEnv::setApplication("Toonz","5.0");
//
DVAPI void setApplication(string applicationName, string applicationVersion);

DVAPI string getApplicationName();
DVAPI string getApplicationVersion();

// es.: TEnv::setModuleFullName("Toonz 5.0.1 Harlequin");
// (default: "<applicationName> <applicationVersion>")
DVAPI void setApplicationFullName(string applicationFullName);
DVAPI string getApplicationFullName();

// es.: TEnv::setModuleName("inknpaint")
// (default: "<applicationName>")
DVAPI void setModuleName(string moduleName);
DVAPI string getModuleName();

// es.: TEnv::setRootVarName("TABROOT");
// (default: toUpper(<applicationName> + "ROOT"))
DVAPI string getRootVarName();
DVAPI void setRootVarName(string varName);

// es.: TEnv::setRootVarName("Toonz");
// (default: <programName>)
DVAPI void setSystemVarPrefix(string prefix);
DVAPI string getSystemVarPrefix();

// su Windows ritorna
//   'SOFTWARE\Digital Video\<applicationName>\<applicationVersion>\<rootvar>'
// su Unix/Linux/MacOsX
//   '<rootvar>'
DVAPI TFilePath getRootVarPath();

// restituisce il valore della variabile di sistema varName
// (su window aggiunge "SOFTWARE\Digital Video\....." all'inizio)
DVAPI string getSystemVarStringValue(string varName);
DVAPI TFilePath getSystemVarPathValue(string varName);
DVAPI TFilePathSet getSystemVarPathSetValue(string varName);

DVAPI TFilePath getStuffDir();
DVAPI TFilePath getConfigDir();
//DVAPI TFilePath getProfilesDir();

// per l'utilizzo di ToonzLib senza che sia definita una TOONZROOT
// bisogna chiamare TEnv::setStuffDir(stuffdir) prima di ogni altra operazione
DVAPI void setStuffDir(const TFilePath &stuffDir);

DVAPI TFilePath getDllRelativeDir();
DVAPI void setDllRelativeDir(const TFilePath &dllRelativeDir);

DVAPI void saveAllEnvVariables();

/*

  enum SystemFileId {
    StuffDir,
    ConfigDir,
    ProfilesDir
  };



  // bisogna assegnare (in qualsiasi ordine) tutti i vari nomi dei registry alle varie
  // directory di sistema. 
  // StuffDir e' obbligatoria, tutte le altre hanno un valore di default relativo a StuffDir
  DVAPI void defineSystemPath(SystemFileId id, const TFilePath &registryName);

  // restituisce il file (non la variabile di registro)
  DVAPI TFilePath getSystemPath(SystemFileId id);
*/

//=========================================================

//=========================================================

} // namespace TEnv

//=========================================================

#endif
