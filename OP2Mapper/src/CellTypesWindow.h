#pragma once

#include "ToolPalette.h"

class Editor;
class QListWidget;

// "Cell Types" palette (OP2Mapper3 fCellTypes, tool 2): a plain list of the
// 32 cell types. Opens with nothing selected (nothing armed); clicking a
// row, including the already-selected one, arms the cell-type tool.
class CellTypesWindow : public ToolPalette
{
    Q_OBJECT
public:
    CellTypesWindow(Editor *editor, QWidget *mainWindow);
    void clearSelection();

private:
    void apply(int row);

    Editor *m_editor;
    QListWidget *m_list = nullptr;
};
