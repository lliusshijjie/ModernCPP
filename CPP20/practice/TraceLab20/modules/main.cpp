import tracelab.model;
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    const trace_app::Status status = trace_app::cli(argc, argv);
    return trace_app::exit_code(status);
}
