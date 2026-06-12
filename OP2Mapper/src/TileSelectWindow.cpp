#include "TileSelectWindow.h"

#include "MapDocument.h"
#include "Settings.h"
#include "TileClipboard.h"
#include "TileGroupIO.h"
#include "TileImageCache.h"

#include <QCloseEvent>
#include <QDir>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

namespace {
constexpr const char *kGeometryKey = "TileSelectWindow/geometry";
}

TileSelectWindow::TileSelectWindow(QWidget *parent)
    : QWidget(parent, Qt::Tool)
{
    setWindowTitle(tr("Tile Selection"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    m_saveButton = new QPushButton(tr("Save as Tile Group..."), this);
    m_saveButton->setEnabled(false);
    layout->addWidget(m_saveButton);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(false);
    m_previewLabel = new QLabel;
    m_previewLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_previewLabel->setStyleSheet(QStringLiteral("background-color: #404040;"));
    scroll->setWidget(m_previewLabel);
    layout->addWidget(scroll, 1);

    m_statusLabel = new QLabel(tr("No tiles copied."), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(m_statusLabel);

    resize(220, 320);
    restoreGeometry();

    connect(m_saveButton, &QPushButton::clicked,
            this, &TileSelectWindow::onSaveClicked);
}

void TileSelectWindow::setClipboard(const TileClipboard *clip)
{
    m_clipboard = clip;
    refresh();
}

void TileSelectWindow::setDocument(MapDocument *doc)
{
    m_doc = doc;
    refresh();
}

void TileSelectWindow::refresh()
{
    if (!m_clipboard || m_clipboard->isEmpty() || !m_doc || !m_doc->cache()) {
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(QString());
        m_previewLabel->resize(QSize(0, 0));
        m_statusLabel->setText(tr("No tiles copied."));
        m_saveButton->setEnabled(false);
        return;
    }

    constexpr int T = TileImageCache::TileSize;
    QImage img(m_clipboard->width() * T, m_clipboard->height() * T,
               QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    QPainter p(&img);
    for (int y = 0; y < m_clipboard->height(); ++y) {
        for (int x = 0; x < m_clipboard->width(); ++x) {
            const auto &c = m_clipboard->at(x, y);
            if (c.tilesetIdx < 0 || c.imageIdx < 0)
                continue;
            const QImage *strip = m_doc->cache()->tileset(
                static_cast<std::size_t>(c.tilesetIdx));
            if (!strip)
                continue;
            const int srcY = c.imageIdx * T;
            if (srcY + T > strip->height())
                continue;
            p.drawImage(QPoint(x * T, y * T), *strip, QRect(0, srcY, T, T));
        }
    }
    p.end();

    const QPixmap pm = QPixmap::fromImage(img);
    m_previewLabel->setPixmap(pm);
    m_previewLabel->resize(pm.size());
    m_statusLabel->setText(tr("%1 × %2 tiles")
                               .arg(m_clipboard->width())
                               .arg(m_clipboard->height()));
    m_saveButton->setEnabled(true);
}

void TileSelectWindow::onSaveClicked()
{
    if (!m_clipboard || m_clipboard->isEmpty() || !m_doc)
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(this,
        tr("Save Tile Group"),
        tr("Name:"),
        QLineEdit::Normal,
        QString(),
        &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    // Sanitize: drop characters illegal in Windows filenames.
    QString safe = name.trimmed();
    for (QChar &c : safe) {
        if (QString::fromLatin1("<>:\"/\\|?*").contains(c))
            c = QLatin1Char('_');
    }

    const QString exportDir = QDir(Settings::tileGroupsPath())
                                  .filePath(QStringLiteral("Exported"));

    QString err;
    if (!TileGroupIO::writeExport(*m_clipboard, *m_doc, exportDir, safe, err)) {
        QMessageBox::warning(this, tr("Save Tile Group"),
            tr("Failed to save tile group:\n%1").arg(err));
        return;
    }
    m_statusLabel->setText(tr("Saved: %1").arg(safe));
    emit tileGroupSaved(safe);
}

void TileSelectWindow::closeEvent(QCloseEvent *event)
{
    saveGeometry();
    QWidget::closeEvent(event);
}

void TileSelectWindow::saveGeometry()
{
    QSettings s;
    s.setValue(QLatin1String(kGeometryKey), QWidget::saveGeometry());
}

void TileSelectWindow::restoreGeometry()
{
    QSettings s;
    const QByteArray g = s.value(QLatin1String(kGeometryKey)).toByteArray();
    if (!g.isEmpty())
        QWidget::restoreGeometry(g);
}
