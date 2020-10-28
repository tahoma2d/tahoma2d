#pragma once

#ifndef INCREMENT_H
#define INCREMENT_H

#include <QObject>

class IncrementGuides : public QObject {  // Singleton
  Q_OBJECT

public:
  static IncrementGuides* instance() {
    static IncrementGuides _instance;
    return &_instance;
  };

private:
  IncrementGuides();
  ~IncrementGuides();

  // public:

  // public slots:

  // signals:
};

#endif  // INCREMENT_H