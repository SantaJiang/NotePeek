#ifndef SAVELOG_H
#define SAVELOG_H

#include <QObject>

class QFile;
class QTcpSocket;
class QTcpServer;

enum MsgType {
    MsgType_Debug    = 0x0001,
    MsgType_Info     = 0x0002,
    MsgType_Warning  = 0x0004,
    MsgType_Critical = 0x0008,
    MsgType_Fatal    = 0x0010,
};

class SaveLog : public QObject
{
    Q_OBJECT
public:
    static SaveLog *Instance();
    explicit SaveLog(QObject *parent = nullptr);
    ~SaveLog();

private:
    static QScopedPointer<SaveLog> self;

    bool isRun;
    int maxRow, currentRow;
    int maxSize;
    bool toNet;
    bool useContext;

    QFile *file;
    QString path;
    QString name;
    QString fileName;
    MsgType msgType;

private:
    void openFile(const QString &fileName);

public:
    bool getUseContext();
    MsgType getMsgType();

Q_SIGNALS:
    void send(const QString &content);

public Q_SLOTS:
    void start();
    void stop();

    void clear();
    void save(const QString &content);

    void setMaxRow(int maxRow);
    void setMaxSize(int maxSize);

    void setListenPort(int listenPort);
    void setToNet(bool toNet);
    void setUseContext(bool useContext);

    void setPath(const QString &path);
    void setName(const QString &name);
    void setMsgType(const MsgType &msgType);
};

class SendLog : public QObject
{
    Q_OBJECT
public:
    static SendLog *Instance();
    explicit SendLog(QObject *parent = nullptr);
    ~SendLog();

private:
    static QScopedPointer<SendLog> self;

    int listenPort;
    QTcpSocket *socket;
    QTcpServer *server;

private slots:
    void newConnection();

public Q_SLOTS:
    void setListenPort(int listenPort);

    void start();
    void stop();

    void send(const QString &content);
};

#endif // SAVELOG_H
