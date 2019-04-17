#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QProcess>

#include <QtHttpServer/QHttpServer>
#include <QtHttpServer/QHttpServerRequest>
#include <QtHttpServer/QHttpServerResponse>

QByteArray git(const QString &program, const QStringList &args, const QString &project, const QByteArray &data = QByteArray()) {
    QProcess process;
    process.start(program, QStringList(args) << QDir::current().absoluteFilePath(project));
    if (!data.isEmpty())
        process.write(data);
    process.waitForFinished();
    return process.readAllStandardOutput();
}

QHttpServerResponse response(const QString &contentType, const QByteArray &data)
{
    QHttpServerResponse ret(contentType.toUtf8(), data);
    ret.addHeader("Expires", "Fri, 01 Jan 1980 00:00:00 GMT");
    ret.addHeader("Pragma", "no-cache");
    ret.addHeader("Cache-Control", "no-cache, max-age=0, must-revalidate");
    return ret;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QHttpServer server;

    server.route("/", [] () {
        return QHttpServerResponse("text/plain", "Welcome");
    });
    server.route("/<arg>/info/refs", QHttpServerRequest::Method::GET, [] (const QUrl &project, const QHttpServerRequest &request) {
        QString service = request.query().queryItemValue(QStringLiteral("service"));
        if (!service.startsWith(QStringLiteral("git-")))
            return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);

        QString packet = QStringLiteral("# service=%1\n").arg(service);
        QByteArray data = QStringLiteral("%1%2").arg(4 + packet.length(), 4, 16, QLatin1Char('0')).arg(packet).toUtf8();
        data += "0000" + git(service, {"--stateless-rpc", "--advertise-refs"}, project.toString());
        return response(QStringLiteral("application/x-%1-advertisement").arg(service), data);
    });
    server.route("/<arg>/git-receive-pack", QHttpServerRequest::Method::POST, [] (const QUrl &project, const QHttpServerRequest &request) {
        return response("application/x-git-receive-pack-result", git("git-receive-pack", {"--stateless-rpc"}, project.toString(), request.body()));
    });
    server.route("/<arg>/git-upload-pack", QHttpServerRequest::Method::POST, [] (const QUrl &project, const QHttpServerRequest &request) {
        return response("application/x-git-upload-pack-result", git("git-upload-pack", {"--stateless-rpc"}, project.toString(), request.body()));
    });

    qDebug() << server.listen(QHostAddress::Any, 8181);
    return app.exec();
}
