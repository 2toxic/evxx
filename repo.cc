#include <map>
#include <string>
#include <iostream>
#include <algorithm>
#include <fstream>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <cstring>

#include "repo.hh"

namespace ev {

namespace {
namespace ini {

typedef std::map <std::string, std::map <std::string, std::string>> ini_t;
typedef std::map <int, int> parse_errors_t;

void trim (std::string& s) {
	auto filter = [] (char c) { return !std::isspace (c); };
	s.erase (s.begin (), std::find_if (s.begin (), s.end (), filter));
	s.erase (std::find_if (s.rbegin (), s.rend (), filter).base (), s.end ());
}

const int BAD_SECTION = 1;
const int BAD_VALUE = 2;

std::pair <ini_t, parse_errors_t> 
read_from (std::istream& is, int silent) {
	std::string line;
	std::string current_section = "";
	ini_t result;
	parse_errors_t errors;
	int lineno = 0;

	while (std::getline (is, line)) {
		lineno++;
		trim (line);
		if (line.size () == 0)
			continue;

		switch (line[0]) {
			case '[': {
				if (*(line.rbegin ()) != ']') {
					if (silent & BAD_SECTION)
						errors[lineno] = BAD_SECTION;
					else
						throw std::runtime_error ("section name brace is not closed");
				}

				else {
					current_section.clear ();
					current_section = std::string (line.begin () + 1, line.end () - 1);
					result[current_section] = {};
				}
				break;
			}
			default: {
				size_t delim_pos = line.find ('=');
				if (delim_pos == std::string::npos) {
					if (silent & BAD_VALUE)
						errors[lineno] = BAD_VALUE;
					else
						throw std::runtime_error ("key without value");
				}

				std::string key (line.begin (), line.begin () + delim_pos);
				std::string value (line.begin () + delim_pos + 1, line.end ());
				trim (key);
				trim (value);
				result[current_section][key] = value;
				break;
			}
		}
	}
	return std::make_pair (result, errors);
}

void write_to (std::ostream& os, ini_t& data) {
	for (auto& section: data) {
		if (!section.first.empty ())
			os << "[" << section.first << "]" << std::endl;

		for (auto& pair: section.second)
			os << pair.first << " = " << pair.second << std::endl;

		if (section.second.size () > 0)
			os << std::endl;
	}
}

} // namespace ini

ev::path find_dir () {
	ev::path cwd = ev::path::cwd ();

    while (true) {
        auto candidate = cwd / REPO_DIRNAME;
        if (candidate.exists ())
            return candidate;

        if (cwd.is_root ())
        	break;
        cwd = cwd.dirname ();
    }

    throw std::runtime_error ("repo dir not found");
    return ev::path ();
}

bool check_dir (ev::path dir) {
	bool ret = true;
	if (::access ((dir / REPO_FILENAME).c_str (), F_OK) != 0)
		ret = false;

	return ret;
}

} // namespace

ev::time file_record :: mod_time_from_disk () {
	if (disk_time != ev::time ())
		return disk_time;

	struct stat buf;
	if (stat (filename.c_str (), &buf) != 0)
		disk_time = ev::time ();

	disk_time = ev::time (buf.st_mtim.tv_sec, buf.st_mtim.tv_nsec);

	return disk_time;
}

repo :: repo (bool overload_for_empty_ctor) {
	(void) overload_for_empty_ctor;
}

repo :: repo ():
	dirname (find_dir ()),
	records (),
	opts (),
	rnd (::time (NULL))
{
	if (!check_dir (dirname))
		throw std::runtime_error ("repo dir is broken");

	std::fstream is ((dirname / REPO_FILENAME).str (), std::ios_base::in);
	auto data = ini::read_from (is, 0).first;

	opts = data[""];
	data.erase (data.find (""));

	for (auto& pair: data) {
		file_record rec;
		rec.filename = ev::path (pair.first);
		if (pair.second["exec_filename"].empty ())
			throw std::runtime_error ("missing exec filename");
		rec.exec_filename = ev::path (pair.second["exec_filename"]);

		rec.mod_time = ev::time (pair.second["mod_time"]);
		records[rec.filename] = rec;
	}
}

repo :: ~repo () {
	write ();
}

void repo :: write () const {
	std::fstream os ((dirname / REPO_FILENAME).str (), std::ios_base::out);
	ini::ini_t data;
	data[""] = opts;

	for (auto &pair: records) {
		data[pair.first.str ()]["exec_filename"] = pair.second.exec_filename.str ();
		data[pair.first.str ()]["mod_time"] = pair.second.mod_time.to_string ();
	}

	ini::write_to (os, data);
}

repo repo :: create (ev::path where) {
	ev::path dirname = where / REPO_DIRNAME;
	ev::path filename = dirname / REPO_FILENAME;
	if (dirname.exists ())
		throw std::runtime_error ("repo exists");

	int rv = ::mkdir (dirname.c_str (), 0755);
	if (rv != 0)
		throw std::runtime_error (strerror (errno));

	rv = ::creat (filename.c_str (), 0644);
	if (rv == -1)
		throw std::runtime_error (strerror (errno));

	return repo ();
}

repo::options& repo :: get_options () {
	return opts;
}

bool repo :: exists (ev::path filename) const {
	return records.find (filename) != records.cend ();
}

ev::file_record& repo :: operator [] (ev::path filename) {
	return records[filename];
}

void repo :: emplace (ev::path filename) {
	if (exists (filename))
		throw std::runtime_error ("record exists");

    ev::path exec_path;
    do {
    	auto basename = ev::path (ev::n2hex (rnd ()));
    	exec_path = dirname / basename;
    } while (exec_path.exists ());

    ev::file_record rec;
    rec.filename = ev::path (filename);
    rec.exec_filename = ev::path (exec_path);
    rec.mod_time = ev::time ();

	records.insert (std::make_pair (filename, rec));
}

} // namespace ev

