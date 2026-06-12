#include "TileGroupsWindow.h"

#include "Settings.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

namespace {
constexpr const char *kGeometryKey = "TileGroupsWindow/geometry";
constexpr const char *kDefaultFolderLabel = "(Default)";
constexpr int kFolderPathRole = Qt::UserRole + 1;   // absolute path
constexpr int kJsonPathRole = Qt::UserRole + 1;     // absolute path
}

TileGroupsWindow::TileGroupsWindow(QWidget *parent)
    : QWidget(parent, Qt::Tool)
{
    setWindowTitle(tr("Tile Groups"));

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // -- Left column: folder/group combos + preview + path label
    auto *left = new QVBoxLayout;
    {
        auto *folderLabel = new QLabel(tr("Folder:"));
        m_folderCombo = new QComboBox;
        m_folderCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        left->addWidget(folderLabel);
        left->addWidget(m_folderCombo);

        auto *groupLabel = new QLabel(tr("Tile Group:"));
        m_groupCombo = new QComboBox;
        m_groupCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        left->addWidget(groupLabel);
        left->addWidget(m_groupCombo);

        auto *scroll = new QScrollArea;
        m_previewLabel = new QLabel;
        m_previewLabel->setAlignment(Qt::AlignCenter);
        m_previewLabel->setStyleSheet(
            QStringLiteral("background-color: #404040;"));
        m_previewLabel->setMinimumSize(220, 200);
        scroll->setWidget(m_previewLabel);
        scroll->setWidgetResizable(true);
        left->addWidget(scroll, 1);

        m_pathLabel = new QLabel;
        m_pathLabel->setStyleSheet(QStringLiteral("color: gray;"));
        m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_pathLabel->setWordWrap(true);
        left->addWidget(m_pathLabel);
    }
    root->addLayout(left, 2);

    // -- Right column: details + place button
    auto *right = new QVBoxLayout;
    {
        auto *form = new QFormLayout;
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto makeValue = []() {
            auto *l = new QLabel;
            l->setStyleSheet(QStringLiteral("color: #4040a0;"));
            l->setTextInteractionFlags(Qt::TextSelectableByMouse);
            return l;
        };
        m_countValue   = makeValue();
        m_tilesetValue = makeValue();
        m_nameValue    = makeValue();
        m_widthValue   = makeValue();
        m_heightValue  = makeValue();
        m_notesValue   = makeValue();
        m_notesValue->setWordWrap(true);
        form->addRow(tr("Tile Groups:"), m_countValue);
        form->addRow(tr("TileSet:"),     m_tilesetValue);
        form->addRow(tr("Name:"),        m_nameValue);
        form->addRow(tr("Width:"),       m_widthValue);
        form->addRow(tr("Height:"),      m_heightValue);
        form->addRow(tr("Notes:"),       m_notesValue);
        right->addLayout(form);

        right->addStretch(1);

        m_placeButton = new QPushButton(tr("Place Tile Group"));
        m_placeButton->setEnabled(false);
        right->addWidget(m_placeButton);

        auto *shortcutLabel = new QLabel(tr("Ctrl+B to Place"));
        shortcutLabel->setAlignment(Qt::AlignCenter);
        shortcutLabel->setStyleSheet(QStringLiteral("color: gray;"));
        right->addWidget(shortcutLabel);
    }
    root->addLayout(right, 1);

    resize(580, 420);
    restoreGeometry();

    connect(m_folderCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TileGroupsWindow::onFolderChanged);
    connect(m_groupCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TileGroupsWindow::onGroupChanged);
    connect(m_placeButton, &QPushButton::clicked,
            this, &TileGroupsWindow::onPlaceClicked);

    refresh();
}

void TileGroupsWindow::refresh(const QString &selectFolder)
{
    const QSignalBlocker bf(m_folderCombo);
    const QSignalBlocker bg(m_groupCombo);

    m_folderCombo->clear();
    m_groupCombo->clear();
    m_previewLabel->setPixmap(QPixmap());
    m_previewLabel->setText(QString());
    m_pathLabel->clear();

    const QString rootPath = Settings::tileGroupsPath();
    QDir rootDir(rootPath);
    if (!rootDir.exists()) {
        m_pathLabel->setText(tr("Folder not found: %1\n"
            "Use File → Settings to set the Tilegroups folder.")
            .arg(rootPath));
        m_countValue->setText(QStringLiteral("0"));
        m_placeButton->setEnabled(false);
        return;
    }

    // Categories: each subfolder + (Default) for loose root JSONs.
    const QStringList subs = rootDir.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    int selectIdx = -1;
    for (const QString &s : subs) {
        m_folderCombo->addItem(s);
        m_folderCombo->setItemData(m_folderCombo->count() - 1,
                                   rootDir.filePath(s), kFolderPathRole);
        if (!selectFolder.isEmpty()
            && s.compare(selectFolder, Qt::CaseInsensitive) == 0) {
            selectIdx = m_folderCombo->count() - 1;
        }
    }
    const QStringList rootJsons = rootDir.entryList(
        QStringList{ QStringLiteral("*.json") }, QDir::Files);
    if (!rootJsons.isEmpty()) {
        m_folderCombo->addItem(tr(kDefaultFolderLabel));
        m_folderCombo->setItemData(m_folderCombo->count() - 1,
                                   rootPath, kFolderPathRole);
    }

    if (m_folderCombo->count() == 0) {
        m_pathLabel->setText(
            tr("No tile groups in %1.\n"
               "Use the Tile Selection window to save your first group.")
                .arg(rootPath));
        m_countValue->setText(QStringLiteral("0"));
        m_placeButton->setEnabled(false);
        return;
    }

    m_folderCombo->setCurrentIndex(selectIdx >= 0 ? selectIdx : 0);
    populateGroupsForCurrentFolder();
    updateDetailsAndPreview();
}

QString TileGroupsWindow::folderPath(int folderRow) const
{
    if (folderRow < 0 || folderRow >= m_folderCombo->count())
        return QString();
    return m_folderCombo->itemData(folderRow, kFolderPathRole).toString();
}

QString TileGroupsWindow::currentJsonPath() const
{
    const int row = m_groupCombo->currentIndex();
    if (row < 0)
        return QString();
    return m_groupCombo->itemData(row, kJsonPathRole).toString();
}

void TileGroupsWindow::populateGroupsForCurrentFolder()
{
    const QSignalBlocker bg(m_groupCombo);
    m_groupCombo->clear();

    const QString fp = folderPath(m_folderCombo->currentIndex());
    if (fp.isEmpty())
        return;

    QDir d(fp);
    const QStringList jsons = d.entryList(QStringList{ QStringLiteral("*.json") },
                                          QDir::Files, QDir::Name);
    for (const QString &fn : jsons) {
        m_groupCombo->addItem(QFileInfo(fn).completeBaseName());
        m_groupCombo->setItemData(m_groupCombo->count() - 1,
                                  d.filePath(fn), kJsonPathRole);
    }
    m_countValue->setText(QString::number(m_groupCombo->count()));
    if (m_groupCombo->count() > 0)
        m_groupCombo->setCurrentIndex(0);
}

void TileGroupsWindow::onFolderChanged(int)
{
    populateGroupsForCurrentFolder();
    updateDetailsAndPreview();
}

void TileGroupsWindow::onGroupChanged(int)
{
    updateDetailsAndPreview();
}

void TileGroupsWindow::updateDetailsAndPreview()
{
    const QString jsonPath = currentJsonPath();
    if (jsonPath.isEmpty()) {
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(tr("(no group)"));
        m_pathLabel->clear();
        m_tilesetValue->clear();
        m_nameValue->clear();
        m_widthValue->clear();
        m_heightValue->clear();
        m_notesValue->clear();
        m_placeButton->setEnabled(false);
        return;
    }

    // Show preview from accompanying .bmp.
    const QFileInfo info(jsonPath);
    const QString bmpPath = info.dir()
        .filePath(info.completeBaseName() + QStringLiteral(".bmp"));
    QPixmap pm(bmpPath);
    if (pm.isNull())
        m_previewLabel->setText(tr("(no preview)"));
    else
        m_previewLabel->setPixmap(pm);

    m_pathLabel->setText(jsonPath);

    // Parse header for details.
    QFile f(jsonPath);
    QJsonObject header;
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = f.readAll();
        f.close();
        const QJsonDocument doc = QJsonDocument::fromJson(bytes);
        if (doc.isObject())
            header = doc.object().value(QStringLiteral("header")).toObject();
    }
    m_tilesetValue->setText(header.value(QStringLiteral("tileset")).toString());
    m_nameValue->setText(header.value(QStringLiteral("name")).toString());
    m_widthValue->setText(QString::number(header.value(QStringLiteral("width")).toInt()));
    m_heightValue->setText(QString::number(header.value(QStringLiteral("height")).toInt()));
    m_notesValue->setText(header.value(QStringLiteral("notes")).toString());

    m_placeButton->setEnabled(true);
}

void TileGroupsWindow::onPlaceClicked()
{
    const QString p = currentJsonPath();
    if (!p.isEmpty())
        emit placeRequested(p);
}

void TileGroupsWindow::keyPressEvent(QKeyEvent *event)
{
    // Ctrl+B → place current group, matching VB shortcut.
    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_B) {
        if (m_placeButton->isEnabled())
            onPlaceClicked();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void TileGroupsWindow::closeEvent(QCloseEvent *event)
{
    saveGeometry();
    QWidget::closeEvent(event);
}

void TileGroupsWindow::saveGeometry()
{
    QSettings s;
    s.setValue(QLatin1String(kGeometryKey), QWidget::saveGeometry());
}

void TileGroupsWindow::restoreGeometry()
{
    QSettings s;
    const QByteArray g = s.value(QLatin1String(kGeometryKey)).toByteArray();
    if (!g.isEmpty())
        QWidget::restoreGeometry(g);
}
