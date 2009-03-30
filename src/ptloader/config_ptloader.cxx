// Filename: config_ptloader.cxx
// Created by:  drose (26Apr01)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) Carnegie Mellon University.  All rights reserved.
//
// All use of this software is subject to the terms of the revised BSD
// license.  You should have received a copy of this license along
// with this source code in a file named "LICENSE."
//
////////////////////////////////////////////////////////////////////

#include "config_ptloader.h"
#include "loaderFileTypePandatool.h"

#include "config_flt.h"
#include "fltToEggConverter.h"
#include "config_lwo.h"
#include "lwoToEggConverter.h"
#include "dxfToEggConverter.h"
#include "vrmlToEggConverter.h"
#include "config_xfile.h"
#include "xFileToEggConverter.h"

#ifdef HAVE_FCOLLADA
#ifndef WIN32
#include "daeToEggConverter.h"
#endif
#endif

#include "dconfig.h"
#include "loaderFileTypeRegistry.h"
#include "eggData.h"

ConfigureDef(config_ptloader);
NotifyCategoryDef(ptloader, "");

ConfigureFn(config_ptloader) {
  init_libptloader();
}

ConfigVariableEnum<DistanceUnit> ptloader_units
("ptloader-units", DU_invalid,
 PRC_DESC("Specifies the preferred units into which models will be converted "
          "when using libptloader to automatically convert files to Panda "
          "at load time, via e.g. \"pview myMayaFile.mb\"."));

////////////////////////////////////////////////////////////////////
//     Function: init_libptloader
//  Description: Initializes the library.  This must be called at
//               least once before any of the functions or classes in
//               this library can be used.  Normally it will be
//               called by the static initializers and need not be
//               called explicitly, but special cases exist.
////////////////////////////////////////////////////////////////////
void
init_libptloader() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;

  LoaderFileTypePandatool::init_type();

  LoaderFileTypeRegistry *reg = LoaderFileTypeRegistry::get_global_ptr();

  init_liblwo();
  FltToEggConverter *flt = new FltToEggConverter;
  reg->register_type(new LoaderFileTypePandatool(flt));

  init_libflt();
  LwoToEggConverter *lwo = new LwoToEggConverter;
  reg->register_type(new LoaderFileTypePandatool(lwo));

  DXFToEggConverter *dxf = new DXFToEggConverter;
  reg->register_type(new LoaderFileTypePandatool(dxf));

  VRMLToEggConverter *vrml = new VRMLToEggConverter;
  reg->register_type(new LoaderFileTypePandatool(vrml));

  init_libxfile();
  XFileToEggConverter *xfile = new XFileToEggConverter;
  reg->register_type(new LoaderFileTypePandatool(xfile));

#ifdef HAVE_FCOLLADA
#ifndef WIN32
  DAEToEggConverter *dae = new DAEToEggConverter;
  reg->register_type(new LoaderFileTypePandatool(dae));
#endif
#endif

#ifdef HAVE_MAYA
  // Register the Maya converter as a deferred type.  We don't compile
  // it in directly, because it's big and bulky; we don't need to
  // force people to load up libmayaloader (and, along with it, all of
  // the Maya API libraries) until they actually try to load a Maya
  // file.
  reg->register_deferred_type("mb", "mayaloader");
  reg->register_deferred_type("ma", "mayaloader");
#endif
}
