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
#include <QProcess>
#include <QRegularExpression>
#include <QApplication>
#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#endif
MainWindow::MainWindow(TdClient* c) : td_(c) {
  setWindowTitle("Telegram Police"); setWindowIcon(QIcon(":/icon.png")); resize(900, 620);
  setAttribute(Qt::WA_TranslucentBackground);
  QString sv = QSettings().value("bg").toString(); if (!sv.isEmpty()) bg_ = QPixmap(sv);
  // 2018 Telegram: white list on the left, blue header, light chat area
  setStyleSheet("QListWidget{color:#eee;background:rgba(8,8,8,150);border:0;font-size:14px}"
    "QListWidget::item{padding:10px}QListWidget::item:selected{background:#3a3a3a;color:#fff}"
    "QLabel#t{background:rgba(0,0,0,220);color:#fff;font-weight:bold;padding:12px;font-size:15px}"
    "QListWidget#m{color:#eee;background:rgba(0,0,0,70);border:0;font-size:14px;outline:0}QListWidget#m::item{padding:6px;margin:3px 40px;border-radius:6px}QLabel#r{background:rgba(28,28,28,235);color:#ddd;padding:6px 12px}"
    "QLineEdit{color:#fff;background:rgba(8,8,8,190);border:0;border-top:1px solid #3a3a3a;padding:12px;font-size:14px}");
  list_ = new QListWidget;
  profile_ = new QPushButton("Me"); profile_->setStyleSheet("background:rgba(0,0,0,220);color:#fff;border:0;padding:12px;text-align:left;font-weight:bold");
  connect(profile_, &QPushButton::clicked, this, [this]{
    if (input_->property("m").toString() != "chat") { QMessageBox::information(this, "My profile", "You are not logged in yet. Type your phone number in the box at the bottom first."); return; }
    if (meText_.isEmpty()) { td_->send({{"@type","getMe"},{"@extra","me"}}); QMessageBox::information(this, "My profile", "Still loading your profile, try again in a few seconds."); return; }
    QMessageBox::information(this, "My profile", meText_); });
  auto* left = new QVBoxLayout; left->setSpacing(0); left->setContentsMargins(0,0,0,0);
  logoutBtn_ = new QPushButton("Log out"); logoutBtn_->setStyleSheet("background:rgba(28,28,28,235);color:#fff;border:0;padding:12px");
  connect(logoutBtn_, &QPushButton::clicked, this, [this]{
    if (QMessageBox::question(this, "Log out", "Log out of this account? You can log in with another number.") == QMessageBox::Yes)
      td_->send({{"@type","logOut"}}); });
  auto* topRow = new QHBoxLayout; topRow->setSpacing(0); topRow->addWidget(profile_, 1); topRow->addWidget(logoutBtn_);
  left->addLayout(topRow); left->addWidget(list_);
  auto* leftW = new QWidget; leftW->setLayout(left); leftW->setFixedWidth(300);
  title_ = new QLabel("Telegram Police"); title_->setObjectName("t"); title_->setWordWrap(true);
  msgs_ = new QListWidget; msgs_->setObjectName("m"); msgs_->setWordWrap(true); msgs_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  msgs_->viewport()->installEventFilter(this); msgs_->setContextMenuPolicy(Qt::NoContextMenu);
  hold_ = new QTimer(this); hold_->setSingleShot(true); hold_->setInterval(700);
  connect(hold_, &QTimer::timeout, this, [this]{ if (pressItem_) { QMenu m; auto* a = m.addAction("Reply"); auto* b = m.addAction(pinned_?"Unpin reply":"Pin Reply");
    auto* c = m.exec(QCursor::pos()); if (c==a) setReply(pressItem_, false); else if (c==b) { if (pinned_) clearReply(); else setReply(pressItem_, true); } pressItem_ = nullptr; } });
  replyBar_ = new QLabel; replyBar_->setObjectName("r"); replyX_ = new QPushButton("X"); replyX_->setFlat(true); replyX_->setStyleSheet("color:#fff;padding:6px 12px;background:rgba(28,28,28,235);border:0");
  connect(replyX_, &QPushButton::clicked, this, &MainWindow::clearReply);
  auto* rb = new QWidget; auto* rl = new QHBoxLayout(rb); rl->setContentsMargins(0,0,0,0); rl->setSpacing(0); rl->addWidget(replyBar_,1); rl->addWidget(replyX_); rb->hide(); replyW_ = rb; input_ = new QLineEdit; input_->setPlaceholderText("Write a message...");
  login_ = nullptr;
  auto* bgBtn = new QPushButton("Background"); bgBtn->setStyleSheet("background:rgba(28,28,28,235);color:#fff;border:0;padding:12px");
  connect(bgBtn, &QPushButton::clicked, this, [this]{
    QString f = QFileDialog::getOpenFileName(this, "Chat background", QString(), "Images (*.png *.jpg *.jpeg *.bmp)");
    if (f.isEmpty()) return; QSettings().setValue("bg", f); applyBg(f); });
  QString saved = QSettings().value("bg").toString(); if (!saved.isEmpty()) applyBg(saved);
  resendBtn_ = new QPushButton("Resend code (SMS)"); resendBtn_->setStyleSheet("background:#8b1a1a;color:#fff;border:0;padding:12px;font-weight:bold"); resendBtn_->hide();
  connect(resendBtn_, &QPushButton::clicked, this, [this]{ title_->setText("Asking for a new code..."); td_->send({{"@type","resendAuthenticationCode"}}); });
  auto* bar = new QHBoxLayout; bar->setSpacing(0); bar->addWidget(title_, 1); bar->addWidget(resendBtn_); bar->addWidget(bgBtn);
  auto* right = new QVBoxLayout; right->setSpacing(0); right->setContentsMargins(0,0,0,0);
  right->addLayout(bar); right->addWidget(msgs_); right->addWidget(replyW_); right->addWidget(input_);
  auto* h = new QHBoxLayout(this); h->setSpacing(0); h->setContentsMargins(0,0,0,0);
  h->addWidget(leftW); h->addLayout(right, 1);
  connect(td_, &TdClient::update, this, &MainWindow::onUpdate, Qt::QueuedConnection);
  connect(input_, &QLineEdit::returnPressed, this, &MainWindow::submit);
  connect(list_, &QListWidget::currentRowChanged, this, [this](int r){ if (r>=0) openChat(list_->item(r)->data(Qt::UserRole).toLongLong()); });
  msgs_->viewport()->setAutoFillBackground(false);
}
void MainWindow::paintEvent(QPaintEvent*) {
  QPainter p(this); p.fillRect(rect(), QColor(0,0,0,130)); // light tint over the frosted blur
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
    if (s == "authorizationStateWaitPhoneNumber") { { QSettings q; title_->setText(QString("Log in: type your phone with country code (+20...) and press Enter.  [API ID %1, hash %2 chars - hash must be 32]").arg(q.value("api_id").toInt()).arg(q.value("api_hash").toString().size())); } input_->setPlaceholderText("Phone number, e.g. +201234567890"); input_->setProperty("m","phone"); }
    else if (s == "authorizationStateWaitCode") { QString ty = o["authorization_state"].toObject()["code_info"].toObject()["type"].toObject()["@type"].toString();
      QString where = ty=="authenticationCodeTypeTelegramMessage" ? "in your Telegram app (chat named Telegram) on another device" : ty=="authenticationCodeTypeSms" || ty=="authenticationCodeTypeSmsWord" || ty=="authenticationCodeTypeSmsPhrase" ? "by SMS" : ty=="authenticationCodeTypeCall" || ty=="authenticationCodeTypeFlashCall" || ty=="authenticationCodeTypeMissedCall" ? "by phone call" : "(" + ty + ")";
      auto ci = o["authorization_state"].toObject()["code_info"].toObject(); QString nt = ci["next_type"].toObject()["@type"].toString().replace("authenticationCodeType","");
      title_->setText("Code sent " + where + " to " + ci["phone_number"].toString() + ". Type it and press Enter. If nothing arrives after " + QString::number(ci["timeout"].toInt()) + "s, type resend (next: " + (nt.isEmpty() ? "none" : nt) + ")."); input_->setPlaceholderText("Login code"); input_->setProperty("m","code"); resendBtn_->show(); }
    else if (s == "authorizationStateWaitPassword") { title_->setText("Type your 2-step verification password and press Enter"); input_->setPlaceholderText("2-step password"); input_->setProperty("m","pass"); input_->setEchoMode(QLineEdit::Password); }
    else if (s == "authorizationStateClosed") { QProcess::startDetached(QApplication::applicationFilePath(), {}); QApplication::quit(); }
    else if (s == "authorizationStateReady") { resendBtn_->hide(); input_->setEchoMode(QLineEdit::Normal); input_->setPlaceholderText("Write a message..."); input_->setProperty("m","chat"); title_->setText("Telegram Police");
      td_->send({{"@type","getMe"},{"@extra","me"}}); td_->send({{"@type","loadChats"},{"limit",100}}); td_->send({{"@type","getChats"},{"limit",100}}); }
  } else if (t == "user" && o["@extra"].toString() != "me") {
    qint64 uid = o["id"].toVariant().toLongLong();
    QString n = (o["first_name"].toString()+" "+o["last_name"].toString()).trimmed();
    auto un = o["usernames"].toObject()["active_usernames"].toArray();
    QString extra = !un.isEmpty() ? "@"+un[0].toString() : (o["phone_number"].toString().isEmpty() ? "" : "+"+o["phone_number"].toString());
    QString label = extra.isEmpty() ? n : n + "  " + extra; userLabel_[uid] = label;
    for (auto* it : pending_.take(uid)) it->setText(label + "\n" + it->data(Qt::UserRole+1).toString());
  } else if (t == "user" && meText_.isEmpty()) {
    QString n = (o["first_name"].toString()+" "+o["last_name"].toString()).trimmed();
    QString un = o["usernames"].toObject()["active_usernames"].toArray().isEmpty() ? "" : "@"+o["usernames"].toObject()["active_usernames"].toArray()[0].toString();
    meText_ = n + "\n" + un + "\n+" + o["phone_number"].toString(); profile_->setText(n);
  } else if (t == "error") {
    title_->setText("Error: " + o["message"].toString()); title_->setToolTip(o["message"].toString());
  } else if (t == "chats") {
    for (auto v : o["chat_ids"].toArray()) td_->send({{"@type","getChat"},{"chat_id",v.toVariant().toLongLong()}});
  } else if (t == "chat") {
    QString type = o["type"].toObject()["@type"].toString();
    if (type=="chatTypeSecret"||type=="chatTypeBasicGroup"||type=="chatTypeSupergroup"||type=="chatTypePrivate") {
      qint64 id = o["id"].toVariant().toLongLong(); names_[id] = o["title"].toString();
      isGroup_[id] = (type=="chatTypeBasicGroup") || (type=="chatTypeSupergroup" && !o["type"].toObject()["is_channel"].toBool());
      auto* it = new QListWidgetItem(o["title"].toString()); it->setData(Qt::UserRole, id); list_->addItem(it);
    }
  } else if (t == "updateNewMessage" || t == "message") {
    QJsonObject m = t=="message" ? o : o["message"].toObject();
    if (m["chat_id"].toVariant().toLongLong() == cur_) addMsg(m);
  } else if (t == "messages") {
    QJsonArray a = o["messages"].toArray();
    for (int i = a.size()-1; i >= 0; --i) onUpdate(a[i].toObject().isEmpty()? QJsonObject{} : [&]{ auto m = a[i].toObject(); m["@type"]="message"; return m; }());
  }
}
void MainWindow::openChat(qint64 id) {
  cur_ = id; pending_.clear(); curGroup_ = isGroup_.value(id); msgs_->clear(); clearReply(); title_->setText(names_[id]);
  td_->send({{"@type","getChatHistory"},{"chat_id",id},{"from_message_id",0},{"offset",0},{"limit",50},{"only_local",false}});
}
void MainWindow::submit() {
  QString m = input_->property("m").toString(), v = input_->text(); if (v.isEmpty()) return; input_->clear();
  if (m=="phone") { QString ph = v; ph.remove(QRegularExpression("[\\s\\-\\(\\)]")); if (!ph.startsWith("+")) ph = "+" + ph; title_->setText("Sending code request to " + ph + " ..."); td_->send({{"@type","setAuthenticationPhoneNumber"},{"phone_number",ph}}); }
  else if (m=="code" && v.trimmed().toLower()=="resend") { title_->setText("Asking for a new code..."); td_->send({{"@type","resendAuthenticationCode"}}); }
  else if (m=="code") td_->send({{"@type","checkAuthenticationCode"},{"code",v}});
  else if (m=="pass") td_->send({{"@type","checkAuthenticationPassword"},{"password",v}});
  else if (cur_) {
    QJsonObject req{{"@type","sendMessage"},{"chat_id",cur_},{"input_message_content",QJsonObject{{"@type","inputMessageText"},{"text",QJsonObject{{"@type","formattedText"},{"text",v}}}}}};
    if (replyId_) req["reply_to"] = QJsonObject{{"@type","inputMessageReplyToMessage"},{"message_id",(double)replyId_}};
    td_->send(req);
  }
  if (!pinned_) clearReply();
}

void MainWindow::applyBg(const QString& f) {
  QPixmap pm(f); if (pm.isNull()) return; bg_ = pm; update();
}

void MainWindow::addMsg(const QJsonObject& m) {
  QJsonObject c = m["content"].toObject(); QString txt = c["@type"]=="messageText" ? c["text"].toObject()["text"].toString() : "[media]";
  bool out = m["is_outgoing"].toBool();
  auto* it = new QListWidgetItem(txt); it->setData(Qt::UserRole, m["id"].toVariant().toLongLong()); it->setData(Qt::UserRole+1, txt);
  QJsonObject sd = m["sender_id"].toObject();
  if (curGroup_ && !out && sd["@type"].toString() == "messageSenderUser") {
    qint64 uid = sd["user_id"].toVariant().toLongLong();
    if (userLabel_.contains(uid)) it->setText(userLabel_[uid] + "\n" + txt);
    else { it->setText("...\n" + txt); bool first = !pending_.contains(uid); pending_[uid].append(it); if (first) td_->send({{"@type","getUser"},{"user_id",(double)uid}}); }
  }
  it->setBackground(QColor(out ? "#2b2b2b" : "#262626")); it->setForeground(QColor("#fff"));
  it->setTextAlignment(out ? Qt::AlignRight : Qt::AlignLeft); msgs_->addItem(it); msgs_->scrollToBottom();
}
void MainWindow::setReply(QListWidgetItem* it, bool pin) {
  replyId_ = it->data(Qt::UserRole).toLongLong(); pinned_ = pin; replyPreview_ = it->data(Qt::UserRole+1).toString().left(60);
  replyBar_->setText((pin ? "Pinned reply: " : "Replying to: ") + replyPreview_); replyW_->show(); input_->setFocus();
}
void MainWindow::clearReply() { replyId_ = 0; pinned_ = false; if (replyW_) replyW_->hide(); }
bool MainWindow::eventFilter(QObject* o, QEvent* e) {
  if (o != msgs_->viewport()) return false;
  auto* me = dynamic_cast<QMouseEvent*>(e);
  if (!me) return false;
  if (e->type() == QEvent::MouseButtonPress) {
    if (me->button() == Qt::RightButton) { auto* it = msgs_->itemAt(me->pos()); if (it) { pressItem_ = it; QMenu m; auto* a = m.addAction("Reply"); auto* b = m.addAction(pinned_?"Unpin reply":"Pin Reply");
      auto* c = m.exec(me->globalPosition().toPoint()); if (c==a) setReply(it,false); else if (c==b) { if (pinned_) clearReply(); else setReply(it,true); } pressItem_ = nullptr; return true; } }
    pressPos_ = me->pos(); pressItem_ = msgs_->itemAt(me->pos()); if (pressItem_) hold_->start();
  } else if (e->type() == QEvent::MouseMove) {
    if ((me->pos() - pressPos_).manhattanLength() > 12) hold_->stop();
  } else if (e->type() == QEvent::MouseButtonRelease) {
    hold_->stop();
    if (pressItem_ && qAbs(me->pos().x() - pressPos_.x()) > 70 && qAbs(me->pos().y() - pressPos_.y()) < 30) { setReply(pressItem_, false); }
    pressItem_ = nullptr;
  }
  return false;
}
