// Filename: pStatMonitor.cxx
// Created by:  drose (09Jul00)
// 
////////////////////////////////////////////////////////////////////

#include "pStatMonitor.h"

#include <pStatCollectorDef.h>


////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::Constructor
//       Access: Public
//  Description: 
////////////////////////////////////////////////////////////////////
PStatMonitor::
PStatMonitor() {
  _client_known = false;
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::Destructor
//       Access: Public, Virtual
//  Description: 
////////////////////////////////////////////////////////////////////
PStatMonitor::
~PStatMonitor() {
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::hello_from
//       Access: Public
//  Description: Called shortly after startup time with the greeting
//               from the client.  This indicates the client's
//               reported hostname and program name.
////////////////////////////////////////////////////////////////////
void PStatMonitor::
hello_from(const string &hostname, const string &progname) {
  _client_known = true;
  _client_hostname = hostname;
  _client_progname = progname;
  got_hello();
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::bad_version
//       Access: Public
//  Description: Called shortly after startup time with the greeting
//               from the client.  In this case, the client seems to
//               have an incompatible version and will be
//               automatically disconnected; the server should issue a
//               message to that effect.
////////////////////////////////////////////////////////////////////
void PStatMonitor::
bad_version(const string &hostname, const string &progname,
            int client_major, int client_minor,
            int server_major, int server_minor) {
  _client_known = true;
  _client_hostname = hostname;
  _client_progname = progname;
  got_bad_version(client_major, client_minor,
                  server_major, server_minor);
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::set_client_data
//       Access: Public
//  Description: Called by the PStatServer at setup time to set the
//               new data pointer for the first time.
////////////////////////////////////////////////////////////////////
void PStatMonitor::
set_client_data(PStatClientData *client_data) {
  _client_data = client_data;
  initialized();
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::is_alive
//       Access: Public
//  Description: Returns true if the client is alive and connected,
//               false otherwise.
////////////////////////////////////////////////////////////////////
bool PStatMonitor::
is_alive() const {
  if (_client_data.is_null()) {
    // Not yet, but in a second probably.
    return false;
  }
  return _client_data->is_alive();
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::close
//       Access: Public
//  Description: Closes the client connection if it is active.
////////////////////////////////////////////////////////////////////
void PStatMonitor::
close() {
  if (!_client_data.is_null()) {
    _client_data->close();
  }
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::get_collector_color
//       Access: Public
//  Description: Returns the color associated with the indicated
//               collector.  If the collector has no associated color,
//               or is unknown, a new color will be made up on the
//               spot and associated with this collector for the rest
//               of the session.
////////////////////////////////////////////////////////////////////
const RGBColorf &PStatMonitor::
get_collector_color(int collector_index) {
  Colors::iterator ci;
  ci = _colors.find(collector_index);
  if (ci != _colors.end()) {
    return (*ci).second;
  }

  // Ask the client data about the color.
  if (!_client_data.is_null() && 
      _client_data->has_collector(collector_index)) {
    const PStatCollectorDef &def =
      _client_data->get_collector_def(collector_index);

    if (def._suggested_color != RGBColorf::zero()) {
      ci = _colors.insert(Colors::value_type(collector_index, def._suggested_color)).first;
      return (*ci).second;
    }
  }

  // We didn't have a color for the collector; make one up.
  RGBColorf random_color;
  random_color[0] = (float)rand() / (float)RAND_MAX;
  random_color[1] = (float)rand() / (float)RAND_MAX;
  random_color[2] = (float)rand() / (float)RAND_MAX;

  ci = _colors.insert(Colors::value_type(collector_index, random_color)).first;
  return (*ci).second;
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::get_view
//       Access: Public
//  Description: Returns a view on the given thread index.  If there
//               is no such view already for the indicated thread,
//               this will create one.  This view can be used to
//               examine the accumulated data for the given thread.
////////////////////////////////////////////////////////////////////
PStatView &PStatMonitor::
get_view(int thread_index) {
  Views::iterator vi;
  vi = _views.find(thread_index);
  if (vi == _views.end()) {
    vi = _views.insert(Views::value_type(thread_index, PStatView())).first;
    (*vi).second.set_thread_data(_client_data->get_thread_data(thread_index));
  }
  return (*vi).second;
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::get_level_view
//       Access: Public
//  Description: Returns a view on the level value (as opposed to
//               elapsed time) for the given collector over the given
//               thread.  If there is no such view already for the
//               indicated thread, this will create one.
////////////////////////////////////////////////////////////////////
PStatView &PStatMonitor::
get_level_view(int collector_index, int thread_index) {
  LevelViews::iterator lvi;
  lvi = _level_views.find(collector_index);
  if (lvi == _level_views.end()) {
    lvi = _level_views.insert(LevelViews::value_type(collector_index, Views())).first;
  }
  Views &views = (*lvi).second;
  
  Views::iterator vi;
  vi = views.find(thread_index);
  if (vi == views.end()) {
    vi = views.insert(Views::value_type(thread_index, PStatView())).first;
    (*vi).second.set_thread_data(_client_data->get_thread_data(thread_index));
    (*vi).second.constrain(collector_index, true);
  }
  return (*vi).second;
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::initialized
//       Access: Public, Virtual
//  Description: Called after the monitor has been fully set up.  At
//               this time, it will have a valid _client_data pointer,
//               and things like is_alive() and close() will be
//               meaningful.  However, we may not yet know who we're
//               connected to (is_client_known() may return false),
//               and we may not know anything about the threads or
//               collectors we're about to get data on.
////////////////////////////////////////////////////////////////////
void PStatMonitor::
initialized() {
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::got_hello
//       Access: Public, Virtual
//  Description: Called when the "hello" message has been received
//               from the client.  At this time, the client's hostname
//               and program name will be known.
////////////////////////////////////////////////////////////////////
void PStatMonitor::
got_hello() {
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::got_bad_version
//       Access: Public, Virtual
//  Description: Like got_hello(), this is called when the "hello"
//               message has been received from the client.  At this
//               time, the client's hostname and program name will be
//               known.  However, the client appears to be an
//               incompatible version and the connection will be
//               terminated; the monitor should issue a message to
//               that effect.
////////////////////////////////////////////////////////////////////
void PStatMonitor::
got_bad_version(int, int, int, int) {
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::new_collector
//       Access: Public, Virtual
//  Description: Called whenever a new Collector definition is
//               received from the client.  Generally, the client will
//               send all of its collectors over shortly after
//               connecting, but there's no guarantee that they will
//               all be received before the first frames are received.
//               The monitor should be prepared to accept new Collector
//               definitions midstream.
////////////////////////////////////////////////////////////////////
void PStatMonitor::
new_collector(int) {
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::new_thread
//       Access: Public, Virtual
//  Description: Called whenever a new Thread definition is
//               received from the client.  Generally, the client will
//               send all of its threads over shortly after
//               connecting, but there's no guarantee that they will
//               all be received before the first frames are received.
//               The monitor should be prepared to accept new Thread
//               definitions midstream.
////////////////////////////////////////////////////////////////////
void PStatMonitor::
new_thread(int) {
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::new_data
//       Access: Public, Virtual
//  Description: Called as each frame's data is made available.  There
//               is no gurantee the frames will arrive in order, or
//               that all of them will arrive at all.  The monitor
//               should be prepared to accept frames received
//               out-of-order or missing.  The use of the
//               PStatFrameData / PStatView objects to report the data
//               will facilitate this.
////////////////////////////////////////////////////////////////////
void PStatMonitor::
new_data(int, int) {
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::lost_connection
//       Access: Public, Virtual
//  Description: Called whenever the connection to the client has been
//               lost.  This is a permanent state change.  The monitor
//               should update its display to represent this, and may
//               choose to close down automatically.
////////////////////////////////////////////////////////////////////
void PStatMonitor::
lost_connection() {
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::idle
//       Access: Public, Virtual
//  Description: If has_idle() returns true, this will be called
//               periodically to allow the monitor to update its
//               display or whatever it needs to do.
////////////////////////////////////////////////////////////////////
void PStatMonitor::
idle() {
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::has_idle
//       Access: Public, Virtual
//  Description: Should be redefined to return true if you want to
//               redefine idle() and expect it to be called.
////////////////////////////////////////////////////////////////////
bool PStatMonitor::
has_idle() {
  return false;
}

////////////////////////////////////////////////////////////////////
//     Function: PStatMonitor::is_thread_safe
//       Access: Public, Virtual
//  Description: Should be redefined to return true if this monitor
//               class can handle running in a sub-thread.
//
//               This is not related to the question of whether it can
//               handle multiple different PStatThreadDatas; this is
//               strictly a question of whether or not the monitor
//               itself wants to run in a sub-thread.
////////////////////////////////////////////////////////////////////
bool PStatMonitor::
is_thread_safe() {
  return false;
}
