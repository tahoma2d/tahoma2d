

#ifndef TEXTERNFX_H
#define TEXTERNFX_H

#include "tbasefx.h"
#include "trasterfx.h"

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//---------------------------------------------------------

class DVAPI TExternFx : public TBaseRasterFx
{
public:
	TExternFx()
	{
		setName(L"ExternFx");
	}

	static TExternFx *create(string name);
	void getNames(vector<string> &names);
};

//---------------------------------------------------------

class DVAPI TExternalProgramFx : public TExternFx
{

	FX_DECLARATION(TExternalProgramFx)

	class Port
	{
	public:
		string m_name;
		TRasterFxPort *m_port; // n.b. la porta di output ha m_port=0
		string m_ext;		   // estensione del file in cui si legge/scrive l'immagine
		Port()
			: m_name(), m_port(0), m_ext() {}
		Port(string name, string ext, TRasterFxPort *port)
			: m_name(name), m_port(port), m_ext(ext) {}
	};

	std::map<string, Port> m_ports;
	// std::map<string, TParamP> m_params;
	std::vector<TParamP> m_params;

	TFilePath m_executablePath;
	string m_args;
	string m_externFxName;

public:
	TExternalProgramFx(string name);
	TExternalProgramFx();
	~TExternalProgramFx();

	void initialize(string name);

	virtual TFx *clone(bool recursive = true) const;

	bool doGetBBox(double frame, TRectD &bBox, const TRenderSettings &info);
	void doCompute(TTile &tile, double frame, const TRenderSettings &ri);

	bool canHandle(const TRenderSettings &info, double frame) { return false; }

	void setExecutable(TFilePath fp, string args)
	{
		m_executablePath = fp;
		m_args = args;
	}

	void addPort(string portName, string ext, bool isInput);
	// void addParam(string paramName, const TParamP &param);

	void loadData(TIStream &is);
	void saveData(TOStream &os);

	void setExternFxName(string name) { m_externFxName = name; }
	string getExternFxName() const { return m_externFxName; };

private:
	// not implemented
	TExternalProgramFx(const TExternalProgramFx &);
	TExternalProgramFx &operator=(const TExternalProgramFx &);
};

#endif
