#include "mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QJsonArray>
#include <QDateTime>
#include <QPushButton>
#include <QFileDialog>
#include <QSettings>
#include <QImage>
#include <QMessageBox>
#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#endif
MainWindow::MainWindow(TdClient* c) : td_(c) {
  setWindowTitle("Telegram Police"); setWindowIcon(QIcon(":/icon.png")); resize(900, 620);
  setAttribute(Qt::WA_TranslucentBackground);
  QString sv = QSettings().value("bg").toString(); if (!sv.isEmpty()) bg_ = QPixmap(sv);
  // 2018 Telegram: white list on the left, blue header, light chat area
  setStyleSheet("QListWidget{color:#eee;background:rgba(15,15,18,150);border:0;font-size:14px}"
    "QListWidget::item{padding:10px}QListWidget::item:selected{background:#3a3a40;color:#fff}"
    "QLabel#t{background:rgba(8,8,10,220);color:#fff;font-weight:bold;padding:12px;font-size:15px}"
    "QTextBrowser{color:#eee;background:rgba(0,0,0,70);border:0;font-size:14px}"
    "QLineEdit{color:#fff;background:rgba(15,15,18,190);border:0;border-top:1px solid #3a3a40;padding:12px;font-size:14px}");
  list_ = new QListWidget;
  profile_ = new QPushButton("Me"); profile_->setStyleSheet("background:rgba(8,8,10,220);color:#fff;border:0;padding:12px;text-align:left;font-weight:bold");
  connect(profile_, &QPushButton::clicked, this, [this]{
    QMessageBox::information(this, "My profile", meText_.isEmpty() ? "Loading..." : meText_); });
  auto* left = new QVBoxLayout; left->setSpacing(0); left->setContentsMargins(0,0,0,0);
  left->addWidget(profile_); left->addWidget(list_);
  auto* leftW = new QWidget; leftW->setLayout(left); leftW->setFixedWidth(300);
  title_ = new QLabel("Telegram Police"); title_->setObjectName("t");
  msgs_ = new QTextBrowser; input_ = new QLineEdit; input_->setPlaceholderText("Write a message...");
  login_ = nullptr;
  auto* bgBtn = new QPushButton("Background"); bgBtn->setStyleSheet("background:rgba(30,30,34,235);color:#fff;border:0;padding:12px");
  connect(bgBtn, &QPushButton::clicked, this, [this]{
    QString f = QFileDialog::getOpenFileName(this, "Chat background", QString(), "Images (*.png *.jpg *.jpeg *.bmp)");
    if (f.isEmpty()) return; QSettings().setValue("bg", f); applyBg(f); });
  QString saved = QSettings().value("bg").toString(); if (!saved.isEmpty()) applyBg(saved);
  auto* bar = new QHBoxLayout; bar->setSpacing(0); bar->addWidget(title_, 1); bar->addWidget(bgBtn);
  auto* right = new QVBoxLayout; right->setSpacing(0); right->setContentsMargins(0,0,0,0);
  right->addLayout(bar); right->addWidget(msgs_); right->addWidget(input_);
  auto* h = new QHBoxLayout(this); h->setSpacing(0); h->setContentsMargins(0,0,0,0);
  h->addWidget(leftW); h->addLayout(right, 1);
  connect(td_, &TdClient::update, this, &MainWindow::onUpdate, Qt::QueuedConnection);
  connect(input_, &QLineEdit::returnPressed, this, &MainWindow::submit);
  connect(list_, &QListWidget::currentRowChanged, this, [this](int r){ if (r>=0) openChat(list_->item(r)->data(Qt::UserRole).toLongLong()); });
  msgs_->viewport()->setAutoFillBackground(false);
}
void MainWindow::paintEvent(QPaintEvent*) {
  QPainter p(this); p.fillRect(rect(), QColor(10,10,12,120)); // light tint over the frosted blur
  if (!bg_.isNull()) { p.drawPixmap(300, 44, width()-300, height()-88, bg_); }
}
void MainWindow::showEvent(QShowEvent*) {
#ifdef _WIN32
  HWND h = (HWND)winId(); int t = 3; // DWMSBT_TRANSIENTWINDOW = acrylic blur (Windows 11)
  DwmSetWindowAttribute(h, 38, &t, sizeof(t));
  MARGINS m = {-1,-1,-1,-1}; DwmExtendFrameIntoClientArea(h, &m);
#endif
}
void MainWindow::onUpdate(QJsonObject o) {
  QString t = o["@type"].toString();
  if (t == "updateAuthorizationState") {
    QString s = o["authorization_state"].toObject()["@type"].toString();
    if (s == "authorizationStateWaitPhoneNumber") { title_->setText("Enter phone number (+20...) below"); login_ = input_; input_->setProperty("m","phone"); }
    else if (s == "authorizationStateWaitCode") { title_->setText("Enter the code Telegram sent you"); input_->setProperty("m","code"); }
    else if (s == "authorizationStateWaitPassword") { title_->setText("Enter your 2-step password"); input_->setProperty("m","pass"); }
    else if (s == "authorizationStateReady") { input_->setProperty("m","chat"); title_->setText("Telegram Police");
      td_->send({{"@type","getMe"}}); td_->send({{"@type","loadChats"},{"limit",100}}); td_->send({{"@type","getChats"},{"limit",100}}); }
  } else if (t == "user" && o["id"].toVariant().toLongLong() > 0 && meText_.isEmpty()) {
    QString n = (o["first_name"].toString()+" "+o["last_name"].toString()).trimmed();
    QString un = o["usernames"].toObject()["active_usernames"].toArray().isEmpty() ? "" : "@"+o["usernames"].toObject()["active_usernames"].toArray()[0].toString();
    meText_ = n + "\n" + un + "\n+" + o["phone_number"].toString(); profile_->setText(n);
  } else if (t == "chats") {
    for (auto v : o["chat_ids"].toArray()) td_->send({{"@type","getChat"},{"chat_id",v.toVariant().toLongLong()}});
  } else if (t == "chat") {
    QString type = o["type"].toObject()["@type"].toString();
    if (type=="chatTypeSecret"||type=="chatTypeBasicGroup"||type=="chatTypeSupergroup"||type=="chatTypePrivate") {
      qint64 id = o["id"].toVariant().toLongLong(); names_[id] = o["title"].toString();
      auto* it = new QListWidgetItem(o["title"].toString()); it->setData(Qt::UserRole, id); list_->addItem(it);
    }
  } else if (t == "updateNewMessage" || t == "message") {
    QJsonObject m = t=="message" ? o : o["message"].toObject();
    if (m["chat_id"].toVariant().toLongLong() == cur_ && m["content"].toObject()["@type"] == "messageText") {
      bool out = m["is_outgoing"].toBool();
      msgs_->append(QString("<div align='%1'><span style='color:#fff;background:%2'>&nbsp;%3&nbsp;</span></div>").arg(out?"right":"left", out?"#2e2e35":"#2a2a30",
        m["content"].toObject()["text"].toObject()["text"].toString().toHtmlEscaped()));
    }
  } else if (t == "messages") {
    QJsonArray a = o["messages"].toArray();
    for (int i = a.size()-1; i >= 0; --i) onUpdate(a[i].toObject().isEmpty()? QJsonObject{} : [&]{ auto m = a[i].toObject(); m["@type"]="message"; return m; }());
  }
}
void MainWindow::openChat(qint64 id) {
  cur_ = id; msgs_->clear(); title_->setText(names_[id]);
  td_->send({{"@type","getChatHistory"},{"chat_id",id},{"from_message_id",0},{"offset",0},{"limit",50},{"only_local",false}});
}
void MainWindow::submit() {
  QString m = input_->property("m").toString(), v = input_->text(); if (v.isEmpty()) return; input_->clear();
  if (m=="phone") td_->send({{"@type","setAuthenticationPhoneNumber"},{"phone_number",v}});
  else if (m=="code") td_->send({{"@type","checkAuthenticationCode"},{"code",v}});
  else if (m=="pass") td_->send({{"@type","checkAuthenticationPassword"},{"password",v}});
  else if (cur_) td_->send({{"@type","sendMessage"},{"chat_id",cur_},{"input_message_content",QJsonObject{{"@type","inputMessageText"},{"text",QJsonObject{{"@type","formattedText"},{"text",v}}}}}});
}

void MainWindow::applyBg(const QString& f) {
  QPixmap pm(f); if (pm.isNull()) return; bg_ = pm; update();
}
