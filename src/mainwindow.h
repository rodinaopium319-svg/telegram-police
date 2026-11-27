#pragma once
#include <QWidget>
#include <QListWidget>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QTimer>
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
protected: bool eventFilter(QObject*, QEvent*) override; void paintEvent(QPaintEvent*) override; void showEvent(QShowEvent*) override;
private slots:
  void onUpdate(QJsonObject o);
private:
  TdClient* td_; QListWidget* list_; QListWidget* msgs_; QLabel* replyBar_; QWidget* replyW_ = nullptr; QPushButton* replyX_; qint64 replyId_ = 0; bool pinned_ = false; QString replyPreview_; QPoint pressPos_; QListWidgetItem* pressItem_ = nullptr; QTimer* hold_; QLineEdit* input_; QLabel* title_; QPushButton* profile_ = nullptr; QString meText_; QLineEdit* login_;
  QMap<qint64,QString> names_; qint64 cur_ = 0; QPixmap bg_;
  void setReply(QListWidgetItem*, bool pin); void clearReply(); void addMsg(const QJsonObject&); void applyBg(const QString&); void openChat(qint64 id); void submit();
};
