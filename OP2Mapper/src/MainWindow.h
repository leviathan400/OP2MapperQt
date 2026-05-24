#pragma once

#include <QMainWindow>
#include <memory>

class MapDocument;
class MapView;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenMap();
    void onCloseMap();
    void onSetOp2Folder();
    void onTileHovered(int x, int y);

private:
    QString ensureOp2Folder();

    MapView *m_mapView = nullptr;
    QLabel *m_statusPath = nullptr;
    QLabel *m_statusCoords = nullptr;
    QAction *m_closeAction = nullptr;
    QAction *m_cellTypesAction = nullptr;
    QAction *m_gridAction = nullptr;
    std::unique_ptr<MapDocument> m_doc;
};
