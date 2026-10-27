// SPDX-License-Identifier: MIT
#pragma once
#include <QLayout>
#include <QWidget>
#include <limits>
namespace Unbound {
inline int colorWheelsMinimumWidth(QWidget *wheels)
{
    int controls=0,spacing=std::numeric_limits<int>::max();
    for(auto top:wheels->findChildren<QWidget *>("primaryTopHoldFrame")) {
        auto layout=top->layout();
        if(!layout) continue;
        int intrinsic=0,allocated=0,count=0;
        for(int i=0;i<layout->count();++i) {
            auto item=layout->itemAt(i);
            auto group=item->widget();
            if(!group) continue;
            const int hint=group->minimumSizeHint().width();
            if(hint<0) continue;
            intrinsic+=hint;
            allocated+=item->minimumSize().width();
            ++count;
        }
        if(!count) continue;
        controls=qMax(controls,intrinsic);
        // Bars can retain expanded margins/gaps, just as wheel groups retain
        // their last allocated minimum. Use the tightest native mode's spacing
        // budget with the widest mode's intrinsic controls. Native layouts still
        // distribute available space; no child minimum or padding is rewritten.
        spacing=qMin(spacing,qMax(0,layout->minimumSize().width()-allocated));
    }
    return controls>0 && spacing!=std::numeric_limits<int>::max() ? controls+spacing : 0;
}
}
