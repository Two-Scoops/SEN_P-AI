#include "CustomTableStyle.h"

void SidewaysTextHeader::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const{
    if(orientation() == Qt::Horizontal){
        if (!rect.isValid())
        return;
        // get the state of the section
        QStyleOptionHeader opt;
        initStyleOption(&opt);
        QStyle::State state = QStyle::State_None;
        if (isEnabled())
        state |= QStyle::State_Enabled;
        if (window()->isActiveWindow())
        state |= QStyle::State_Active;
        if (isSortIndicatorShown() && sortIndicatorSection() == logicalIndex)
            opt.sortIndicator = (sortIndicatorOrder() == Qt::AscendingOrder) ? QStyleOptionHeader::SortDown : QStyleOptionHeader::SortUp;

        // setup the style options structure
        opt.rect = rect;
        opt.section = logicalIndex;
        opt.state |= state;

        opt.iconAlignment = Qt::AlignVCenter;

        opt.text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();

        QPointF oldBO = painter->brushOrigin();


        // the section position
        int visual = visualIndex(logicalIndex);
        Q_ASSERT(visual != -1);
        if (count() == 1)
         opt.position = QStyleOptionHeader::OnlyOneSection;
        else if (visual == 0)
         opt.position = QStyleOptionHeader::Beginning;
        else if (visual == count() - 1)
         opt.position = QStyleOptionHeader::End;
        else
         opt.position = QStyleOptionHeader::Middle;

        // the selected position
        //store the header text
        QString headerText = opt.text;
        //reset the header text to no text
        opt.text = QString();
        //draw the control (unrotated!)
        style()->drawControl(QStyle::CE_Header, &opt, painter, this);

        painter->save();
        painter->translate(rect.x(), rect.y());
        painter->rotate(90); // or 270
        painter->drawText(0, 0, headerText);
        painter->restore();

        painter->setBrushOrigin(oldBO);
     } else
        QHeaderView::paintSection (painter,rect,logicalIndex );
}
