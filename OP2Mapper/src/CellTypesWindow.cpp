// CellTypesWindow: the Cell Types palette.

#include "CellTypesWindow.h"

#include "CellTypeInfo.h"
#include "Editor.h"

#include <QListWidget>
#include <QSignalBlocker>

CellTypesWindow::CellTypesWindow(Editor *editor, QWidget *mainWindow)
    : ToolPalette(QStringLiteral("CellTypes"), QStringLiteral("fCellTypes"), mainWindow)
    , m_editor(editor)
{
    setWindowTitle(tr("Cell Types"));
    setFixedSize(217, 445);

    m_list = new QListWidget(this);
    m_list->setGeometry(12, 12, 192, 420);
    for (int row = 0; row < CellTypeInfo::Count; ++row)
        m_list->addItem(CellTypeInfo::listText(row));
    m_list->setCurrentRow(-1);

    connect(m_list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item) { apply(m_list->row(item)); });
    // Selection (mouse or arrow keys) arms the tool. Not currentRowChanged:
    // the list sets a current row when it first gets focus, which would arm
    // Fast Passable 1 without the user picking it.
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        const auto sel = m_list->selectedItems();
        if (!sel.isEmpty())
            apply(m_list->row(sel.first()));
    });
}

void CellTypesWindow::apply(int row)
{
    const int ct = CellTypeInfo::listRowToCellType(row);
    if (ct < 0)
        return;
    m_editor->pickCellType(ct);
}

// Clears the highlight without arming or disarming anything.
void CellTypesWindow::clearSelection()
{
    const QSignalBlocker block(m_list);
    m_list->clearSelection();
    m_list->setCurrentRow(-1);
}
