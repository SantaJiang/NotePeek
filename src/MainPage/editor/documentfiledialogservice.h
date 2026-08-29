#ifndef DOCUMENTFILEDIALOGSERVICE_H
#define DOCUMENTFILEDIALOGSERVICE_H

#include <QString>

class QWidget;

// UI adapter for save-file selection and display names. Persistence services
// never show dialogs or depend on editor widgets.
class DocumentFileDialogService final
{
public:
    static QString promptSaveFileName(QWidget *parent);
    static QString displayNameForSavePrompt(const QString &filePath);
};

#endif // DOCUMENTFILEDIALOGSERVICE_H
