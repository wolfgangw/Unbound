// SPDX-License-Identifier: MIT
#include "probe.cpp"
#include <QCheckBox>
#include <cstdio>
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QCheckBox source;QObject owner;int calls=0;bool last=false;
    new BoolSignalProbe(&source,source.metaObject()->indexOfSignal("toggled(bool)"),&owner,[&](bool value){++calls;last=value;});
    source.setChecked(true);if(calls!=1||!last)return 1;
    source.setChecked(false);if(calls!=2||last)return 2;
    std::puts("PASS: diagnostic receiver observes native bool signals without changing source state");
}
