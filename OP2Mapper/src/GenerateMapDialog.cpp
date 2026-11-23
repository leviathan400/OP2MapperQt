// GenerateMapDialog: File > Generate New Map, a port of OP2Mapper3's
// fGenerateMap. Laid out in code; texts and Mapper3.ini keys match the VB form.

#include "GenerateMapDialog.h"

#include "AppLog.h"
#include "IniFile.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSlider>
#include <QSpinBox>
#include <QThread>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

const QString kSection = QStringLiteral("GenerateMap");

// Sizes with a blank map template.
const QVector<QSize> kSizes = {
    { 64, 64 }, { 64, 128 }, { 64, 256 }, { 128, 64 }, { 128, 128 },
    { 128, 256 }, { 256, 64 }, { 256, 128 }, { 256, 256 }, { 512, 256 },
};

// Generator templates: display name and library name.
const QVector<QPair<QString, QString>> kTemplates = {
    { QStringLiteral("Plains"), QStringLiteral("plains") },
    { QStringLiteral("Canyon"), QStringLiteral("canyon") },
    { QStringLiteral("Desert"), QStringLiteral("desert") },
    { QStringLiteral("Islands"), QStringLiteral("islands") },
    { QStringLiteral("Ice"), QStringLiteral("ice") },
    { QStringLiteral("New Terra"), QStringLiteral("newterra") },
};

// Each template's terrain mix as set by op2maplib's applyTemplate
// (src/mapgen/field.cpp). The library has no getter for the mix, so it is
// mirrored here to show it on the sliders (as OP2Mapper3 does).
double templateMix(const QString &t, const QString &ground)
{
    struct Entry { const char *t; const char *g; double share; };
    static const Entry table[] = {
        { "plains", "mud", 0.45 }, { "plains", "roc", 0.25 }, { "plains", "san", 0.15 }, { "plains", "veg", 0.15 },
        { "canyon", "roc", 0.55 }, { "canyon", "mud", 0.25 }, { "canyon", "san", 0.10 }, { "canyon", "veg", 0.10 },
        { "desert", "san", 0.60 }, { "desert", "mud", 0.30 }, { "desert", "roc", 0.08 }, { "desert", "veg", 0.02 },
        { "islands", "mud", 0.35 }, { "islands", "roc", 0.35 }, { "islands", "san", 0.20 }, { "islands", "veg", 0.10 },
        { "ice", "sno", 0.45 }, { "ice", "roc", 0.35 }, { "ice", "mud", 0.20 },
        { "newterra", "mud", 0.68 }, { "newterra", "san", 0.20 }, { "newterra", "roc", 0.12 },
    };
    for (const Entry &e : table)
        if (t == QLatin1String(e.t) && ground == QLatin1String(e.g))
            return e.share;
    return 0.0;
}

// Terrain mix sliders: label and profile ground id.
const QVector<QPair<QString, QString>> kMixGrounds = {
    { QStringLiteral("Mud"), QStringLiteral("mud") },
    { QStringLiteral("Rock"), QStringLiteral("roc") },
    { QStringLiteral("Sand"), QStringLiteral("san") },
    { QStringLiteral("Vegetation"), QStringLiteral("veg") },
    { QStringLiteral("Snow"), QStringLiteral("sno") },
};

// Feature positions: display name and the library's sector code (German
// compass: o = east).
const QVector<QPair<QString, QString>> kSectors = {
    { QStringLiteral("Random"), QString() },
    { QStringLiteral("North-west"), QStringLiteral("nw") },
    { QStringLiteral("North"), QStringLiteral("n") },
    { QStringLiteral("North-east"), QStringLiteral("no") },
    { QStringLiteral("West"), QStringLiteral("w") },
    { QStringLiteral("Centre"), QStringLiteral("c") },
    { QStringLiteral("East"), QStringLiteral("o") },
    { QStringLiteral("South-west"), QStringLiteral("sw") },
    { QStringLiteral("South"), QStringLiteral("s") },
    { QStringLiteral("South-east"), QStringLiteral("so") },
};

const char *kFeatureNames[] = { "Valley", "Mountain", "Plateau", "Clearing", "Choke" };

int readInt(const QString &key, int def) { return IniFile::intValue(kSection, key, def); }

} // namespace

GenerateMapDialog::GenerateMapDialog(Host host, QWidget *parent)
    : QDialog(parent), m_host(std::move(host))
{
    setWindowTitle(tr("Generate New Map"));
    setWindowIcon(QIcon(QStringLiteral(":/images/IconLarge.ico")));
    setWindowFlags((windowFlags() | Qt::WindowMinimizeButtonHint) & ~Qt::WindowContextHelpButtonHint);
    setModal(false);
    buildLayout();
    loadSettings();
}

GenerateMapDialog::~GenerateMapDialog()
{
    if (m_worker) {
        m_worker->wait();
        delete m_worker;
    }
}

QString GenerateMapDialog::templateName() const
{
    return kTemplates[std::max(0, m_template->currentIndex())].second;
}

// ---- Layout ------------------------------------------------------------------

QSlider *GenerateMapDialog::addSlider(QGridLayout *grid, int &row, const QString &label, int min, int max,
                                      std::function<QString(int)> format)
{
    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(min, max);
    slider->setPageStep(5);
    auto *value = new QLabel;
    value->setMinimumWidth(54);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    connect(slider, &QSlider::valueChanged, value, [value, format](int v) { value->setText(format(v)); });
    value->setText(format(slider->value()));
    grid->addWidget(new QLabel(label), row, 0);
    grid->addWidget(slider, row, 1);
    grid->addWidget(value, row, 2);
    m_inputs << slider;
    ++row;
    return slider;
}

void GenerateMapDialog::buildLayout()
{
    auto *outer = new QVBoxLayout(this);
    auto *grid = new QGridLayout;
    grid->setColumnMinimumWidth(0, 120);
    grid->setColumnMinimumWidth(1, 200);
    grid->setVerticalSpacing(4);
    int row = 0;
    auto heading = [&](const QString &text) {
        auto *l = new QLabel(text);
        QFont f = l->font();
        f.setBold(true);
        l->setFont(f);
        l->setStyleSheet(QStringLiteral("color: #0066CC;"));
        grid->addWidget(l, row++, 0, 1, 3);
    };
    auto field = [&](const QString &label, QWidget *w) {
        grid->addWidget(new QLabel(label), row, 0);
        grid->addWidget(w, row++, 1, 1, 2);
        m_inputs << w;
    };
    const auto percent = [](int v) { return QStringLiteral("%1 %").arg(v); };

    m_size = new QComboBox;
    for (const QSize &s : kSizes)
        m_size->addItem(QStringLiteral("%1 x %2").arg(s.width()).arg(s.height()));
    field(tr("Size:"), m_size);

    m_template = new QComboBox;
    for (const auto &t : kTemplates)
        m_template->addItem(t.first);
    field(tr("Template:"), m_template);
    connect(m_template, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        if (!m_loading)
            applyTemplateDefaults();
    });

    auto *seedRow = new QWidget;
    auto *seedLayout = new QHBoxLayout(seedRow);
    seedLayout->setContentsMargins(0, 0, 0, 0);
    m_seed = new QLineEdit;
    m_roll = new QPushButton(tr("Roll"));
    m_roll->setFixedWidth(64);
    seedLayout->addWidget(m_seed, 1);
    seedLayout->addWidget(m_roll);
    field(tr("Seed:"), seedRow);
    connect(m_roll, &QPushButton::clicked, this, &GenerateMapDialog::roll);

    heading(tr("Terrain mix"));
    for (const auto &g : kMixGrounds)
        m_mix << addSlider(grid, row, g.first + QLatin1Char(':'), 0, 100, percent);
    m_roughness = addSlider(grid, row, tr("Roughness:"), 0, 100, percent);
    m_minBiome = addSlider(grid, row, tr("Min. biome size:"), 1, 30,
                           [](int v) { return QLocale().toString(v / 10.0, 'f', 1) + QStringLiteral(" %"); });

    heading(tr("Layout"));
    m_symmetry = new QComboBox;
    m_symmetry->addItems({ tr("None"), tr("Mirror left / right"), tr("Mirror top / bottom"),
                           tr("Rotated 180°"), tr("Four-way (4 players)") });
    field(tr("Symmetry:"), m_symmetry);
    m_cliffs = addSlider(grid, row, tr("Cliffs:"), 0, 50, percent);
    m_details = addSlider(grid, row, tr("Rocks and craters:"), 0, 100, percent);
    m_spawns = new QSpinBox;
    m_spawns->setRange(0, 8);
    m_spawns->setFixedWidth(60);
    grid->addWidget(new QLabel(tr("Spawn points:")), row, 0);
    grid->addWidget(m_spawns, row++, 1, Qt::AlignLeft);
    m_inputs << m_spawns;
    m_volcanoes = new QSpinBox;
    m_volcanoes->setRange(0, 8);
    m_volcanoes->setFixedWidth(60);
    grid->addWidget(new QLabel(tr("Volcanoes:")), row, 0);
    grid->addWidget(m_volcanoes, row++, 1, Qt::AlignLeft);
    m_inputs << m_volcanoes;

    // Features: buttons to add each kind, the list, and the selected one's settings.
    heading(tr("Features"));
    auto *buttons = new QWidget;
    auto *bl = new QHBoxLayout(buttons);
    bl->setContentsMargins(0, 0, 0, 0);
    for (int k = 0; k < 5; ++k) {
        auto *b = new QPushButton(tr(kFeatureNames[k]));
        connect(b, &QPushButton::clicked, this, [this, k] { addFeature(k); });
        bl->addWidget(b);
        m_inputs << b;
    }
    grid->addWidget(buttons, row++, 0, 1, 3);
    m_features = new QListWidget;
    m_features->setFixedHeight(70);
    grid->addWidget(m_features, row++, 0, 1, 3);
    m_inputs << m_features;
    connect(m_features, &QListWidget::currentRowChanged, this, [this] { updateFeatureEditor(); });

    m_where = new QComboBox;
    m_to = new QComboBox;
    for (int i = 0; i < kSectors.size(); ++i) {
        m_where->addItem(kSectors[i].first);
        m_to->addItem(i == 0 ? tr("Opposite") : kSectors[i].first);
    }
    field(tr("Where:"), m_where);
    field(tr("To:"), m_to);
    m_featureSize = addSlider(grid, row, tr("Size:"), 0, 100, percent);
    auto *lavaRow = new QWidget;
    auto *ll = new QHBoxLayout(lavaRow);
    ll->setContentsMargins(0, 0, 0, 0);
    m_lava = new QCheckBox(tr("Lava floor"));
    m_removeFeature = new QPushButton(tr("Remove Feature"));
    ll->addWidget(m_lava);
    ll->addStretch();
    ll->addWidget(m_removeFeature);
    grid->addWidget(lavaRow, row++, 1, 1, 2);
    m_inputs << m_lava << m_removeFeature;
    connect(m_where, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { featureSettingChanged(); });
    connect(m_to, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { featureSettingChanged(); });
    connect(m_featureSize, &QSlider::valueChanged, this, [this] { featureSettingChanged(); });
    connect(m_lava, &QCheckBox::toggled, this, [this] { featureSettingChanged(); });
    connect(m_removeFeature, &QPushButton::clicked, this, &GenerateMapDialog::removeFeature);

    outer->addLayout(grid);

    // Result of the last generate, then the buttons.
    m_status = new QLabel(tr("The map is built straight into the main view as a new, unsaved map."));
    m_status->setWordWrap(true);
    m_status->setMinimumHeight(40);
    m_status->setStyleSheet(QStringLiteral("color: #6D6D6D;"));
    outer->addWidget(m_status);
    auto *bottom = new QHBoxLayout;
    bottom->addStretch();
    m_generate = new QPushButton(tr("Generate"));
    m_close = new QPushButton(tr("Close"));
    m_generate->setDefault(true);
    bottom->addWidget(m_generate);
    bottom->addWidget(m_close);
    outer->addLayout(bottom);
    m_inputs << m_size << m_template << m_seed << m_roll << m_generate;
    connect(m_generate, &QPushButton::clicked, this, &GenerateMapDialog::generate);
    connect(m_close, &QPushButton::clicked, this, &QDialog::close);

    layout()->setSizeConstraint(QLayout::SetFixedSize);
    updateFeatureEditor();
}

// ---- Settings ----------------------------------------------------------------

// A template was picked: set the mix, roughness and volcanoes to its values.
// Everything else is left as it is.
void GenerateMapDialog::applyTemplateDefaults()
{
    const QString t = templateName();
    for (int i = 0; i < m_mix.size(); ++i)
        m_mix[i]->setValue(static_cast<int>(std::lround(templateMix(t, kMixGrounds[i].second) * 100)));
    m_roughness->setValue(std::clamp(static_cast<int>(std::lround(Op2Lib::mapGenTemplateDefault(t, QStringLiteral("roughness")) * 100)),
                                     m_roughness->minimum(), m_roughness->maximum()));
    m_volcanoes->setValue(std::min(m_volcanoes->maximum(),
                                   static_cast<int>(Op2Lib::mapGenTemplateDefault(t, QStringLiteral("volcanoes")))));
}

// The last settings from Mapper3.ini (defaults: 128 x 128 plains).
void GenerateMapDialog::loadSettings()
{
    m_loading = true;
    const QString size = IniFile::value(kSection, QStringLiteral("Size"), QStringLiteral("128x128"));
    int sizeIndex = 4;
    for (int i = 0; i < kSizes.size(); ++i)
        if (size == QStringLiteral("%1x%2").arg(kSizes[i].width()).arg(kSizes[i].height()))
            sizeIndex = i;
    m_size->setCurrentIndex(sizeIndex);
    const QString t = IniFile::value(kSection, QStringLiteral("Template"), QStringLiteral("plains"));
    int ti = 0;
    for (int i = 0; i < kTemplates.size(); ++i)
        if (kTemplates[i].second == t)
            ti = i;
    m_template->setCurrentIndex(ti);
    applyTemplateDefaults();
    // Saved values override the template's.
    for (int i = 0; i < m_mix.size(); ++i)
        m_mix[i]->setValue(readInt(QStringLiteral("Mix_") + kMixGrounds[i].second, m_mix[i]->value()));
    m_roughness->setValue(readInt(QStringLiteral("Roughness"), m_roughness->value()));
    m_volcanoes->setValue(readInt(QStringLiteral("Volcanoes"), m_volcanoes->value()));
    m_seed->setText(IniFile::value(kSection, QStringLiteral("Seed"), QStringLiteral("1234")));
    m_minBiome->setValue(readInt(QStringLiteral("MinBiome"), 6));
    m_symmetry->setCurrentIndex(std::clamp(readInt(QStringLiteral("Symmetry"), 0), 0, m_symmetry->count() - 1));
    m_cliffs->setValue(readInt(QStringLiteral("Cliffs"), 0));
    m_details->setValue(readInt(QStringLiteral("Details"), 50));
    m_spawns->setValue(readInt(QStringLiteral("Spawns"), 2));
    m_loading = false;
}

// The settings (not the features) to Mapper3.ini.
void GenerateMapDialog::saveSettings()
{
    const QSize s = kSizes[std::max(0, m_size->currentIndex())];
    IniFile::setValue(kSection, QStringLiteral("Size"), QStringLiteral("%1x%2").arg(s.width()).arg(s.height()));
    IniFile::setValue(kSection, QStringLiteral("Template"), templateName());
    IniFile::setValue(kSection, QStringLiteral("Seed"), m_seed->text().trimmed());
    for (int i = 0; i < m_mix.size(); ++i)
        IniFile::setInt(kSection, QStringLiteral("Mix_") + kMixGrounds[i].second, m_mix[i]->value());
    IniFile::setInt(kSection, QStringLiteral("Roughness"), m_roughness->value());
    IniFile::setInt(kSection, QStringLiteral("MinBiome"), m_minBiome->value());
    IniFile::setInt(kSection, QStringLiteral("Symmetry"), m_symmetry->currentIndex());
    IniFile::setInt(kSection, QStringLiteral("Cliffs"), m_cliffs->value());
    IniFile::setInt(kSection, QStringLiteral("Details"), m_details->value());
    IniFile::setInt(kSection, QStringLiteral("Spawns"), m_spawns->value());
    IniFile::setInt(kSection, QStringLiteral("Volcanoes"), m_volcanoes->value());
}

// ---- Features ----------------------------------------------------------------

QString GenerateMapDialog::featureText(const FeatureEntry &f) const
{
    QString text = QStringLiteral("%1 - %2").arg(tr(kFeatureNames[f.kind]), kSectors[f.where].first);
    if (f.kind == Op2Lib::MapGenFeature::Valley || f.kind == Op2Lib::MapGenFeature::Choke)
        text += tr(" to ") + (f.to == 0 ? tr("opposite") : kSectors[f.to].first);
    text += tr(", size %1 %").arg(f.size);
    if (f.kind == Op2Lib::MapGenFeature::Valley && f.lava)
        text += tr(", lava");
    return text;
}

void GenerateMapDialog::addFeature(int kind)
{
    FeatureEntry f;
    f.kind = kind;
    m_featureList.append(f);
    m_features->addItem(featureText(f));
    m_features->setCurrentRow(m_featureList.size() - 1);
}

void GenerateMapDialog::removeFeature()
{
    const int i = m_features->currentRow();
    if (i < 0)
        return;
    m_featureList.removeAt(i);
    delete m_features->takeItem(i);
    if (!m_featureList.isEmpty())
        m_features->setCurrentRow(std::min(i, static_cast<int>(m_featureList.size()) - 1));
    updateFeatureEditor();
}

// Shows the selected feature's settings; To and Lava only where they apply.
void GenerateMapDialog::updateFeatureEditor()
{
    const int i = m_features->currentRow();
    const bool has = i >= 0 && i < m_featureList.size();
    m_loading = true;
    m_where->setEnabled(has && !m_generating);
    m_featureSize->setEnabled(has && !m_generating);
    m_removeFeature->setEnabled(has && !m_generating);
    const FeatureEntry f = has ? m_featureList[i] : FeatureEntry();
    m_to->setEnabled(has && !m_generating
                     && (f.kind == Op2Lib::MapGenFeature::Valley || f.kind == Op2Lib::MapGenFeature::Choke));
    m_lava->setEnabled(has && !m_generating && f.kind == Op2Lib::MapGenFeature::Valley);
    m_where->setCurrentIndex(has ? f.where : -1);
    m_to->setCurrentIndex(has ? f.to : -1);
    m_featureSize->setValue(has ? f.size : 50);
    m_lava->setChecked(has && f.lava);
    m_loading = false;
}

void GenerateMapDialog::featureSettingChanged()
{
    if (m_loading)
        return;
    const int i = m_features->currentRow();
    if (i < 0 || i >= m_featureList.size())
        return;
    FeatureEntry &f = m_featureList[i];
    f.where = std::max(0, m_where->currentIndex());
    f.to = std::max(0, m_to->currentIndex());
    f.size = m_featureSize->value();
    f.lava = m_lava->isChecked();
    m_features->item(i)->setText(featureText(f));
}

// ---- Generate ----------------------------------------------------------------

Op2Lib::MapGenRequest GenerateMapDialog::buildRequest(quint64 seed) const
{
    Op2Lib::MapGenRequest r;
    const QSize s = kSizes[std::max(0, m_size->currentIndex())];
    r.templateName = templateName();
    r.width = s.width();
    r.height = s.height();
    r.seed = seed;
    r.roughness = m_roughness->value() / 100.0;
    r.minBiome = m_minBiome->value() / 1000.0;
    r.symmetry = std::max(0, m_symmetry->currentIndex());
    r.cliffs = m_cliffs->value() / 100.0;
    r.details = m_details->value() / 100.0;
    r.spawns = m_spawns->value();
    r.volcanoes = m_volcanoes->value();
    for (int i = 0; i < m_mix.size(); ++i)
        if (m_mix[i]->value() > 0)
            r.mix.append({ kMixGrounds[i].second, m_mix[i]->value() / 100.0 });
    for (const FeatureEntry &f : m_featureList) {
        Op2Lib::MapGenFeature mf;
        mf.kind = f.kind;
        mf.where = kSectors[f.where].second;
        mf.to = kSectors[f.to].second;
        mf.size = f.size / 100.0;
        mf.lava = f.lava;
        r.features.append(mf);
    }
    return r;
}

void GenerateMapDialog::roll()
{
    if (m_generating)
        return;
    m_seed->setText(QString::number(QRandomGenerator::global()->bounded(1, INT_MAX)));
    generate();
}

void GenerateMapDialog::generate()
{
    if (m_generating)
        return;
    bool ok = false;
    const quint64 seed = m_seed->text().trimmed().toULongLong(&ok);
    if (!ok) {
        QMessageBox::warning(this, windowTitle(), tr("The seed must be a whole number (0 or more)."));
        m_seed->setFocus();
        return;
    }
    if (!Op2Lib::profile()) {
        QMessageBox::warning(this, windowTitle(), tr("op2maplib couldn't be loaded: %1").arg(Op2Lib::profileError()));
        return;
    }
    // Ask about unsaved changes before the (possibly long) generate, not after.
    if (!m_host.isUntouchedGeneratedMap() && !m_host.confirmDiscardChanges())
        return;

    m_request = buildRequest(seed);
    saveSettings();
    m_generating = true;
    setBusy(true);
    m_status->setText(tr("Generating %1 x %2 (seed %3)...").arg(m_request.width).arg(m_request.height).arg(m_request.seed));
    m_startedMs = QDateTime::currentMSecsSinceEpoch();
    m_worker = QThread::create([this] { m_ok = Op2Lib::generateMap(m_request, m_result, m_error); });
    connect(m_worker, &QThread::finished, this, &GenerateMapDialog::onGenerated);
    m_worker->start();
}

void GenerateMapDialog::onGenerated()
{
    m_worker->deleteLater();
    m_worker = nullptr;
    const double seconds = (QDateTime::currentMSecsSinceEpoch() - m_startedMs) / 1000.0;
    if (m_ok) {
        const QString fileName = QStringLiteral("Generated_%1_%2_%3x%4.map")
                                     .arg(m_request.templateName).arg(m_request.seed)
                                     .arg(m_request.width).arg(m_request.height);
        if (m_host.showGeneratedMap(m_result, fileName)) {
            m_status->setText(describeResult(m_result, seconds));
            AppLog::write(QStringLiteral("Generate"),
                          QStringLiteral("Generated %1 in %2 s").arg(fileName).arg(seconds, 0, 'f', 1));
        } else {
            m_status->setText(tr("Not shown: the map in the main view was kept."));
        }
    } else {
        AppLog::write(QStringLiteral("Generate"), QStringLiteral("Map generation failed: %1").arg(m_error));
        m_status->setText(tr("Generation failed."));
        QMessageBox::critical(this, windowTitle(), tr("The map couldn't be generated: %1").arg(m_error));
    }
    m_result = Op2Lib::GeneratedMap();
    m_generating = false;
    setBusy(false);
}

// One line like "95 % passable · Mud 45 %, Rock 25 % · 2 spawn points (connected) · 340 ms".
QString GenerateMapDialog::describeResult(const Op2Lib::GeneratedMap &r, double seconds) const
{
    QStringList parts;
    parts << tr("%1 % passable").arg(std::lround(r.passableShare * 100));
    QStringList grounds;
    for (int g = 0; g < r.groundShares.size(); ++g)
        if (r.groundShares[g] >= 0.005)
            grounds << QStringLiteral("%1 %2 %").arg(Op2Lib::groundName(g)).arg(std::lround(r.groundShares[g] * 100));
    if (!grounds.isEmpty())
        parts << grounds.join(QStringLiteral(", "));
    if (r.plateauShare > 0)
        parts << tr("cliffs %1 %").arg(QLocale().toString(r.plateauShare * 100, 'f', 1));
    if (!r.spawns.isEmpty()) {
        QString s = r.spawns.size() == 1 ? tr("1 spawn point") : tr("%1 spawn points").arg(r.spawns.size());
        if (r.spawns.size() > 1)
            s += r.connected ? tr(" (connected)") : tr(" (NOT connected)");
        parts << s;
    }
    if (!r.volcanoes.isEmpty())
        parts << (r.volcanoes.size() == 1 ? tr("1 volcano") : tr("%1 volcanoes").arg(r.volcanoes.size()));
    parts << (seconds < 1 ? tr("%1 ms").arg(std::lround(seconds * 1000))
                          : tr("%1 s").arg(QLocale().toString(seconds, 'f', 1)));
    return parts.join(QStringLiteral(" · "));
}

// Inputs are disabled while a map is generated (Close stays enabled but
// refuses until it's done).
void GenerateMapDialog::setBusy(bool busy)
{
    for (QWidget *w : m_inputs)
        w->setEnabled(!busy);
    if (busy)
        setCursor(Qt::WaitCursor);
    else
        unsetCursor();
    if (!busy)
        updateFeatureEditor();
}

// Can't close mid-generate: the result would have nowhere to go.
void GenerateMapDialog::closeEvent(QCloseEvent *event)
{
    if (m_generating) {
        event->ignore();
        return;
    }
    saveSettings();
    QDialog::closeEvent(event);
}

void GenerateMapDialog::reject()
{
    if (m_generating)
        return;
    saveSettings();
    QDialog::reject();
}
