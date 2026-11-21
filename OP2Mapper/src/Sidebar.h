#pragma once

#include <QColor>
#include <QLabel>
#include <QScrollArea>
#include <QStringList>

class QGroupBox;
class QListWidget;
class QPushButton;
class QVBoxLayout;
struct ToolLabels;

// Single-line label that ends in "…" when too long and shows the full text
// as a tooltip (WinForms AutoEllipsis).
class ElideLabel : public QLabel
{
public:
    using QLabel::QLabel;
    void setFullText(const QString &text);
    void setColor(const QColor &c);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QString m_full;
};

// The 264 px right-hand panel of OP2Mapper3's main window: Map, Tilesets,
// Tile Under Cursor, View and Editing sections.
class Sidebar : public QScrollArea
{
    Q_OBJECT
public:
    explicit Sidebar(QWidget *parent = nullptr);

    void clearMap();
    void setMapInfo(const QString &fileName, int w, int h, int tileCount);
    // rows: "name.bmp  (N tiles)" or "name.bmp  (not found)"
    void setTilesets(const QStringList &rows, int missing);
    void setTileInfo(bool valid, const QString &coords, const QString &tileset,
                     int imageIndex, int mappingIndex, const QString &cellType,
                     const QImage &tile, const QString &missingTileset);
    void setZoomText(const QString &text, bool locked);
    void setEditModeEnabled(bool enabled, bool on);
    void setToolLabels(const ToolLabels &labels, bool showIcon);

signals:
    void resetZoomClicked();
    void editModeClicked();
    void clearToolClicked();

private:
    QGroupBox *section(const QString &title, QVBoxLayout *&inner);
    ElideLabel *row(QVBoxLayout *layout, const QString &text);

    ElideLabel *m_mapName, *m_mapSize, *m_mapImage, *m_tileCount;
    QGroupBox *m_tilesetsBox;
    QListWidget *m_tilesetList;
    ElideLabel *m_coords, *m_tileset, *m_imageIndex, *m_mappingIndex, *m_cellType;
    QLabel *m_smallPreview, *m_bigPreview;
    QLabel *m_zoom;
    QPushButton *m_resetZoom;
    QPushButton *m_editMode;
    QPushButton *m_clearTool;
    QLabel *m_toolIcon;
    ElideLabel *m_toolLabel, *m_placementLabel;
};
