#pragma once
#include <QObject>
#include <QJsonObject>
#include <QThread>
class TdClient : public QObject {
  Q_OBJECT
public:
  TdClient(int apiId, QString apiHash, QObject* p = nullptr);
  ~TdClient();
  void send(const QJsonObject& o);
  void start(); // call after connecting to update(), so no early update is lost
signals:
  void update(QJsonObject o);
private:
  void* client_; QThread th_; bool run_ = true; bool paramsSent_ = false;
  int apiId_; QString apiHash_;
  void loop(); void handleAuth(const QJsonObject&);
};
