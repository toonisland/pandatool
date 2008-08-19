#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <crtdbg.h>
#include "errno.h"
#include "Max.h"
#include "eggGroup.h"
#include "eggTable.h"
#include "eggXfmSAnim.h"
#include "eggData.h"
#include "pandatoolbase.h"
#include "referenceCount.h"
#include "pointerTo.h"
#include "namable.h"
#include "modstack.h"

#include <iostream>
#include <fstream>
#include <vector>

#include "windef.h"
#include "windows.h"

#include "Max.h"
#include "iparamb2.h"
#include "iparamm2.h"
#include "istdplug.h"
#include "iskin.h"
#include "maxResource.h"
#include "stdmat.h"
#include "phyexp.h"
#include "surf_api.h"
#include "bipexp.h"

#include "eggCoordinateSystem.h"
#include "eggGroup.h"
#include "eggPolygon.h"
#include "eggTextureCollection.h"
#include "eggTexture.h"
#include "eggVertex.h"
#include "eggVertexPool.h"
#include "eggNurbsCurve.h"
#include "pandatoolbase.h"
#include "somethingToEgg.h"
#include "somethingToEggConverter.h"
#include "eggXfmSAnim.h"

#include "maxNodeDesc.h"
#include "maxNodeTree.h"
#include "maxOptionsDialog.h"
#include "maxToEggConverter.h"
#include "maxEgg.h"

#include "maxNodeDesc.cxx"
#include "maxNodeTree.cxx"
#include "maxOptionsDialog.cxx"
#include "maxToEggConverter.cxx"
#include "maxEgg.cxx"
