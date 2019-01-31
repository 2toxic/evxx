#pragma once
#include <time.h>

#include <string>
#include <utility>
#include <map>
#include <vector>

#include "repo.hh"
#include "util.hh"

#define EV_BUFSIZE 4096

static const char *EV_BUILD_SYMBOLS = "-g";
static const char *EV_BUILD_OPTIMIZE = "-O3";
static const char *EV_BUILD_MACRO = "-D_LOCAL_SRC";

static const char *EV_CC_TEMPLATE =                                              \
    "#include <bits/stdc++.h>\n"                                                 \
    "using namespace std;\n"                                                     \
    "typedef long long i64;typedef unsigned long long u64;\n"                    \
    "#ifdef _LOCAL_SRC\n"                                                        \
    "#define db(...) fprintf (stderr, __VA_ARGS__)\n"                            \
    "#define set_io\n"                                                           \
    "#else\n"                                                                    \
    "#define db(...)\n"                                                          \
    "#define set_io {ios_base::sync_with_stdio(0);cin.tie(0);cout.tie(0);}\n"    \
    "#endif\n\n"                                                                 \
                                                                                 \
    "int main () {\n"                                                            \
    "    set_io;\n\n"                                                            \
    "    return 0;\n"                                                            \
    "}\n";


void print_help (const char *argv0);

struct options {
    ev::path fname;
    enum {
        CMD_UNKNOWN,
        CMD_INIT,
        CMD_PREP,
        CMD_BUILD,
        CMD_RUN,
        CMD_SHOW
    } cmd;
    bool quiet,    // TODO
         show_sys,
         show_usr,
         show_rss,
         symbols,
         optimize,
         macro;

    options ():
        fname    (),
        cmd      (CMD_UNKNOWN),
        quiet    (false),
        show_sys (false),
        show_usr (true),
        show_rss (true),
        symbols  (true),
        optimize (false),
        macro    (true)
    {}

};

options parse_argv (int argc, char **argv);

std::pair <int, ev::time> exec_cc (std::vector <std::string> args);
std::vector <std::string> sub_args (ev::repo::options& repo_opts, ev::file_record rec, options opts);

int build (ev::path filename, options opts);
int run   (ev::path filename, options opts);
int show  (ev::path filename);
int prep  (ev::path filename);
int init  ();

void report_signal (int retstatus);
void show_usage (struct rusage usg, options opts);

std::string find_file ();
std::string find_dir ();
std::string canon_filename (std::string filename);
