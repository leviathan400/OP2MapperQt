// TileSelectWindow: the Tile Selection palette.

#include "TileSelectWindow.h"

#include "CellTypeInfo.h"
#include "Editor.h"
#include "MapDocument.h"
#include "MapRenderer.h"
#include "Settings.h"
#include "TileGroupIO.h"

#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QUrl>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kZoomLevels[] = { 1.0, 0.75, 0.5, 0.25 };
// Client sizes matching OP2Mapper3's 498x484 / 1000x880 outer sizes.
constexpr QSize kNormal(482, 445);
constexpr QSize kLarge(984, 841);
} // namespace

// The preview surface inside the scroll area.
class TileSelectCanvas : public QWidget
{
public:
    TileSelectCanvas(TileSelectWindow *owner, QScrollArea *scroll)
        : QWidget(scroll), m_owner(owner), m_scroll(scroll) {}

    void setImage(const QImage *img)
    {
        m_img = img;
        const QSize view = m_scroll->viewport()->size();
        resize(img && !img->isNull() ? img->size().expandedTo(view) : view);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        if (!m_img || m_img->isNull()) {
            p.fillRect(rect(), QColor(211, 211, 211));
            QFont f(QStringLiteral("Arial"));
            f.setPixelSize(13);
            p.setFont(f);
            p.setPen(Qt::black);
            p.drawText(QPoint(10, 10 + p.fontMetrics().ascent()), QObject::tr("No tiles copied"));
            p.drawText(QPoint(10, 30 + p.fontMetrics().ascent()), QObject::tr("Use Copy (Ctrl+C) to copy tiles"));
            return;
        }
        p.fillRect(rect(), QColor(169, 169, 169));
        p.drawImage(0, 0, *m_img);
    }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::RightButton) {
            m_panning = true;
            m_downPos = m_last = e->globalPosition().toPoint();
            setCursor(Qt::OpenHandCursor);
        }
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (!m_panning)
            return;
        const QPoint g = e->globalPosition().toPoint();
        const QPoint d = m_last - g;
        m_last = g;
        m_scroll->horizontalScrollBar()->setValue(m_scroll->horizontalScrollBar()->value() + d.x());
        m_scroll->verticalScrollBar()->setValue(m_scroll->verticalScrollBar()->value() + d.y());
    }
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::RightButton || !m_panning)
            return;
        m_panning = false;
        unsetCursor();
        const QPoint d = e->globalPosition().toPoint() - m_downPos;
        if (std::sqrt(double(d.x()) * d.x() + double(d.y()) * d.y()) < 5 && m_img && !m_img->isNull())
            m_owner->showContextMenu(e->globalPosition().toPoint());
    }
    void wheelEvent(QWheelEvent *e) override
    {
        if (e->modifiers() & Qt::ControlModifier) {
            m_owner->stepZoom(e->angleDelta().y());
            e->accept();
            return;
        }
        e->ignore();
    }

private:
    TileSelectWindow *m_owner;
    QScrollArea *m_scroll;
    const QImage *m_img = nullptr;
    bool m_panning = false;
    QPoint m_downPos, m_last;
};

TileSelectWindow::TileSelectWindow(Editor *editor, QWidget *mainWindow)
    : ToolPalette(QStringLiteral("TileSelect"), QStringLiteral("fTileSelect"), mainWindow)
    , m_editor(editor)
{
    setWindowTitle(tr("Tile Selection"));
    m_large = Settings::windowBool(QStringLiteral("TileSelectLarge"), false);
    const QString z = Settings::windowValue(QStringLiteral("TileSelectZoom"), QStringLiteral("100")).trimmed();
    if (z.compare(QLatin1String("Fit"), Qt::CaseInsensitive) == 0) {
        m_fit = true;
    } else {
        bool ok = false;
        const int pct = z.toInt(&ok);
        if (ok && (pct == 100 || pct == 75 || pct == 50 || pct == 25))
            m_zoom = pct / 100.0;
    }
    m_showCellTypes = Settings::windowBool(QStringLiteral("TileSelectShowCellTypes"), false);

    m_scroll = new QScrollArea(this);
    m_scroll->setFrameShape(QFrame::Box);
    m_scroll->setWidgetResizable(false);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea { background: #A9A9A9; }"));
    m_canvas = new TileSelectCanvas(this, m_scroll);
    m_scroll->setWidget(m_canvas);
    applyWindowSize();

    connect(m_editor, &Editor::clipboardChanged, this, &TileSelectWindow::refreshPreview);
    connect(m_editor->document(), &MapDocument::documentReplaced, this, &TileSelectWindow::refreshPreview);
    refreshPreview();
}

// Normal or Large size, shrunk to the screen and kept inside it.
void TileSelectWindow::applyWindowSize()
{
    QSize target = m_large ? kLarge : kNormal;
    if (QScreen *s = screen()) {
        const QRect wa = s->availableGeometry();
        target = target.boundedTo(wa.size() - QSize(16, 39));
        setFixedSize(target);
        const QRect f = frameGeometry();
        const int x = std::max(wa.left(), std::min(f.left(), wa.right() + 1 - f.width()));
        const int y = std::max(wa.top(), std::min(f.top(), wa.bottom() + 1 - f.height()));
        if (isVisible())
            move(x, y);
    } else {
        setFixedSize(target);
    }
    m_scroll->setGeometry(1, 1, width() - 2, height() - 2);
}

// Pixels per tile for the current zoom, or the largest that fits for "Zoom to Fit Window".
int TileSelectWindow::previewTileSize(int w, int h) const
{
    const QSize view = m_scroll->size() - QSize(2, 2);
    if (m_fit)
        return std::max(1, std::min(32, std::min(view.width() / w, view.height() / h)));
    return std::max(1, static_cast<int>(std::lround(32 * m_zoom)));
}

// Each new copy opens at the largest zoom that shows it whole; if none does, the current zoom stays.
void TileSelectWindow::pickZoomForNewCopy(int w, int h)
{
    const QSize view = m_scroll->size() - QSize(2, 2);
    for (double z : kZoomLevels) {
        const int ts = std::max(1, static_cast<int>(std::lround(32 * z)));
        if (w * ts <= view.width() && h * ts <= view.height()) {
            m_zoom = z;
            m_fit = false;
            m_scroll->horizontalScrollBar()->setValue(0);
            m_scroll->verticalScrollBar()->setValue(0);
            return;
        }
    }
}

// Renders the copy: light grey background, grey grid when tiles are at least
// 8 px, optional cell-type shading, dark blue border. Large copies use a
// smaller tile size so the image stays within 4096 px.
void TileSelectWindow::refreshPreview()
{
    m_preview = QImage();
    const TileClipboardData &c = m_editor->clipboard();
    if (c.isEmpty()) {
        setWindowTitle(tr("Tile Selection - No Copied Tiles"));
        m_canvas->setImage(nullptr);
        return;
    }
    if (c.generation != m_autoZoomedGeneration) {
        m_autoZoomedGeneration = c.generation;
        pickZoomForNewCopy(c.width, c.height);
    }
    setWindowTitle(m_fit ? tr("Tile Selection - %1×%2 Tiles (Fit)").arg(c.width).arg(c.height)
                         : tr("Tile Selection - %1×%2 Tiles (%3%)").arg(c.width).arg(c.height)
                               .arg(static_cast<int>(m_zoom * 100)));
    const MapDocument *doc = m_editor->document();
    if (!doc || !doc->isLoaded()) {
        m_canvas->setImage(nullptr);
        return;
    }
    const int ts = std::max(1, std::min(previewTileSize(c.width, c.height), 4096 / std::max(c.width, c.height)));
    QImage img(c.width * ts, c.height * ts, QImage::Format_ARGB32);
    img.fill(QColor(211, 211, 211));
    QPainter p(&img);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    for (int y = 0; y < c.height; ++y) {
        for (int x = 0; x < c.width; ++x) {
            const QImage tile = MapRenderer::mappingImage(*doc, c.mappingAt(x, y));
            if (!tile.isNull())
                p.drawImage(QRect(x * ts, y * ts, ts, ts), tile);
            if (ts >= 8) {
                p.setPen(QColor(128, 128, 128));
                p.drawRect(x * ts, y * ts, ts, ts);
            }
        }
    }
    if (m_showCellTypes && c.cellTypes.size() >= c.mappings.size()) {
        for (int y = 0; y < c.height; ++y) {
            for (int x = 0; x < c.width; ++x) {
                QColor col = CellTypeInfo::overlayColor(c.cellTypeAt(x, y));
                col.setAlpha(CellTypeInfo::OverlayAlpha);
                p.fillRect(x * ts, y * ts, ts, ts, col);
            }
        }
    }
    p.setPen(QPen(QColor(0, 0, 139), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(1, 1, img.width() - 2, img.height() - 2));
    p.end();
    m_preview = img;
    m_canvas->setImage(&m_preview);
}

void TileSelectWindow::setLargeWindow(bool on)
{
    m_large = on;
    Settings::setWindowBool(QStringLiteral("TileSelectLarge"), on);
    applyWindowSize();
    savePosition();
    refreshPreview();
}

// Keeps the same point of the copy centred and saves the zoom.
void TileSelectWindow::setZoom(double zoom, bool fit)
{
    QScrollBar *h = m_scroll->horizontalScrollBar();
    QScrollBar *v = m_scroll->verticalScrollBar();
    const QSize view = m_scroll->viewport()->size();
    double cx = 0.5, cy = 0.5;
    if (!m_preview.isNull()) {
        cx = (h->value() + view.width() / 2.0) / m_preview.width();
        cy = (v->value() + view.height() / 2.0) / m_preview.height();
    }
    m_zoom = zoom;
    m_fit = fit;
    Settings::setWindowValue(QStringLiteral("TileSelectZoom"),
                             fit ? QStringLiteral("Fit") : QString::number(static_cast<int>(zoom * 100)));
    refreshPreview();
    if (!m_preview.isNull()) {
        h->setValue(std::max(0, static_cast<int>(cx * m_preview.width() - view.width() / 2.0)));
        v->setValue(std::max(0, static_cast<int>(cy * m_preview.height() - view.height() / 2.0)));
    }
}

void TileSelectWindow::setShowCellTypes(bool on)
{
    m_showCellTypes = on;
    Settings::setWindowBool(QStringLiteral("TileSelectShowCellTypes"), on);
    const int hv = m_scroll->horizontalScrollBar()->value();
    const int vv = m_scroll->verticalScrollBar()->value();
    refreshPreview();
    m_scroll->horizontalScrollBar()->setValue(hv);
    m_scroll->verticalScrollBar()->setValue(vv);
}

// Ctrl+wheel steps 100 / 75 / 50 / 25%; from Fit it restarts at 100%.
void TileSelectWindow::stepZoom(int delta)
{
    int idx = 0;
    if (!m_fit) {
        for (int i = 0; i < 4; ++i)
            if (std::abs(kZoomLevels[i] - m_zoom) < 0.001)
                idx = i;
    }
    idx = delta > 0 ? idx - 1 : idx + 1;
    idx = std::clamp(idx, 0, 3);
    setZoom(kZoomLevels[idx], false);
}

void TileSelectWindow::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        stepZoom(event->angleDelta().y());
        event->accept();
        return;
    }
    QScrollBar *bar = (event->modifiers() & Qt::ShiftModifier) ? m_scroll->horizontalScrollBar()
                                                               : m_scroll->verticalScrollBar();
    bar->setValue(bar->value() - event->angleDelta().y());
    event->accept();
}

// Title-bar menu extras: Large Window, the zoom levels, Show Cell Types.
void TileSelectWindow::addMenuExtras(QMenu *menu)
{
    QAction *large = menu->addAction(tr("Large Window"));
    large->setCheckable(true);
    large->setChecked(m_large);
    connect(large, &QAction::triggered, this, [this] { setLargeWindow(!m_large); });
    menu->addSeparator();
    for (double z : kZoomLevels) {
        QAction *a = menu->addAction(tr("Zoom %1%").arg(static_cast<int>(z * 100)));
        a->setCheckable(true);
        a->setChecked(!m_fit && std::abs(m_zoom - z) < 0.001);
        connect(a, &QAction::triggered, this, [this, z] { setZoom(z, false); });
    }
    QAction *fit = menu->addAction(tr("Zoom to Fit Window"));
    fit->setCheckable(true);
    fit->setChecked(m_fit);
    connect(fit, &QAction::triggered, this, [this] { setZoom(m_zoom, true); });
    menu->addSeparator();
    QAction *cells = menu->addAction(tr("Show Cell Types"));
    cells->setCheckable(true);
    cells->setChecked(m_showCellTypes);
    connect(cells, &QAction::triggered, this, [this] { setShowCellTypes(!m_showCellTypes); });
}

void TileSelectWindow::showContextMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    QAction *exp = menu.addAction(tr("Export Tile Group"));
    menu.addSeparator();
    QAction *cells = menu.addAction(tr("Show Cell Types"));
    cells->setCheckable(true);
    cells->setChecked(m_showCellTypes);
    QAction *chosen = menu.exec(globalPos);
    if (chosen == exp)
        exportTileGroup();
    else if (chosen == cells)
        setShowCellTypes(!m_showCellTypes);
}

// Right-click > Export Tile Group: writes <Tilegroups>/Exported/<name>.bmp
// (then offers to show it), then <name>.json; existing files are replaced.
void TileSelectWindow::exportTileGroup()
{
    const TileClipboardData &c = m_editor->clipboard();
    const MapDocument *doc = m_editor->document();
    if (c.isEmpty() || m_preview.isNull() || !doc || !doc->isLoaded()) {
        QMessageBox::information(this, tr("Export Tiles"), tr("No tiles to export. Please copy a tile group first."));
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Export Tile Group"),
        tr("Enter a name for this tile group:"), QLineEdit::Normal,
        QStringLiteral("TileGroup_%1x%2").arg(c.width).arg(c.height), &ok);
    if (!ok || name.isEmpty())
        return;
    QString safe = name;
    for (QChar &ch : safe) {
        if (ch.unicode() < 0x20 || QStringLiteral("\"<>|:*?\\/").contains(ch))
            ch = QLatin1Char('_');
    }
    QString root = Settings::tileGroupsPath();
    if (root.isEmpty()) {
        root = Settings::defaultTileGroupsPath();
        Settings::setTileGroupsPath(root);
    }
    const QString dir = QDir(root).filePath(QStringLiteral("Exported"));
    QDir().mkpath(dir);
    const QString bmpPath = QDir(dir).filePath(safe + QStringLiteral(".bmp"));
    const QString jsonPath = QDir(dir).filePath(safe + QStringLiteral(".json"));

    QString err;
    if (TileGroupIO::writeBmp(bmpPath, *doc, c.width, c.height, c.mappings, err)) {
        if (QMessageBox::question(this, tr("Open Folder"),
                tr("Do you want to open the folder containing the exported file?"),
                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
#ifdef Q_OS_WIN
            QProcess::startDetached(QStringLiteral("explorer.exe"),
                { QStringLiteral("/select,") + QDir::toNativeSeparators(bmpPath) });
#else
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
#endif
        }
    } else {
        QMessageBox::critical(this, tr("Export Error"), tr("Error exporting tile group: %1").arg(err));
    }
    if (!TileGroupIO::writeJson(jsonPath, c.width, c.height, c.mappings,
                                TileGroupIO::tilesetPrefix(*doc, c.mappings), safe, err)) {
        QMessageBox::critical(this, tr("Export Error"), tr("Error exporting tile group to JSON: %1").arg(err));
    }
    emit tileGroupExported();
}

void TileSelectWindow::showEvent(QShowEvent *event)
{
    ToolPalette::showEvent(event);
    m_canvas->setImage(m_preview.isNull() ? nullptr : &m_preview);
}
