// Filename: lwoToEggConverter.cxx
// Created by:  drose (25Apr01)
// 
////////////////////////////////////////////////////////////////////

#include "lwoToEggConverter.h"
#include "cLwoLayer.h"
#include "cLwoPoints.h"
#include "cLwoPolygons.h"
#include "cLwoSurface.h"

#include <lwoHeader.h>
#include <lwoLayer.h>
#include <lwoPoints.h>
#include <lwoPolygons.h>
#include <lwoVertexMap.h>
#include <lwoTags.h>
#include <lwoPolygonTags.h>


////////////////////////////////////////////////////////////////////
//     Function: LwoToEggConverter::Constructor
//       Access: Public
//  Description: 
////////////////////////////////////////////////////////////////////
LwoToEggConverter::
LwoToEggConverter(EggData &egg_data) : _egg_data(egg_data) {
  _generic_layer = (CLwoLayer *)NULL;
  _error = false;
}

////////////////////////////////////////////////////////////////////
//     Function: LwoToEggConverter::Destructor
//       Access: Public
//  Description: 
////////////////////////////////////////////////////////////////////
LwoToEggConverter::
~LwoToEggConverter() {
  if (_generic_layer != (CLwoLayer *)NULL) {
    delete _generic_layer;
  }

  Layers::iterator li;
  for (li = _layers.begin(); li != _layers.end(); ++li) {
    CLwoLayer *layer = (*li);
    if (layer != (CLwoLayer *)NULL) {
      delete layer;
    }
  }

  Points::iterator pi;
  for (pi = _points.begin(); pi != _points.end(); ++pi) {
    CLwoPoints *points = (*pi);
    delete points;
  }

  Polygons::iterator gi;
  for (gi = _polygons.begin(); gi != _polygons.end(); ++gi) {
    CLwoPolygons *polygons = (*gi);
    delete polygons;
  }

  Surfaces::iterator si;
  for (si = _surfaces.begin(); si != _surfaces.end(); ++si) {
    CLwoSurface *surface = (*si).second;
    delete surface;
  }
}

////////////////////////////////////////////////////////////////////
//     Function: LwoToEggConverter::convert_lwo
//       Access: Public
//  Description: Fills up the egg_data structure according to the
//               indicated lwo structure.
////////////////////////////////////////////////////////////////////
bool LwoToEggConverter::
convert_lwo(const LwoHeader *lwo_header) {
  _lwo_header = lwo_header;

  collect_lwo();
  make_egg();
  connect_egg();

  _egg_data.remove_unused_vertices();

  return !_error;
}

////////////////////////////////////////////////////////////////////
//     Function: LwoToEggConverter::get_layer
//       Access: Public
//  Description: Returns a pointer to the layer with the given index
//               number, or NULL if there is no such layer.
////////////////////////////////////////////////////////////////////
CLwoLayer *LwoToEggConverter::
get_layer(int number) const {
  if (number >= 0 && number < (int)_layers.size()) {
    return _layers[number];
  }
  return (CLwoLayer *)NULL;
}

////////////////////////////////////////////////////////////////////
//     Function: LwoToEggConverter::get_surface
//       Access: Public
//  Description: Returns a pointer to the surface definition with the
//               given name, or NULL if there is no such surface.
////////////////////////////////////////////////////////////////////
CLwoSurface *LwoToEggConverter::
get_surface(const string &name) const {
  Surfaces::const_iterator si;
  si = _surfaces.find(name);
  if (si != _surfaces.end()) {
    return (*si).second;
  }
  return (CLwoSurface *)NULL;
}

////////////////////////////////////////////////////////////////////
//     Function: LwoToEggConverter::collect_lwo
//       Access: Private
//  Description: Walks through the chunks in the Lightwave data and
//               creates wrapper objects for each relevant piece.
////////////////////////////////////////////////////////////////////
void LwoToEggConverter::
collect_lwo() {  
  CLwoLayer *last_layer = (CLwoLayer *)NULL;
  CLwoPoints *last_points = (CLwoPoints *)NULL;
  CLwoPolygons *last_polygons = (CLwoPolygons *)NULL;

  const LwoTags *tags = (const LwoTags *)NULL;

  int num_chunks = _lwo_header->get_num_chunks();
  for (int i = 0; i < num_chunks; i++) {
    const IffChunk *chunk = _lwo_header->get_chunk(i);

    if (chunk->is_of_type(LwoLayer::get_class_type())) {
      const LwoLayer *lwo_layer = DCAST(LwoLayer, chunk);
      CLwoLayer *layer = new CLwoLayer(this, lwo_layer);
      int number = layer->get_number();
      slot_layer(number);

      if (_layers[number] != (CLwoLayer *)NULL) {
	nout << "Warning: multiple layers with number " << number << "\n";
      }
      _layers[number] = layer;
      last_layer = layer;
      last_points = (CLwoPoints *)NULL;
      last_polygons = (CLwoPolygons *)NULL;

    } else if (chunk->is_of_type(LwoPoints::get_class_type())) {
      if (last_layer == (CLwoLayer *)NULL) {
	last_layer = make_generic_layer();
      }

      const LwoPoints *lwo_points = DCAST(LwoPoints, chunk);
      CLwoPoints *points = new CLwoPoints(this, lwo_points, last_layer);
      _points.push_back(points);
      last_points = points;

    } else if (chunk->is_of_type(LwoVertexMap::get_class_type())) {
      if (last_points == (CLwoPoints *)NULL) {
	nout << "Vertex map chunk encountered without a preceding points chunk.\n";
      } else {
	const LwoVertexMap *lwo_vmap = DCAST(LwoVertexMap, chunk);
	last_points->add_vmap(lwo_vmap);
      }

    } else if (chunk->is_of_type(LwoTags::get_class_type())) {
      tags = DCAST(LwoTags, chunk);

    } else if (chunk->is_of_type(LwoPolygons::get_class_type())) {
      if (last_points == (CLwoPoints *)NULL) {
	nout << "Polygon chunk encountered without a preceding points chunk.\n";
      } else {
	const LwoPolygons *lwo_polygons = DCAST(LwoPolygons, chunk);
	CLwoPolygons *polygons = 
	  new CLwoPolygons(this, lwo_polygons, last_points);
	_polygons.push_back(polygons);
	last_polygons = polygons;
      }

    } else if (chunk->is_of_type(LwoPolygonTags::get_class_type())) {
      if (last_polygons == (CLwoPolygons *)NULL) {
	nout << "Polygon tags chunk encountered without a preceding polygons chunk.\n";
      } else if (tags == (LwoTags *)NULL) {
	nout << "Polygon tags chunk encountered without a preceding tags chunk.\n";
      } else {
	const LwoPolygonTags *lwo_ptags = DCAST(LwoPolygonTags, chunk);
	last_polygons->add_ptags(lwo_ptags, tags);
      }

    } else if (chunk->is_of_type(LwoSurface::get_class_type())) {
      if (last_layer == (CLwoLayer *)NULL) {
	last_layer = make_generic_layer();
      }

      const LwoSurface *lwo_surface = DCAST(LwoSurface, chunk);
      CLwoSurface *surface = new CLwoSurface(this, lwo_surface);

      bool inserted = _surfaces.insert(Surfaces::value_type(surface->get_name(), surface)).second;
      if (!inserted) {
	nout << "Multiple surface definitions named " << surface->get_name() << "\n";
	delete surface;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: LwoToEggConverter::make_egg
//       Access: Private
//  Description: Makes egg structures for all of the conversion
//               wrapper objects.
////////////////////////////////////////////////////////////////////
void LwoToEggConverter::
make_egg() {  
  if (_generic_layer != (CLwoLayer *)NULL) {
    _generic_layer->make_egg();
  }

  Layers::iterator li;
  for (li = _layers.begin(); li != _layers.end(); ++li) {
    CLwoLayer *layer = (*li);
    if (layer != (CLwoLayer *)NULL) {
      layer->make_egg();
    }
  }

  Points::iterator pi;
  for (pi = _points.begin(); pi != _points.end(); ++pi) {
    CLwoPoints *points = (*pi);
    points->make_egg();
  }

  Polygons::iterator gi;
  for (gi = _polygons.begin(); gi != _polygons.end(); ++gi) {
    CLwoPolygons *polygons = (*gi);
    polygons->make_egg();
  }
}

////////////////////////////////////////////////////////////////////
//     Function: LwoToEggConverter::connect_egg
//       Access: Private
//  Description: Connects together all of the egg structures.
////////////////////////////////////////////////////////////////////
void LwoToEggConverter::
connect_egg() {  
  if (_generic_layer != (CLwoLayer *)NULL) {
    _generic_layer->connect_egg();
  }

  Layers::iterator li;
  for (li = _layers.begin(); li != _layers.end(); ++li) {
    CLwoLayer *layer = (*li);
    if (layer != (CLwoLayer *)NULL) {
      layer->connect_egg();
    }
  }

  Points::iterator pi;
  for (pi = _points.begin(); pi != _points.end(); ++pi) {
    CLwoPoints *points = (*pi);
    points->connect_egg();
  }

  Polygons::iterator gi;
  for (gi = _polygons.begin(); gi != _polygons.end(); ++gi) {
    CLwoPolygons *polygons = (*gi);
    polygons->connect_egg();
  }
}

////////////////////////////////////////////////////////////////////
//     Function: LwoToEggConverter::slot_layer
//       Access: Private
//  Description: Ensures that there is space in the _layers array to
//               store an element at position number.
////////////////////////////////////////////////////////////////////
void LwoToEggConverter::
slot_layer(int number) {
  while (number >= (int)_layers.size()) {
    _layers.push_back((CLwoLayer *)NULL);
  }
  nassertv(number >= 0 && number < (int)_layers.size());
}

////////////////////////////////////////////////////////////////////
//     Function: LwoToEggConverter::make_generic_layer
//       Access: Private
//  Description: If a geometry definition is encountered in the
//               Lightwave file before a layer definition, we should
//               make a generic layer to hold the geometry.  This
//               makes and returns a single layer for this purpose.
//               It should not be called twice.
////////////////////////////////////////////////////////////////////
CLwoLayer *LwoToEggConverter::
make_generic_layer() {
  nassertr(_generic_layer == (CLwoLayer *)NULL, _generic_layer);

  PT(LwoLayer) layer = new LwoLayer;
  layer->make_generic();

  _generic_layer = new CLwoLayer(this, layer);
  return _generic_layer;
}
