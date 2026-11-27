#pragma once
#include <QWidget>
#include <QListWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QShowEvent>
#include "tdclient.h"
class MainWindow : public QWidget {
  Q_OBJECT
public:
  MainWindow(TdClient* c);
protected: void paintEvent(QPaintEvent*) override; void showEvent(QShowEvent*) override;
private slots:
  void onUpdate(QJsonObject o);
private:
  TdClient* td_; QListWidget* list_; QTextBrowser* msgs_; QLineEdit* input_; QLabel* title_; QPushButton* profile_ = nullptr; QString meText_; QLineEdit* login_;
  QMap<qint64,QString> names_; qint64 cur_ = 0; QPixmap bg_;
  void applyBg(const QString&); void openChat(qint64 id); void submit();
};
