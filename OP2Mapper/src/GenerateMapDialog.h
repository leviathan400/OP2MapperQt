#pragma once

#include "Op2Lib.h"

#include <QDialog>
#include <QVector>
#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QSpinBox;
class QThread;

// File > Generate New Map (OP2Mapper3 fGenerateMap): settings for op2maplib's
// map generator. Pick a size, a template (which sets the terrain mix,
// roughness and volcanoes), a seed and the other settings, optionally add
// terrain features, then Generate. The map is built straight into the main
// view as a new, unsaved map; generating again replaces it as long as it
// hasn't been edited. Roll picks a new seed and generates. Settings are
// remembered in Mapper3.ini [GenerateMap]. Generation runs on a worker
// thread; the window stays open so seeds can be tried one after another.
class GenerateMapDialog : public QDialog
{
    Q_OBJECT
public:
    // Hooks into the main window.
    struct Host {
        std::function<bool()> isUntouchedGeneratedMap;
        std::function<bool()> confirmDiscardChanges;
        // Shows the result as a new, unsaved map named fileName. Returns
        // false if nothing changed (the reason has been shown).
        std::function<bool(const Op2Lib::GeneratedMap &, const QString &fileName)> showGeneratedMap;
    };

    GenerateMapDialog(Host host, QWidget *parent);
    ~GenerateMapDialog() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void reject() override;

private:
    struct FeatureEntry {
        int kind = Op2Lib::MapGenFeature::Valley;
        int where = 0; // index into the sector list
        int to = 0;
        int size = 50; // 0..100
        bool lava = false;
    };

    void buildLayout();
    QSlider *addSlider(class QGridLayout *grid, int &row, const QString &label, int min, int max,
                       std::function<QString(int)> format);
    void applyTemplateDefaults();
    void loadSettings();
    void saveSettings();
    void addFeature(int kind);
    void removeFeature();
    void updateFeatureEditor();
    void featureSettingChanged();
    QString featureText(const FeatureEntry &f) const;
    Op2Lib::MapGenRequest buildRequest(quint64 seed) const;
    void roll();
    void generate();
    void onGenerated();
    void setBusy(bool busy);
    QString describeResult(const Op2Lib::GeneratedMap &r, double seconds) const;
    QString templateName() const;

    Host m_host;
    QComboBox *m_size = nullptr;
    QComboBox *m_template = nullptr;
    QLineEdit *m_seed = nullptr;
    QPushButton *m_roll = nullptr;
    QVector<QSlider *> m_mix;
    QSlider *m_roughness = nullptr;
    QSlider *m_minBiome = nullptr;
    QComboBox *m_symmetry = nullptr;
    QSlider *m_cliffs = nullptr;
    QSlider *m_details = nullptr;
    QSpinBox *m_spawns = nullptr;
    QSpinBox *m_volcanoes = nullptr;
    QListWidget *m_features = nullptr;
    QComboBox *m_where = nullptr;
    QComboBox *m_to = nullptr;
    QSlider *m_featureSize = nullptr;
    QCheckBox *m_lava = nullptr;
    QPushButton *m_removeFeature = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_generate = nullptr;
    QPushButton *m_close = nullptr;
    QVector<QWidget *> m_inputs; // disabled while generating

    QVector<FeatureEntry> m_featureList;
    bool m_loading = false;
    bool m_generating = false;

    // Worker state for the running generate.
    QThread *m_worker = nullptr;
    Op2Lib::MapGenRequest m_request;
    Op2Lib::GeneratedMap m_result;
    QString m_error;
    bool m_ok = false;
    qint64 m_startedMs = 0;
};
