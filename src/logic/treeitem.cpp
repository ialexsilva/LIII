#include "treeitem.h"

#include <QMetaProperty>
#include <QDateTime>
#include <QMetaObject>
#include <QMetaEnum>

#include "utilities/utils.h"
#include "utilities/instantiator.h"

#include "global_functions.h"
#include "downloadtype.h"
#include "utilities/translation.h"
#include "globals.h"

#include <unordered_map>
#include <utility>

REGISTER_QOBJECT_METATYPE(TreeItem)

QString itemDCStatusToString(const ItemDC::eSTATUSDC status)
{
    static const std::unordered_map<ItemDC::eSTATUSDC, utilities::Tr::Translation, std::hash<char> > statusToTr
    {
        { ItemDC::eQUEUED,      TREEVIEW_QUEUED_STATUS },
        { ItemDC::eDOWNLOADING, TREEVIEW_DOWNLOADING_STATUS },
        { ItemDC::eCONNECTING,  TREEVIEW_CONNECTING_STATUS },
        { ItemDC::eFINISHED,    TREEVIEW_COMPLETE_STATUS },
        { ItemDC::ePAUSED,      TREEVIEW_PAUSED_STATUS },
        { ItemDC::eERROR,       TREEVIEW_FAILED_STATUS },
        { ItemDC::eSEEDING,     TREEVIEW_SEEDING_STATUS },
        { ItemDC::eSTALLED,     TREEVIEW_STALLED_STATUS },
        { ItemDC::eSTARTING,    TREEVIEW_STARTING_STATUS },
        { ItemDC::eSTOPPED,     TREEVIEW_STOPPED_STATUS },
    };

    auto it = statusToTr.find(status);

    return (statusToTr.end() != it ? utilities::Tr::Tr(it->second) : QString());
}


void ItemDC::setStatusEx(int val)
{
    // TODO validate
    m_eStatus = (eSTATUSDC) val;
    if (eDOWNLOADING == m_eStatus || eCONNECTING == m_eStatus || eSTALLED == m_eStatus)
    {
        m_eStatus = eQUEUED;
    }
}


int TreeItem::l_count = 0;

TreeItem::TreeItem(const QString& a_url, TreeItem* a_parent)
    : m_priority(0)
{
    setID(++l_count);
    setInitialURL(a_url);
    QString l_source = global_functions::GetNormalizedDomain(a_url);
    setSource(l_source);

    parentItem = a_parent;

    if (a_parent)
    {
        setPriority(a_parent->childCount());
    }
}


TreeItem::~TreeItem()
{
    qDeleteAll(childItems);
    childItems.clear();
}

void TreeItem::appendChild(TreeItem* child)
{
    childItems.append(child);
}

TreeItem* TreeItem::child(int row)
{
    return childItems.value(row);
}

int TreeItem::childCount() const
{
    return childItems.count();
}

int TreeItem::row() const
{
    if (parentItem)
    {
        return parentItem->childItems.indexOf(const_cast<TreeItem*>(this));
    }

    return 0;
}

TreeItem* TreeItem::parent() const
{
    return parentItem;
}

int TreeItem::lastIndexOf(TreeItem* a_child) const
{
    return childItems.lastIndexOf(a_child);
}

TreeItem* TreeItem::findItemByID(ItemID a_id)
{
    return findItem([a_id](const TreeItem* it) { return it->getID() == a_id; });
}

TreeItem* TreeItem::findItemByURL(const QString& a_str)
{
    return findItem([a_str](const TreeItem* it) { return it->initialURL() == a_str; });
}


bool TreeItem::removeChildItem(TreeItem* a_item)
{
    int index = lastIndexOf(a_item);
    if (index < 0)
    {
        return false;
    }

    childItems.removeAt(index);
    delete a_item;

    return true;
}


bool TreeItem::insertChildren(int position, int count, int columns)
{
    if (position < 0 || position > childItems.size())
    {
        return false;
    }

    for (int row = 0; row < count; ++row)
    {
        TreeItem* item = new TreeItem(QString("test"), this);
        childItems.insert(position, item);
    }

    return true;
}

bool TreeItem::removeChildren(int position, int count)
{
    if (position < 0 || position + count > childItems.size())
    {
        return false;
    }

    for (int row = 0; row < count; ++row)
    {
        delete childItems.takeAt(position);
    }

    return true;
}

QObjectList TreeItem::getChildItems() const
{
    QObjectList result;
    result.reserve(childItems.size());
    std::copy(childItems.begin(), childItems.end(), std::back_inserter(result));
    return result;
}

void TreeItem::setChildItems(const QObjectList& items)
{
    qDeleteAll(childItems);
    childItems.clear();
    childItems.reserve(items.size());
    for (QObject* item : items)
    {
        Q_ASSERT_X(dynamic_cast<TreeItem*>(item), Q_FUNC_INFO, "Argument is not a list of TreeItem-s!");

        auto* treeItem = static_cast<TreeItem*>(item);
        treeItem->parentItem = this;
        childItems.push_back(treeItem);
    }
}
