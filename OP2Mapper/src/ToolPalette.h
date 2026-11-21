#pragma once

#include <QList>
#include <QString>
#include <QTimer>
#include <QWidget>

class QMenu;

// Base for the six tool windows (OP2Mapper3 ToolPalette.vb): fixed-size
// tool windows without a close button, opened and closed with edit mode.
//   - position saved in Mapper3.ini [Window] <Key>Left/Top (+Width/Height)
//     and restored only if the title-bar strip lands on some screen
//   - snapping to screen edges and other palettes while moving (Windows;
//     hold Ctrl to move freely)
//   - title-bar menu (right-click the caption or Alt+Space):
//     "Always On Top (This Window)", "All Tool Windows Always On Top",
//     plus per-palette extras
class ToolPalette : public QWidget
{
    Q_OBJECT
public:
    // key: INI position key ("Tileset", "CellTypes", ...)
    // className: VB form class for AlwaysOnTop_<class> ("fTileSet", ...)
    ToolPalette(const QString &key, const QString &className, QWidget *mainWindow);
    ~ToolPalette() override;

    QString key() const { return m_key; }

    // True if the INI has a usable position; moves there.
    bool restorePosition();
    void savePosition();

    bool alwaysOnTop() const { return m_alwaysOnTop; }
    void setAlwaysOnTop(bool on, bool persist);

    // Shows the window (keeping the stay-on-top flag).
    void open();

    static QList<ToolPalette *> palettes();
    static bool allToolWindowsAlwaysOnTop();
    static void setAllAlwaysOnTop(bool on);

    // OP2Mapper3 ArrangeToolWindowsSmart: palettes without a usable saved
    // position get the default layout (another monitor if there is one;
    // fixed cluster on screens >= 1700 px wide, else right-edge packing).
    static void arrange(QWidget *mainWindow, const QList<ToolPalette *> &list, bool onlyUnplaced);

    // Emitted when the global "all on top" choice changes from a title menu.
    static ToolPalette *notifier();

signals:
    void allAlwaysOnTopChanged(bool on);

protected:
    // Extra entries for the title-bar menu (after a separator).
    virtual void addMenuExtras(QMenu *menu) { Q_UNUSED(menu); }
    void showTitleMenu(const QPoint &globalPos);

    void moveEvent(QMoveEvent *event) override;
    void hideEvent(QHideEvent *event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private:
    QString m_key;
    QString m_className;
    bool m_alwaysOnTop = true;
    QTimer m_saveTimer;
};
