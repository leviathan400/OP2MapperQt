#pragma once

#include <QGraphicsScene>

class MapDocument;

class MapScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit MapScene(QObject *parent = nullptr);

    void setDocument(const MapDocument *doc);

    void setShowCellTypes(bool on);
    bool showCellTypes() const { return m_showCellTypes; }

    void setShowGrid(bool on);
    bool showGrid() const { return m_showGrid; }

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    const MapDocument *m_doc = nullptr;
    bool m_showCellTypes = false;
    bool m_showGrid = false;
};
