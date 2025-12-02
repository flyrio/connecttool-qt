#include "backend.h"
#include "members_model.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>

int main(int argc, char *argv[]) {
  QCoreApplication::setOrganizationName(QStringLiteral("ConnectTool"));
  QCoreApplication::setApplicationName(QStringLiteral("ConnectTool"));

  QGuiApplication app(argc, argv);
  QQuickStyle::setStyle(QStringLiteral("Material"));

  qmlRegisterUncreatableType<FriendsModel>("ConnectTool", 1, 0, "FriendsModel",
                                           "Provided by backend");
  qmlRegisterUncreatableType<MembersModel>("ConnectTool", 1, 0, "MembersModel",
                                           "Provided by backend");

  Backend backend;

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);

  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

  engine.loadFromModule("ConnectTool", "Main");

  return app.exec();
}
