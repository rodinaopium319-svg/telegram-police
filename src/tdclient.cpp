#include "tdclient.h"
#include <td/telegram/td_json_client.h>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
TdClient::TdClient(int id, QString hash, QObject* p) : QObject(p), apiId_(id), apiHash_(hash) {
  td_execute("{\"@type\":\"setLogVerbosityLevel\",\"new_verbosity_level\":1}");
  client_ = td_json_client_create();
}
void TdClient::start() {
  QThread* t = QThread::create([this]{ loop(); }); t->start();
  send({{"@type","getAuthorizationState"}});
}
TdClient::~TdClient() { run_ = false; }
void TdClient::send(const QJsonObject& o) {
  td_json_client_send(client_, QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
}
void TdClient::loop() {
  while (run_) {
    const char* r = td_json_client_receive(client_, 1.0);
    if (!r) continue;
    QJsonObject o = QJsonDocument::fromJson(r).object();
    if (o["@type"] == "updateAuthorizationState") handleAuth(o["authorization_state"].toObject());
    else if (o["@type"].toString().startsWith("authorizationState")) { handleAuth(o); QJsonObject w{{"@type","updateAuthorizationState"},{"authorization_state",o}}; emit update(w); continue; }
    emit update(o);
  }
}
void TdClient::handleAuth(const QJsonObject& s) {
  QString t = s["@type"].toString();
  if (t == "authorizationStateWaitTdlibParameters" && !paramsSent_) {
    paramsSent_ = true;
    QString dir = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QDir().mkpath(dir); QDir().mkpath(dir + "/files");
    send({{"@type","setTdlibParameters"},{"database_directory",dir},{"files_directory",dir + "\\files"},{"use_message_database",false},
      {"use_file_database",false},{"use_chat_info_database",true},{"api_id",apiId_},{"api_hash",apiHash_},
      {"system_language_code","en"},{"device_model","Desktop"},{"application_version","1.0"}});
  }
}
