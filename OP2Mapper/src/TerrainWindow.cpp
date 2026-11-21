#include "TerrainWindow.h"

#include "Editor.h"
#include "Op2Lib.h"

#include <QComboBox>
#include <algorithm>
#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>

namespace {

QIcon swatch(const QColor &c)
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.fillRect(1, 1, 12, 12, c);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 13, 13);
    return QIcon(pm);
}

// Odd sizes only: an even value is rounded up to the next odd one.
class OddSpinBox : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;
protected:
    void fixup(QString &input) const override
    {
        bool ok = false;
        int v = input.toInt(&ok);
        if (!ok)
            return;
        v = std::clamp(v, minimum(), maximum());
        if (v % 2 == 0)
            v = std::min(maximum(), v + 1);
        input = QString::number(v);
    }
    QValidator::State validate(QString &input, int &pos) const override
    {
        const QValidator::State s = QSpinBox::validate(input, pos);
        if (s != QValidator::Acceptable)
            return s;
        return input.toInt() % 2 == 1 ? QValidator::Acceptable : QValidator::Intermediate;
    }
};

} // namespace

TerrainWindow::TerrainWindow(Editor *editor, QWidget *mainWindow)
    : ToolPalette(QStringLiteral("Terrain"), QStringLiteral("fTerrain"), mainWindow)
    , m_editor(editor)
{
    setWindowTitle(tr("Terrain Brush"));
    constexpr int W = 264;
    constexpr int M = 8;
    const int inner = W - 2 * M;
    int y = M;

    const QString help = tr("Pick a ground, then left-drag on the map. Borders blend with the game's "
                            "transition tiles. Ctrl + mouse wheel changes the size. Clear Tool stops the brush.")
        + QLatin1Char('\n')
        + tr("Draw Cliff: click points on the map (they snap to cliff slopes); Enter places the cliff. "
             "Backspace removes the last point, Esc clears them. Click the first point again for a closed cliff.");

    if (!Op2Lib::profile()) {
        auto *err = new QLabel(tr("The terrain brush is unavailable: %1").arg(Op2Lib::profileError()), this);
        err->setStyleSheet(QStringLiteral("color: #8B0000;"));
        err->setWordWrap(true);
        err->setGeometry(M, y, inner, 80);
        y += 86;
    } else {
        const auto grounds = Op2Lib::paintableGrounds();
        const int bw = (inner - 8) / 2;
        for (int i = 0; i < grounds.size(); ++i) {
            const auto &g = grounds[i];
            auto *b = new QPushButton(swatch(g.color), g.name, this);
            b->setCheckable(true);
            b->setGeometry(M + (i % 2) * (bw + 8), y + (i / 2) * 34, bw, 30);
            b->setProperty("ground", g.index);
            connect(b, &QPushButton::toggled, this, [this, g](bool on) {
                if (m_syncing)
                    return;
                if (on)
                    m_editor->armTerrain(g.index, g.name, g.color);
                else if (m_editor->terrainGround() == g.index && m_editor->terrainToolActive())
                    m_editor->disarmTerrain();
                syncFromState();
            });
            m_groundButtons << b;
        }
        y += ((grounds.size() + 1) / 2) * 34 + 4;

        auto *sizeLabel = new QLabel(tr("Brush size (tiles):"), this);
        sizeLabel->setGeometry(M, y + 2, inner - 60, 20);
        m_size = new OddSpinBox(this);
        m_size->setRange(3, 25);
        m_size->setSingleStep(2);
        m_size->setAlignment(Qt::AlignRight);
        m_size->setGeometry(W - M - 56, y, 56, 22);
        m_size->setValue(m_editor->brushSize());
        connect(m_size, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
            if (!m_syncing)
                m_editor->setBrushSize(v);
        });
        y += 30;

        auto *clean = new QPushButton(tr("Clean Up Transitions..."), this);
        clean->setGeometry(M, y, inner, 28);
        connect(clean, &QPushButton::clicked, m_editor, &Editor::cleanUpTransitions);
        y += 36;

        auto *line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        line->setGeometry(M, y, inner, 2);
        y += 10;

        m_cliffButton = new QPushButton(tr("Draw Cliff"), this);
        m_cliffButton->setCheckable(true);
        m_cliffButton->setGeometry(M, y, inner, 30);
        connect(m_cliffButton, &QPushButton::toggled, this, [this](bool on) {
            if (m_syncing)
                return;
            if (on)
                m_editor->armCliff();
            else
                m_editor->disarmCliff();
            syncFromState();
        });
        y += 38;

        auto *sideLabel = new QLabel(tr("High side:"), this);
        sideLabel->setGeometry(M, y + 2, inner - 134, 20);
        m_side = new QComboBox(this);
        m_side->addItems({ tr("Left of the line"), tr("Right of the line"), tr("North"), tr("South") });
        m_side->setGeometry(W - M - 130, y, 130, 22);
        m_side->setCurrentIndex(m_editor->cliffSide());
        y += 30;

        auto *heightLabel = new QLabel(tr("Wall height (tiles):"), this);
        heightLabel->setGeometry(M, y + 2, inner - 60, 20);
        m_height = new QSpinBox(this);
        m_height->setRange(3, 6);
        m_height->setAlignment(Qt::AlignRight);
        m_height->setGeometry(W - M - 56, y, 56, 22);
        m_height->setValue(m_editor->cliffHeight());
        y += 30;

        auto applyCliff = [this] {
            if (!m_syncing)
                m_editor->setCliffOptions(m_side->currentIndex(), m_height->value());
        };
        connect(m_side, qOverload<int>(&QComboBox::currentIndexChanged), this, applyCliff);
        connect(m_height, qOverload<int>(&QSpinBox::valueChanged), this, applyCliff);

        auto *place = new QPushButton(tr("Place Cliff (Enter)"), this);
        place->setGeometry(M, y, inner, 28);
        connect(place, &QPushButton::clicked, m_editor, &Editor::placeCliff);
        y += 36;
    }

    auto *helpLabel = new QLabel(help, this);
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet(QStringLiteral("color: #6D6D6D;"));
    helpLabel->setFixedWidth(inner);
    const int hh = helpLabel->heightForWidth(inner);
    helpLabel->setGeometry(M, y, inner, hh);
    y += hh + M;

    setFixedSize(W, y);
    connect(m_editor, &Editor::terrainStateChanged, this, &TerrainWindow::syncFromState);
    syncFromState();
}

void TerrainWindow::syncFromState()
{
    m_syncing = true;
    const bool brush = m_editor->terrainToolActive();
    for (QPushButton *b : m_groundButtons)
        b->setChecked(brush && b->property("ground").toInt() == m_editor->terrainGround());
    if (m_cliffButton)
        m_cliffButton->setChecked(m_editor->cliffToolActive());
    if (m_size && m_size->value() != m_editor->brushSize())
        m_size->setValue(m_editor->brushSize());
    m_syncing = false;
}
