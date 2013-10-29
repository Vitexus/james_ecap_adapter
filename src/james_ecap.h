/* (C) 2008  The Measurement Factory */

#ifndef JAMES_ADAPTER_H
#define JAMES_ADAPTER_H

#define DEBUG

// this file should be included first from all sample sources
#define MYSQLPP_MYSQL_HEADERS_BURIED

#ifdef HAVE_CONFIG_H
#include "autoconf.h"
#endif

#ifdef __GNUC__
  #define UNUSED __attribute__((__unused__))
#else
  #define UNUSED
#endif

#ifdef DEBUG
  #define FUNCENTER() std::cerr << "==> " << __FUNCTION__ << std::endl
#else
  #define FUNCENTER()
#endif

#define ERR cerr << __FUNCTION__ << "(), "

#endif /* JAMES_ADAPTER_H */
