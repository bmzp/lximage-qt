/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2013  PCMan <email>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "application.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include "applicationadaptor.h"

using namespace LxImage;

static const char* serviceName = "org.lxde.LxImage";
static const char* ifaceName = "org.lxde.LxImage.Application";

Application::Application(int& argc, char** argv):
  QApplication(argc, argv),
  libFm() {
  // QDBusConnection::sessionBus().registerObject("/org/pcmanfm/Application", this);
  QDBusConnection dbus = QDBusConnection::sessionBus();

  if(dbus.registerService(serviceName)) {
    // we successfully registered the service
    isPrimaryInstance = true;

    new ApplicationAdaptor(this);
    dbus.registerObject("/Application", this);

    // connect(this, SIGNAL(aboutToQuit()), SLOT(onAboutToQuit()));

    // FIXME: read icon theme name from config (razor-Qt/KDE config, gtkrc, or xsettings?)
    QIcon::setThemeName("oxygen");
    Fm::IconTheme::setThemeName("oxygen");
  }
  else {
    // an service of the same name is already registered.
    // we're not the first instance
    isPrimaryInstance = false;
  }
}

bool Application::init() {
  // install the translations built-into Qt itself
  qtTranslator.load("qt_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath));
  installTranslator(&qtTranslator);

  // install libfm-qt translator
  installTranslator(libFm.translator());

  // install our own tranlations
  translator.load("lximage-qt_" + QLocale::system().name(), LXIMAGE_DATA_DIR "/translations");
  installTranslator(&translator);

  if(!parseCommandLineArgs(QCoreApplication::argc(), QCoreApplication::argv()))
    return false;
  return true;
}

bool Application::parseCommandLineArgs(int argc, char** argv) {

  struct FakeTr {
    FakeTr(int reserved = 20) {
      strings.reserve(reserved);
    }

    const char* operator()(const char* str) {
      QString translated = QApplication::translate(NULL, str);
      strings.push_back(translated.toUtf8());
      return strings.back().constData();
    }
    QVector<QByteArray> strings;
  };

  bool keepRunning = false;
  // It's really a shame that the great Qt library does not come
  // with any command line parser.
  // After trying some Qt ways, I finally realized that glib is the best.
  // Simple, efficient, effective, and does not use signal/slot!
  // The only drawback is the translated string returned by tr() is
  // a temporary one. We need to store them in a list to keep them alive. :-(
  char** file_names = NULL;

  { // this block is required to limit the scope of FakeTr object so it don't affect
    // other normal QObject::tr() outside the block.
    FakeTr tr; // a functor used to override QObject::tr().
    // it convert the translated strings to UTF8 and add them to a list to
    // keep them alive during the option parsing process.
    GOptionEntry option_entries[] = {
      {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &file_names, NULL, tr("[FILE1, FILE2,...]")},
      { NULL }
    };

    GOptionContext* context = g_option_context_new("");
    g_option_context_add_main_entries(context, option_entries, NULL);
    GError* error = NULL;

    if(!g_option_context_parse(context, &argc, &argv, &error)) {
      // show error and exit
      g_fprintf(stderr, "%s\n\n", error->message);
      g_error_free(error);
      g_option_context_free(context);
      return false;
    }
    g_option_context_free(context);
  }

  QStringList paths;
  if(file_names) {
    for(char** filename = file_names; *filename; ++filename) {
      QString path(*filename);
      paths.push_back(path);
    }
  }

  if(isPrimaryInstance) {
    // TODO: load settings
    keepRunning = true;
    newWindow(paths);
  }
  else {
    // we're not the primary instance.
    // call the primary instance via dbus to do operations
    QDBusConnection dbus = QDBusConnection::sessionBus();
    QDBusInterface iface(serviceName, "/Application", ifaceName, dbus, this);
    iface.call("newWindow", paths);
  }
  // cleanup
  g_strfreev(file_names);
  return keepRunning;
}

void Application::newWindow(QStringList files) {
  LxImage::MainWindow* window;
  if(files.empty()) {
    window = new LxImage::MainWindow();
    window->show();
  }
  else {
    Q_FOREACH(QString fileName, files) {
      window = new LxImage::MainWindow();
      window->openImageFile(fileName);
      window->show();
    }
  }
}

