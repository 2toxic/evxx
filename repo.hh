#pragma once

#include <map>
#include <string>
#include <vector>
#include <random>

#include "util.hh"

namespace ev {

static const ev::path REPO_DIRNAME =  ev::path (".evd");
static const ev::path REPO_FILENAME = ev::path ("evil");

struct file_record {
    ev::path filename;
    ev::path exec_filename;
    ev::time mod_time;

    ev::time mod_time_from_disk ();
private:
    ev::time disk_time;
};

class repo {
public:
    typedef std::map <std::string, std::string> options;

private:
    ev::path dirname;
    std::map <ev::path, file_record> records;
    options opts;

    std::mt19937 rnd;

public:

    repo ();
    repo (const repo&) = default;
    ~repo ();

    static repo create (ev::path where);

    void write () const;

    options& get_options ();
    bool exists (ev::path filename) const;
    file_record& operator [] (ev::path filename);
    void emplace (ev::path filename);

};

} // namespace ev
