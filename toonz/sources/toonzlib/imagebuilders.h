#pragma once

#ifndef IMAGE_BUILDERS_H
#define IMAGE_BUILDERS_H

#include "tfilepath.h"

#include "toonz/imagemanager.h"
#include "toonz/tcamera.h"

// ToonzLib includes
#include "toonz/stage.h"

//======================================================

//  Forward declarations

class TImageInfo;
class TXshSimpleLevel;

//======================================================

//***************************************************************************************
//    ImageLoader  class declaration
//***************************************************************************************

/*!
  ImageLoader is the specialized ImageBuilder class used to automatically load
  images from a level file on hard disk.

  Refer to ImageLoader::BuildExtData for a description of the allowed options
  for
  image loading.
*/
class ImageLoader final : public ImageBuilder {
public:
  struct BuildExtData {
    const TXshSimpleLevel *m_sl;  //!< TXshSimpleLevel instance associated to an
                                  //! image loading request
    TFrameId m_fid;  //!< m_sl's fid at which the image will be loaded

    int m_subs;  //!< The subsampling factor for image loading (0 meaning either
    //!< 'the currently stored one' if an image is already cached, or
    //!< m_sl's subsampling property otherwise)
    bool m_icon;  //!< Whether the icon (if any) should be loaded instead
    
    TPointD m_cameraDPI;  // for Rasterizer

    TFilePath m_fullPath;

  public:
    BuildExtData(const TXshSimpleLevel *sl, const TFrameId &fid, int subs = 0,
                 bool icon = false, TFilePath fullPath = TFilePath())
        : m_sl(sl)
        , m_fid(fid)
        , m_subs(subs)
        , m_icon(icon)
        , m_cameraDPI(Stage::inch, Stage::inch)
        , m_fullPath(fullPath) {}
  };

public:
  ImageLoader(const TFilePath &path, const TFrameId &fid);

  bool isImageCompatible(int imFlags, void *extData) override;

  /*--
   * Implement ImageBuilder virtual functions. All icons and images are stored
   * in the cache on loading
   * --*/
  void buildAllIconsAndPutInCache(TXshSimpleLevel *level,
                                  std::vector<TFrameId> fids,
                                  std::vector<std::string> iconIds,
                                  bool cacheImagesAsWell) override;

  /* Exposed to allow Fid to be updated due to a renumber operation
  */
  void setFid(const TFrameId &fid) override;

protected:
  bool getInfo(TImageInfo &info, int imFlags, void *extData) override;
  TImageP build(int imFlags, void *extData) override;

  void invalidate() override;

  inline int buildSubsampling(int imFlags, BuildExtData *data);

private:
  TFilePath m_path;  //!< Level path to load images from
  TFrameId m_fid;    //!< Frame of the level to load

  bool m_64bitCompatible;  //!< Whether current image is 64-bit compatible
  bool m_floatCompatible;  //!< Whether current image is float compatible
  int m_subsampling;       //!< Current image subsampling
  //!< NOTE: Should this be replaced by requests to the TImageCache?

  double m_colorSpaceGamma;  // current gamma. only used in EXR levels
};

//-----------------------------------------------------------------------------

class ImageRasterizer final : public ImageBuilder {
public:
  ImageRasterizer() {}
  ~ImageRasterizer() {}

  TPointD m_cameraDPI;
  bool m_antiAliasing;

  //Imagemaneget::getImage
  bool isImageCompatible(int imFlags, void *extData) override;

protected:
  bool getInfo(TImageInfo &info, int imFlags, void *extData) override;
  TImageP build(int imFlags, void *extData) override;
};

//-----------------------------------------------------------------------------

class ImageFiller final : public ImageBuilder {
public:
  ImageFiller() {}
  ~ImageFiller() {}

  bool isImageCompatible(int imFlags, void *extData) override { return true; }

protected:
  bool getInfo(TImageInfo &info, int imFlags, void *extData) override;
  TImageP build(int imFlags, void *extData) override;
};

#endif  // IMAGE_BUILDERS_H
