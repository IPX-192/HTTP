#ifndef VISAPPBUS_GLOBAL_H
#define VISAPPBUS_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(VISAPPTOOL_LIBRARY)
#  define VISAPPTOOL_EXPORT Q_DECL_EXPORT
#else
#  define VISAPPTOOL_EXPORT Q_DECL_IMPORT
#endif

#endif // VISAPPBUS_GLOBAL_H
