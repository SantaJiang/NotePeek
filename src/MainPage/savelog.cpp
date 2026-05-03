#pragma execution_character_set("utf-8")

#include "savelog.h"
#include <QMutex>
#include <QDir>
#include <QFile>
#include <QTcpSocket>
#include <QTcpServer>
#include <QDateTime>
#include <QApplication>
#include <QTimer>
#include <QTextStream>
#include <QStringList>

#define QDATE     qPrintable(QDate::currentDate().toString("yyyy-MM-dd"))
#define QDATETIMS qPrintable(QDateTime::currentDateTime().toString("yyyy-MM-dd-HH-mm-ss"))

void Log(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    QString content;

    int msgType = SaveLog::Instance()->getMsgType();
    switch (type) {
    case QtDebugMsg:
        if (msgType & MsgType_Debug)
            content = QStringLiteral("Debug %1").arg(msg);
        break;
    case QtInfoMsg:
        if (msgType & MsgType_Info)
            content = QStringLiteral("Info  %1").arg(msg);
        break;
    case QtWarningMsg:
        if (msgType & MsgType_Warning)
            content = QStringLiteral("Warn  %1").arg(msg);
        break;
    case QtCriticalMsg:
        if (msgType & MsgType_Critical)
            content = QStringLiteral("Error %1").arg(msg);
        break;
    case QtFatalMsg:
        if (msgType & MsgType_Fatal)
            content = QStringLiteral("Fatal %1").arg(msg);
        break;
    }

    if (content.isEmpty())
        return;

    if (SaveLog::Instance()->getUseContext()) {
        int line = context.line;
        QString file = context.file;
        QString function = context.function;
        if (line > 0)
            content = QStringLiteral("Line: %1  File: %2  Func: %3\n%4").arg(line).arg(file).arg(function).arg(content);
    }

    SaveLog::Instance()->save(content);
}

QScopedPointer<SaveLog> SaveLog::self;
SaveLog *SaveLog::Instance()
{
    if (self.isNull()) {
        static QMutex mutex;
        QMutexLocker locker(&mutex);
        if (self.isNull())
            self.reset(new SaveLog);
    }
    return self.data();
}

SaveLog::SaveLog(QObject *parent) : QObject(parent)
{
    connect(this, &SaveLog::send, SendLog::Instance(), &SendLog::send);

    isRun = false;
    maxRow = currentRow = 0;
    maxSize = 128;
    toNet = false;
    useContext = false;

    file = new QFile(this);
    path = qApp->applicationDirPath();
    QString str = qApp->applicationFilePath();
    QStringList list = str.split('/');
    name = list.last().split('.').first();
    fileName.clear();

    msgType = MsgType(MsgType_Debug | MsgType_Info | MsgType_Warning | MsgType_Critical | MsgType_Fatal);
}

SaveLog::~SaveLog()
{
    this->stop();
}

void SaveLog::openFile(const QString &newFileName)
{
    if (this->fileName != newFileName) {
        this->fileName = newFileName;
        if (file->isOpen())
            file->close();
        file->setFileName(newFileName);
        if (!file->open(QIODevice::WriteOnly | QIODevice::Append | QFile::Text))
            qWarning() << "SaveLog: failed to open log file:" << newFileName;
    }
}

bool SaveLog::getUseContext()
{
    return useContext;
}

MsgType SaveLog::getMsgType()
{
    return msgType;
}

void SaveLog::start()
{
    if (isRun)
        return;
    isRun = true;
    qInstallMessageHandler(Log);
}

void SaveLog::stop()
{
    if (!isRun)
        return;
    isRun = false;
    this->clear();
    qInstallMessageHandler(nullptr);
}

void SaveLog::clear()
{
    currentRow = 0;
    fileName.clear();
    if (file->isOpen())
        file->close();
}

void SaveLog::save(const QString &content)
{
    if (toNet) {
        Q_EMIT send(content);
        return;
    }

    QDir dir(path);
    if (!dir.exists())
        dir.mkpath(path);

    bool needOpen = false;
    if (maxRow > 0) {
        currentRow++;
        needOpen = fileName.isEmpty() || (currentRow >= maxRow);
    } else if (maxSize > 0) {
        needOpen = fileName.isEmpty() || (file->size() > qint64(maxSize) * 1024);
    } else {
        openFile(QStringLiteral("%1/%2_log_%3.txt").arg(path, name, QDATE));
    }

    if ((maxRow > 0 || maxSize > 0) && needOpen) {
        currentRow = 0;
        openFile(QStringLiteral("%1/%2_log_%3.txt").arg(path, name, QDATETIMS));
    }

    QTextStream stream(file);
    stream << content << '\n';
}

void SaveLog::setMaxRow(int maxRow)
{
    if (maxRow >= 0) {
        this->maxRow = maxRow;
        this->clear();
    }
}

void SaveLog::setMaxSize(int maxSize)
{
    if (maxSize >= 0) {
        this->maxSize = maxSize;
        this->clear();
    }
}

void SaveLog::setListenPort(int listenPort)
{
    SendLog::Instance()->setListenPort(listenPort);
}

void SaveLog::setToNet(bool toNet)
{
    this->toNet = toNet;
    if (toNet)
        SendLog::Instance()->start();
    else
        SendLog::Instance()->stop();
}

void SaveLog::setUseContext(bool useContext)
{
    this->useContext = useContext;
}

void SaveLog::setPath(const QString &path)
{
    this->path = path;
}

void SaveLog::setName(const QString &name)
{
    this->name = name;
}

void SaveLog::setMsgType(const MsgType &msgType)
{
    this->msgType = msgType;
}

// SendLog

QScopedPointer<SendLog> SendLog::self;
SendLog *SendLog::Instance()
{
    if (self.isNull()) {
        static QMutex mutex;
        QMutexLocker locker(&mutex);
        if (self.isNull())
            self.reset(new SendLog);
    }
    return self.data();
}

SendLog::SendLog(QObject *parent) : QObject(parent)
    , listenPort(6000)
    , socket(nullptr)
{
    server = new QTcpServer(this);
    connect(server, &QTcpServer::newConnection, this, &SendLog::newConnection);
}

SendLog::~SendLog()
{
    if (socket)
        socket->disconnectFromHost();
    server->close();
}

void SendLog::newConnection()
{
    while (server->hasPendingConnections())
        socket = server->nextPendingConnection();
}

void SendLog::setListenPort(int listenPort)
{
    this->listenPort = listenPort;
}

void SendLog::start()
{
    server->listen(QHostAddress::AnyIPv4, listenPort);
}

void SendLog::stop()
{
    if (socket) {
        socket->disconnectFromHost();
        socket = nullptr;
    }
    server->close();
}

void SendLog::send(const QString &content)
{
    if (socket && socket->isOpen())
        socket->write(content.toUtf8());
}
