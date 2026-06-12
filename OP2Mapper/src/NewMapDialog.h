#pragma once

#include <QDialog>

class QComboBox;

// Modal dialog letting the user pick a blank-map template size when creating
// a new map. Mirrors VB fNewMap.vb's size selector (10 fixed templates).
// Biome presets from the VB version are intentionally deferred.
class NewMapDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewMapDialog(QWidget *parent = nullptr);

    int selectedWidth() const { return m_width; }
    int selectedHeight() const { return m_height; }

private slots:
    void onAccept();

private:
    QComboBox *m_sizeCombo = nullptr;
    int m_width = 0;
    int m_height = 0;
};
