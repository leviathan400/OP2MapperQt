// ToolPalette: shared behaviour of the six tool windows (OP2Mapper3
// ToolPalette.vb): position persistence, smart default layout, snapping and
// the always-on-top title-bar menu.

#include "ToolPalette.h"

#include "Settings.h"

#include <QApplication>
#include <QGuiApplication>
#include <QHideEvent>
#include <QIcon>
#include <QMenu>
#include <QMoveEvent>
#include <QScreen>
#include <QWindow>

#include <algorithm>
#include <climits>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {
QList<ToolPalette *> g_palettes;
constexpr int kSnap = 20;

// A saved position is usable if the 30 px title-bar strip touches some
// screen, so a window last shown on a disconnected monitor gets the default
// layout instead of opening off-screen.
bool titleStripOnScreen(const QRect &frame)
{
    const QRect strip(frame.left(), frame.top(), frame.width(), 30);
    for (QScreen *s : QGuiApplication::screens()) {
        if (s->availableGeometry().intersects(strip))
            return true;
    }
    return false;
}

int overlapArea(const QRect &a, const QRect &b)
{
    const QRect i = a.intersected(b);
    return i.isEmpty() ? 0 : i.width() * i.height();
}

// OP2Mapper3 FindFreeSpot: try positions against the work area's right edge
// and the edges of windows already placed; pick the least overlap, then the
// right-most, then the top-most. Fills columns from the right edge.
QRect findFreeSpot(const QRect &wa, const QSize &size, const QList<QRect> &placed)
{
    QList<int> xs{ wa.right() + 1 - size.width() };
    QList<int> ys{ wa.top() };
    for (const QRect &r : placed) {
        xs << r.left() - size.width() << r.right() + 1 - size.width();
        ys << r.bottom() + 1 << r.top();
    }
    QRect best;
    int bestScore = INT_MAX;
    for (int x : xs) {
        for (int y : ys) {
            const int cx = std::clamp(x, wa.left(), std::max(wa.left(), wa.right() + 1 - size.width()));
            const int cy = std::clamp(y, wa.top(), std::max(wa.top(), wa.bottom() + 1 - size.height()));
            const QRect cand(QPoint(cx, cy), size);
            int score = 0;
            for (const QRect &r : placed)
                score += overlapArea(cand, r);
            if (score < bestScore
                || (score == bestScore && (cand.x() > best.x() || (cand.x() == best.x() && cand.y() < best.y())))) {
                bestScore = score;
                best = cand;
            }
        }
    }
    return best;
}
} // namespace

// A Qt::Tool window without a close button. Stay-on-top comes from
// AlwaysOnTop_<class>, defaulting to ToolWindowsAlwaysOnTop.
ToolPalette::ToolPalette(const QString &key, const QString &className, QWidget *mainWindow)
    : QWidget(mainWindow, Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowTitleHint)
    , m_key(key), m_className(className)
{
    setWindowIcon(QIcon(QStringLiteral(":/images/lists.ico")));
    m_alwaysOnTop = Settings::windowBool(QStringLiteral("AlwaysOnTop_") + className,
                                         allToolWindowsAlwaysOnTop());
    setWindowFlag(Qt::WindowStaysOnTopHint, m_alwaysOnTop);
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(400);
    connect(&m_saveTimer, &QTimer::timeout, this, &ToolPalette::savePosition);
    g_palettes.append(this);
}

ToolPalette::~ToolPalette()
{
    g_palettes.removeAll(this);
}

QList<ToolPalette *> ToolPalette::palettes() { return g_palettes; }

ToolPalette *ToolPalette::notifier()
{
    return g_palettes.isEmpty() ? nullptr : g_palettes.first();
}

bool ToolPalette::allToolWindowsAlwaysOnTop()
{
    return Settings::windowBool(QStringLiteral("ToolWindowsAlwaysOnTop"), true);
}

// Sets every palette (open or not) and the global key, and syncs View > Tool Windows Always On Top.
void ToolPalette::setAllAlwaysOnTop(bool on)
{
    Settings::setWindowBool(QStringLiteral("ToolWindowsAlwaysOnTop"), on);
    static const char *classes[] = { "fTileSet", "fCellTypes", "fTileSelect", "fTileGroups", "fMiniMap", "fTerrain" };
    for (const char *c : classes)
        Settings::setWindowBool(QStringLiteral("AlwaysOnTop_") + QString::fromLatin1(c), on);
    for (ToolPalette *p : g_palettes)
        p->setAlwaysOnTop(on, false);
    if (ToolPalette *n = notifier())
        emit n->allAlwaysOnTopChanged(on);
}

// Changing window flags hides the window, so it is shown again in place.
void ToolPalette::setAlwaysOnTop(bool on, bool persist)
{
    m_alwaysOnTop = on;
    if (persist)
        Settings::setWindowBool(QStringLiteral("AlwaysOnTop_") + m_className, on);
    const bool visible = isVisible();
    const QPoint p = pos();
    setWindowFlag(Qt::WindowStaysOnTopHint, on);
    if (visible) {
        move(p);
        show();
    }
}

void ToolPalette::open()
{
    show();
    raise();
}

bool ToolPalette::restorePosition()
{
    const QString l = m_key + QStringLiteral("Left");
    const QString t = m_key + QStringLiteral("Top");
    if (!Settings::hasWindowValue(l) || !Settings::hasWindowValue(t))
        return false;
    bool okL = false, okT = false;
    const int x = Settings::windowValue(l).trimmed().toInt(&okL);
    const int y = Settings::windowValue(t).trimmed().toInt(&okT);
    if (!okL || !okT)
        return false;
    if (!titleStripOnScreen(QRect(QPoint(x, y), frameGeometry().size())))
        return false;
    move(x, y);
    return true;
}

void ToolPalette::savePosition()
{
    if (!isVisible() || isMinimized())
        return;
    const QRect f = frameGeometry();
    Settings::setWindowInt(m_key + QStringLiteral("Left"), f.x());
    Settings::setWindowInt(m_key + QStringLiteral("Top"), f.y());
    Settings::setWindowInt(m_key + QStringLiteral("Width"), f.width());
    Settings::setWindowInt(m_key + QStringLiteral("Height"), f.height());
}

// Saved shortly after a move ends, and again when the palette hides.
void ToolPalette::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    if (isVisible())
        m_saveTimer.start();
}

void ToolPalette::hideEvent(QHideEvent *event)
{
    if (m_saveTimer.isActive())
        m_saveTimer.stop();
    savePosition();
    QWidget::hideEvent(event);
}

// The title-bar menu: the two always-on-top items, then the palette's own extras (Tile Selection adds size, zoom and cell types).
void ToolPalette::showTitleMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    QAction *self = menu.addAction(tr("Always On Top (This Window)"));
    self->setCheckable(true);
    self->setChecked(m_alwaysOnTop);
    bool allOn = true;
    for (ToolPalette *p : g_palettes) {
        if (p->isVisible() && !p->alwaysOnTop())
            allOn = false;
    }
    QAction *all = menu.addAction(tr("All Tool Windows Always On Top"));
    all->setCheckable(true);
    all->setChecked(allOn);
    QMenu extras;
    addMenuExtras(&extras);
    if (!extras.actions().isEmpty()) {
        menu.addSeparator();
        for (QAction *a : extras.actions())
            menu.addAction(a);
    }
    QAction *chosen = menu.exec(globalPos);
    if (chosen == self)
        setAlwaysOnTop(!m_alwaysOnTop, true);
    else if (chosen == all)
        setAllAlwaysOnTop(!allOn);
}

#ifdef Q_OS_WIN
namespace {
// Distance between the window rect and the visible (DWM) frame, so that
// snapping lines up the visible edges.
QMargins invisibleBorders(HWND hwnd)
{
    RECT wr, fr;
    if (!GetWindowRect(hwnd, &wr))
        return {};
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &fr, sizeof fr)))
        return {};
    return QMargins(fr.left - wr.left, fr.top - wr.top, wr.right - fr.right, wr.bottom - fr.bottom);
}

QRect visibleRect(HWND hwnd)
{
    RECT fr;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &fr, sizeof fr)))
        return QRect(QPoint(fr.left, fr.top), QPoint(fr.right - 1, fr.bottom - 1));
    RECT wr;
    GetWindowRect(hwnd, &wr);
    return QRect(QPoint(wr.left, wr.top), QPoint(wr.right - 1, wr.bottom - 1));
}
} // namespace

// Windows only:
//   WM_MOVING        snap the visible frame (DWM bounds, so the invisible resize
//                    borders don't leave gaps) to screen work areas and other
//                    palettes within 20 px; holding Ctrl moves freely
//   WM_NCRBUTTONUP   right-click on the caption opens the title-bar menu
//   Alt+Space        opens it at the window's top-left
bool ToolPalette::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    MSG *msg = static_cast<MSG *>(message);
    switch (msg->message) {
    case WM_MOVING: {
        if (GetKeyState(VK_CONTROL) & 0x8000)
            break;
        RECT *r = reinterpret_cast<RECT *>(msg->lParam);
        const QMargins b = invisibleBorders(msg->hwnd);
        // Proposed visible rect (physical pixels).
        QRect vis(QPoint(r->left + b.left(), r->top + b.top()),
                  QPoint(r->right - 1 - b.right(), r->bottom - 1 - b.bottom()));
        const int w = vis.width(), h = vis.height();
        int x = vis.left(), y = vis.top();
        QList<QRect> areas;
        EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR mon, HDC, LPRECT, LPARAM data) -> BOOL {
            auto *list = reinterpret_cast<QList<QRect> *>(data);
            MONITORINFO info{};
            info.cbSize = sizeof info;
            if (GetMonitorInfo(mon, &info)) {
                const RECT &wa = info.rcWork;
                list->append(QRect(QPoint(wa.left, wa.top), QPoint(wa.right - 1, wa.bottom - 1)));
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&areas));
        for (const QRect &wa : areas) {
            if (std::abs(vis.left() - wa.left()) < kSnap) x = wa.left();
            if (std::abs(vis.right() - wa.right()) < kSnap) x = wa.right() - w + 1;
            if (std::abs(vis.top() - wa.top()) < kSnap) y = wa.top();
            if (std::abs(vis.bottom() - wa.bottom()) < kSnap) y = wa.bottom() - h + 1;
        }
        for (ToolPalette *o : g_palettes) {
            if (o == this || !o->isVisible())
                continue;
            const QRect ov = visibleRect(reinterpret_cast<HWND>(o->winId()));
            const bool vOverlap = vis.top() <= ov.bottom() && vis.bottom() >= ov.top();
            const bool hOverlap = vis.left() <= ov.right() && vis.right() >= ov.left();
            if (vOverlap) {
                if (std::abs(vis.left() - (ov.right() + 1)) < kSnap) x = ov.right() + 1;
                if (std::abs(vis.right() + 1 - ov.left()) < kSnap) x = ov.left() - w;
            }
            if (hOverlap) {
                if (std::abs(vis.top() - (ov.bottom() + 1)) < kSnap) y = ov.bottom() + 1;
                if (std::abs(vis.bottom() + 1 - ov.top()) < kSnap) y = ov.top() - h;
            }
            if (std::abs(vis.left() - ov.left()) < kSnap) x = ov.left();
            if (std::abs(vis.top() - ov.top()) < kSnap) y = ov.top();
            if (std::abs(vis.right() - ov.right()) < kSnap) x = ov.right() - w + 1;
        }
        const int dx = x - vis.left();
        const int dy = y - vis.top();
        r->left += dx;
        r->right += dx;
        r->top += dy;
        r->bottom += dy;
        *result = TRUE;
        return true;
    }
    case WM_NCRBUTTONDOWN:
        if (msg->wParam == HTCAPTION) {
            *result = 0;
            return true;
        }
        break;
    case WM_NCRBUTTONUP:
        if (msg->wParam == HTCAPTION) {
            showTitleMenu(QCursor::pos());
            *result = 0;
            return true;
        }
        break;
    case WM_SYSCOMMAND:
        if ((msg->wParam & 0xFFF0) == SC_KEYMENU && msg->lParam == ' ') {
            showTitleMenu(mapToGlobal(QPoint(0, 0)));
            *result = 0;
            return true;
        }
        break;
    default:
        break;
    }
    return QWidget::nativeEvent(eventType, message, result);
}
#endif

// OP2Mapper3 ArrangeToolWindowsSmart. Palettes with a usable saved position
// keep it. The rest go on the largest other monitor if there is one. On a
// work area at least 1700 px wide they take a fixed cluster at the top-right
// when it doesn't overlap anything; everything else is packed with
// findFreeSpot, tall windows first. New positions are saved.
void ToolPalette::arrange(QWidget *mainWindow, const QList<ToolPalette *> &list, bool onlyUnplaced)
{
    QList<ToolPalette *> kept;
    QList<ToolPalette *> todo;
    for (ToolPalette *p : list) {
        if (onlyUnplaced && p->restorePosition())
            kept << p;
        else
            todo << p;
    }
    if (todo.isEmpty())
        return;

    // Screen: the largest other monitor if there are several.
    QScreen *mainScreen = mainWindow->screen();
    QScreen *target = mainScreen;
    const auto screens = QGuiApplication::screens();
    if (screens.size() > 1) {
        qint64 bestArea = -1;
        for (QScreen *s : screens) {
            if (s == mainScreen)
                continue;
            const QRect a = s->availableGeometry();
            const qint64 area = qint64(a.width()) * a.height();
            if (area > bestArea) {
                bestArea = area;
                target = s;
            }
        }
    }
    const QRect wa = target->availableGeometry();

    auto byKey = [&](const QString &k) -> ToolPalette * {
        for (ToolPalette *p : list)
            if (p->key() == k)
                return p;
        return nullptr;
    };
    ToolPalette *ct = byKey(QStringLiteral("CellTypes"));
    ToolPalette *ts = byKey(QStringLiteral("Tileset"));
    ToolPalette *sel = byKey(QStringLiteral("TileSelect"));
    ToolPalette *mm = byKey(QStringLiteral("MiniMap"));
    ToolPalette *tg = byKey(QStringLiteral("TileGroups"));
    ToolPalette *te = byKey(QStringLiteral("Terrain"));

    QList<QRect> placed;
    for (ToolPalette *p : kept)
        placed << p->frameGeometry();

    QHash<ToolPalette *, QRect> fixed;
    if (wa.width() >= 1700) {
        const int R = wa.right() + 1, Tp = wa.top();
        const int ctW = ct ? ct->frameGeometry().width() : 217;
        const int tsW = ts ? ts->frameGeometry().width() : 248;
        if (ct) fixed[ct] = QRect(QPoint(R - ctW, Tp), ct->frameGeometry().size());
        if (ts) fixed[ts] = QRect(QPoint(R - ctW - tsW, Tp), ts->frameGeometry().size());
        if (sel) fixed[sel] = QRect(QPoint(R - ctW - tsW - sel->frameGeometry().width(), Tp), sel->frameGeometry().size());
        if (mm) {
            const QPoint p = sel ? QPoint(fixed[sel].left(), Tp + sel->frameGeometry().height())
                                 : QPoint(R - mm->frameGeometry().width() - 500, Tp + 245);
            fixed[mm] = QRect(p, mm->frameGeometry().size());
        }
        if (tg) {
            const int y = ts ? Tp + ts->frameGeometry().height() : Tp + 445;
            fixed[tg] = QRect(QPoint(R - tg->frameGeometry().width(), y), tg->frameGeometry().size());
        }
    }

    QList<ToolPalette *> done;
    for (ToolPalette *p : { ct, ts, sel, mm, tg }) {
        if (!p || !todo.contains(p) || !fixed.contains(p))
            continue;
        const QRect r = fixed[p];
        bool ok = wa.contains(r);
        for (const QRect &q : placed)
            if (overlapArea(r, q) > 0)
                ok = false;
        if (ok) {
            p->move(r.topLeft());
            placed << r;
            done << p;
        }
    }
    for (ToolPalette *p : { ct, ts, te, sel, mm, tg }) {
        if (!p || !todo.contains(p) || done.contains(p))
            continue;
        const QRect r = findFreeSpot(wa, p->frameGeometry().size(), placed);
        p->move(r.topLeft());
        placed << r;
        done << p;
    }
    for (ToolPalette *p : todo)
        p->savePosition();
}
