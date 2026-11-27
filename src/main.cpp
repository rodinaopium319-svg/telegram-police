#include <QApplication>
#include <QSettings>
#include <QInputDialog>
#include <QFontDatabase>
#include <QFont>
#include "mainwindow.h"
int main(int argc, char** argv) {
  QApplication a(argc, argv); a.setApplicationName("TelegramPolice"); a.setOrganizationName("TelegramPolice");
  int fid = QFontDatabase::addApplicationFont(":/Inter.ttf");
  if (fid >= 0) a.setFont(QFont(QFontDatabase::applicationFontFamilies(fid).value(0), 10));
  QSettings s; // API ID/hash entered once, stored locally, never hardcoded
  if (!s.contains("api_id")) {
    s.setValue("api_id", QInputDialog::getInt(nullptr, "Telegram Police", "API ID (my.telegram.org):", 0, 0, 2147483647));
    s.setValue("api_hash", QInputDialog::getText(nullptr, "Telegram Police", "API hash:"));
  }
  TdClient c(s.value("api_id").toInt(), s.value("api_hash").toString());
  MainWindow w(&c); w.show(); c.start(); return a.exec();
}
