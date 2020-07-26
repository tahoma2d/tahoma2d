#pragma once

#ifndef ABOUTPOPUP_H
#define ABOUTPOPUP_H

#include "toonzqt/dvdialog.h"
#include <QLabel>
#include <QWidget>
#include <Qt>

// forward declaration
class QPushButton;
class QLineEdit;



class AboutClickableLabel : public QLabel {
    Q_OBJECT

public:
    explicit AboutClickableLabel(QWidget* parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~AboutClickableLabel();

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event);

};

//=============================================================================
// AboutPopup
//-----------------------------------------------------------------------------

class AboutPopup final : public DVGui::Dialog {
    Q_OBJECT

        QPushButton* m_closeBtn;

public:
    AboutPopup(QWidget* parent);

};

#endif  // ABOUTPOPUP_H