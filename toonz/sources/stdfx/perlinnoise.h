

#ifndef PERLINNOISE_H
#define PERLINOISE_H
#include "tfxparam.h"
#include "tspectrumparam.h"

enum {
	PNOISE_CLOUDS,
	PNOISE_WOODS
};

class PerlinNoise
{

	static int Size;
	static int TimeSize;
	static int Offset;
	static double Pixel_size;
	float *Noise;
	double LinearNoise(double x, double y, double t);

public:
	PerlinNoise();
	~PerlinNoise() { delete[] Noise; };
	double Turbolence(double u, double v, double k, double grain);
	double Turbolence(double u, double v, double k, double grain, double min, double max);
	double Marble(double u, double v, double k, double grain);
	double Marble(double u, double v, double k, double grain, double min, double max);
};
/*---------------------------------------------------------------------------*/
void doClouds(
	const TRasterP &ras,
	const TSpectrumParamP colors,
	TPointD pos,
	double evolution,
	double size,
	double min,
	double max,
	int type,
	double scale,
	double frame);

#endif
