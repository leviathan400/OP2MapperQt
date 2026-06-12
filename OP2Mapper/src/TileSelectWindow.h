#pragma once

#include <QWidget>

class MapDocument;
class TileClipboard;
class QLabel;
class QPushButton;

// Tool palette mirroring VB fTileSelect.vb — shows a preview of the current
// clipboard contents and a button to export the region as a tile group
// (BMP + JSON pair) into the configured Tilegroups\Exported\ folder.
class TileSelectWindow : public QWidget
{
    Q_OBJECT
public:
    explicit TileSelectWindow(QWidget *parent = nullptr);

    // Both pointers must outlive this window or be cleared before destruction.
    void setClipboard(const TileClipboard *clip);
    void setDocument(MapDocument *doc);

public slots:
    // Call after the clipboard's contents change so the preview repaints.
    void refresh();

signals:
    // Fired after a successful tile-group export so the TileGroupsWindow can
    // refresh and highlight the Exported folder.
    void tileGroupSaved(const QString &name);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onSaveClicked();

private:
    void saveGeometry();
    void restoreGeometry();

    const TileClipboard *m_clipboard = nullptr;
    MapDocument *m_doc = nullptr;
    QPushButton *m_saveButton = nullptr;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
};
