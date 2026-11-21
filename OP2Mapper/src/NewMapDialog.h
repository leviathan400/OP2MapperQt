#pragma once

#include <QDialog>
#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

// File > New Map (OP2Mapper3 fNewMap): name, auto file name, tileset,
// notes, size and starting biome. The owner supplies the create step:
// returning false keeps the dialog open.
class NewMapDialog : public QDialog
{
    Q_OBJECT
public:
    enum Biome { Blank = 0, Terrain0, Terrain1, Terrain2 };

    struct Result {
        QString mapName;
        QString fileName;
        QString tileset;
        QString notes;
        int width = 128;
        int height = 128;
        Biome biome = Blank;
    };

    using CreateFn = std::function<bool(const Result &)>;
    NewMapDialog(CreateFn create, QWidget *parent = nullptr);

    // Error from extracting the blank templates (shown on open), if any.
    void reportTemplateProblems();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void regenerateFileName();
    void selectBiome(int index);
    void onOk();

    CreateFn m_create;
    QLineEdit *m_mapName = nullptr;
    QLineEdit *m_fileName = nullptr;
    QLineEdit *m_tileset = nullptr;
    QPlainTextEdit *m_notes = nullptr;
    QComboBox *m_size = nullptr;
    QList<QLabel *> m_biomePanels;
    QList<QLabel *> m_biomeLabels;
    int m_biome = 0;
};
