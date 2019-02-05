#include <cstring>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <signal.h>

#include <fstream>
#include <iterator>
#include <sstream>
#include <iostream>

#include "evx.hh"

int main (int argc, char **argv) {
    cmd_options opts = parse_argv (argc, argv);
    if (opts.quiet)
        ev::set_log_level (LOG_ERR);
    else
        ev::set_log_level (LOG_INFO);


    auto get_filename = [opts] (bool must_exist) -> ev::path {
        if (opts.fname.str ().empty ()) {
            ev::log (LOG_FAIL, "missing filename");
            exit (EXIT_FAILURE);
        }

        if (must_exist)
            return opts.fname.absolute ();

        return opts.fname;
    };

    int ret = 0;
    try {
        switch (opts.cmd) {
            case cmd_options::CMD_INIT:
                return init ();

            case cmd_options::CMD_PREP:
                return prep  (get_filename (false));

            case cmd_options::CMD_BUILD:
                ret = build (get_filename (true), opts);
                break;

            case cmd_options::CMD_RUN: {
                ret = build (get_filename (true), opts);
                if (ret == 0)
                    ret = run (get_filename (true), opts);
                break;
            }
            case cmd_options::CMD_SHOW:
                ret = show (get_filename (true));
                break;
            default:
                ev::log (LOG_FAIL, "missing command");
                exit (EXIT_FAILURE);
        }
    }
    catch (std::runtime_error& e) {
        ev::log (LOG_FAIL, "%s", e.what ());
    }

    return ret;
}

void print_help (const char *argv0) {
    static const char *help = \
        "ev - stupid tool for contests\n"                           \
        "Usage: %s [OPTION...] [target]\n\n"                        \
                                                                    \
        "COMMANDS:\n"                                               \
        "    -i        init repo in current directory\n"            \
        "    -r        run (and maybe build) target\n"              \
        "    -b        build target\n"                              \
        "    -p        write template into target\n"                \
        "    -s        show absolute path of executable\n\n"        \
        "    -h        print this and exit\n"                       \
        "    -v        print version and exit\n\n"                  \
                                                                    \
        "OPTIONS:\n"                                                \
        "    -q        dont print errors\n"                         \
        "    -y        show system time\n"                          \
        "    -u        show user time\n"                            \
        "    -m        show rss mem usage\n"                        \
        "    -g        generate %s symbols\n"                       \
        "    -o        optimize with %s\n"                          \
        "    -D        define %s macro\n\n"                         \
        "Options may be prefixed with 'n' to invert action\n"       ;

    fprintf (stderr, help, argv0, EV_BUILD_SYMBOLS, EV_BUILD_OPTIMIZE, EV_BUILD_MACRO);
    exit (EXIT_SUCCESS);
}

void print_version () {
#ifdef EV_COMMIT
    fprintf (stderr, "ev 0.1(%s)\n", EV_COMMIT);
#else
    fprintf (stderr, "ev 0.1(unknown)\n");
#endif
    exit (EXIT_SUCCESS);
}

cmd_options parse_argv (int argc, char **argv) {
    if (argc < 1)
        die_msg ("argc (=%d) < 1", argc);
    cmd_options result;
    int c;
    int no_mod = 0;
    opterr = 0;

    while ((c = getopt (argc, argv, "irbpshvnqyumgoD")) != -1) {
    switch (c) {
        case 'h': print_help (argv[0]); break;
        case 'v': print_version (); break;

        case 'i': result.cmd = cmd_options::CMD_INIT; break;
        case 'r': result.cmd = cmd_options::CMD_RUN; break;
        case 'b': result.cmd = cmd_options::CMD_BUILD; break;
        case 'p': result.cmd = cmd_options::CMD_PREP; break;
        case 's': result.cmd = cmd_options::CMD_SHOW; break;

        case 'n': no_mod ^= 1; break;
        case 'q': result.quiet =    1 ^ no_mod; no_mod = 0; break;
        case 'y': result.show_sys = 1 ^ no_mod; no_mod = 0; break;
        case 'u': result.show_usr = 1 ^ no_mod; no_mod = 0; break;
        case 'm': result.show_rss = 1 ^ no_mod; no_mod = 0; break;
        case 'g': result.symbols =  1 ^ no_mod; no_mod = 0; break;
        case 'o': result.optimize = 1 ^ no_mod; no_mod = 0; break;
        case 'D': result.macro =    1 ^ no_mod; no_mod = 0; break;
        case '?':
            if (isprint (optopt))
                die_msg ("Unknown option: %c", optopt);
            else
                die_msg ("Unknown symbol: 0x%x", optopt);
        default:
            die_msg ("getopt failed");
    }
    }

    if (optind < argc)
        result.fname = ev::path (argv[optind]);

    return result;
}

std::pair <int, ev::time> exec_cc (std::vector <std::string> args) {
    char **c_vec_args = (char **)malloc ((args.size () + 1) * sizeof (char *));
    for (size_t i = 0; i < args.size (); ++i) {
        c_vec_args[i] = strdup (args[i].c_str ());
    }
    c_vec_args[args.size ()] = (char *)NULL;

    pid_t pid = fork ();
    ev::time start = ev::time::now ();
    if (pid < 0)
        ev::die_errno ("fork ()", errno);

    else if (pid == 0) {
        execvp (c_vec_args[0], c_vec_args);
        ev::die_errno ("execvp", errno);
    }
    else {
        for (size_t i = 0; i < args.size (); ++i)
            free (c_vec_args[i]);
        free (c_vec_args);

        int wstatus;
        waitpid (pid, &wstatus, 0);
        ev::time end = ev::time::now ();
        return std::make_pair (WEXITSTATUS (wstatus), end - start);
    }

    return {}; /* Never reached: execvp does not return on success */
}


std::vector <std::string> sub_args (ev::repo::options& repo_opts, ev::file_record rec, cmd_options opts) {
    std::vector <std::string> res;
    if (repo_opts.find ("toolchain") != repo_opts.end ())
        res.push_back (repo_opts["toolchain"]);
    else
        res.push_back ("g++");

    res.push_back (rec.filename.str ());
    res.push_back ("-o");
    res.push_back (rec.exec_filename.str ());

    if (opts.symbols)
        res.push_back (EV_BUILD_SYMBOLS);
    if (opts.optimize)
        res.push_back (EV_BUILD_OPTIMIZE);
    if (opts.macro)
        res.push_back (EV_BUILD_MACRO);

    for (auto kv: repo_opts) {
        if (kv.first == "toolchain")
            continue;

        /* split string by space */
        std::istringstream iss (kv.second);
        std::vector <std::string> kv_vec (
            std::istream_iterator <std::string> {iss},
            std::istream_iterator <std::string> ()
        );
        res.insert (res.end (), kv_vec.begin (), kv_vec.end ());
    }

    return res;
}

int prep (ev::path filename) {
    if (filename.exists ()) {
        ev::log (LOG_WARN, "file exists");
        return 1;
    }
    std::ofstream os (filename.str ());
    os << EV_CC_TEMPLATE;
    ev::log (LOG_INFO, "write: ok");
    return 0;
}

int build (ev::path filename, cmd_options opts) {
    auto r = ev::repo ();
    if (!r.exists (filename)) {
        ev::log (LOG_INFO, "new file");
        r.emplace (filename);
    }

    bool need_compile = false;
    need_compile |= r[filename].mod_time_from_disk () == ev::time ();
    need_compile |= r[filename].mod_time == ev::time ();
    need_compile |= r[filename].mod_time < r[filename].mod_time_from_disk ();

    if (need_compile) {
        auto args = sub_args (r.get_options (), r[filename], opts);
        auto ret = exec_cc (args);
        if (ret.first == 0) {
            ev::log (LOG_INFO, "built in %.3lfs", ret.second.to_sec ());
            r[filename].mod_time = r[filename].mod_time_from_disk ();
        }
        else
            ev::log (LOG_ERR, "build failed");
        
        return ret.first;
    }
    else {
        ev::log (LOG_INFO, "src untouched");
        return 0;
    }
}

int init () {
    auto cwd = ev::path::cwd ();
    ev::repo::create (cwd.absolute ());
    ev::log (LOG_INFO, "init ok");
    return 0;
}

int show (ev::path filename) {
    auto r = ev::repo ();
    if (r.exists (filename))
        std::cout << r[filename].exec_filename.c_str () << std::endl;
    else {
        ev::log (LOG_ERR, "no such record");
        return 1;
    }
    return 0;
}

int run (ev::path filename, cmd_options opts) {
    filename = ev::repo ()[filename].exec_filename;
    char * args[2];
    args[0] = const_cast <char *> (filename.c_str ());
    args[1] = NULL;

    int stdin_fwd[2];
    if (pipe (stdin_fwd) == -1)
        ev::die_errno ("pipe()", errno);

    pid_t pid = fork ();
    if (pid < 0)
        ev::die_errno ("fork()", errno);

    else if (pid == 0) {
        close (stdin_fwd[1]);
        if (dup2 (stdin_fwd[0], STDIN_FILENO) == -1)
            ev::die_errno ("dup2()", errno);

        execvp (filename.c_str (), args);
        ev::die_errno ("execvp()", errno);
    }
    else {
        close (stdin_fwd[0]);

        pid_t stdin_fwd_pid = fork ();
        if (stdin_fwd_pid < 0)
            ev::die_errno ("fork()", errno);

        else if (stdin_fwd_pid == 0) {
            char buf[EV_BUFSIZE];
            while (true) {
                int rv = read (STDIN_FILENO, buf, EV_BUFSIZE);
                write (stdin_fwd[1], buf, rv);
            }
        }

        int retstatus;
        struct rusage usg;
        wait4 (pid, &retstatus, 0, &usg);

        kill (stdin_fwd_pid, SIGTERM);

        /* FIXME write '\n' through signal to stdin_fwd_pid */
        fprintf (stderr, "\n");

        report_signal (retstatus);
        show_usage (usg, opts);

        waitpid (stdin_fwd_pid, NULL, 0);
    }
    return 0;
}

void show_usage (struct rusage usg, cmd_options opts) {
        ev::time utime (usg.ru_utime.tv_sec, usg.ru_utime.tv_usec * 1000);
        ev::time stime (usg.ru_stime.tv_sec, usg.ru_stime.tv_usec * 1000);

        if (opts.show_usr)
            ev::log (LOG_WARN, "usr: %.3lf", utime.to_sec ());
        if (opts.show_sys)
            ev::log (LOG_WARN, "sys: %.3lf", stime.to_sec ());
        if (opts.show_rss)
            ev::log (LOG_WARN, "rss: %ldK (=%ldM)", usg.ru_maxrss, usg.ru_maxrss / 1000);
}

void report_signal (int retstatus) {
    if (WIFSIGNALED (retstatus)) {
        char signame[5];
        switch (WTERMSIG (retstatus)) {
            case SIGHUP:  strcpy (signame, "HUP");  break;
            case SIGINT:  strcpy (signame, "INT");  break;
            case SIGQUIT: strcpy (signame, "QUIT"); break;
            case SIGILL:  strcpy (signame, "ILL");  break;
            case SIGABRT: strcpy (signame, "ABRT"); break;
            case SIGFPE:  strcpy (signame, "FPE");  break;
            case SIGKILL: strcpy (signame, "KILL"); break;
            case SIGSEGV: strcpy (signame, "SEGV"); break;
            case SIGPIPE: strcpy (signame, "PIPE"); break;
            case SIGALRM: strcpy (signame, "ALRM"); break;
            case SIGTERM: strcpy (signame, "TERM"); break;
            case SIGUSR1: strcpy (signame, "USR1"); break;
            case SIGUSR2: strcpy (signame, "USR2"); break;
            default: signame[0] = '\0'; break;
        }
        char coredump[] = "core dump";
        char terminated[] = "terminated";
        if (signame[0])
            ev::log (LOG_ERR, "SIG: %s (%s)", signame,
                     WCOREDUMP (retstatus) ? coredump : terminated);
        else
            ev::log (LOG_ERR, "SIG: %d (%s)", WTERMSIG (retstatus),
                     WCOREDUMP (retstatus) ? coredump : terminated);
    }
}

