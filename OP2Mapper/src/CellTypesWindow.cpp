#include "CellTypesWindow.h"

#include "CellTypeInfo.h"

#include <OP2Utility.h>

#include <QCloseEvent>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QVBoxLayout>

namespace {
constexpr const char *kGeometryKey = "CellTypesWindow/geometry";

// Display order + short codes, matching VB fCellTypes.vb. Indices are the
// row position; the CellType is the wire value stored in the map.
struct Entry {
    OP2Utility::CellType type;
    const char *code;
};
const Entry kEntries[] = {
    { OP2Utility::CellType::FastPassible1,    "F1"  },
    { OP2Utility::CellType::FastPassible2,    "F2"  },
    { OP2Utility::CellType::MediumPassible1,  "M1"  },
    { OP2Utility::CellType::MediumPassible2,  "M2"  },
    { OP2Utility::CellType::SlowPassible1,    "S1"  },
    { OP2Utility::CellType::SlowPassible2,    "S2"  },
    { OP2Utility::CellType::Impassible1,      "I1"  },
    { OP2Utility::CellType::Impassible2,      "I2"  },
    { OP2Utility::CellType::NorthCliffs,      "NC"  },
    { OP2Utility::CellType::CliffsHighSide,   "CHS" },
    { OP2Utility::CellType::CliffsLowSide,    "CLS" },
    { OP2Utility::CellType::VentsAndFumaroles,"V"   },
    { OP2Utility::CellType::zPad12,           "Z12" },
    { OP2Utility::CellType::zPad13,           "Z13" },
    { OP2Utility::CellType::zPad14,           "Z14" },
    { OP2Utility::CellType::zPad15,           "Z15" },
    { OP2Utility::CellType::zPad16,           "Z16" },
    { OP2Utility::CellType::zPad17,           "Z17" },
    { OP2Utility::CellType::zPad18,           "Z18" },
    { OP2Utility::CellType::zPad19,           "Z19" },
    { OP2Utility::CellType::zPad20,           "Z20" },
    { OP2Utility::CellType::DozedArea,        "D"   },
    { OP2Utility::CellType::Rubble,           "R"   },
    { OP2Utility::CellType::NormalWall,       "NW"  },
    { OP2Utility::CellType::MicrobeWall,      "MW"  },
    { OP2Utility::CellType::LavaWall,         "LW"  },
    { OP2Utility::CellType::Tube0,            "T0"  },
    { OP2Utility::CellType::Tube1,            "T1"  },
    { OP2Utility::CellType::Tube2,            "T2"  },
    { OP2Utility::CellType::Tube3,            "T3"  },
    { OP2Utility::CellType::Tube4,            "T4"  },
    { OP2Utility::CellType::Tube5,            "T5"  },
};

QPixmap swatch(OP2Utility::CellType ct)
{
    QColor c = CellTypeInfo::color(ct);
    c.setAlpha(255); // solid swatch reads better than the overlay alpha
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setPen(QColor(40, 40, 40));
    p.setBrush(c);
    p.drawRect(0, 0, 15, 15);
    return pm;
}
}

CellTypesWindow::CellTypesWindow(QWidget *parent)
    : QWidget(parent, Qt::Tool)
{
    setWindowTitle(tr("Cell Types"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setIconSize(QSize(16, 16));
    layout->addWidget(m_list);

    populate();
    resize(220, 480);

    connect(m_list, &QListWidget::itemActivated,
            this, &CellTypesWindow::onItemActivated);
    connect(m_list, &QListWidget::itemClicked,
            this, &CellTypesWindow::onItemActivated);

    restoreGeometry();
}

void CellTypesWindow::populate()
{
    m_list->clear();
    for (const auto &e : kEntries) {
        const QString text = tr("%1 (%2)")
            .arg(CellTypeInfo::name(e.type))
            .arg(QLatin1String(e.code));
        auto *item = new QListWidgetItem(swatch(e.type), text, m_list);
        item->setData(Qt::UserRole, static_cast<int>(e.type));
    }
}

void CellTypesWindow::onItemActivated(QListWidgetItem *item)
{
    if (!item)
        return;
    const int value = item->data(Qt::UserRole).toInt();
    emit cellTypeSelected(value);
}

void CellTypesWindow::clearSelection()
{
    m_list->clearSelection();
    m_list->setCurrentRow(-1);
}

void CellTypesWindow::closeEvent(QCloseEvent *event)
{
    saveGeometry();
    QWidget::closeEvent(event);
}

void CellTypesWindow::saveGeometry()
{
    QSettings s;
    s.setValue(QLatin1String(kGeometryKey), QWidget::saveGeometry());
}

void CellTypesWindow::restoreGeometry()
{
    QSettings s;
    const QByteArray g = s.value(QLatin1String(kGeometryKey)).toByteArray();
    if (!g.isEmpty())
        QWidget::restoreGeometry(g);
}
