// Filename: programBase.cxx
// Created by:  drose (13Feb00)
// 
////////////////////////////////////////////////////////////////////

#include "programBase.h"
#include "wordWrapStream.h"

#include <pystub.h>
// Since programBase.cxx includes pystub.h, no program that links with
// progbase needs to do so.  No Python code should attempt to link
// with libprogbase.so.

#include <indent.h>
#include <dSearchPath.h>
#include <coordinateSystem.h>
#include <dconfig.h>
#include <config_dconfig.h>

#include <stdlib.h>
#include <algorithm>
#include <ctype.h>

// If our system getopt() doesn't come with getopt_long_only(), then use
// the GNU flavor that we've got in tool for this purpose.
#ifndef HAVE_GETOPT_LONG_ONLY
#include <gnu_getopt.h>
#else
#include <getopt.h>
#endif

// This manifest is defined if we are running on a system (e.g. most
// any Unix) that allows us to determine the width of the terminal
// screen via an ioctl() call.  It's just handy to know for formatting
// output nicely for the user.
#ifdef IOCTL_TERMINAL_WIDTH
#include <termios.h>
#ifndef TIOCGWINSZ
#include <sys/ioctl.h>
#endif  // TIOCGWINSZ
#endif  // IOCTL_TERMINAL_WIDTH

bool ProgramBase::SortOptionsByIndex::
operator () (const Option *a, const Option *b) const {
  if (a->_index_group != b->_index_group) {
    return a->_index_group < b->_index_group;
  }
  return a->_sequence < b->_sequence;
}

// This should be called at program termination just to make sure
// Notify gets properly flushed before we exit, if someone calls
// exit().  It's probably not necessary, but why not be phobic about
// it?
static void flush_nout() {
  nout << flush;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::Constructor
//       Access: Public
//  Description: 
////////////////////////////////////////////////////////////////////
ProgramBase::
ProgramBase() {
  // A call to pystub() to force libpystub.so to be linked in.
  pystub();

  // Set up Notify to write output to our own formatted stream.
  Notify::ptr()->set_ostream_ptr(new WordWrapStream(this), true);

  // And we'll want to be sure to flush that in all normal exit cases.
  atexit(&flush_nout);

  _next_sequence = 0;
  _sorted_options = false;
  _last_newline = false;
  _got_terminal_width = false;
  _got_option_indent = false;

  add_option("h", "", 100, 
	     "Display this help page.", 
	     &ProgramBase::handle_help_option);

  // Should we report DConfig's debugging information?
  if (dconfig_cat.is_debug()) {
    dconfig_cat.debug() 
      << "DConfig took " << Config::get_total_time_config_init() 
      << " CPU seconds initializing, and " 
      << Config::get_total_time_external_init()
      << " CPU seconds calling external initialization routines.\n";
    dconfig_cat.debug()
      << "ConfigTable::GetSym() was called " 
      << Config::get_total_num_get() << " times.\n";
  }

  // It's nice to start with a blank line.
  nout << "\r";
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::Destructor
//       Access: Public, Virtual
//  Description: 
////////////////////////////////////////////////////////////////////
ProgramBase::
~ProgramBase() {
  // Reset Notify in case any messages get sent after our
  // destruction--our stream is no longer valid.
  Notify::ptr()->set_ostream_ptr(NULL, false);
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::show_description
//       Access: Public
//  Description: Writes the program description to stderr.
////////////////////////////////////////////////////////////////////
void ProgramBase::
show_description() {
  nout << _description << "\n";
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::show_usage
//       Access: Public
//  Description: Writes the usage line(s) to stderr.
////////////////////////////////////////////////////////////////////
void ProgramBase::
show_usage() {
  nout << "\rUsage:\n";
  Runlines::const_iterator ri;
  string prog = "  " +_program_name.get_basename();

  for (ri = _runlines.begin(); ri != _runlines.end(); ++ri) {
    show_text(prog, prog.length() + 1, *ri);
  }
  nout << "\r";
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::show_options
//       Access: Public
//  Description: Describes each of the available options to stderr.
////////////////////////////////////////////////////////////////////
void ProgramBase::
show_options() {
  sort_options();
  if (!_got_option_indent) {
    get_terminal_width();
    _option_indent = min(15, (int)(_terminal_width * 0.25));
    _got_option_indent = true;
  }

  nout << "Options:\n";
  OptionsByIndex::const_iterator oi;
  for (oi = _options_by_index.begin(); oi != _options_by_index.end(); ++oi) {
    const Option &opt = *(*oi);
    string prefix = "  -" + opt._option + " " + opt._parm_name;
    show_text(prefix, _option_indent, opt._description + "\r");
  }
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::show_text
//       Access: Public
//  Description: Formats the indicated text and its prefix for output
//               to stderr with the known _terminal_width.
////////////////////////////////////////////////////////////////////
void ProgramBase::
show_text(const string &prefix, int indent_width, string text) { 
  get_terminal_width();

  // This is correct!  It goes go to cerr, not to nout.  Sending it to
  // nout would be cyclic, since nout is redefined to map back through
  // this function.
  format_text(cerr, _last_newline, 
	      prefix, indent_width, text, _terminal_width);
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::parse_command_line
//       Access: Public, Virtual
//  Description: Dispatches on each of the options on the command
//               line, and passes the remaining parameters to
//               handle_args().  If an error on the command line is
//               detected, will automatically call show_usage() and
//               exit(1).
////////////////////////////////////////////////////////////////////
void ProgramBase::
parse_command_line(int argc, char *argv[]) {
  _program_name = argv[0];
  int i;
  for (i = 1; i < argc; i++) {
    _program_args.push_back(argv[i]);
  }

  // Build up the long options list and the short options string for
  // getopt_long_only().
  vector<struct option> long_options;
  string short_options;

  // We also need to build a temporary map of int index numbers to
  // Option pointers.  We'll pass these index numbers to GNU's
  // getopt_long() so we can tell one option from another.
  typedef map<int, const Option *> Options;
  Options options;

  OptionsByName::const_iterator oi;
  int next_index = 256;

  // Let's prefix the option string with "-" to tell GNU getopt that
  // we want it to tell us the post-option arguments, instead of
  // trying to meddle with ARGC and ARGV (which we aren't using
  // directly).
  short_options = "-";

  for (oi = _options_by_name.begin(); oi != _options_by_name.end(); ++oi) {
    const Option &opt = (*oi).second;

    int index;
    if (opt._option.length() == 1) {
      // This is a "short" option; its option string consists of only
      // one letter.  Its index is the letter itself.
      index = (int)opt._option[0];

      short_options += opt._option;
      if (!opt._parm_name.empty()) {
	// This option takes an argument.
	short_options += ':';
      }
    } else {
      // This is a "long" option; we'll assign it the next available
      // index.
      index = ++next_index;
    }

    // Now add it to the GNU data structures.
    struct option gopt;
    gopt.name = (char *)opt._option.c_str();
    gopt.has_arg = (opt._parm_name.empty()) ? 
      no_argument : required_argument;
    gopt.flag = (int *)NULL;
    
    // Return an index into the _options_by_index array, offset by 256
    // so we don't confuse it with '?'.
    gopt.val = index;
    
    long_options.push_back(gopt);

    options[index] = &opt;
  }

  // Finally, add one more structure, all zeroes, to indicate the end
  // of the options.
  struct option gopt;
  memset(&gopt, 0, sizeof(gopt));
  long_options.push_back(gopt);

  // We'll use this vector to save the non-option arguments.
  // Generally, these will all be at the end, but with the GNU
  // extensions, they need not be.
  Args remaining_args;

  // Now call getopt_long() to actually parse the arguments.
  extern char *optarg;
  const struct option *long_opts = &long_options[0];

  int flag = 
    getopt_long_only(argc, argv, short_options.c_str(), long_opts, NULL);
  while (flag != EOF) {
    string arg;
    if (optarg != NULL) {
      arg = optarg;
    }

    switch (flag) {
    case '?':
      // Invalid option or parameter.
      show_usage();
      exit(1);

    case '\x1':
      // A special return value from getopt() indicating a non-option
      // argument.
      remaining_args.push_back(arg);
      break;

    default:
      {
	// A normal option.  Figure out which one it is.
	Options::const_iterator ii;
	ii = options.find(flag);
	if (ii == options.end()) {
	  nout << "Internal error!  Invalid option index returned.\n";
	  abort();
	}
	
	const Option &opt = *(*ii).second;
	bool okflag = true;
	if (opt._option_function != (OptionDispatch)NULL) {
	  okflag = (this->*opt._option_function)(opt._option, arg, 
						 opt._option_data);
	}
	if (opt._bool_var != (bool *)NULL) {
	  (*opt._bool_var) = true;
	}
	
	if (!okflag) {
	  show_usage();
	  exit(1);
	}
      }
    }

    flag = 
      getopt_long_only(argc, argv, short_options.c_str(), long_opts, NULL);
  }

  if (!handle_args(remaining_args)) {
    show_usage();
    exit(1);
  }

  if (!post_command_line()) {
    show_usage();
    exit(1);
  }
}


////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::handle_args
//       Access: Protected, Virtual
//  Description: Does something with the additional arguments on the
//               command line (after all the -options have been
//               parsed).  Returns true if the arguments are good,
//               false otherwise.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
handle_args(ProgramBase::Args &args) {
  if (!args.empty()) {
    nout << "Unexpected arguments on command line:\n";
    copy(args.begin(), args.end(), ostream_iterator<string>(nout, " "));
    nout << "\r";
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::post_command_line
//       Access: Protected, Virtual
//  Description: This is called after the command line has been
//               completely processed, and it gives the program a
//               chance to do some last-minute processing and
//               validation of the options and arguments.  It should
//               return true if everything is fine, false if there is
//               an error.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
post_command_line() {
  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::set_program_description
//       Access: Protected
//  Description: Sets the description of the program that will be
//               reported by show_usage().  The description should be
//               one long string of text.  Embedded newline characters
//               are interpreted as paragraph breaks and printed as
//               blank lines.
////////////////////////////////////////////////////////////////////
void ProgramBase::
set_program_description(const string &description) {
  _description = description;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::clear_runlines
//       Access: Protected
//  Description: Removes all of the runlines that were previously
//               added, presumably before adding some new ones.
////////////////////////////////////////////////////////////////////
void ProgramBase::
clear_runlines() {
  _runlines.clear();
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::add_runline
//       Access: Protected
//  Description: Adds an additional line to the list of lines that
//               will be displayed to describe briefly how the program
//               is to be run.  Each line should be something like
//               "[opts] arg1 arg2", that is, it does *not* include
//               the name of the program, but it includes everything
//               that should be printed after the name of the program.
//
//               Normally there is only one runline for a given
//               program, but it is possible to define more than one.
////////////////////////////////////////////////////////////////////
void ProgramBase::
add_runline(const string &runline) {
  _runlines.push_back(runline);
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::clear_options
//       Access: Protected
//  Description: Removes all of the options that were previously
//               added, presumably before adding some new ones.
//               Normally you wouldn't want to do this unless you want
//               to completely replace all of the options defined by
//               base classes.
////////////////////////////////////////////////////////////////////
void ProgramBase::
clear_options() {
  _options_by_name.clear();
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::add_option
//       Access: Protected
//  Description: Adds (or redefines) a command line option.  When
//               parse_command_line() is executed it will look for
//               these options (followed by a hyphen) on the command
//               line; when a particular option is found it will call
//               the indicated option_function, supplying the provided
//               option_data.  This allows the user to define a
//               function that does some special behavior for any
//               given option, or to use any of a number of generic
//               pre-defined functions to fill in data for each
//               option.
//
//               Each option may or may not take a parameter.  If
//               parm_name is nonempty, it is assumed that the option
//               does take a parameter (and parm_name contains the
//               name that will be printed by show_options()).  This
//               parameter will be supplied as the second parameter to
//               the dispatch function.  If parm_name is empty, it is
//               assumed that the option does not take a parameter.
//               There is no provision for optional parameters.
//
//               The options are listed first in order by their
//               index_group number, and then in the order that
//               add_option() was called.  This provides a mechanism
//               for listing the options defined in derived classes
//               before those of the base classes.
////////////////////////////////////////////////////////////////////
void ProgramBase::
add_option(const string &option, const string &parm_name,
	   int index_group, const string &description, 
	   OptionDispatch option_function,
	   bool *bool_var, void *option_data) {
  Option opt;
  opt._option = option;
  opt._parm_name = parm_name;
  opt._index_group = index_group;
  opt._sequence = ++_next_sequence;
  opt._description = description;
  opt._option_function = option_function;
  opt._bool_var = bool_var;
  opt._option_data = option_data;

  _options_by_name[option] = opt;
  _sorted_options = false;

  if (bool_var != (bool *)NULL) {
    (*bool_var) = false;
  }
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::redescribe_option
//       Access: Protected
//  Description: Changes the description associated with a
//               previously-defined option.  Returns true if the
//               option was changed, false if it hadn't been defined.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
redescribe_option(const string &option, const string &description) {
  OptionsByName::iterator oi = _options_by_name.find(option);
  if (oi == _options_by_name.end()) {
    return false;
  }
  (*oi).second._description = description;
  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::remove_option
//       Access: Protected
//  Description: Removes a previously-defined option.  Returns true if
//               the option was removed, false if it hadn't existed.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
remove_option(const string &option) {
  OptionsByName::iterator oi = _options_by_name.find(option);
  if (oi == _options_by_name.end()) {
    return false;
  }
  _options_by_name.erase(oi);
  _sorted_options = false;
  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::dispatch_none
//       Access: Protected
//  Description: Standard dispatch function for an option that takes
//               no parameters, and does nothing special.  Typically
//               this would be used for a boolean flag, whose presence
//               means something and whose absence means something
//               else.  Use the bool_var parameter to add_option() to
//               determine whether the option appears on the command
//               line or not.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
dispatch_none(const string &, const string &, void *) {
  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::dispatch_count
//       Access: Protected
//  Description: Standard dispatch function for an option that takes
//               no parameters, but whose presence on the command line
//               increments an integer counter for each time it
//               appears.  -v is often an option that works this way.
//               The data pointer is to an int counter variable.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
dispatch_count(const string &, const string &, void *var) {
  int *ip = (int *)var;
  (*ip)++;

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::dispatch_int
//       Access: Protected
//  Description: Standard dispatch function for an option that takes
//               one parameter, which is to be interpreted as an
//               integer.  The data pointer is to an int variable.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
dispatch_int(const string &opt, const string &arg, void *var) {
  if (arg.empty()) {
    nout << "-" << opt << " requires an integer parameter.\n";
    return false;
  }

  int *ip = (int *)var;
  const char *arg_str = arg.c_str();
  char *endptr;
  (*ip) = strtol(arg_str, &endptr, 0);

  if (*endptr != '\0') {
    nout << "Invalid integer parameter for -" << opt << ": " 
	 << arg << "\n";
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::dispatch_double
//       Access: Protected
//  Description: Standard dispatch function for an option that takes
//               one parameter, which is to be interpreted as a
//               double.  The data pointer is to an double variable.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
dispatch_double(const string &opt, const string &arg, void *var) {
  if (arg.empty()) {
    nout << "-" << opt << " requires a floating-point parameter.\n";
    return false;
  }

  double *ip = (double *)var;
  const char *arg_str = arg.c_str();
  char *endptr;
  (*ip) = strtod(arg_str, &endptr);

  if (*endptr != '\0') {
    nout << "Invalid floating-point parameter for -" << opt << ": " 
	 << arg << "\n";
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::dispatch_string
//       Access: Protected
//  Description: Standard dispatch function for an option that takes
//               one parameter, which is to be interpreted as a
//               string.  The data pointer is to a string variable.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
dispatch_string(const string &, const string &arg, void *var) {
  string *ip = (string *)var;
  (*ip) = arg;

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::dispatch_filename
//       Access: Protected
//  Description: Standard dispatch function for an option that takes
//               one parameter, which is to be interpreted as a
//               filename.  The data pointer is to a Filename variable.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
dispatch_filename(const string &opt, const string &arg, void *var) {
  if (arg.empty()) {
    nout << "-" << opt << " requires a filename parameter.\n";
    return false;
  }

  Filename *ip = (Filename *)var;
  (*ip) = arg;

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::dispatch_search_path
//       Access: Protected
//  Description: Standard dispatch function for an option that takes
//               one parameter, which is to be interpreted as a
//               colon-delimited search path.  The data pointer is to
//               a DSearchPath variable.  This kind of option may
//               appear multiple times on the command line; each time,
//               the new search paths are appended.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
dispatch_search_path(const string &opt, const string &arg, void *var) {
  if (arg.empty()) {
    nout << "-" << opt << " requires a search path parameter.\n";
    return false;
  }

  DSearchPath *ip = (DSearchPath *)var;
  ip->append_path(arg);

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::dispatch_coordinate_system
//       Access: Protected
//  Description: Standard dispatch function for an option that takes
//               one parameter, which is to be interpreted as a
//               coordinate system string.  The data pointer is to a
//               CoordinateSystem variable.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
dispatch_coordinate_system(const string &opt, const string &arg, void *var) {
  CoordinateSystem *ip = (CoordinateSystem *)var;
  (*ip) = parse_coordinate_system_string(arg);

  if ((*ip) == CS_invalid) {
    nout << "Invalid coordinate system for -" << opt << ": " << arg << "\n"
	 << "Valid coordinate system strings are any of 'y-up', 'z-up', "
      "'y-up-left', or 'z-up-left'.\n";
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::handle_help_option
//       Access: Protected
//  Description: Called when the user enters '-h', this describes how
//               to use the program and then exits.
////////////////////////////////////////////////////////////////////
bool ProgramBase::
handle_help_option(const string &, const string &, void *) {
  show_description();
  show_usage();
  show_options();
  exit(0);

  return false;
}


////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::format_text
//       Access: Protected, Static
//  Description: Word-wraps the indicated text to the indicated output
//               stream.  The first line is prefixed with the
//               indicated prefix, then tabbed over to indent_width
//               where the text actually begins.  A newline is
//               inserted at or before column line_width.  Each
//               subsequent line begins with indent_width spaces.
//
//               An embedded newline character ('\n') forces a line
//               break, while an embedded carriage-return character
//               ('\r'), or two or more consecutive newlines, marks a
//               paragraph break, which is usually printed as a blank
//               line.  Redundant newline and carriage-return
//               characters are generally ignored.
//
//               The flag last_newline should be initialized to false
//               for the first call to format_text, and then preserved
//               for future calls; it tracks the state of trailing
//               newline characters between calls so we can correctly
//               identify doubled newlines.
////////////////////////////////////////////////////////////////////
void ProgramBase::
format_text(ostream &out, bool &last_newline,
	    const string &prefix, int indent_width,
	    const string &text, int line_width) {
  indent_width = min(indent_width, line_width - 20);
  int indent_amount = indent_width;
  bool initial_break = false;

  if (!prefix.empty()) {
    out << prefix;
    indent_amount = indent_width - prefix.length();
    if ((int)prefix.length() + 1 > indent_width) {
      out << "\n";
      initial_break = true;
      indent_amount = indent_width;
    }
  }

  size_t p = 0;

  // Skip any initial whitespace and newlines.
  while (p < text.length() && isspace(text[p])) {
    if (text[p] == '\r' ||
	(p > 0 && text[p] == '\n' && text[p - 1] == '\n') ||
 	(p == 0 && text[p] == '\n' && last_newline)) {
      if (!initial_break) {
	// Here's an initial paragraph break, however.
	out << "\n";
	initial_break = true;
      }
      indent_amount = indent_width;

    } else if (text[p] == '\n') {
      // Largely ignore an initial newline.
      indent_amount = indent_width;

    } else if (text[p] == ' ') {
      // Do count up leading spaces.
      indent_amount++;
    }
    p++;
  }

  last_newline = (!text.empty() && text[text.length() - 1] == '\n');

  while (p < text.length()) {
    // Look for the paragraph or line break--the next newline
    // character, if any.
    size_t par = text.find_first_of("\n\r", p);
    bool is_paragraph_break = false;
    if (par == string::npos) {
      par = text.length();
      /*
	This shouldn't be necessary.
    } else {
      is_paragraph_break = (text[par] == '\r');
      */
    }

    indent(out, indent_amount);

    size_t eol = p + (line_width - indent_width);
    if (eol >= par) {
      // The rest of the paragraph fits completely on the line.
      eol = par;

    } else {
      // The paragraph doesn't fit completely on the line.  Determine
      // the best place to break the line.  Look for the last space
      // before the ideal eol.
      size_t min_eol = max((int)p, (int)eol - 25);
      size_t q = eol;
      while (q > min_eol && !isspace(text[q])) {
	q--;
      }
      // Now roll back to the last non-space before this one.
      while (q > min_eol && isspace(text[q])) {
	q--;
      }

      if (q != min_eol) {
	// Here's a good place to stop!
	eol = q + 1;

      } else {
	// The line cannot be broken cleanly.  Just let it keep going;
	// don't try to wrap it.
	eol = par;
      }
    }
    out << text.substr(p, eol - p) << "\n";
    p = eol;

    // Skip additional whitespace between the lines.
    while (p < text.length() && isspace(text[p])) {
      if (text[p] == '\r' ||
	  (p > 0 && text[p] == '\n' && text[p - 1] == '\n')) {
	is_paragraph_break = true;
      }
      p++;
    }

    if (eol == par && is_paragraph_break) {
      // Print the paragraph break as a blank line.
      out << "\n";
      if (p >= text.length()) {
	// If we end on a paragraph break, don't try to insert a new
	// one in the next pass.
	last_newline = false;
      }
    }

    indent_amount = indent_width;
  }
}


////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::sort_options
//       Access: Private
//  Description: Puts all the options in order by index number
//               (e.g. in the order they were added, within
//               index_groups), for output by show_options().
////////////////////////////////////////////////////////////////////
void ProgramBase::
sort_options() {
  if (!_sorted_options) {
    _options_by_index.clear();
    
    OptionsByName::const_iterator oi;
    for (oi = _options_by_name.begin(); oi != _options_by_name.end(); ++oi) {
      _options_by_index.push_back(&(*oi).second);
    }
    
    sort(_options_by_index.begin(), _options_by_index.end(),
	 SortOptionsByIndex());
    _sorted_options = true;
  }
}

////////////////////////////////////////////////////////////////////
//     Function: ProgramBase::get_terminal_width
//       Access: Private
//  Description: Attempts to determine the ideal terminal width for
//               formatting output.
////////////////////////////////////////////////////////////////////
void ProgramBase::
get_terminal_width() {
  if (!_got_terminal_width) {
#ifdef IOCTL_TERMINAL_WIDTH
    struct winsize size;
    int result = ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&size);
    if (result < 0) {
      // Couldn't determine the width for some reason.  Instead of
      // complaining, just punt.
      _terminal_width = 72;
    } else {

      // Subtract 10% for the comfort margin at the edge.
      _terminal_width = size.ws_col - min(8, (int)(size.ws_col * 0.1));
    }
#else   // IOCTL_TERMINAL_WIDTH
    _terminal_width = 72;
#endif  // IOCTL_TERMINAL_WIDTH
    _got_terminal_width = true;
    _got_option_indent = false;
  }
}

