#pragma once
#include <time.h>
#include <string>

const int EV_NANOSEC_IN_SEC = 1000 * 1000 * 1000;
namespace ev {

class time {
public:
    typedef struct timespec time_type;
    typedef decltype (time_type::tv_nsec) nsec_type;
    typedef decltype (time_type::tv_sec)  sec_type;

    time ();
    time (const time&) = default;
    time (const time_t);
    time (sec_type, nsec_type);
    time (std::string);

    double to_sec () const;
    std::string to_string () const;

    static time now ();

    friend time operator - (const time&, const time&);
    friend time operator + (const time&, const time&);
    time& operator -= (const time&);
    time& operator += (const time&);
    friend bool operator < (const time&, const time&);
    friend bool operator == (const time&, const time&);
    friend bool operator != (const time&, const time&);

private:
    time_type tm;
};

class path {
    std::string path_;
    bool is_absolute_;

public:
    path ();
    explicit path (std::string);
    path (const path&) = default;

    const char *c_str () const;

    friend path operator / (const path&, const path&);
    path& operator /= (const path&);

    bool is_absolute () const;
    bool exists () const;
    bool is_root () const;

    path absolute () const;
    path dirname () const;
    static path cwd ();

    std::string str () const;

    friend bool operator < (const path&, const path&); /* To use as key in container */
};

void die_errno (const char *msg, int save_errno, int exit_status = EXIT_FAILURE);

template <typename T, size_t hex_len = sizeof (T) * 2>
std::string n2hex (T n) {
    static const char* digits = "0123456789ABCDEF";
    std::string rc (hex_len, '0');

    for (size_t i = 0, shift = (hex_len - 1) * 4; i < hex_len; ++i, shift -= 4)
        rc[i] = digits[(n >> shift) & 0x0f];

    return rc;
}

#define LOG_INFO   1
#define LOG_WARN   2
#define LOG_ERR    3
#define LOG_FAIL   4
void log (int lvl, const char *fmt, ...);
void set_log_level (int lvl);

#define FAIL_MSG(...) do {   \
        ev::log (LOG_FAIL, __VA_ARGS__);   \
        exit (EXIT_FAILURE); \
    } while (0)

} // namespace ev
