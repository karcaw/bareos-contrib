CONFIG += qt cross-win32
CONFIG -= debug_and_release
CONFIG( debug, debug|release )  {
   CONFIG -= release
} else {
   CONFIG -= debug
   CONFIG += release
}

bins.path = ./
bins.files = ./bat
confs.path = ./
confs.commands = ./install_conf_file

TEMPLATE     = app
TARGET       = bat
DEPENDPATH  += .
INCLUDEPATH += ../.. ../../include ../include ../compat/include ../../qt-console
VPATH        = ../../qt-console

LIBS        += -mwindows ../lib/libbareos.a ../lib/libbareos.dll -lwsock32
DEFINES     += HAVE_WIN32 HAVE_MINGW

RESOURCES    = main.qrc
MOC_DIR      = moc
OBJECTS_DIR  = obj
UI_DIR       = ui

# Main window
FORMS += main.ui
FORMS += prefs.ui
FORMS += label/label.ui
FORMS += relabel/relabel.ui
FORMS += mount/mount.ui
FORMS += console/console.ui
FORMS += restore/restore.ui restore/prerestore.ui restore/brestore.ui
FORMS += restore/runrestore.ui restore/restoretree.ui
FORMS += run/run.ui run/runcmd.ui run/estimate.ui run/prune.ui
FORMS += select/select.ui select/textinput.ui
FORMS += medialist/medialist.ui mediaedit/mediaedit.ui joblist/joblist.ui
FORMS += medialist/mediaview.ui
FORMS += clients/clients.ui storage/storage.ui fileset/fileset.ui
FORMS += joblog/joblog.ui jobs/jobs.ui job/job.ui
FORMS += help/help.ui mediainfo/mediainfo.ui
FORMS += status/dirstat.ui storage/content.ui
FORMS += status/clientstat.ui
FORMS += status/storstat.ui

# Main directory
HEADERS += mainwin.h bat.h bat_conf.h qstd.h pages.h
SOURCES += main.cpp bat_conf.cpp mainwin.cpp qstd.cpp pages.cpp

# bcomm
HEADERS += bcomm/dircomm.h
SOURCES += bcomm/dircomm.cpp

# Console
HEADERS += console/console.h
SOURCES += console/console.cpp

# Restore
HEADERS += restore/restore.h
SOURCES += restore/prerestore.cpp restore/restore.cpp restore/brestore.cpp

# Label dialog
HEADERS += label/label.h
SOURCES += label/label.cpp

# Relabel dialog
HEADERS += relabel/relabel.h
SOURCES += relabel/relabel.cpp

# Mount dialog
HEADERS += mount/mount.h
SOURCES += mount/mount.cpp

# Run dialog
HEADERS += run/run.h
SOURCES += run/run.cpp run/runcmd.cpp run/estimate.cpp run/prune.cpp

# Select dialog
HEADERS += select/select.h select/textinput.h
SOURCES += select/select.cpp select/textinput.cpp

## MediaList
HEADERS += medialist/medialist.h
SOURCES += medialist/medialist.cpp

# MediaView
HEADERS += medialist/mediaview.h
SOURCES += medialist/mediaview.cpp

## MediaEdit
HEADERS += mediaedit/mediaedit.h
SOURCES += mediaedit/mediaedit.cpp

## JobList
HEADERS += joblist/joblist.h
SOURCES += joblist/joblist.cpp

## Clients
HEADERS += clients/clients.h
SOURCES += clients/clients.cpp

## Storage
HEADERS += storage/storage.h
SOURCES += storage/storage.cpp

## Storage content
HEADERS += storage/content.h
SOURCES += storage/content.cpp

## Fileset
HEADERS += fileset/fileset.h
SOURCES += fileset/fileset.cpp

## Job log
HEADERS += joblog/joblog.h
SOURCES += joblog/joblog.cpp

## Job
HEADERS += job/job.h
SOURCES += job/job.cpp

## Jobs
HEADERS += jobs/jobs.h
SOURCES += jobs/jobs.cpp

## RestoreTree
HEADERS += restore/restoretree.h
SOURCES += restore/restoretree.cpp

# Help dialog
HEADERS += help/help.h
SOURCES += help/help.cpp

# Media info dialog
HEADERS += mediainfo/mediainfo.h
SOURCES += mediainfo/mediainfo.cpp

## Status Dir
HEADERS += status/dirstat.h
SOURCES += status/dirstat.cpp

## Status Client
HEADERS += status/clientstat.h
SOURCES += status/clientstat.cpp

## Status Client
HEADERS += status/storstat.h
SOURCES += status/storstat.cpp

# Utility sources
HEADERS += util/fmtwidgetitem.h util/comboutil.h
SOURCES += util/fmtwidgetitem.cpp util/comboutil.cpp

INSTALLS += bins
INSTALLS += confs

QMAKE_EXTRA_TARGETS += depend

TRANSLATIONS += ts/bat_fr.ts ts/bat_de.ts

# Windows Icon and App Info
win32:RC_FILE = batres.rc
