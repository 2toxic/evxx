#include <cstdio>
#include <cstdarg>
#include <errno.h>
#include <cstring>
#include <unistd.h>
#include <libgen.h>

#include <sstream>

#include "util.hh"

namespace ev {

void die_errno (const char *msg, int save_errno, int exit_status) {
    fprintf (stderr, "%s: %s\n", msg, strerror (save_errno));
    exit (exit_status);
}

static int log_level = 0;
void set_log_level (int lvl) {
    log_level = lvl;
}

void log (int lvl, const char *fmt, ...) {
    if (lvl < log_level) return;
    va_list vl;
    va_start (vl, fmt);
    char buf[512];
    vsnprintf (buf, 512, fmt, vl);
    va_end (vl);

    char label[12];
    switch (lvl) {
        case LOG_INFO: strcpy (label, "\x1b[33mI\x1b[0m"); break;
        case LOG_ERR:  strcpy (label, "\x1b[31mE\x1b[0m"); break;
        case LOG_WARN: strcpy (label, "\x1b[35mW\x1b[0m"); break;
        case LOG_FAIL: strcpy (label, "\x1b[31mF\x1b[0m"); break;
        default:
            die_msg ("1 <= lvl <= 4 is not true"); break;
    }

    fprintf (stderr, " %s %s\n", label, buf);
}

time :: time () {
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
}

time :: time (sec_type sec, nsec_type nsec) {
    tm.tv_sec = sec;
    tm.tv_nsec = nsec;
}

time :: time (const time_t sec) {
    tm.tv_sec = sec;
    tm.tv_nsec = 0;
}

time :: time (std::string str) {
    size_t sec_len =  sizeof (sec_type)  * 2,
           nsec_len = sizeof (nsec_type) * 2;
    if (str.size () < sec_len + nsec_len) {
        tm.tv_sec = 0;
        tm.tv_nsec = 0;
        return;
    }

    std::string sec_str (str.begin (), str.begin () + sec_len);
    std::string nsec_str (str.begin () + sec_len + 1, str.end ());
    tm.tv_sec = std::stoll (sec_str, nullptr, 16);
    tm.tv_nsec = std::stoll (nsec_str, nullptr, 16);
}

double time :: to_sec () const {
    return (double)tm.tv_sec + (double)tm.tv_nsec / (double)EV_NANOSEC_IN_SEC;
}

std::string time :: to_string () const {
    return n2hex (tm.tv_sec) + n2hex (tm.tv_nsec);
}

time time :: now () {
    time_type ts;
    clock_gettime (CLOCK_REALTIME, &ts);
    return time (ts.tv_sec, ts.tv_nsec);
}

time operator - (const time& lhs, const time& rhs) {
    time res (lhs.tm.tv_sec, lhs.tm.tv_nsec);
    res -= rhs;
    return res;
}

time& time :: operator -= (const time& rhs) {
    this->tm.tv_sec = this->tm.tv_sec - rhs.tm.tv_sec;
    this->tm.tv_nsec = this->tm.tv_nsec - rhs.tm.tv_nsec;
    if (this->tm.tv_nsec < 0) {
        this->tm.tv_nsec += EV_NANOSEC_IN_SEC;
        this->tm.tv_sec--;
    }
    return *this;
}

bool operator < (const time& lhs, const time& rhs) {
    if (lhs.tm.tv_sec < rhs.tm.tv_sec)
        return true;
    if (lhs.tm.tv_sec == rhs.tm.tv_sec && lhs.tm.tv_nsec < rhs.tm.tv_nsec)
        return true;
    return false;
}

bool operator == (const time& lhs, const time& rhs) {
    return lhs.tm.tv_sec == rhs.tm.tv_sec && lhs.tm.tv_nsec == rhs.tm.tv_nsec;
}

bool operator != (const time& lhs, const time& rhs) {
    return !(lhs == rhs);
}



path :: path ():
    path_ ()
{}

path :: path (std::string src):
    path_ (src)
{}

const char * path :: c_str () const {
    return path_.c_str ();
}

path operator / (const path& lhs, const path& rhs) {
    path res = lhs;
    res /= rhs;
    return res;
}

path& path :: operator /= (const path& rhs) {
    if (rhs.is_absolute ())
        throw std::runtime_error ("cannot append absolute path");

    path_ += "/" + rhs.path_;
    return *this;
}

bool path :: is_absolute () const {
    return path_.size () > 0 && path_[0] == '/';
}

bool path :: exists () const {
    return ::access (c_str (), F_OK) == 0;
}

bool path :: is_root () const {
    return path_ == "/";
}

path path :: absolute () const {
    if (!exists ())
        throw std::runtime_error ("path does not exist");

    char *buf = canonicalize_file_name (c_str ());
    if (!buf)
        throw std::runtime_error (strerror (errno));

    path res ((std::string (buf)));
    free (buf);
    return res;
}

path path :: dirname () const {
    char *buf = strdup (path_.c_str ());
    char *dirname_ = ::dirname (buf);
    path res ((std::string (dirname_)));
    free (buf);
    return res;
}

path path :: cwd () {
    char * c_cwd = getcwd (0, 0);
    path result ((c_cwd));
    free (c_cwd);
    return result;
}

std::string path :: str () const {
    return path_;
}

bool operator < (const path& lhs, const path& rhs) {
    return lhs.path_ < rhs.path_;
}

} // namespace ev
