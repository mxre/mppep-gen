#ifdef _WIN32
#ifdef _DLL
#ifdef _DEBUG
#pragma comment(lib, "msvcrtd")
#pragma comment(lib, "vcruntimed")
#pragma comment(lib, "ucrtd")
#pragma comment(lib, "msvcprtd")
#else // _DEBUG
#pragma comment(lib, "msvcrt")
#pragma comment(lib, "vcruntime")
#pragma comment(lib, "ucrt")
#pragma comment(lib, "msvcprt")
#endif // _DEBUG
#else // _DLL
#ifdef _DEBUG
#pragma comment(lib, "libcmtd")
#pragma comment(lib, "libvcruntimed")
#pragma comment(lib, "libucrtdib")
#pragma comment(lib, "libcpmtd")
#else // _DEBUG
#pragma comment(lib, "libcmt")
#pragma comment(lib, "libvcruntime")
#pragma comment(lib, "libucrt")
#pragma comment(lib, "libcpmt")
#endif // _DEBUG
#endif //_DLL 
#pragma comment(lib, "kernel32")
#endif

#include "Taxon.cpp"
#include "ThreadPool.cpp"
#include "CPUTime.cpp"
#include "Timer.cpp"
#include "PhylogeneticLoader.cpp"
