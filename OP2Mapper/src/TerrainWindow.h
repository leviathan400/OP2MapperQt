#pragma once

#include "ToolPalette.h"

#include <QList>

class Editor;
class QComboBox;
class QPushButton;
class QSpinBox;

// "Terrain Brush" palette (OP2Mapper3 fTerrain, tool 6): ground buttons,
// brush size, Clean Up Transitions, and the Draw Cliff controls.
class TerrainWindow : public ToolPalette
{
    Q_OBJECT
public:
    TerrainWindow(Editor *editor, QWidget *mainWindow);
    void syncFromState();

private:
    Editor *m_editor;
    QList<QPushButton *> m_groundButtons;
    QSpinBox *m_size = nullptr;
    QPushButton *m_cliffButton = nullptr;
    QComboBox *m_side = nullptr;
    QSpinBox *m_height = nullptr;
    bool m_syncing = false;
};
