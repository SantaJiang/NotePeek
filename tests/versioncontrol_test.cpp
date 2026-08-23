#include "versioncontrol/gitcommandrunner.h"
#include "versioncontrol/gitexecutable.h"
#include "versioncontrol/gitrepositoryservice.h"
#include "versioncontrol/gitstatusparser.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace
{

void appendRecord(QByteArray *output, const QByteArray &record)
{
    output->append(record);
    output->append('\0');
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

struct ProcessResult
{
    int exitCode = -1;
    QByteArray standardOutput;
    QByteArray standardError;
};

class EnvironmentVariableGuard
{
public:
    explicit EnvironmentVariableGuard(const QByteArray &name)
        : m_name(name)
        , m_wasSet(qEnvironmentVariableIsSet(name.constData()))
        , m_value(qgetenv(name.constData()))
    {
    }

    ~EnvironmentVariableGuard()
    {
        if (m_wasSet)
            qputenv(m_name.constData(), m_value);
        else
            qunsetenv(m_name.constData());
    }

private:
    QByteArray m_name;
    bool m_wasSet;
    QByteArray m_value;
};

ProcessResult runGit(const QString &gitExecutable, const QString &workingDirectory, const QStringList &arguments)
{
    QProcess process;
    process.setWorkingDirectory(workingDirectory);
    process.start(gitExecutable, arguments);
    if (!process.waitForStarted(3000) || !process.waitForFinished(10000))
        return {};
    return {process.exitCode(), process.readAllStandardOutput(), process.readAllStandardError()};
}

bool initializeRepository(const QString &gitExecutable, const QString &path)
{
    return runGit(gitExecutable, path, {QStringLiteral("init"), QStringLiteral("--quiet")}).exitCode == 0;
}

} // namespace

class VersionControlTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesGitVersions()
    {
        QCOMPARE(GitExecutable::parseVersion(QByteArray("git version 2.50.1 (Apple Git-155)")),
                 (GitVersion{2, 50, 1, QStringLiteral("git version 2.50.1 (Apple Git-155)")}));
        QCOMPARE(GitExecutable::parseVersion(QByteArray("git version 2.55.0.windows.5")),
                 (GitVersion{2, 55, 0, QStringLiteral("git version 2.55.0.windows.5")}));
        QCOMPARE(GitExecutable::parseVersion(QByteArray("git version 2.23")),
                 (GitVersion{2, 23, 0, QStringLiteral("git version 2.23")}));
        QVERIFY(!GitExecutable::parseVersion(QByteArray("not git")).isValid());
    }

    void findsConfiguredGitOnlyWhenItExists()
    {
        const QString git = GitExecutable::find();
        if (git.isEmpty())
        {
            QTest::qSkip("Git is not installed on this test runner.", __FILE__, __LINE__);
            return;
        }

        QCOMPARE(GitExecutable::find(git), QFileInfo(git).canonicalFilePath());
        QVERIFY(GitExecutable::find(QStringLiteral("/definitely/missing/git")).isEmpty());
    }

    void parsesPorcelainV2Status()
    {
        QByteArray output;
        appendRecord(&output, QByteArray("# branch.oid 0123456789abcdef"));
        appendRecord(&output, QByteArray("# branch.head feature/status"));
        appendRecord(&output, QByteArray("# branch.upstream origin/feature/status"));
        appendRecord(&output, QByteArray("# branch.ab +3 -2"));
        appendRecord(&output, QByteArray("# stash 4"));
        appendRecord(&output, QByteArray("# future.header ignored"));
        appendRecord(&output, QByteArray("# future.valueless"));
        appendRecord(&output, QByteArray("1 .M N... 100644 100644 100644 aaaa bbbb src/main file.cpp"));
        appendRecord(&output, QByteArray("1 M. N... 100644 100644 100644 aaaa bbbb  leading-space.txt"));
        appendRecord(&output, QByteArray("2 R. N... 100644 100644 100644 aaaa bbbb R087 new name.txt"));
        appendRecord(&output, QByteArray("old name.txt"));
        appendRecord(&output, QByteArray("u UU N... 100644 100644 100644 100644 aaaa bbbb cccc conflict.txt"));
        appendRecord(&output, QStringLiteral("? 新文件\nname.txt").toUtf8());
        appendRecord(&output, QByteArray("! ignored.log"));

        const GitStatusParseResult parsed = GitStatusParser::parsePorcelainV2(output);
        QVERIFY2(parsed.succeeded(), qPrintable(parsed.error));
        QCOMPARE(parsed.snapshot.headOid, QStringLiteral("0123456789abcdef"));
        QCOMPARE(parsed.snapshot.branchName, QStringLiteral("feature/status"));
        QCOMPARE(parsed.snapshot.upstreamBranch, QStringLiteral("origin/feature/status"));
        QCOMPARE(parsed.snapshot.ahead, 3);
        QCOMPARE(parsed.snapshot.behind, 2);
        QCOMPARE(parsed.snapshot.stashCount, 4);
        QCOMPARE(parsed.snapshot.files.size(), 6);

        const GitFileStatus modified = parsed.snapshot.files.at(0);
        QCOMPARE(modified.recordType, GitChangeRecordType::Ordinary);
        QCOMPARE(modified.indexStatus, QLatin1Char('.'));
        QCOMPARE(modified.workTreeStatus, QLatin1Char('M'));
        QCOMPARE(modified.path, QStringLiteral("src/main file.cpp"));

        QCOMPARE(parsed.snapshot.files.at(1).path, QStringLiteral(" leading-space.txt"));
        const GitFileStatus renamed = parsed.snapshot.files.at(2);
        QCOMPARE(renamed.recordType, GitChangeRecordType::RenamedOrCopied);
        QCOMPARE(renamed.renameOrCopyScore, QStringLiteral("R087"));
        QCOMPARE(renamed.path, QStringLiteral("new name.txt"));
        QCOMPARE(renamed.originalPath, QStringLiteral("old name.txt"));
        QVERIFY(parsed.snapshot.files.at(3).isConflict());
        QVERIFY(parsed.snapshot.files.at(4).isUntracked());
        QCOMPARE(parsed.snapshot.files.at(4).path, QStringLiteral("新文件\nname.txt"));
        QVERIFY(parsed.snapshot.files.at(5).isIgnored());
    }

    void parsesInitialAndDetachedHeads()
    {
        QByteArray initial;
        appendRecord(&initial, QByteArray("# branch.oid (initial)"));
        appendRecord(&initial, QByteArray("# branch.head main"));
        GitStatusParseResult parsed = GitStatusParser::parsePorcelainV2(initial);
        QVERIFY(parsed.succeeded());
        QVERIFY(parsed.snapshot.initialCommit);
        QCOMPARE(parsed.snapshot.branchName, QStringLiteral("main"));

        QByteArray detached;
        appendRecord(&detached, QByteArray("# branch.oid abcdef"));
        appendRecord(&detached, QByteArray("# branch.head (detached)"));
        parsed = GitStatusParser::parsePorcelainV2(detached);
        QVERIFY(parsed.succeeded());
        QVERIFY(parsed.snapshot.detachedHead);
        QVERIFY(parsed.snapshot.branchName.isEmpty());
    }

    void rejectsMalformedKnownRecords()
    {
        QByteArray malformed;
        appendRecord(&malformed, QByteArray("# branch.ab +x -2"));
        QVERIFY(!GitStatusParser::parsePorcelainV2(malformed).succeeded());

        malformed.clear();
        appendRecord(&malformed, QByteArray("2 R. missing-fields"));
        QVERIFY(!GitStatusParser::parsePorcelainV2(malformed).succeeded());

        malformed.clear();
        appendRecord(&malformed, QByteArray("# branch.oid"));
        QVERIFY(!GitStatusParser::parsePorcelainV2(malformed).succeeded());
    }

    void asynchronouslyRunsGitWithoutShellParsing()
    {
        const QString git = GitExecutable::find();
        if (git.isEmpty())
        {
            QTest::qSkip("Git is not installed on this test runner.", __FILE__, __LINE__);
            return;
        }

        GitCommandRunner runner(git);
        QSignalSpy finished(&runner, &GitCommandRunner::commandFinished);
        runner.start(QDir::tempPath(), {QStringLiteral("--version")});
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        const GitCommandResult result = finished.takeFirst().at(0).value<GitCommandResult>();
        QVERIFY2(result.succeeded(), qPrintable(result.processError));
        QVERIFY(result.standardOutput.startsWith("git version"));
        QCOMPARE(result.arguments, QStringList{QStringLiteral("--version")});
    }

    void missingGitDegradesWithoutBlocking()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        GitRepositoryService service(QStringLiteral("/definitely/missing/git"));
        QSignalSpy states(&service, &GitRepositoryService::stateChanged);
        service.setDirectory(directory.path());
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), GitRepositoryState::GitUnavailable, 5000);
        QVERIFY(!states.isEmpty());
        QVERIFY(!service.installation().isAvailable());
        QVERIFY(!service.installation().error.isEmpty());
    }

    void distinguishesNonRepositoryFromGitFailure()
    {
        const QString git = GitExecutable::find();
        if (git.isEmpty())
        {
            QTest::qSkip("Git is not installed on this test runner.", __FILE__, __LINE__);
            return;
        }

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        GitRepositoryService service;
        service.setDirectory(directory.path());
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), GitRepositoryState::NotRepository, 5000);
        QVERIFY(service.repositoryRoot().isEmpty());

        QVERIFY(initializeRepository(git, directory.path()));
        QSignalSpy snapshots(&service, &GitRepositoryService::snapshotChanged);
        service.setDirectory(directory.path());
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), GitRepositoryState::Ready, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(snapshots.count(), 1, 5000);
        QCOMPARE(service.repositoryRoot(), QFileInfo(directory.path()).canonicalFilePath());
    }

    void ignoresAmbientRepositoryDiscoveryOverrides()
    {
        const QString git = GitExecutable::find();
        if (git.isEmpty())
        {
            QTest::qSkip("Git is not installed on this test runner.", __FILE__, __LINE__);
            return;
        }

        QTemporaryDir repository;
        QVERIFY(repository.isValid());
        QVERIFY(initializeRepository(git, repository.path()));
        QVERIFY(QDir(repository.path()).mkpath(QStringLiteral("nested")));

        const EnvironmentVariableGuard gitDirGuard(QByteArrayLiteral("GIT_DIR"));
        const EnvironmentVariableGuard ceilingGuard(QByteArrayLiteral("GIT_CEILING_DIRECTORIES"));
        qputenv("GIT_DIR", QByteArrayLiteral("/definitely/missing/repository"));
        qputenv("GIT_CEILING_DIRECTORIES", repository.path().toUtf8());

        GitRepositoryService service(git);
        service.setDirectory(repository.filePath(QStringLiteral("nested")));
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), GitRepositoryState::Ready, 5000);
        QCOMPARE(service.repositoryRoot(), QFileInfo(repository.path()).canonicalFilePath());
    }

    void discoversRepositoryAndRefreshesStatus()
    {
        const QString git = GitExecutable::find();
        if (git.isEmpty())
        {
            QTest::qSkip("Git is not installed on this test runner.", __FILE__, __LINE__);
            return;
        }

        QTemporaryDir repository;
        QVERIFY(repository.isValid());
        QVERIFY2(initializeRepository(git, repository.path()), "git init failed");
        QVERIFY(QDir(repository.path()).mkpath(QStringLiteral("nested/child")));
        QVERIFY(writeFile(repository.filePath(QStringLiteral("untracked file.txt")), QByteArray("first\n")));

        GitRepositoryService service(git);
        QSignalSpy snapshots(&service, &GitRepositoryService::snapshotChanged);
        QSignalSpy errors(&service, &GitRepositoryService::errorOccurred);
        service.setDirectory(repository.filePath(QStringLiteral("nested/child")));
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), GitRepositoryState::Ready, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(snapshots.count(), 1, 5000);
        QVERIFY2(errors.isEmpty(), qPrintable(errors.isEmpty() ? QString() : errors.first().first().toString()));
        QCOMPARE(service.repositoryRoot(), QFileInfo(repository.path()).canonicalFilePath());
        QCOMPARE(service.snapshot().repositoryRoot, service.repositoryRoot());
        QCOMPARE(service.snapshot().revision, quint64(1));
        QCOMPARE(service.snapshot().files.size(), 1);
        QVERIFY(service.snapshot().files.first().isUntracked());

        QVERIFY(writeFile(repository.filePath(QStringLiteral("second.txt")), QByteArray("second\n")));
        snapshots.clear();
        service.requestRefresh();
        service.requestRefresh();
        service.requestRefresh();
        QTRY_COMPARE_WITH_TIMEOUT(snapshots.count(), 1, 5000);
        QCOMPARE(service.snapshot().revision, quint64(2));
        QCOMPARE(service.snapshot().files.size(), 2);
    }

    void parsesRealRenameRecords()
    {
        const QString git = GitExecutable::find();
        if (git.isEmpty())
        {
            QTest::qSkip("Git is not installed on this test runner.", __FILE__, __LINE__);
            return;
        }

        QTemporaryDir repository;
        QVERIFY(repository.isValid());
        QVERIFY(initializeRepository(git, repository.path()));
        const QString oldPath = repository.filePath(QStringLiteral("old name.txt"));
        const QString newPath = repository.filePath(QStringLiteral("new name.txt"));
        QVERIFY(writeFile(oldPath, QByteArray("rename me\n")));
        QCOMPARE(runGit(git, repository.path(),
                        {QStringLiteral("add"), QStringLiteral("--"), QStringLiteral("old name.txt")})
                     .exitCode,
                 0);
        QCOMPARE(runGit(git, repository.path(),
                        {QStringLiteral("-c"), QStringLiteral("user.name=NotePeek Test"), QStringLiteral("-c"),
                         QStringLiteral("user.email=test@notepeek.invalid"), QStringLiteral("-c"),
                         QStringLiteral("commit.gpgSign=false"), QStringLiteral("-c"),
                         QStringLiteral("core.hooksPath=.git/no-hooks"), QStringLiteral("commit"),
                         QStringLiteral("--quiet"), QStringLiteral("-m"), QStringLiteral("initial")})
                     .exitCode,
                 0);
        QVERIFY(QFile::rename(oldPath, newPath));
        QCOMPARE(runGit(git, repository.path(), {QStringLiteral("add"), QStringLiteral("-A")}).exitCode, 0);

        const ProcessResult status = runGit(git, repository.path(),
                                            {QStringLiteral("status"), QStringLiteral("--porcelain=v2"),
                                             QStringLiteral("-z"), QStringLiteral("--renames")});
        QCOMPARE(status.exitCode, 0);
        const GitStatusParseResult parsed = GitStatusParser::parsePorcelainV2(status.standardOutput);
        QVERIFY2(parsed.succeeded(), qPrintable(parsed.error));
        QCOMPARE(parsed.snapshot.files.size(), 1);
        QCOMPARE(parsed.snapshot.files.first().recordType, GitChangeRecordType::RenamedOrCopied);
        QCOMPARE(parsed.snapshot.files.first().path, QStringLiteral("new name.txt"));
        QCOMPARE(parsed.snapshot.files.first().originalPath, QStringLiteral("old name.txt"));
    }

    void clearsRepositoryStateWhenMetadataDisappears()
    {
        const QString git = GitExecutable::find();
        if (git.isEmpty())
        {
            QTest::qSkip("Git is not installed on this test runner.", __FILE__, __LINE__);
            return;
        }

        QTemporaryDir repository;
        QVERIFY(repository.isValid());
        QVERIFY(initializeRepository(git, repository.path()));
        QVERIFY(writeFile(repository.filePath(QStringLiteral("untracked.txt")), QByteArrayLiteral("content\n")));

        GitRepositoryService service(git);
        service.setDirectory(repository.path());
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), GitRepositoryState::Ready, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().files.size(), 1, 5000);

        QVERIFY(QDir(repository.path()).rename(QStringLiteral(".git"), QStringLiteral("git-metadata-removed")));
        service.refreshNow();
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), GitRepositoryState::NotRepository, 5000);
        QVERIFY(service.repositoryRoot().isEmpty());
        QVERIFY(service.snapshot().files.isEmpty());
    }
};

QTEST_MAIN(VersionControlTest)
#include "versioncontrol_test.moc"
