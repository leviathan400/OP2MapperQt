#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

// Tool palette mirroring VB fTileGroups.vb. Two combos drive selection:
// Folder (category subdir, with "(Default)" for loose root JSONs) and
// Tile Group (the .json file). Selecting a group loads its BMP preview
// and shows header details on the right. Place button emits a signal
// that MainWindow handles by entering paste mode with the group's
// resolved clipboard contents.
class TileGroupsWindow : public QWidget
{
    Q_OBJECT
public:
    explicit TileGroupsWindow(QWidget *parent = nullptr);

    // Re-scan the tile-groups path from Settings. If selectFolder is
    // non-empty, switch to that folder afterwards (used by export-then-
    // refresh to auto-select "Exported").
    void refresh(const QString &selectFolder = QString());

signals:
    // Fired when the user clicks Place with a valid selection. Absolute path.
    void placeRequested(const QString &jsonPath);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onFolderChanged(int folderRow);
    void onGroupChanged(int groupRow);
    void onPlaceClicked();

private:
    QString folderPath(int folderRow) const;
    QString currentJsonPath() const;
    void populateGroupsForCurrentFolder();
    void updateDetailsAndPreview();
    void saveGeometry();
    void restoreGeometry();

    QComboBox *m_folderCombo = nullptr;
    QComboBox *m_groupCombo = nullptr;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_pathLabel = nullptr;

    QLabel *m_countValue = nullptr;
    QLabel *m_tilesetValue = nullptr;
    QLabel *m_nameValue = nullptr;
    QLabel *m_widthValue = nullptr;
    QLabel *m_heightValue = nullptr;
    QLabel *m_notesValue = nullptr;

    QPushButton *m_placeButton = nullptr;
};
