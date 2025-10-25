
#include "tnzimage.h"
#include "tiio.h"
#include "tfiletype.h"
#include "thirdparty.h"

//-------------------------------------------------------------------

/*!
    TImageReader::load MUST throw an exception in case of error (file not found,
   ...)
    TImageWriter::save MUST throw an exception in case of error (write denied,
                                                                 disk full, ...)
    N.B.: gestire nei TImageWriter il caso "disk full"!!!
*/

// Math needs to be included before the rest on MAC - but I've not figured out
// why (it would be included anyway though)
#include <math.h>

// Platform-specific includes
#if defined(_WIN32)

#if !defined(x64) && !defined(__GNUC__)

#undef list
#undef map
#undef iterator
#undef float_t
#undef GetProcessInformation
#undef int_fast8_t
#undef int_fast16_t
#undef uint_fast16_t

#endif

#include "./zcc/tiio_zcc.h"

#endif

// Common includes
//#include "./quantel/tiio_quantel.h"
#include "./sgi/tiio_sgi.h"
#include "./tga/tiio_tga.h"
#include "./png/tiio_png.h"
/* Can't build this on FreeBSD: libtiff requires internal API access.
 * Probably some one get luck to fix this. */
#ifndef FREEBSD
#include "./tif/tiio_tif.h"
#include "./tzp/tiio_tzp.h"
#include "./tzp/tiio_plt.h"
#endif
#include "./psd/tiio_psd.h"
#include "./avi/tiio_avi.h"
#include "./pli/tiio_pli.h"
#include "./tzl/tiio_tzl.h"
#include "./svg/tiio_svg.h"
#include "./ffmpeg/tiio_gif.h"
#include "./ffmpeg/tiio_webm.h"
#include "./ffmpeg/tiio_mp4.h"
#include "./ffmpeg/tiio_apng.h"
#include "./ffmpeg/tiio_ff_mov.h"
#include "./mesh/tiio_mesh.h"
#include "./sprite/tiio_sprite.h"
#include "./exr/tiio_exr.h"

//-------------------------------------------------------------------

// static TPluginInfo info("imageIOPlugin");

// TLIBMAIN
void initImageIo(bool lightVersion) {
  TLevelWriter::define("pli", TLevelWriterPli::create, false);
  TLevelReader::define("pli", TLevelReaderPli::create);
  TFileType::declare("pli", TFileType::VECTOR_LEVEL);

  TLevelReader::define("svg", 0, TLevelReaderSvg::create);
  TFileType::declare("svg", TFileType::VECTOR_IMAGE);
  TLevelWriter::define("svg", TLevelWriterSvg::create, false);
  Tiio::defineWriterProperties("svg", new Tiio::SvgWriterProperties());

  TLevelWriter::define("tzl", TLevelWriterTzl::create, false);
  TLevelReader::define("tzl", TLevelReaderTzl::create);
  TFileType::declare("tzl", TFileType::RASTER_LEVEL);

  TLevelWriter::define("tlv", TLevelWriterTzl::create, false);
  TLevelReader::define("tlv", TLevelReaderTzl::create);
  TFileType::declare("tlv", TFileType::CMAPPED_LEVEL);

/* Can't build this on FreeBSD: libtiff requires internal API access.
 * Probably some one get luck to fix this. */
#ifndef FREEBSD
  Tiio::defineReaderMaker("tzp", Tiio::makeTzpReader);
  Tiio::defineWriterMaker("tzp", Tiio::makeTzpWriter, false);
  TFileType::declare("tzp", TFileType::CMAPPED_IMAGE);

  Tiio::defineReaderMaker("tzu", Tiio::makeTzpReader);
  Tiio::defineWriterMaker("tzu", Tiio::makeTzpWriter, false);
  TFileType::declare("tzu", TFileType::CMAPPED_IMAGE);

  Tiio::defineReaderMaker("tzi", Tiio::makeTziReader);
  TFileType::declare("tzi", TFileType::RASTER_IMAGE);

  Tiio::defineReaderMaker("plt", Tiio::makePltReader);
  Tiio::defineWriterMaker("plt", Tiio::makePltWriter, false);
  TFileType::declare("plt", TFileType::RASTER_IMAGE);
#endif

  TLevelWriter::define("psd", TLevelWriterPsd::create, false);
  TLevelReader::define("psd", TLevelReaderPsd::create);
  TFileType::declare("psd", TFileType::RASTER_LEVEL);

  TLevelWriter::define("mesh", TLevelWriterMesh::create, false);
  TLevelReader::define("mesh", TLevelReaderMesh::create);
  TFileType::declare("mesh", TFileType::MESH_IMAGE);

  TFileType::declare("tpl", TFileType::PALETTE_LEVEL);

  Tiio::defineReaderMaker("png", Tiio::makePngReader);
  Tiio::defineWriterMaker("png", Tiio::makePngWriter, true);
  TFileType::declare("png", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("png", new Tiio::PngWriterProperties());

  Tiio::defineReaderMaker("tga", Tiio::makeTgaReader);
  Tiio::defineWriterMaker("tga", Tiio::makeTgaWriter, true);
  TFileType::declare("tga", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("tga", new Tiio::TgaWriterProperties());

/* Can't build this on FreeBSD: libtiff requires internal API access.
 * Probably some one get luck to fix this. */
#ifndef FREEBSD
  Tiio::defineReaderMaker("tif", Tiio::makeTifReader);
  Tiio::defineWriterMaker("tif", Tiio::makeTifWriter, true);
  TFileType::declare("tif", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("tif", new Tiio::TifWriterProperties());

  Tiio::defineReaderMaker("tiff", Tiio::makeTifReader);
  Tiio::defineWriterMaker("tiff", Tiio::makeTifWriter, false);
  TFileType::declare("tiff", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("tiff", new Tiio::TifWriterProperties());
#endif

  Tiio::defineReaderMaker("sgi", Tiio::makeSgiReader);
  Tiio::defineWriterMaker("sgi", Tiio::makeSgiWriter, true);
  TFileType::declare("sgi", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("sgi", new Tiio::SgiWriterProperties());

  Tiio::defineReaderMaker("rgb", Tiio::makeSgiReader);
  Tiio::defineWriterMaker("rgb", Tiio::makeSgiWriter, true);
  TFileType::declare("rgb", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("rgb", new Tiio::SgiWriterProperties());

  TLevelWriter::define("spritesheet", TLevelWriterSprite::create, true);
  TFileType::declare("spritesheet", TFileType::RASTER_LEVEL);
  Tiio::defineWriterProperties("spritesheet",
                               new Tiio::SpriteWriterProperties());

  Tiio::defineReaderMaker("exr", Tiio::makeExrReader);
  Tiio::defineWriterMaker("exr", Tiio::makeExrWriter, true);
  TFileType::declare("exr", TFileType::RASTER_IMAGE);
  Tiio::defineWriterProperties("exr", new Tiio::ExrWriterProperties());

// ffmpeg
#if !defined(_WIN32) || defined(x64) || (defined(_WIN32) && defined(__GNUC__))
  if (ThirdParty::checkFFmpeg()) {
    if (Ffmpeg::checkFormat("webm")) {
      TLevelWriter::define("webm", TLevelWriterWebm::create, true);
      TLevelReader::define("webm", TLevelReaderWebm::create);
      TFileType::declare("webm", TFileType::RASTER_LEVEL);
      Tiio::defineWriterProperties("webm", new Tiio::WebmWriterProperties());
    }
    if (Ffmpeg::checkFormat("gif")) {
      TLevelWriter::define("gif", TLevelWriterGif::create, true);
      TLevelReader::define("gif", TLevelReaderGif::create);
      TFileType::declare("gif", TFileType::RASTER_LEVEL);
      Tiio::defineWriterProperties("gif", new Tiio::GifWriterProperties());
    }
    if (Ffmpeg::checkFormat("mp4")) {
      TLevelWriter::define("mp4", TLevelWriterMp4::create, true);
      TLevelReader::define("mp4", TLevelReaderMp4::create);
      TFileType::declare("mp4", TFileType::RASTER_LEVEL);
      Tiio::defineWriterProperties("mp4", new Tiio::Mp4WriterProperties());
    }
    if (Ffmpeg::checkFormat("apng")) {
      TLevelWriter::define("apng", TLevelWriterAPng::create, true);
      TLevelReader::define("apng", TLevelReaderAPng::create);
      TFileType::declare("apng", TFileType::RASTER_LEVEL);
      Tiio::defineWriterProperties("apng", new Tiio::APngWriterProperties());
    }
    if (Ffmpeg::checkFormat("mov")) {
      TLevelWriter::define("mov", TLevelWriterFFMov::create, true);
      TLevelReader::define("mov", TLevelReaderFFMov::create);
      TFileType::declare("mov", TFileType::RASTER_LEVEL);
      Tiio::defineWriterProperties("mov", new Tiio::FFMovWriterProperties());
    }
    if (Ffmpeg::checkFormat("3gp")) {
      TLevelReader::define("3gp", TLevelReaderFFmpeg::create);
      TFileType::declare("3gp", TFileType::RASTER_LEVEL);
    }
    TLevelReader::define("webp", TLevelReaderFFmpeg::create);
    TFileType::declare("webp", TFileType::RASTER_LEVEL);
    TLevelReader::define("ffvideo", TLevelReaderFFmpeg::create);
    TFileType::declare("ffvideo", TFileType::RASTER_LEVEL);
  }
#endif
// end ffmpeg

#ifdef _WIN32

  TLevelWriter::define("avi", TLevelWriterAvi::create, true);
  TLevelReader::define("avi", TLevelReaderAvi::create);
  TFileType::declare("avi", TFileType::RASTER_LEVEL);
  Tiio::defineWriterProperties("avi", new Tiio::AviWriterProperties());

#endif  // _WIN32

}
