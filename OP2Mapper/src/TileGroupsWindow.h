#pragma once

#include "ToolPalette.h"

#include <QImage>

class Editor;
class QComboBox;
class QLabel;
class QScrollArea;
class TileGroupsPreview;

// "Tile Groups" palette (OP2Mapper3 fTileGroups, tool 4): browse the tile
// group library (subfolders of the Tilegroups folder, "(Default)" for loose
// files), preview a group against the current map, and place it.
class TileGroupsWindow : public ToolPalette
{
    Q_OBJECT
public:
    TileGroupsWindow(Editor *editor, QWidget *mainWindow);

    // Re-read the Tilegroups folder; optionally select a folder by name
    // (and rescan it even if it was already selected).
    void refreshFromDisk(const QString &selectFolder = QString());

protected:
    void showEvent(QShowEvent *event) override;

private:
    friend class TileGroupsPreview;
    void loadFolders();
    void onFolderChanged();
    void onGroupChanged();
    void resetLabels();
    void renderPreview();

    Editor *m_editor;
    QString m_root;
    bool m_noGroupsAvailable = true;
    QComboBox *m_folderCombo = nullptr;
    QComboBox *m_groupCombo = nullptr;
    QScrollArea *m_scroll = nullptr;
    TileGroupsPreview *m_preview = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLabel *m_countValue = nullptr;
    QLabel *m_tilesetValue = nullptr;
    QLabel *m_nameValue = nullptr;
    QLabel *m_widthValue = nullptr;
    QLabel *m_heightValue = nullptr;
    QLabel *m_notesValue = nullptr;
    QImage m_previewImage;
};
