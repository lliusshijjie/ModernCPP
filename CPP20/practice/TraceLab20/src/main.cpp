#include "tracelab/entry.hpp"
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    return tracelab::exit_code(tracelab::cli(argc, argv));
}
