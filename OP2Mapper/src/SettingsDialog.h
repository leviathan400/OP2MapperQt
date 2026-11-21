#pragma once

#include <QDialog>

class QLineEdit;

// File > Settings (OP2Mapper3 fSettings): the three folders, with the VB
// validation rules. Writes Mapper3.ini [Paths] on Save.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private:
    void browseOp2();
    void browseFolder(QLineEdit *edit, const QString &title, const QString &fallback);
    bool checkOptionalFolder(QLineEdit *edit, const QString &description);
    void onSave();

    QLineEdit *m_op2 = nullptr;
    QLineEdit *m_working = nullptr;
    QLineEdit *m_tileGroups = nullptr;
};

// Small dialogs
void showAboutDialog(QWidget *parent);
