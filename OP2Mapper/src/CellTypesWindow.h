#pragma once

#include <QWidget>

class QListWidget;
class QListWidgetItem;

class CellTypesWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CellTypesWindow(QWidget *parent = nullptr);

public slots:
    // Visually deselect any active row (without emitting cellTypeSelected).
    // Called when another tool palette (e.g. TilesetWindow) takes focus.
    void clearSelection();

signals:
    // cellTypeValue is the OP2Utility::CellType enum value cast to int.
    void cellTypeSelected(int cellTypeValue);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onItemActivated(QListWidgetItem *item);

private:
    void populate();
    void saveGeometry();
    void restoreGeometry();

    QListWidget *m_list = nullptr;
};
