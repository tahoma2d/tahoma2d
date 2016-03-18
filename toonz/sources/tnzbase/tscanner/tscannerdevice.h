

OBSOLETO OBSOLETO OBSOLETO

	NON INCLUDERE

	/*


#ifndef TSCANNER_DEVICE_H
#define TSCANNER_DEVICE_H

#include <tscanner.h>

#include <set>


class TScannerDevice : public TScanner {
protected:
  std::set<TScannerListener*>m_listeners;
  TScanParam m_brightness, m_contrast, m_threshold, m_dpi;
	int m_paperLeft;

public:
  TScannerDevice();
  virtual ~TScannerDevice() {}

  virtual void selectDevice() = 0;
  virtual bool isAreaSupported() = 0;

  void addListener(TScannerListener*);
  void removeListener(TScannerListener*);

  void notifyImageDone(const TRasterImageP &image);
  void notifyError();

  int getPaperLeftCount() const {return m_paperLeft;}
  void setPaperLeftCount(int count) {m_paperLeft = count;}
  void decrementPaperLeftCount() {--m_paperLeft;}
};

#endif
*/
