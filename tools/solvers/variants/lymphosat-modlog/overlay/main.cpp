#include "formula_profile.h"

#include <charconv>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#ifdef __linux__
#include <sys/prctl.h>
#endif

namespace fs = std::filesystem;

namespace lymphosat {
namespace {

#ifndef LYMPHOSAT_BUNDLE_VERIFY
#define LYMPHOSAT_BUNDLE_VERIFY 0
#endif

constexpr int kAttemptContinue = -1;
constexpr int kAttemptInvalid = -2;

volatile sig_atomic_t g_child_pid = -1;

void forward_signal(int signal) {
    const pid_t child = static_cast<pid_t>(g_child_pid);
    if (child > 0) {
        kill(child, signal);
    }
    std::_Exit(128 + signal);
}

void install_signal_handlers() {
    struct sigaction action {};
    action.sa_handler = forward_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGINT, &action, nullptr);
}

std::string sanitize_for_path(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (char ch : name) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.') {
            out.push_back(ch);
        } else {
            out.push_back('_');
        }
    }
    return out.empty() ? "solver" : out;
}

fs::path executable_dir(const char* argv0) {
    std::error_code ec;
    fs::path p = fs::canonical(argv0, ec);
    if (ec) {
        p = fs::absolute(argv0, ec);
    }
    if (ec) {
        return fs::current_path();
    }
    return p.parent_path();
}

void replay_file(const fs::path& path, FILE* out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return;
    std::vector<char> buffer(1 << 16);
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize got = in.gcount();
        if (got > 0) {
            std::fwrite(buffer.data(), 1, static_cast<std::size_t>(got), out);
        }
    }
    std::fflush(out);
}

struct AttemptResult {
    int exit_code = 1;
    fs::path scratch_base;
    fs::path stdout_path;
    fs::path stderr_path;
    fs::path proof_path;
};

int wait_for_child(pid_t pid) {
    g_child_pid = pid;
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            std::perror("waitpid");
            g_child_pid = -1;
            return 1;
        }
    }
    g_child_pid = -1;

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

AttemptResult run_solver_process(const fs::path& root,
                                 std::string_view solver_name,
                                 const fs::path& solver_rel_path,
                                 const fs::path& formula_path,
                                 const fs::path& requested_proof_path) {
    const std::string safe = sanitize_for_path(solver_name);
    const fs::path attempt_base =
        requested_proof_path.string() + "." + safe + "." + std::to_string(getpid());

    AttemptResult result;
    result.scratch_base = attempt_base;
    result.stdout_path = attempt_base.string() + ".stdout";
    result.stderr_path = attempt_base.string() + ".stderr";
    result.proof_path = attempt_base.string() + ".proof";

    const fs::path solver_path = root / solver_rel_path;
    const std::string solver_string = solver_path.string();
    const std::string formula_string = formula_path.string();
    const std::string proof_string = result.proof_path.string();

    const pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork");
        result.exit_code = 1;
        return result;
    }

    if (pid == 0) {
#ifdef __linux__
        prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif
        FILE* so = std::fopen(result.stdout_path.c_str(), "wb");
        FILE* se = std::fopen(result.stderr_path.c_str(), "wb");
        if (so == nullptr || se == nullptr) {
            std::_Exit(127);
        }
        dup2(fileno(so), STDOUT_FILENO);
        dup2(fileno(se), STDERR_FILENO);
        std::fclose(so);
        std::fclose(se);

        char* const argv[] = {
            const_cast<char*>(solver_string.c_str()),
            const_cast<char*>(formula_string.c_str()),
            const_cast<char*>(proof_string.c_str()),
            nullptr,
        };
        execv(solver_string.c_str(), argv);
        std::fprintf(stderr, "execv failed for %s: %s\n",
                     solver_string.c_str(), std::strerror(errno));
        std::_Exit(127);
    }

    result.exit_code = wait_for_child(pid);
    return result;
}

bool atomic_replace_file(const fs::path& src, const fs::path& dst) {
    if (std::rename(src.c_str(), dst.c_str()) == 0) {
        return true;
    }
    std::fprintf(stderr, "could not move proof from %s to %s: %s\n",
                 src.c_str(), dst.c_str(), std::strerror(errno));
    return false;
}

#if LYMPHOSAT_BUNDLE_VERIFY
std::size_t first_nonspace(const std::string& line) {
    std::size_t i = 0;
    while (i < line.size() &&
           std::isspace(static_cast<unsigned char>(line[i]))) {
        ++i;
    }
    return i;
}

bool dimacs_control_line(const std::string& line, char control) {
    const std::size_t i = first_nonspace(line);
    return i < line.size() && line[i] == control;
}

bool parse_i64_token(const std::string& token, long long& out) {
    const char* begin = token.data();
    const char* end = begin + token.size();
    auto parsed = std::from_chars(begin, end, out);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool literal_var_index(long long lit, std::size_t& var) {
    if (lit == std::numeric_limits<long long>::min()) {
        return false;
    }
    const unsigned long long abs_lit =
        lit < 0
            ? static_cast<unsigned long long>(-(lit + 1)) + 1ULL
            : static_cast<unsigned long long>(lit);
    if (abs_lit > static_cast<unsigned long long>(
                      std::numeric_limits<std::size_t>::max())) {
        return false;
    }
    var = static_cast<std::size_t>(abs_lit);
    return true;
}

bool update_var_bound_from_token(const std::string& token, std::size_t& max_var) {
    long long lit = 0;
    if (!parse_i64_token(token, lit)) {
        return true;
    }
    if (lit == 0) {
        return true;
    }
    std::size_t var = 0;
    if (!literal_var_index(lit, var)) {
        return false;
    }
    if (var > max_var) {
        max_var = var;
    }
    return true;
}

bool cnf_var_bound(const fs::path& cnf_path, std::size_t& max_var) {
    max_var = 0;
    std::ifstream in(cnf_path);
    if (!in) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (dimacs_control_line(line, 'c')) {
            continue;
        }
        if (dimacs_control_line(line, 'p')) {
            std::istringstream header(line);
            std::string p;
            std::string cnf;
            std::string vars;
            header >> p >> cnf >> vars;
            if (p == "p" && cnf == "cnf") {
                long long header_vars = 0;
                if (parse_i64_token(vars, header_vars) && header_vars > 0) {
                    std::size_t header_bound = 0;
                    if (!literal_var_index(header_vars, header_bound)) {
                        return false;
                    }
                    if (header_bound > max_var) {
                        max_var = header_bound;
                    }
                }
            }
            continue;
        }

        std::istringstream tokens(line);
        std::string token;
        while (tokens >> token) {
            if (!update_var_bound_from_token(token, max_var)) {
                return false;
            }
        }
    }
    return true;
}

bool parse_model_assignment(const fs::path& stdout_path,
                            std::size_t max_var,
                            std::vector<unsigned char>& assignment) {
    assignment.assign(max_var + 1, 0);
    bool saw_lit = false;

    std::ifstream in(stdout_path);
    if (!in) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] != 'v') {
            continue;
        }
        if (line.size() > 1 &&
            !std::isspace(static_cast<unsigned char>(line[1]))) {
            continue;
        }

        std::istringstream tokens(line.substr(1));
        std::string token;
        while (tokens >> token) {
            long long lit = 0;
            if (!parse_i64_token(token, lit)) {
                return false;
            }
            if (lit == 0) {
                continue;
            }
            saw_lit = true;

            std::size_t var = 0;
            if (!literal_var_index(lit, var)) {
                return false;
            }
            if (var > max_var) {
                continue;
            }
            const unsigned char value = lit > 0 ? 1 : 2;
            if (assignment[var] != 0 && assignment[var] != value) {
                return false;
            }
            assignment[var] = value;
        }
    }

    return saw_lit;
}

bool verify_cnf_assignment(const fs::path& cnf_path,
                           const std::vector<unsigned char>& assignment) {
    std::ifstream in(cnf_path);
    if (!in) {
        return false;
    }

    bool clause_has_lit = false;
    bool clause_satisfied = false;
    std::string line;
    while (std::getline(in, line)) {
        if (dimacs_control_line(line, 'c') || dimacs_control_line(line, 'p')) {
            continue;
        }

        std::istringstream tokens(line);
        std::string token;
        while (tokens >> token) {
            long long lit = 0;
            if (!parse_i64_token(token, lit)) {
                return false;
            }
            if (lit == 0) {
                if (!clause_has_lit || !clause_satisfied) {
                    return false;
                }
                clause_has_lit = false;
                clause_satisfied = false;
                continue;
            }

            std::size_t var = 0;
            if (!literal_var_index(lit, var)) {
                return false;
            }
            clause_has_lit = true;
            const unsigned char value =
                var < assignment.size() ? assignment[var] : 0;
            if ((lit > 0 && value == 1) || (lit < 0 && value == 2)) {
                clause_satisfied = true;
            }
        }
    }

    return !clause_has_lit || clause_satisfied;
}

bool verify_sat_model_file(const fs::path& cnf_path, const fs::path& stdout_path) {
    std::size_t max_var = 0;
    if (!cnf_var_bound(cnf_path, max_var)) {
        return false;
    }

    std::vector<unsigned char> assignment;
    try {
        if (!parse_model_assignment(stdout_path, max_var, assignment)) {
            return false;
        }
    } catch (...) {
        return false;
    }

    return verify_cnf_assignment(cnf_path, assignment);
}

bool cnf_comment_line(const std::string& line) {
    return dimacs_control_line(line, 'c');
}

bool cnf_has_comment_lines(const fs::path& cnf_path) {
    std::ifstream in(cnf_path);
    std::string line;
    while (std::getline(in, line)) {
        if (cnf_comment_line(line)) {
            return true;
        }
    }
    return false;
}

bool write_cnf_without_comments(const fs::path& src, const fs::path& dst) {
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary);
    if (!in || !out) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (cnf_comment_line(line)) {
            continue;
        }
        out << line;
        if (!in.eof()) {
            out << '\n';
        }
    }
    return static_cast<bool>(out);
}

int run_veripb_process(const fs::path& cnf_path,
                       const fs::path& proof_path,
                       const fs::path& scratch_base) {
    const fs::path stdout_path = scratch_base.string() + ".veripb.stdout";
    const fs::path stderr_path = scratch_base.string() + ".veripb.stderr";
    const std::string cnf_string = cnf_path.string();
    const std::string proof_string = proof_path.string();

    const pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork");
        return 1;
    }

    if (pid == 0) {
#ifdef __linux__
        prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif
        FILE* so = std::fopen(stdout_path.c_str(), "wb");
        FILE* se = std::fopen(stderr_path.c_str(), "wb");
        if (so == nullptr || se == nullptr) {
            std::_Exit(127);
        }
        dup2(fileno(so), STDOUT_FILENO);
        dup2(fileno(se), STDERR_FILENO);
        std::fclose(so);
        std::fclose(se);

        char* const argv[] = {
            const_cast<char*>("veripb"),
            const_cast<char*>("--cnf"),
            const_cast<char*>(cnf_string.c_str()),
            const_cast<char*>(proof_string.c_str()),
            nullptr,
        };
        execvp("veripb", argv);
        std::fprintf(stderr, "execvp failed for veripb: %s\n", std::strerror(errno));
        std::_Exit(127);
    }

    const int rc = wait_for_child(pid);
    if (rc != 0) {
        std::fprintf(stderr, "c composite: veripb exited %d\n", rc);
        replay_file(stdout_path, stderr);
        replay_file(stderr_path, stderr);
    }

    std::error_code ignored;
    fs::remove(stdout_path, ignored);
    fs::remove(stderr_path, ignored);
    return rc;
}

bool verify_unsat_proof_with_veripb(const fs::path& cnf_path,
                                    const fs::path& proof_path,
                                    const fs::path& scratch_base) {
    fs::path checker_cnf = cnf_path;
    fs::path stripped_cnf;

    if (cnf_has_comment_lines(cnf_path)) {
        stripped_cnf = scratch_base.string() + ".stripped.cnf";
        if (!write_cnf_without_comments(cnf_path, stripped_cnf)) {
            std::fprintf(stderr, "c composite: could not strip CNF comments for veripb\n");
            return false;
        }
        checker_cnf = stripped_cnf;
    }

    const int rc = run_veripb_process(checker_cnf, proof_path, scratch_base);
    if (!stripped_cnf.empty()) {
        std::error_code ignored;
        fs::remove(stripped_cnf, ignored);
    }
    return rc == 0;
}
#endif

int run_and_maybe_return(const fs::path& root,
                         std::string_view solver_name,
                         const fs::path& solver_rel_path,
                         const fs::path& formula_path,
                         const fs::path& requested_proof_path) {
    AttemptResult attempt = run_solver_process(
        root, solver_name, solver_rel_path, formula_path, requested_proof_path);

    auto cleanup_attempt_files = [&]() {
        std::error_code ignored;
        fs::remove(attempt.stdout_path, ignored);
        fs::remove(attempt.stderr_path, ignored);
        fs::remove(attempt.proof_path, ignored);
    };

    if (attempt.exit_code == 10 || attempt.exit_code == 20) {
#if LYMPHOSAT_BUNDLE_VERIFY
        bool verified = false;
        if (attempt.exit_code == 10) {
            verified = verify_sat_model_file(formula_path, attempt.stdout_path);
        } else {
            verified = verify_unsat_proof_with_veripb(
                formula_path, attempt.proof_path, attempt.scratch_base);
        }
        if (!verified) {
            std::fprintf(stderr,
                         "c composite: %.*s failed verification; returning UNKNOWN\n",
                         static_cast<int>(solver_name.size()), solver_name.data());
            cleanup_attempt_files();
            return kAttemptInvalid;
        }
#endif
        if (attempt.exit_code == 20) {
            if (!atomic_replace_file(attempt.proof_path, requested_proof_path)) {
                cleanup_attempt_files();
                return 1;
            }
        }
        std::fprintf(stderr, "c composite: solved by %.*s (exit %d)\n",
                     static_cast<int>(solver_name.size()), solver_name.data(),
                     attempt.exit_code);
        replay_file(attempt.stdout_path, stdout);
        replay_file(attempt.stderr_path, stderr);
        cleanup_attempt_files();
        return attempt.exit_code;
    }

    if (attempt.exit_code == 0) {
        std::fprintf(stderr, "c composite: %.*s returned UNKNOWN; continuing\n",
                     static_cast<int>(solver_name.size()), solver_name.data());
    } else {
        std::fprintf(stderr, "c composite: %.*s exited %d; continuing\n",
                     static_cast<int>(solver_name.size()), solver_name.data(),
                     attempt.exit_code);
    }
    cleanup_attempt_files();
    return kAttemptContinue;
}

std::optional<FormulaProfile> load_profile_file(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "could not open profile JSON: %s\n", path.c_str());
        return std::nullopt;
    }
    std::string json((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    return profile_from_json(json);
}

int profile_only_main(const char* formula, const char* output) {
    const FormulaProfile profile = profile_formula(formula);
    const std::string json = profile_to_json(profile);
    if (std::string_view(output) == "-") {
        std::cout << json;
        return profile.parse_error ? 2 : 0;
    }
    std::ofstream out(output);
    if (!out) {
        std::fprintf(stderr, "could not write profile JSON: %s\n", output);
        return 2;
    }
    out << json;
    return profile.parse_error ? 2 : 0;
}

int check_match_main(const char* profile_path, const char* matcher_name) {
    auto profile = load_profile_file(profile_path);
    if (!profile) {
        return 2;
    }

    bool matched = false;
    for (const MatcherEntry& entry : matcher_registry()) {
        if (matcher_name != nullptr && std::string_view(entry.name) != matcher_name) {
            continue;
        }
        if (entry.match(*profile)) {
            matched = true;
            break;
        }
    }
    std::cout << (matched ? "1\n" : "0\n");
    return 0;
}

int list_matchers_main() {
    for (const MatcherEntry& entry : matcher_registry()) {
        std::cout << entry.name << "\n";
    }
    return 0;
}

void usage(const char* argv0) {
    std::fprintf(stderr,
        "usage:\n"
        "  %s <formula.cnf> <proof.out>\n"
        "  %s <formula.cnf> --profile-only <profile.json|->\n"
        "  %s --check-match <profile.json> [matcher-name]\n"
        "  %s --list-matchers\n",
        argv0, argv0, argv0, argv0);
}

}  // namespace
}  // namespace lymphosat

int main(int argc, char** argv) {
    using namespace lymphosat;

    if (argc == 2 && std::string_view(argv[1]) == "--list-matchers") {
        return list_matchers_main();
    }

    if (argc == 3 && std::string_view(argv[1]) == "--check-match") {
        return check_match_main(argv[2], nullptr);
    }

    if (argc == 4 && std::string_view(argv[1]) == "--check-match") {
        return check_match_main(argv[2], argv[3]);
    }

    if (argc == 4 && std::string_view(argv[2]) == "--profile-only") {
        return profile_only_main(argv[1], argv[3]);
    }

    if (argc != 3) {
        usage(argv[0]);
        return 2;
    }

    install_signal_handlers();

    const fs::path root = executable_dir(argv[0]);
    const fs::path formula_path = argv[1];
    const fs::path requested_proof_path = argv[2];
    const FormulaProfile profile = profile_formula(formula_path.string());
    if (profile.parse_error) {
        std::fprintf(stderr, "profile parse error: %s\n",
                     profile.parse_error_message.c_str());
    }

    for (const MatcherEntry& entry : matcher_registry()) {
        bool matched = false;
        try {
            matched = entry.match(profile);
        } catch (...) {
            std::fprintf(stderr, "c composite: matcher %s threw; skipping\n", entry.name);
            matched = false;
        }
        if (!matched) {
            continue;
        }

        std::fprintf(stderr, "c composite: matcher %s accepted formula\n", entry.name);
        const int rc = run_and_maybe_return(
            root, entry.name, entry.solver_path, formula_path, requested_proof_path);
        if (rc == 10 || rc == 20) {
            return rc;
        }
        if (rc == kAttemptInvalid) {
            std::cout << "s UNKNOWN\n";
            return 0;
        }
    }

    const std::string fallback = fallback_solver_name() == nullptr ? "" : fallback_solver_name();
    if (!fallback.empty()) {
        std::fprintf(stderr, "c composite: dispatching fallback %s\n",
                     fallback.c_str());
        const fs::path fallback_path = fs::path("solvers") / fallback / "solver" / "solver";
        const int rc = run_and_maybe_return(
            root, fallback, fallback_path, formula_path, requested_proof_path);
        if (rc == 10 || rc == 20 || rc == 0) {
            return rc;
        }
        if (rc == kAttemptInvalid) {
            std::cout << "s UNKNOWN\n";
            return 0;
        }
    }

    std::cout << "s UNKNOWN\n";
    return 0;
}
