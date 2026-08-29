#include <QtTest>

#include "editor/documentsession.h"

class DocumentSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void saveTransitionsPreserveEditsMadeDuringWrite()
    {
        DocumentSession session;
        QCOMPARE(session.persistenceState(), DocumentPersistenceState::Clean);

        session.beginSave();
        QCOMPARE(session.persistenceState(), DocumentPersistenceState::Saving);
        session.updateForModification(false);
        QCOMPARE(session.persistenceState(), DocumentPersistenceState::Saving);

        session.completeSave(true);
        QCOMPARE(session.persistenceState(), DocumentPersistenceState::Dirty);
        session.beginSave();
        session.completeSave(false);
        QCOMPARE(session.persistenceState(), DocumentPersistenceState::Clean);
    }

    void conflictsAndFailuresAreNotHiddenByFurtherEdits()
    {
        DocumentSession session;
        session.setExternalConflict(true, true);
        QVERIFY(session.hasExternalConflict());
        QVERIFY(session.wasDeletedExternally());
        QCOMPARE(session.persistenceState(), DocumentPersistenceState::ExternalConflict);

        session.updateForModification(true);
        QCOMPARE(session.persistenceState(), DocumentPersistenceState::ExternalConflict);
        session.updateForModification(false);
        QCOMPARE(session.persistenceState(), DocumentPersistenceState::ExternalConflict);
        session.setExternalConflict(false);
        QVERIFY(!session.hasExternalConflict());
        QVERIFY(!session.wasDeletedExternally());
        session.updateForModification(true);
        QCOMPARE(session.persistenceState(), DocumentPersistenceState::Dirty);

        session.failSave();
        session.updateForModification(true);
        QCOMPARE(session.persistenceState(), DocumentPersistenceState::SaveFailed);
        session.updateForModification(false);
        QCOMPARE(session.persistenceState(), DocumentPersistenceState::Clean);
    }

    void forcedModificationIsSharedState()
    {
        DocumentSession session;
        QVERIFY(!session.isModified(false));
        QVERIFY(session.forceModified(false));
        QVERIFY(session.isModified(false));
        QVERIFY(!session.forceModified(false));
        session.clearForcedModification();
        QVERIFY(!session.isModified(false));
        QVERIFY(session.isModified(true));
    }

    void encodingConversionCanCommitOrRollback()
    {
        DocumentSession session;
        DocumentFormat original = session.format();
        original.encoding = DocumentEncoding::Utf8Bom;
        session.setFormat(original);

        session.beginEncodingConversion(DocumentEncoding::Utf16LittleEndian);
        QVERIFY(session.hasPendingEncodingConversion());
        QCOMPARE(session.format().encoding, DocumentEncoding::Utf16LittleEndian);
        session.finishEncodingConversion(false);
        QCOMPARE(session.format().encoding, DocumentEncoding::Utf8Bom);
        QVERIFY(!session.hasPendingEncodingConversion());

        session.beginEncodingConversion(DocumentEncoding::Utf16BigEndian);
        session.finishEncodingConversion(true);
        QCOMPARE(session.format().encoding, DocumentEncoding::Utf16BigEndian);
    }

    void largeFilePromotionAndReloadGenerationAreMonotonic()
    {
        DocumentSession session;
        session.promoteToLargeFileMode(true);
        session.promoteToLargeFileMode(false);
        QVERIFY(session.isLargeFileMode());

        const quint64 first = session.beginEncodingReload();
        const quint64 second = session.beginEncodingReload();
        QVERIFY(second > first);
        QVERIFY(!session.isCurrentEncodingReload(first));
        QVERIFY(session.isCurrentEncodingReload(second));
    }
};

QTEST_MAIN(DocumentSessionTest)
#include "documentsession_test.moc"
