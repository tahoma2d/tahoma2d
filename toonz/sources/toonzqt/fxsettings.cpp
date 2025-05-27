

#include "toonzqt/fxsettings.h"
#include "toonzqt/gutil.h"
#include "toonzqt/keyframenavigator.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/fxhistogramrender.h"
#include "toonzqt/histogram.h"

#include "tmacrofx.h"
#include "tstream.h"
#include "tparamcontainer.h"
#include "tspectrumparam.h"
#include "tfxattributes.h"
#include "toutputproperties.h"
#include "pluginhost.h"
#include "tenv.h"
#include "tsystem.h"
#include "docklayout.h"

#include "toonz/tcamera.h"
#include "toonz/toonzfolders.h"
#include "toonz/tcolumnfx.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/scenefx.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/preferences.h"
#include "tw/stringtable.h"

#include <QVBoxLayout>
#include <QToolBar>
#include <QIcon>
#include <QAction>
#include <QStackedWidget>
#include <QLabel>
#include <QMap>
#include <QPainter>
#include <QCheckBox>
#include <QPushButton>

#include <QDesktopServices>
#include <QUrl>
#include <QGuiApplication>
#include <QScreen>

using namespace DVGui;

namespace {

TFxP getCurrentFx(const TFxP &currentFx, std::wstring actualId) {
  if (currentFx->getFxId() == actualId) return currentFx;
  int i;
  for (i = 0; i < currentFx->getInputPortCount(); i++) {
    TFxP fx = getCurrentFx(currentFx->getInputPort(i)->getFx(), actualId);
    if (fx.getPointer()) return fx;
  }
  return 0;
}

bool hasEmptyInputPort(const TFxP &currentFx) {
  if (!currentFx.getPointer()) return true;
  if (currentFx->getInputPortCount() == 0) return false;
  return hasEmptyInputPort(currentFx->getInputPort(0)->getFx());
}

// find the field by parameter name and register the field and its label widget
bool findItemByParamName(QLayout *layout, std::string name,
                         QList<QWidget *> &ret) {
  for (int i = 0; i < layout->count(); i++) {
    QLayoutItem *item = layout->itemAt(i);
    if (!item) continue;
    if (item->widget()) {
      ParamField *pf = dynamic_cast<ParamField *>(item->widget());
      if (pf && pf->getParamName().toStdString() == name) {
        ret.push_back(pf);
        if (i > 0 && layout->itemAt(i - 1)->widget()) {
          QLabel *label =
              dynamic_cast<QLabel *>(layout->itemAt(i - 1)->widget());
          if (label) ret.push_back(label);
        }
        return true;
      }
      // the widget may be a container of another layout
      else if (item->widget()->layout()) {
        if (findItemByParamName(item->widget()->layout(), name, ret))
          return true;
      }
    } else if (item->layout()) {
      if (findItemByParamName(item->layout(), name, ret)) return true;
    }
  }
  return false;
};
}  // namespace

//=============================================================================
// ParamViewer
//-----------------------------------------------------------------------------

ParamsPage::ParamsPage(QWidget *parent, ParamViewer *paramViewer)
    : QFrame(parent)
    , m_paramViewer(paramViewer)
    , m_horizontalLayout(NULL)
    , m_groupLayout(NULL) {
  m_fxHistogramRender = new FxHistogramRender();
  setFrameStyle(QFrame::StyledPanel);

  m_mainLayout = new QGridLayout(this);
  m_mainLayout->setContentsMargins(12, 12, 12, 12);
  m_mainLayout->setVerticalSpacing(10);
  m_mainLayout->setHorizontalSpacing(5);

  m_mainLayout->setColumnStretch(0, 0);
  m_mainLayout->setColumnStretch(1, 1);

  setLayout(m_mainLayout);
}

//-----------------------------------------------------------------------------

ParamsPage::~ParamsPage() {}

//-----------------------------------------------------------------------------

void ParamsPage::setPageField(TIStream &is, const TFxP &fx, bool isVertical) {
  // m_horizontalLayout dovrebbe essere stato inizializzato prima di entrare nel
  // metodo, per sicurezza verifico.
  if (isVertical == false && !m_horizontalLayout) {
    m_horizontalLayout = new QHBoxLayout();
    m_horizontalLayout->setContentsMargins(0, 0, 0, 0);
    m_horizontalLayout->setSpacing(5);
  }

  /*--
   * HBoxLayoutを挿入するとき、最初のパラメータ名はGridlayoutのColumn0に入れるため
   * --*/
  bool isFirstParamInRow = true;

  while (!is.matchEndTag()) {
    std::string tagName;
    if (!is.matchTag(tagName)) throw TException("expected tag");
    if (tagName == "control") {
      /*--- 設定ファイルからインタフェースの桁数を決める (PairSliderのみ実装。)
       * ---*/
      int decimals            = -1;
      std::string decimalsStr = is.getTagAttribute("decimals");
      if (decimalsStr != "") {
        decimals = QString::fromStdString(decimalsStr).toInt();
      }

      std::string name;
      is >> name;
      is.matchEndTag();

      /*-- Layout設定名とFxParameterの名前が一致するものを取得 --*/
      TParamP param = fx->getParams()->getParam(name);
      bool isHidden =
          (param) ? fx->getParams()->getParamVar(name)->isHidden() : true;

      if (param && !isHidden) {
        std::string paramName = fx->getFxType() + "." + name;
        QString str =
            QString::fromStdWString(TStringTable::translate(paramName));
        ParamField *field = ParamField::create(this, str, param);
        if (field) {
          if (decimals >= 0) field->setPrecision(decimals);
          m_fields.push_back(field);
          /*-- hboxタグに挟まれているとき --*/
          if (isVertical == false) {
            assert(m_horizontalLayout);
            QLabel *label = new QLabel(str, this);
            label->setObjectName("FxSettingsLabel");
            if (isFirstParamInRow) {
              int currentRow = m_mainLayout->rowCount();
              m_mainLayout->addWidget(label, currentRow, 0,
                                      Qt::AlignRight | Qt::AlignVCenter);
              isFirstParamInRow = false;
            } else
              m_horizontalLayout->addWidget(label, 0,
                                            Qt::AlignRight | Qt::AlignVCenter);
            m_horizontalLayout->addWidget(field);
            m_horizontalLayout->addSpacing(10);
          } else {
            int currentRow = m_mainLayout->rowCount();
            QLabel *label  = new QLabel(str, this);
            label->setObjectName("FxSettingsLabel");
            m_mainLayout->addWidget(label, currentRow, 0,
                                    Qt::AlignRight | Qt::AlignVCenter);
            m_mainLayout->addWidget(field, currentRow, 1);
          }
          connect(field, SIGNAL(currentParamChanged()), m_paramViewer,
                  SIGNAL(currentFxParamChanged()));
          connect(field, SIGNAL(actualParamChanged()), m_paramViewer,
                  SIGNAL(actualFxParamChanged()));
          connect(field, SIGNAL(paramKeyToggle()), m_paramViewer,
                  SIGNAL(paramKeyChanged()));
        }
      }
    } else if (tagName == "label") {
      std::string name;
      is >> name;
      is.matchEndTag();
      QString str = QString::fromStdWString(TStringTable::translate(name));
      if (isVertical == false) {
        assert(m_horizontalLayout);
        m_horizontalLayout->addWidget(new QLabel(str));
      } else {
        int currentRow = m_mainLayout->rowCount();
        m_mainLayout->addWidget(new QLabel(str), currentRow,
                                0, 1, 2);
      }
    } else if (tagName == "separator") {
      // <separator/> o <separator label="xxx"/>
      std::string label = is.getTagAttribute("label");
      QString str = QString::fromStdWString(TStringTable::translate(label));
      Separator *sep = new Separator(str, this);
      int currentRow = m_mainLayout->rowCount();
      m_mainLayout->addWidget(sep, currentRow, 0, 1, 2);
      m_mainLayout->setRowStretch(currentRow, 0);
    } else if (tagName == "histogram") {
      Histogram *histogram = new Histogram();
      m_fxHistogramRender->setHistograms(histogram->getHistograms());
      if (isVertical == false) {
        assert(m_horizontalLayout);
        m_horizontalLayout->addWidget(histogram);
      } else {
        int currentRow = m_mainLayout->rowCount();
        m_mainLayout->addWidget(histogram, currentRow, 0, 1, 2);
      }
    } else if (tagName == "test") {
      // <test/>
      // box->add(new WidgetBox(new TestSeparator(page)));
    } else if (tagName == "hbox") {
      int currentRow     = m_mainLayout->rowCount();
      m_horizontalLayout = new QHBoxLayout();
      m_horizontalLayout->setContentsMargins(0, 0, 0, 0);
      m_horizontalLayout->setSpacing(5);
      setPageField(is, fx, false);
      m_mainLayout->addLayout(m_horizontalLayout, currentRow, 1, 1, 2);
    } else if (tagName == "vbox") {
      int shrink                   = 0;
      std::string shrinkStr        = is.getTagAttribute("shrink");
      std::string modeSensitiveStr = is.getTagAttribute("modeSensitive");
      if (shrinkStr != "" || modeSensitiveStr != "") {
        QWidget *tmpWidget;
        if (shrinkStr != "") {
          shrink              = QString::fromStdString(shrinkStr).toInt();
          std::string label   = is.getTagAttribute("label");
          QString str         = QString::fromStdWString(TStringTable::translate(label));
          QCheckBox *checkBox = new QCheckBox(this);
          QHBoxLayout *sepLay = new QHBoxLayout();
          sepLay->setContentsMargins(0, 0, 0, 0);
          sepLay->setSpacing(5);
          sepLay->addWidget(checkBox, 0);
          sepLay->addWidget(new Separator(str, this),
                            1);
          int currentRow = m_mainLayout->rowCount();
          m_mainLayout->addLayout(sepLay, currentRow, 0, 1, 2);
          m_mainLayout->setRowStretch(currentRow, 0);
          tmpWidget = new ModeSensitiveBox(this, checkBox);
          checkBox->setChecked(shrink == 1);
          tmpWidget->setVisible(shrink == 1);
        } else {  // modeSensitiveStr != ""
          QList<int> modes;
          QStringList modeListStr =
              QString::fromStdString(is.getTagAttribute("mode"))
                  .split(',', Qt::SkipEmptyParts);
          for (QString modeNum : modeListStr) modes.push_back(modeNum.toInt());
          // find the mode combobox
          ModeChangerParamField *modeChanger = nullptr;
          for (int r = 0; r < m_mainLayout->rowCount(); r++) {
            QLayoutItem *li = m_mainLayout->itemAtPosition(r, 1);
            if (!li || !li->widget()) continue;
            ModeChangerParamField *field =
                dynamic_cast<ModeChangerParamField *>(li->widget());
            if (!field ||
                field->getParamName().toStdString() != modeSensitiveStr)
              continue;
            modeChanger = field;
            break;
          }
          // modeChanger may be in another vbox in the page
          if (!modeChanger) {
            QList<ModeChangerParamField *> allModeChangers =
                findChildren<ModeChangerParamField *>();
            for (auto field : allModeChangers) {
              if (field->getParamName().toStdString() == modeSensitiveStr) {
                modeChanger = field;
                break;
              }
            }
          }
          assert(modeChanger);
          tmpWidget = new ModeSensitiveBox(this, modeChanger, modes);
        }

        int currentRow           = m_mainLayout->rowCount();
        QGridLayout *keepMainLay = m_mainLayout;
        // temporary switch the layout
        m_mainLayout = new QGridLayout();
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_mainLayout->setVerticalSpacing(10);
        m_mainLayout->setHorizontalSpacing(5);
        m_mainLayout->setColumnStretch(0, 0);
        m_mainLayout->setColumnStretch(1, 1);
        setPageField(is, fx, true);

        tmpWidget->setLayout(m_mainLayout);
        // turn back the layout
        m_mainLayout = keepMainLay;
        m_mainLayout->addWidget(tmpWidget, currentRow, 0, 1, 2);
      } else
        setPageField(is, fx, true);
    }
    /*-- PixelParamFieldが２つあるとき、一方のRGB値を他方にコピーするボタン --*/
    else if (tagName == "rgb_link_button") {
      /*-- リンクさせたいパラメータを２つ得る --*/
      std::string name1, name2;
      is >> name1;
      is >> name2;
      is.matchEndTag();

      /*-- 既に作ってあるGUIを探索し、対応する２つを得て格納 --*/
      PixelParamField *ppf1 = 0;
      PixelParamField *ppf2 = 0;
      for (int r = 0; r < m_mainLayout->rowCount(); r++) {
        QLayoutItem *li = m_mainLayout->itemAtPosition(r, 1);
        if (!li) continue;
        QWidget *w = li->widget();
        if (!w) continue;

        ParamField *pf = dynamic_cast<ParamField *>(w);
        if (pf) {
          PixelParamField *ppf = dynamic_cast<PixelParamField *>(pf);
          if (ppf) {
            if (ppf1 == 0 && ppf->getParamName().toStdString() == name1)
              ppf1 = ppf;

            if (ppf2 == 0 && ppf->getParamName().toStdString() == name2)
              ppf2 = ppf;
          }
        }
      }
      if (ppf1 == 0 || ppf2 == 0) continue;

      /*-- ボタンのラベルのため 翻訳する --*/
      std::string paramName1 = fx->getFxType() + "." + name1;
      std::string paramName2 = fx->getFxType() + "." + name2;
      QString str1 =
          QString::fromStdWString(TStringTable::translate(paramName1));
      QString str2 =
          QString::fromStdWString(TStringTable::translate(paramName2));

      RgbLinkButtons *linkBut =
          new RgbLinkButtons(str1, str2, this, ppf1, ppf2);

      int currentRow = m_mainLayout->rowCount();
      m_mainLayout->addWidget(linkBut, currentRow, 1,
                              Qt::AlignLeft | Qt::AlignVCenter);
    }
    /*-- チェックボックスによって他のインタフェースを表示/非表示させる ---*/
    else if (tagName == "visibleToggle") {
      BoolParamField *controller_bpf = 0;
      QList<QWidget *> on_items;
      QList<QWidget *> off_items;
      while (!is.matchEndTag()) {
        std::string tagName;
        if (!is.matchTag(tagName)) throw TException("expected tag");

        if (tagName ==
                "controller" || /*-- 表示をコントロールするチェックボックス --*/
            tagName == "on" || /*-- ONのとき表示されるインタフェース --*/
            tagName == "off") /*-- OFFのとき表示されるインタフェース --*/
        {
          std::string name;
          is >> name;
          is.matchEndTag();

          QList<QWidget *> widgets;
          if (findItemByParamName(m_mainLayout, name, widgets)) {
            if (tagName == "controller") {
              controller_bpf = dynamic_cast<BoolParamField *>(widgets[0]);
            } else if (tagName == "on")
              on_items.append(widgets);
            else if (tagName == "off")
              off_items.append(widgets);
          }
        } else
          throw TException("unexpected tag " + tagName);
      }
      /*-- 表示コントロールをconnect --*/
      if (controller_bpf && (!on_items.isEmpty() || !off_items.isEmpty())) {
        /*-- ラベルとWidgetを両方表示/非表示 --*/
        for (int i = 0; i < on_items.size(); i++) {
          connect(controller_bpf, SIGNAL(toggled(bool)), on_items[i],
                  SLOT(setVisible(bool)));
          on_items[i]->hide();
        }
        for (int i = 0; i < off_items.size(); i++) {
          connect(controller_bpf, SIGNAL(toggled(bool)), off_items[i],
                  SLOT(setHidden(bool)));
          off_items[i]->show();
        }
        connect(controller_bpf, SIGNAL(toggled(bool)), this,
                SIGNAL(preferredPageSizeChanged()));
      } else
        std::cout << "controller_bpf NOT found!" << std::endl;
    } else
      throw TException("unexpected tag " + tagName);
  }

  if (isVertical == false && m_horizontalLayout) {
    m_horizontalLayout->addStretch(1);
  }
}

//-----------------------------------------------------------------------------
// add a slider for global control
void ParamsPage::addGlobalControl(const TFxP &fx) {
  if (!fx->getAttributes()->hasGlobalControl()) return;

  std::string name = "globalIntensity";

  TParamP param = fx->getParams()->getParam(name);
  if (!param) return;

  assert(param->hasUILabel());
  QString str       = QString::fromStdString(param->getUILabel());
  ParamField *field = ParamField::create(this, str, param);
  if (!field) return;

  int currentRow = m_mainLayout->rowCount();
  if (!m_fields.isEmpty()) {
    Separator *sep = new Separator("", this);
    m_mainLayout->addWidget(sep, currentRow, 0, 1, 2);
    m_mainLayout->setRowStretch(currentRow, 0);
    currentRow = m_mainLayout->rowCount();
  }

  m_fields.push_back(field);
  QLabel *label = new QLabel(str, this);
  label->setObjectName("FxSettingsLabel");
  m_mainLayout->addWidget(label, currentRow, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
  m_mainLayout->addWidget(field, currentRow, 1);

  connect(field, SIGNAL(currentParamChanged()), m_paramViewer,
          SIGNAL(currentFxParamChanged()));
  connect(field, SIGNAL(actualParamChanged()), m_paramViewer,
          SIGNAL(actualFxParamChanged()));
  connect(field, SIGNAL(paramKeyToggle()), m_paramViewer,
          SIGNAL(paramKeyChanged()));
}

//-----------------------------------------------------------------------------

void ParamsPage::setPageSpace() {
  if (m_fields.count() != 0) {

    int currentRow = m_mainLayout->rowCount();
    for (int i = 0; i < currentRow; i++) m_mainLayout->setRowStretch(i, 0);
    m_mainLayout->setRowStretch(currentRow, 1);
  }
}

//-----------------------------------------------------------------------------
void ParamsPage::beginGroup(const char *name) {
  m_groupLayout = new QGridLayout();

  QGroupBox *group = new QGroupBox(QString::fromUtf8(name), this);
  group->setLayout(m_groupLayout);
  m_mainLayout->addWidget(group, m_mainLayout->rowCount(), 0, 1, 2);
}

void ParamsPage::endGroup() { m_groupLayout = NULL; }

void ParamsPage::addWidget(QWidget *field, bool isVertical) {
  QLabel *label  = NULL;
  ParamField *pf = qobject_cast<ParamField *>(field);
  if (pf) {
    label = new QLabel(pf->getUIName(), this);
    label->setObjectName("FxSettingsLabel");
    if (!pf->getDescription().isEmpty())
      label->setToolTip(pf->getDescription());
  }

  if (isVertical) {
    if (m_groupLayout) {
      int row = m_groupLayout->rowCount();
      if (label)
        m_groupLayout->addWidget(label, row, 0,
                                 Qt::AlignRight | Qt::AlignVCenter);
      m_groupLayout->addWidget(field, row, 1);
    } else {
      int row = m_mainLayout->rowCount();
      if (label)
        m_mainLayout->addWidget(label, row, 0,
                                Qt::AlignRight | Qt::AlignVCenter);
      m_mainLayout->addWidget(field, row, 1);
    }
  } else {
    if (!m_horizontalLayout) {
      m_horizontalLayout = new QHBoxLayout();
      m_horizontalLayout->setContentsMargins(0, 0, 0, 0);
      m_horizontalLayout->setSpacing(5);
    }
    m_horizontalLayout->addWidget(field);
  }
}

#define TOONZ_DEFINE_NEW_COMPONENT(NAME, MAKE)                                 \
  QWidget *ParamsPage::NAME(TFx *fx, const char *name) {                       \
    TParamP param = fx->getParams()->getParam(name);                           \
    if (!param) return NULL;                                                   \
    QString const paramName =                                                  \
        QString::fromStdString(fx->getFxType() + "." + name);                  \
    ParamField *field = MAKE(this, paramName, param);                          \
    if (!field) return NULL;                                                   \
    m_fields.push_back(field);                                                 \
    connect(field, SIGNAL(currentParamChanged()), m_paramViewer,               \
            SIGNAL(currentFxParamChanged()));                                  \
    connect(field, SIGNAL(actualParamChanged()), m_paramViewer,                \
            SIGNAL(actualFxParamChanged()));                                   \
    connect(field, SIGNAL(paramKeyToggle()), m_paramViewer,                    \
            SIGNAL(paramKeyChanged()));                                        \
    return field;                                                              \
  }

TOONZ_DEFINE_NEW_COMPONENT(newParamField, ParamField::create);
TOONZ_DEFINE_NEW_COMPONENT(newLineEdit, make_lineedit);
TOONZ_DEFINE_NEW_COMPONENT(newSlider, make_slider);
TOONZ_DEFINE_NEW_COMPONENT(newSpinBox, make_spinbox);
TOONZ_DEFINE_NEW_COMPONENT(newCheckBox, make_checkbox);
TOONZ_DEFINE_NEW_COMPONENT(newRadioButton, make_radiobutton);
TOONZ_DEFINE_NEW_COMPONENT(newComboBox, make_combobox);

#undef TOONZ_DEFINE_NEW_COMPONENT

//-----------------------------------------------------------------------------

void ParamsPage::setFx(const TFxP &currentFx, const TFxP &actualFx, int frame) {
  assert(currentFx);
  assert(actualFx);
  for (int i = 0; i < (int)m_fields.size(); i++) {
    ParamField *field = m_fields[i];
    QString fieldName = field->getParamName();
    TFxP fx           = getCurrentFx(currentFx, actualFx->getFxId());
    assert(fx.getPointer());
    TParamP currentParam =
        currentFx->getParams()->getParam(fieldName.toStdString());
    TParamP actualParam =
        actualFx->getParams()->getParam(fieldName.toStdString());
    assert(currentParam);
    assert(actualParam);
    field->setParam(currentParam, actualParam, frame);
  }
  if (actualFx->getInputPortCount() > 0)
    m_fxHistogramRender->computeHistogram(actualFx->getInputPort(0)->getFx(),
                                          frame);
}

//-----------------------------------------------------------------------------

void ParamsPage::setPointValue(int index, const TPointD &p) {
  if (0 <= index && index < (int)m_fields.size())
    m_fields[index]->setPointValue(p);
}

//-----------------------------------------------------------------------------

void ParamsPage::update(int frame) {
  int i;
  for (i = 0; i < (int)m_fields.size(); i++) {
    ParamField *field = m_fields[i];
    field->update(frame);
  }
}

//-----------------------------------------------------------------------------

namespace {

QSize getItemSize(QLayoutItem *item) {
  // layout case
  QHBoxLayout *hLay = dynamic_cast<QHBoxLayout *>(item->layout());
  if (hLay) {
    int tmpWidth = 0, tmpHeight = 0;
    for (int c = 0; c < hLay->count(); c++) {
      QLayoutItem *subItem = hLay->itemAt(c);
      if (!subItem) continue;
      QSize subItemSize = getItemSize(subItem);
      tmpWidth += subItemSize.width();
      if (tmpHeight < subItemSize.height()) tmpHeight = subItemSize.height();
    }
    tmpWidth += (hLay->count() - 1) * 5;
    return QSize(tmpWidth, tmpHeight);
  }

  ParamField *pF = dynamic_cast<ParamField *>(item->widget());
  if (pF) return pF->getPreferredSize();

  Separator *sep = dynamic_cast<Separator *>(item->widget());
  if (sep) return QSize(0, 16);

  Histogram *histo = dynamic_cast<Histogram *>(item->widget());
  if (histo) return QSize(278, 162);

  RgbLinkButtons *linkBut = dynamic_cast<RgbLinkButtons *>(item->widget());
  if (linkBut) return QSize(0, 21);

  return QSize();
}

void updateMaximumPageSize(QGridLayout *layout, int &maxLabelWidth,
                           int &maxWidgetWidth, int &fieldsHeight) {
  /*-- Label側の横幅の最大値を得る --*/
  for (int r = 0; r < layout->rowCount(); r++) {
    /*-- アイテムが無ければ次の行へ --*/
    if (!layout->itemAtPosition(r, 0)) continue;
    /*-- ラベルの横幅を得て、最大値を更新していく --*/
    QLabel *label =
        dynamic_cast<QLabel *>(layout->itemAtPosition(r, 0)->widget());
    QGroupBox *gBox =
        dynamic_cast<QGroupBox *>(layout->itemAtPosition(r, 0)->widget());
    if (label) {
      int tmpWidth = label->fontMetrics().horizontalAdvance(label->text());
      if (maxLabelWidth < tmpWidth) maxLabelWidth = tmpWidth;
    }
    /*-- PlugInFxのGroupパラメータのサイズ --*/
    else if (gBox) {
      QGridLayout *gridLay = dynamic_cast<QGridLayout *>(gBox->layout());
      if (gridLay) {
        updateMaximumPageSize(gridLay, maxLabelWidth, maxWidgetWidth,
                              fieldsHeight);
        /*-- GroupBoxのマージン --*/
        maxLabelWidth += 10;
        maxWidgetWidth += 10;
        fieldsHeight += 20;
      }
    }
  }

  int itemCount = 0;
  /*-- Widget側の最適な縦サイズおよび横幅の最大値を得る --*/
  for (int r = 0; r < layout->rowCount(); r++) {
    /*-- Column1にある可能性のあるもの：ParamField, Histogram, Layout,
     * RgbLinkButtons --*/

    QLayoutItem *item = layout->itemAtPosition(r, 1);
    if (!item || (item->widget() && item->widget()->isHidden())) continue;

    ModeSensitiveBox *box = dynamic_cast<ModeSensitiveBox *>(item->widget());
    if (box) {
      if (!box->isActive()) continue;
      // if (box->isHidden()) continue;
      QGridLayout *innerLay = dynamic_cast<QGridLayout *>(box->layout());
      if (!innerLay) continue;
      int tmpHeight = 0;
      updateMaximumPageSize(innerLay, maxLabelWidth, maxWidgetWidth, tmpHeight);
      fieldsHeight += tmpHeight;

      innerLay->setColumnMinimumWidth(0, maxLabelWidth);
      continue;
    }

    QSize itemSize = getItemSize(item);
    if (maxWidgetWidth < itemSize.width()) maxWidgetWidth = itemSize.width();
    fieldsHeight += itemSize.height();
    itemCount++;
  }

  if (itemCount >= 1) fieldsHeight += itemCount * 10;
}
};  // namespace

QSize ParamsPage::getPreferredSize() {
  int maxLabelWidth  = 0;
  int maxWidgetWidth = 0;
  int fieldsHeight   = 0;

  updateMaximumPageSize(m_mainLayout, maxLabelWidth, maxWidgetWidth,
                        fieldsHeight);
  int lmargin, tmargin, rmargin, bmargin;
  m_mainLayout->getContentsMargins(&lmargin, &tmargin, &rmargin, &bmargin);
  return QSize(
      maxLabelWidth + maxWidgetWidth + m_mainLayout->horizontalSpacing() +
          lmargin + rmargin,
      fieldsHeight + tmargin + bmargin + 31 /* spacing for the swatch */);
}

//=============================================================================
// ParamsPageSet
//-----------------------------------------------------------------------------

ParamsPageSet::ParamsPageSet(QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , m_preferredSize(0, 0)
    , m_helpFilePath("")
    , m_helpCommand("") {
  // TabBar
  m_tabBar = new TabBar(this);
  // This widget is used to set the background color of the tabBar
  // using the styleSheet and to draw the two lines on the bottom size.
  m_tabBarContainer = new TabBarContainter(this);
  m_pagesList       = new QStackedWidget(this);

  m_helpButton = new QPushButton(tr(""), this);
  m_helpButton->setIconSize(QSize(20, 20));
  m_helpButton->setIcon(createQIcon("help"));
  m_helpButton->setFixedWidth(28);
  m_helpButton->setToolTip(tr("View help page"));

  m_parent = dynamic_cast<ParamViewer *>(parent);
  m_pageFxIndexTable.clear();
  m_tabBar->setDrawBase(false);
  m_tabBar->setObjectName("FxSettingsTabBar");
  m_helpButton->setFixedHeight(20);
  m_helpButton->setObjectName("FxSettingsHelpButton");
  m_helpButton->setFocusPolicy(Qt::NoFocus);

  m_warningMark = new QLabel(this);
  static QIcon warningIcon(":Resources/paramignored_on.svg");
  m_warningMark->setPixmap(warningIcon.pixmap(QSize(22, 22)));
  m_warningMark->setFixedSize(22, 22);
  m_warningMark->setStyleSheet(
      "margin: 0px; padding: 0px; background-color: rgba(0,0,0,0);");

  //----layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  {
    QHBoxLayout *hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->addSpacing(0);
    {
      hLayout->addWidget(m_tabBar);
      hLayout->addWidget(m_warningMark);
      hLayout->addStretch(1);
      hLayout->addWidget(m_helpButton);
    }
    m_tabBarContainer->setLayout(hLayout);

    mainLayout->addWidget(m_tabBarContainer);

    mainLayout->addWidget(m_pagesList);

    Separator *sep = new Separator("", this);
    sep->setMaximumHeight(0);
    mainLayout->addWidget(sep, 0);
  }
  setLayout(mainLayout);

  connect(m_tabBar, SIGNAL(currentChanged(int)), this, SLOT(setPage(int)));

  m_helpButton->hide();
}

//-----------------------------------------------------------------------------

ParamsPageSet::~ParamsPageSet() {}

//-----------------------------------------------------------------------------

void ParamsPageSet::setPage(int index) {
  if (m_tabBar->count() == 0 || m_pagesList->count() == 0) return;
  assert(index >= 0 && index < m_pagesList->count());
  m_pagesList->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void ParamsPageSet::setFx(const TFxP &currentFx, const TFxP &actualFx,
                          int frame) {
  TMacroFx *currentFxMacro = dynamic_cast<TMacroFx *>(currentFx.getPointer());
  if (currentFxMacro) {
    TMacroFx *actualFxMacro = dynamic_cast<TMacroFx *>(actualFx.getPointer());
    assert(actualFxMacro);
    const std::vector<TFxP> &currentFxMacroFxs = currentFxMacro->getFxs();
    const std::vector<TFxP> &actualFxMacroFxs  = actualFxMacro->getFxs();
    assert(currentFxMacroFxs.size() == actualFxMacroFxs.size());
    for (int i = 0; i < m_pagesList->count(); i++) {
      ParamsPage *page = getParamsPage(i);
      if (!page || !m_pageFxIndexTable.contains(page)) continue;
      int index = m_pageFxIndexTable[page];
      page->setFx(currentFxMacroFxs[index], actualFxMacroFxs[index], frame);
    }
  } else {
    for (int i = 0; i < m_pagesList->count(); i++) {
      ParamsPage *page = getParamsPage(i);
      if (!page) continue;
      page->setFx(currentFx, actualFx, frame);
    }
  }
}

//-----------------------------------------------------------------------------

void ParamsPageSet::updatePage(int frame, bool onlyParam) {
  if (!m_pagesList) return;
  int i;
  for (i = 0; i < m_pagesList->count(); i++) {
    ParamsPage *page = getParamsPage(i);
    if (!page) continue;
    page->update(frame);
    if (!onlyParam) page->getFxHistogramRender()->invalidateFrame(frame);
  }
}

//-----------------------------------------------------------------------------

void ParamsPageSet::setScene(ToonzScene *scene) {
  if (!m_pagesList) return;
  int i;
  for (i = 0; i < m_pagesList->count(); i++) {
    ParamsPage *page = getParamsPage(i);
    if (!page) continue;
    page->getFxHistogramRender()->setScene(scene);
  }
}

//-----------------------------------------------------------------------------

void ParamsPageSet::setIsCameraViewMode(bool isCameraViewMode) {
  if (!m_pagesList) return;
  int i;
  for (i = 0; i < m_pagesList->count(); i++) {
    ParamsPage *page = getParamsPage(i);
    if (!page) continue;
    page->getFxHistogramRender()->setIsCameraViewMode(isCameraViewMode);
  }
}

//-----------------------------------------------------------------------------

ParamsPage *ParamsPageSet::createParamsPage() {
  return new ParamsPage(this, m_parent);
}

void ParamsPageSet::addParamsPage(ParamsPage *page, const char *name) {
  /*-- このFxで最大サイズのページに合わせてダイアログをリサイズ --*/
  QSize pagePreferredSize = page->getPreferredSize();
  m_preferredSize         = m_preferredSize.expandedTo(
      pagePreferredSize + QSize(m_tabBarContainer->height() + 2,
                                        2)); /*-- 2は上下左右のマージン --*/

  QScrollArea *pane = new QScrollArea(this);
  pane->setWidgetResizable(true);
  pane->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  pane->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  pane->setWidget(page);

  m_tabBar->addSimpleTab(QString::fromUtf8(name));
  m_pagesList->addWidget(pane);
}

//-----------------------------------------------------------------------------

void ParamsPageSet::createControls(const TFxP &fx, int index) {
  if (TMacroFx *macroFx = dynamic_cast<TMacroFx *>(fx.getPointer())) {
    const std::vector<TFxP> &fxs = macroFx->getFxs();
    for (int i = 0; i < (int)fxs.size(); i++) createControls(fxs[i], i);
    return;
  }
  if (RasterFxPluginHost *plugin =
          dynamic_cast<RasterFxPluginHost *>(fx.getPointer())) {
    plugin->build(this);
    std::string url = plugin->getUrl();
    if (!url.empty()) {
      connect(m_helpButton, SIGNAL(pressed()), this, SLOT(openHelpUrl()));
      m_helpButton->show();
      m_helpUrl = url;
    }
    return;
  }

  TFilePath fp = ToonzFolder::getProfileFolder() + "layouts" + "fxs" +
                 (fx->getFxType() + ".xml");

  TIStream is(fp);
  if (!is) return;

  if (fx->getParams()->getParamCount()) {
    try {
      std::string tagName;
      if (!is.matchTag(tagName) || tagName != "fxlayout")
        throw TException("expected <fxlayout>");

      m_helpFilePath = is.getTagAttribute("help_file");
      if (m_helpFilePath != "") {
        connect(m_helpButton, SIGNAL(pressed()), this, SLOT(openHelpFile()));
        m_helpButton->show();
        /*-- pdfファイルのページ指定など、引数が必要な場合の追加fragmentを取得
         * --*/
        m_helpCommand = is.getTagAttribute("help_command");
      }

      while (!is.matchEndTag()) createPage(is, fx, index);
    } catch (TException const &) {
    } catch (...) {
    }
  }
  // else createEmptyPage();
}

//-----------------------------------------------------------------------------

ParamsPage *ParamsPageSet::getCurrentParamsPage() const {
  QScrollArea *scrollAreaPage =
      dynamic_cast<QScrollArea *>(m_pagesList->currentWidget());
  assert(scrollAreaPage);
  return dynamic_cast<ParamsPage *>(scrollAreaPage->widget());
}

//-----------------------------------------------------------------------------

ParamsPage *ParamsPageSet::getParamsPage(int index) const {
  QScrollArea *scrollAreaPage =
      dynamic_cast<QScrollArea *>(m_pagesList->widget(index));
  assert(scrollAreaPage);
  return dynamic_cast<ParamsPage *>(scrollAreaPage->widget());
}

//-----------------------------------------------------------------------------

void ParamsPageSet::createPage(TIStream &is, const TFxP &fx, int index) {
  std::string tagName;
  if (!is.matchTag(tagName) || tagName != "page")
    throw TException("expected <page>");
  std::string pageName = is.getTagAttribute("name");
  if (pageName == "") pageName = "page";

  ParamsPage *paramsPage = new ParamsPage(this, m_parent);

  bool isFirstPageOfFx;
  if (index < 0)
    isFirstPageOfFx = (m_pagesList->count() == 0);
  else  // macro fx case
    isFirstPageOfFx = !(m_pageFxIndexTable.values().contains(index));

  paramsPage->setPage(is, fx, isFirstPageOfFx);

  connect(paramsPage, SIGNAL(preferredPageSizeChanged()), this,
          SLOT(recomputePreferredSize()));

  /*-- このFxで最大サイズのページに合わせてダイアログをリサイズ --*/
  QSize pagePreferredSize = paramsPage->getPreferredSize();
  m_preferredSize         = m_preferredSize.expandedTo(
      pagePreferredSize + QSize(m_tabBarContainer->height() + 2,
                                        2)); /*-- 2は上下左右のマージン --*/

  QScrollArea *scrollAreaPage = new QScrollArea(this);
  scrollAreaPage->setWidgetResizable(true);
  scrollAreaPage->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scrollAreaPage->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scrollAreaPage->setWidget(paramsPage);

  QString str = QString::fromStdWString(TStringTable::translate(pageName));
  m_tabBar->addSimpleTab(str);
  m_pagesList->addWidget(scrollAreaPage);
  if (index >= 0) m_pageFxIndexTable[paramsPage] = index;
}

//-----------------------------------------------------------------------------

void ParamsPageSet::recomputePreferredSize() {
  QSize newSize(0, 0);
  for (int i = 0; i < m_pagesList->count(); i++) {
    QScrollArea *area = dynamic_cast<QScrollArea *>(m_pagesList->widget(i));
    if (!area) continue;
    ParamsPage *page = dynamic_cast<ParamsPage *>(area->widget());
    if (!page) continue;
    QSize pagePreferredSize = page->getPreferredSize();
    newSize                 = newSize.expandedTo(pagePreferredSize +
                                                 QSize(m_tabBarContainer->height() + 2, 2));
  }
  if (!newSize.isEmpty()) {
    m_preferredSize = newSize;
    // resize the parent FxSettings
    m_parent->notifyPreferredSizeChanged(m_preferredSize + QSize(2, 50));
  }
}

//-----------------------------------------------------------------------------

/* TODO: Webサイト内のヘルプに対応すべきか検討 2016.02.01 shun_iwasawa */
void ParamsPageSet::openHelpFile() {
  if (m_helpFilePath == "") return;

  // if (m_helpCommand != "")
  //	commandString += m_helpCommand + " ";

  // Get UI language as set in "Preferences"
  QString currentLanguage = Preferences::instance()->getCurrentLanguage();
  std::string helpDocLang = currentLanguage.toStdString();

  // Assume associated language subdir exists
  TFilePath helpFp = TEnv::getStuffDir() + "doc" + helpDocLang + m_helpFilePath;

  // Verify subdir exists; if not, default to standard doc dir
  if (!TFileStatus(helpFp).doesExist()) {
    helpFp = TEnv::getStuffDir() + "doc" + m_helpFilePath;
  }

  // commandString +=
  // QString::fromStdWString(helpFp.getWideString()).toStdString();
  // QString command = QString::fromStdString(m_helpFilePath);
  // system(commandString.c_str());
  // QProcess process;
  // process.start(command);
  QDesktopServices::openUrl(
      QUrl::fromLocalFile(QString::fromStdWString(helpFp.getWideString())));
}

void ParamsPageSet::openHelpUrl() {
  QDesktopServices::openUrl(QUrl(QString(m_helpUrl.c_str())));
}

void ParamsPageSet::updateWarnings(const TFxP &currentFx, bool isFloat) {
  if (!isFloat) {
    m_warningMark->hide();
    return;
  }

  bool isFloatSupported = true;

  TMacroFx *currentFxMacro = dynamic_cast<TMacroFx *>(currentFx.getPointer());
  if (currentFxMacro) {
    const std::vector<TFxP> &currentFxMacroFxs = currentFxMacro->getFxs();
    for (auto fxP : currentFxMacroFxs) {
      TRasterFx *rasFx = dynamic_cast<TRasterFx *>(fxP.getPointer());
      if (rasFx) {
        isFloatSupported = isFloatSupported & rasFx->canComputeInFloat();
      }
      if (!isFloatSupported) break;
    }
  } else {
    TRasterFx *rasFx = dynamic_cast<TRasterFx *>(currentFx.getPointer());
    if (rasFx) {
      isFloatSupported = rasFx->canComputeInFloat();
    }
  }

  bool showFloatWarning = isFloat && !isFloatSupported;
  if (!showFloatWarning) {
    m_warningMark->hide();
    return;
  }

  QString warningTxt;
  if (showFloatWarning) {
    warningTxt +=
        tr("This Fx does not support rendering in floating point channel width "
           "(32bit).\n"
           "The output pixel values from this fx will be clamped to 0.0 - 1.0\n"
           "and tone may be slightly discretized.");
  }
  m_warningMark->setToolTip(warningTxt);
  m_warningMark->show();
}

//=============================================================================
// ParamViewer
//-----------------------------------------------------------------------------

ParamViewer::ParamViewer(QWidget *parent, Qt::WindowFlags flags)
    : QFrame(parent, flags), m_fx(0) {
  m_tablePageSet = new QStackedWidget(this);
  m_tablePageSet->addWidget(new QWidget());

  /*-- SwatchViewerを表示/非表示するボタン --*/
  QPushButton *showSwatchButton = new QPushButton("", this);
  QLabel *swatchLabel           = new QLabel(tr("Swatch Viewer"), this);

  swatchLabel->setObjectName("TitleTxtLabel");
  showSwatchButton->setObjectName("menuToggleButton");
  showSwatchButton->setFixedSize(15, 15);
  showSwatchButton->setIcon(createQIcon("menu_toggle"));
  showSwatchButton->setCheckable(true);
  showSwatchButton->setChecked(false);
  showSwatchButton->setFocusPolicy(Qt::NoFocus);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  {
    mainLayout->addWidget(m_tablePageSet, 1);

    QHBoxLayout *showPreviewButtonLayout = new QHBoxLayout();
    showPreviewButtonLayout->setContentsMargins(3, 3, 3, 3);
    showPreviewButtonLayout->setSpacing(3);
    {
      showPreviewButtonLayout->addWidget(showSwatchButton, 0);
      showPreviewButtonLayout->addWidget(swatchLabel, 0);
      showPreviewButtonLayout->addStretch(1);
    }
    mainLayout->addLayout(showPreviewButtonLayout, 0);
  }
  setLayout(mainLayout);

  connect(showSwatchButton, SIGNAL(toggled(bool)), this,
          SIGNAL(showSwatchButtonToggled(bool)));
}

//-----------------------------------------------------------------------------

ParamViewer::~ParamViewer() {}

//-----------------------------------------------------------------------------

void ParamViewer::setFx(const TFxP &currentFx, const TFxP &actualFx, int frame,
                        ToonzScene *scene) {
  if (!actualFx) {
    m_tablePageSet->setCurrentIndex(0);
    return;
  }
  std::string name = actualFx->getFxType();
  if (name == "macroFx") {
    TMacroFx *macroFx = dynamic_cast<TMacroFx *>(currentFx.getPointer());
    if (macroFx) name = macroFx->getMacroFxType();
  } else {
    name += std::to_string(actualFx->getFxVersion());
  }

  int currentIndex = -1;

  QMap<std::string, int>::iterator it;
  it = m_tableFxIndex.find(name);
  if (it == m_tableFxIndex.end()) {
    ParamsPageSet *pageSet = new ParamsPageSet(this);
    currentIndex           = m_tablePageSet->addWidget(pageSet);
    m_tableFxIndex[name]   = currentIndex;
    pageSet->createControls(actualFx);
  } else
    currentIndex = it.value();

  assert(currentIndex >= 0);

  m_tablePageSet->setCurrentIndex(currentIndex);

  getCurrentPageSet()->setScene(scene);

  if (m_fx != currentFx) {
    getCurrentPageSet()->setFx(currentFx, actualFx, frame);
    if (m_actualFx != actualFx) {
      m_actualFx = actualFx;
      QSize pageViewerPreferredSize =
          getCurrentPageSet()->getPreferredSize() + QSize(2, 50);
      emit preferredSizeChanged(pageViewerPreferredSize);
    }
  }
}

//-----------------------------------------------------------------------------

void ParamViewer::setScene(ToonzScene *scene) {
  ParamsPageSet *paramsPageSet = getCurrentPageSet();
  if (!paramsPageSet) return;
  paramsPageSet->setScene(scene);
}

//-----------------------------------------------------------------------------

void ParamViewer::setIsCameraViewMode(bool isCameraViewMode) {
  ParamsPageSet *paramsPageSet = getCurrentPageSet();
  if (!paramsPageSet) return;
  paramsPageSet->setIsCameraViewMode(isCameraViewMode);
}

//-----------------------------------------------------------------------------

void ParamViewer::update(int frame, bool onlyParam) {
  ParamsPageSet *paramsPageSet = getCurrentPageSet();
  if (!paramsPageSet) return;
  paramsPageSet->updatePage(frame, onlyParam);
}

//-----------------------------------------------------------------------------

void ParamViewer::setPointValue(int index, const TPointD &p) {
  // Search the index-th param among all pages
  ParamsPageSet *pageSet = getCurrentPageSet();
  ParamsPage *page       = 0;

  for (int i = 0; i < pageSet->getParamsPageCount(); ++i) {
    page            = pageSet->getParamsPage(i);
    int paramsCount = page->m_fields.count();

    if (index <= paramsCount) break;
    index -= paramsCount;
  }

  if (page) page->setPointValue(index, p);
}

//-----------------------------------------------------------------------------

ParamsPageSet *ParamViewer::getCurrentPageSet() const {
  return dynamic_cast<ParamsPageSet *>(m_tablePageSet->currentWidget());
}

//-----------------------------------------------------------------------------
// show warning if the current Fx does not support float / linear rendering
void ParamViewer::updateWarnings(const TFxP &currentFx, bool isFloat) {
  if (getCurrentPageSet())
    getCurrentPageSet()->updateWarnings(currentFx, isFloat);
}

//=============================================================================
// FxSettings
//-----------------------------------------------------------------------------

FxSettings::FxSettings(QWidget *parent, const TPixel32 &checkCol1,
                       const TPixel32 &checkCol2)
    : QSplitter(Qt::Vertical, parent)
    , m_frameHandle(0)
    , m_fxHandle(0)
    , m_xsheetHandle(0)
    , m_sceneHandle(0)
    , m_levelHandle(0)
    , m_objectHandle(0)
    , m_checkCol1(checkCol1)
    , m_checkCol2(checkCol2)
    , m_isCameraModeView(false)
    , m_container_height(184)
    , m_container_width(390) {
  // param viewer
  m_paramViewer = new ParamViewer(this);
  // swatch
  QWidget *swatchContainer = new QWidget(this);
  m_viewer                 = new SwatchViewer(swatchContainer);
  setWhiteBg();
  createToolBar();

  m_paramViewer->setMinimumHeight(50);
  swatchContainer->setSizePolicy(QSizePolicy::MinimumExpanding,
                                 QSizePolicy::MinimumExpanding);

  //---layout
  addWidget(m_paramViewer);

  QVBoxLayout *swatchLayout = new QVBoxLayout(swatchContainer);
  swatchLayout->setContentsMargins(0, 0, 0, 0);
  swatchLayout->setSpacing(0);
  {
    swatchLayout->addWidget(m_viewer, 0, Qt::AlignHCenter);

    QHBoxLayout *toolBarLayout = new QHBoxLayout();
    {
      toolBarLayout->addWidget(m_toolBar, 0,
                               Qt::AlignHCenter | Qt::AlignBottom);
    }
    swatchLayout->addLayout(toolBarLayout);
  }
  swatchContainer->setLayout(swatchLayout);

  addWidget(swatchContainer);

  //---signal-slot connections
  bool ret = true;
  ret      = ret && connect(m_paramViewer, SIGNAL(currentFxParamChanged()),
                            SLOT(updateViewer()));
  ret      = ret &&
        connect(m_viewer, SIGNAL(pointPositionChanged(int, const TPointD &)),
                SLOT(onPointChanged(int, const TPointD &)));
  ret = ret && connect(m_paramViewer, SIGNAL(preferredSizeChanged(QSize)), this,
                       SLOT(onPreferredSizeChanged(QSize)));
  ret = ret && connect(m_paramViewer, SIGNAL(showSwatchButtonToggled(bool)),
                       this, SLOT(onShowSwatchButtonToggled(bool)));
  assert(ret);

  swatchContainer->hide();

  // Swatch updates should happen only at the end of a separator resize op.
  setStretchFactor(0, 1);
  setStretchFactor(1, 0);
  setOpaqueResize(false);
}

//-----------------------------------------------------------------------------

FxSettings::~FxSettings() {}

//-----------------------------------------------------------------------------

void FxSettings::setFxHandle(TFxHandle *fxHandle) {
  m_fxHandle = fxHandle;
  m_keyframeNavigator->setFxHandle(m_fxHandle);

  ParamField::setFxHandle(m_fxHandle);
}

//-----------------------------------------------------------------------------

void FxSettings::setFrameHandle(TFrameHandle *frameHandle) {
  m_frameHandle = frameHandle;
  m_keyframeNavigator->setFrameHandle(m_frameHandle);
  m_frameNavigator->setFrameHandle(m_frameHandle);
}

//-----------------------------------------------------------------------------

void FxSettings::setXsheetHandle(TXsheetHandle *xsheetHandle) {
  m_xsheetHandle = xsheetHandle;
}

//-----------------------------------------------------------------------------

void FxSettings::setSceneHandle(TSceneHandle *sceneHandle) {
  m_sceneHandle = sceneHandle;
  setCurrentScene();
}

//-----------------------------------------------------------------------------

void FxSettings::setLevelHandle(TXshLevelHandle *levelHandle) {
  m_levelHandle = levelHandle;
}

//-----------------------------------------------------------------------------

void FxSettings::setObjectHandle(TObjectHandle *objectHandle) {
  m_objectHandle = objectHandle;
}

//-----------------------------------------------------------------------------

void FxSettings::createToolBar() {
  m_toolBar = new QToolBar(this);
  m_toolBar->setMovable(false);
  m_toolBar->setFixedHeight(24);
  m_toolBar->setIconSize(QSize(20, 20));
  m_toolBar->setObjectName("MediumPaddingToolBar");
  // m_toolBar->setSizePolicy(QSizePolicy::MinimumExpanding,
  // QSizePolicy::MinimumExpanding);

  // View mode
  QActionGroup *viewModeActGroup = new QActionGroup(m_toolBar);
  viewModeActGroup->setExclusive(false);
  // camera
  QIcon camera       = createQIcon("camera");
  QAction *cameraAct = new QAction(camera, tr("&Camera Preview"), m_toolBar);
  cameraAct->setCheckable(true);
  viewModeActGroup->addAction(cameraAct);
  m_toolBar->addAction(cameraAct);
  // preview
  QIcon preview       = createQIcon("preview");
  QAction *previewAct = new QAction(preview, tr("&Preview"), m_toolBar);
  previewAct->setCheckable(true);
  viewModeActGroup->addAction(previewAct);
  m_toolBar->addAction(previewAct);
  connect(viewModeActGroup, SIGNAL(triggered(QAction *)),
          SLOT(onViewModeChanged(QAction *)));

  m_toolBar->addSeparator();

  QActionGroup *viewModeGroup = new QActionGroup(m_toolBar);
  viewModeGroup->setExclusive(true);

  QAction *whiteBg = new QAction(createQIcon("preview_white"),
                                 tr("&White Background"), m_toolBar);
  whiteBg->setCheckable(true);
  whiteBg->setChecked(true);
  viewModeGroup->addAction(whiteBg);
  connect(whiteBg, SIGNAL(triggered()), this, SLOT(setWhiteBg()));
  m_toolBar->addAction(whiteBg);

  QAction *blackBg = new QAction(createQIcon("preview_black"),
                                 tr("&Black Background"), m_toolBar);
  blackBg->setCheckable(true);
  viewModeGroup->addAction(blackBg);
  connect(blackBg, SIGNAL(triggered()), this, SLOT(setBlackBg()));
  m_toolBar->addAction(blackBg);

  m_checkboardBg = new QAction(createQIcon("preview_checkboard"),
                               tr("&Checkered Background"), m_toolBar);
  m_checkboardBg->setCheckable(true);
  viewModeGroup->addAction(m_checkboardBg);
  connect(m_checkboardBg, SIGNAL(triggered()), this, SLOT(setCheckboardBg()));
  m_toolBar->addAction(m_checkboardBg);

  m_toolBar->addSeparator();

  m_keyframeNavigator = new FxKeyframeNavigator(m_toolBar);
  m_toolBar->addWidget(m_keyframeNavigator);

  m_toolBar->addSeparator();

  m_frameNavigator = new FrameNavigator(m_toolBar);
  m_frameNavigator->setFrameHandle(m_frameHandle);
  m_toolBar->addWidget(m_frameNavigator);
}

//-----------------------------------------------------------------------------

void FxSettings::setFx(const TFxP &currentFx, const TFxP &actualFx) {
  // disconnecting from the fxChanged() signals avoid useless and dangerous
  // updates!!!
  if (m_fxHandle)
    disconnect(m_fxHandle, SIGNAL(fxChanged()), this,
               SLOT(updateParamViewer()));

  TFxP currentFxWithoutCamera = 0;
  if (currentFx && actualFx)
    currentFxWithoutCamera = getCurrentFx(currentFx, actualFx->getFxId());

  if (currentFxWithoutCamera)
    TFxUtil::setKeyframe(currentFxWithoutCamera, m_frameHandle->getFrameIndex(),
                         actualFx, m_frameHandle->getFrameIndex());

  ToonzScene *scene = 0;
  if (m_sceneHandle) scene = m_sceneHandle->getScene();

  // check if the current render settings are float
  bool isFloat = false;
  if (scene) {
    const TRenderSettings ps =
        scene->getProperties()->getPreviewProperties()->getRenderSettings();
    const TRenderSettings os =
        scene->getProperties()->getOutputProperties()->getRenderSettings();
    isFloat = (ps.m_bpp == 128) || (os.m_bpp == 128);
  }

  int frameIndex = 0;
  if (m_frameHandle) frameIndex = m_frameHandle->getFrameIndex();

  m_paramViewer->setFx(currentFxWithoutCamera, actualFx, frameIndex, scene);
  m_paramViewer->setIsCameraViewMode(m_isCameraModeView);
  // show warning if the current Fx does not support float / linear rendering
  m_paramViewer->updateWarnings(currentFxWithoutCamera, isFloat);

  m_viewer->setCameraMode(m_isCameraModeView);

  TDimension cameraSize = TDimension(-1, -1);
  if (scene) cameraSize = scene->getCurrentCamera()->getRes();
  m_viewer->setCameraSize(cameraSize);

  m_viewer->setFx(currentFx, actualFx, frameIndex);

  if (m_fxHandle)
    connect(m_fxHandle, SIGNAL(fxChanged()), this, SLOT(updateParamViewer()));
}

//-----------------------------------------------------------------------------

void FxSettings::setCurrentFrame() {
  int frame = m_frameHandle->getFrameIndex();
  m_paramViewer->update(frame, false);

  // if(m_isCameraModeView)
  setCurrentFx();
  m_viewer->updateFrame(frame);
}

//-----------------------------------------------------------------------------

void FxSettings::changeTitleBar(TFx *fx) {
  DockWidget *popup = dynamic_cast<DockWidget *>(parentWidget());
  if (!popup) return;

  QString titleText(tr("Fx Settings"));
  if (fx) {
    titleText += tr(" : ");
    titleText += QString::fromStdWString(fx->getName());
  }

  popup->setWindowTitle(titleText);
}

//-----------------------------------------------------------------------------

void FxSettings::setCurrentFx() {
  TFx *currFx = m_fxHandle->getFx();

  TFxP actualFx, currentFx;
  if (!currFx || 0 != dynamic_cast<TXsheetFx *>(currFx)) {
    actualFx = currentFx = TFxP();
    setFx(actualFx, currentFx);
    changeTitleBar(currentFx.getPointer());
    return;
  }
  TFxP fx(currFx);
  bool hasEmptyInput = false;
  if (TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx.getPointer()))
    fx = zfx->getZeraryFx();
  else
    hasEmptyInput = hasEmptyInputPort(fx);
  int frame         = m_frameHandle->getFrame();
  ToonzScene *scene = m_sceneHandle->getScene();
  actualFx          = fx;
  bool isEnabled    = actualFx->getAttributes()->isEnabled();
  actualFx->getAttributes()->enable(true);
  if (hasEmptyInput)
    currentFx = actualFx;
  else if (m_viewer->isEnabled()) {
    if (!m_isCameraModeView)
      currentFx = buildSceneFx(scene, frame, actualFx, false);
    else {
      const TRenderSettings rs =
          scene->getProperties()->getPreviewProperties()->getRenderSettings();
      currentFx = buildPartialSceneFx(scene, (double)frame, actualFx, 1, false);
    }
  } else
    currentFx = TFxP();

  if (currentFx) currentFx = currentFx->clone(true);

  // se al frame corrente non c'e' il livello a cui e' applicato l'effetto:
  // current=0, actual!=0
  if (!currentFx) currentFx = actualFx->clone(false);

  actualFx->getAttributes()->enable(isEnabled);
  setFx(currentFx, actualFx);
  changeTitleBar(currentFx.getPointer());
}

//-----------------------------------------------------------------------------

void FxSettings::setCurrentScene() {
  ToonzScene *scene = m_sceneHandle->getScene();
  m_paramViewer->setScene(scene);
}

//-----------------------------------------------------------------------------

void FxSettings::notifySceneChanged() {
  TPixel32 col1, col2;
  Preferences::instance()->getChessboardColors(col1, col2);
  setCheckboardColors(col1, col2);
}

//-----------------------------------------------------------------------------

void FxSettings::showEvent(QShowEvent *event) {
  setCurrentFx();
  setCurrentFrame();
  connect(m_frameHandle, SIGNAL(frameSwitched()), SLOT(setCurrentFrame()));
  if (m_fxHandle) {
    connect(m_paramViewer, SIGNAL(actualFxParamChanged()), m_fxHandle,
            SIGNAL(fxChanged()));
    connect(m_fxHandle, SIGNAL(fxChanged()), SLOT(updateParamViewer()));
    connect(m_fxHandle, SIGNAL(fxSettingsShouldBeSwitched()),
            SLOT(setCurrentFx()));
  }
  if (m_sceneHandle) {
    connect(m_sceneHandle, SIGNAL(sceneChanged()), this,
            SLOT(notifySceneChanged()));
    connect(m_sceneHandle, SIGNAL(sceneSwitched()), this,
            SLOT(setCurrentScene()));
  }
  if (m_xsheetHandle)
    connect(m_xsheetHandle, SIGNAL(xsheetChanged()), SLOT(setCurrentFx()));
  if (m_levelHandle)
    connect(m_levelHandle, SIGNAL(xshLevelChanged()), SLOT(setCurrentFx()));
  if (m_objectHandle)
    connect(m_objectHandle, SIGNAL(objectChanged(bool)), SLOT(setCurrentFx()));
}

//-----------------------------------------------------------------------------

void FxSettings::hideEvent(QHideEvent *) {
  setFx(0, 0);
  disconnect(m_frameHandle, SIGNAL(frameSwitched()), this,
             SLOT(setCurrentFrame()));
  if (m_fxHandle) {
    disconnect(m_fxHandle, SIGNAL(fxChanged()), this, SLOT(setCurrentFx()));
    disconnect(m_fxHandle, SIGNAL(fxChanged()), this,
               SLOT(updateParamViewer()));
    disconnect(m_fxHandle, SIGNAL(fxSettingsShouldBeSwitched()), this,
               SLOT(setCurrentFx()));
  }
  if (m_sceneHandle) {
    disconnect(m_sceneHandle, SIGNAL(sceneChanged()), this,
               SLOT(notifySceneChanged()));
    disconnect(m_sceneHandle, SIGNAL(sceneSwitched()), this,
               SLOT(setCurrentScene()));
  }
  if (m_xsheetHandle)
    disconnect(m_xsheetHandle, SIGNAL(xsheetChanged()), this,
               SLOT(setCurrentFx()));
  if (m_levelHandle)
    disconnect(m_levelHandle, SIGNAL(xshLevelChanged()), this,
               SLOT(setCurrentFx()));
  if (m_objectHandle)
    disconnect(m_objectHandle, SIGNAL(objectChanged(bool)), this,
               SLOT(setCurrentFx()));
}

//-----------------------------------------------------------------------------

void FxSettings::setCheckboardColors(const TPixel32 &col1,
                                     const TPixel32 &col2) {
  m_checkCol1 = col1;
  m_checkCol2 = col2;
  if (m_checkboardBg->isChecked())
    m_viewer->setBgPainter(m_checkCol1, m_checkCol2);
}

//-----------------------------------------------------------------------------

void FxSettings::setWhiteBg() { m_viewer->setBgPainter(TPixel32::White); }

//-----------------------------------------------------------------------------

void FxSettings::setBlackBg() { m_viewer->setBgPainter(TPixel32::Black); }

//-----------------------------------------------------------------------------

void FxSettings::setCheckboardBg() {
  m_viewer->setBgPainter(m_checkCol1, m_checkCol2);
}

//-----------------------------------------------------------------------------

void FxSettings::updateViewer() {
  if (m_viewer->isEnabled())
    m_viewer->updateFrame(m_frameHandle->getFrameIndex());
}

//-----------------------------------------------------------------------------

void FxSettings::updateParamViewer() {
  if (!m_paramViewer || !m_frameHandle) return;
  m_paramViewer->update(m_frameHandle->getFrameIndex(), true);
}

//-----------------------------------------------------------------------------

void FxSettings::onPointChanged(int index, const TPointD &p) {
  m_paramViewer->setPointValue(index, p);
}

//-----------------------------------------------------------------------------

void FxSettings::onViewModeChanged(QAction *triggeredAct) {
  setFocus();
  QString name             = triggeredAct->text();
  bool actIsChecked        = triggeredAct->isChecked();
  QList<QAction *> actions = m_toolBar->actions();
  QAction *cameraAct       = actions[0];
  QAction *previewAct      = actions[1];
  m_viewer->setEnable(actIsChecked);
  if (name == previewAct->text()) {
    if (cameraAct->isChecked()) cameraAct->setChecked(false);
    if (actIsChecked) {
      m_isCameraModeView = false;
      m_paramViewer->setIsCameraViewMode(false);
    }
  } else if (name == cameraAct->text()) {
    if (previewAct->isChecked()) previewAct->setChecked(false);
    if (actIsChecked) {
      m_isCameraModeView = true;
      m_paramViewer->setIsCameraViewMode(true);
    }
  }
  setCurrentFx();
}

//-----------------------------------------------------------------------------

void FxSettings::onPreferredSizeChanged(QSize pvBestSize) {
  DockWidget *popup = dynamic_cast<DockWidget *>(parentWidget());
  if (!popup || !popup->isFloating()) return;

  QSize popupBestSize = pvBestSize;

  static int maximumHeight =
      (QGuiApplication::primaryScreen()->geometry().height()) * 0.9;

  // Set minimum size, just in case
  popupBestSize.setHeight(
      std::min(std::max(popupBestSize.height(), 85), maximumHeight));
  popupBestSize.setWidth(std::max(popupBestSize.width(), 390));

  if (m_toolBar->isVisible()) {
    popupBestSize += QSize(0, m_viewer->height() + m_toolBar->height() + 4);
    popupBestSize.setWidth(
        std::max(popupBestSize.width(), m_viewer->width() + 13));
  }

  QRect geom = popup->geometry();
  geom.setSize(popupBestSize);
  popup->setGeometry(geom);
  popup->update();
}

//-----------------------------------------------------------------------------

void FxSettings::onShowSwatchButtonToggled(bool on) {
  QWidget *bottomContainer = widget(1);

  if (!on) {
    m_container_height =
        bottomContainer->height() + handleWidth() /* ハンドル幅 */;
    m_container_width = m_viewer->width() + 13;
  }
  bottomContainer->setVisible(on);

  DockWidget *popup = dynamic_cast<DockWidget *>(parentWidget());
  if (popup && popup->isFloating()) {
    QRect geom = popup->geometry();

    int height_change = (on) ? m_container_height : -m_container_height;
    int width_change  = 0;

    if (on && m_container_width > geom.width())
      width_change = m_container_width - geom.width();

    geom.setSize(geom.size() + QSize(width_change, height_change));
    popup->setGeometry(geom);
    popup->update();
  }
}

//-----------------------------------------------------------------------------
