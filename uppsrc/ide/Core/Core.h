#ifndef COMMON_H
#define COMMON_H

#include <Esc/Esc.h>
#include <plugin/bz2/bz2.h>
#include <plugin/lz4/lz4.h>
#include <plugin/lzma/lzma.h>
#include <plugin/zstd/zstd.h>

#include "Logger.h"

using namespace Upp;

int CharFilterCid(int c);

int    ReadLNG(CParser& p);
String MakeLNG(int lang);

bool   OldLang();

String        PrintTime(int msecs);
inline String GetPrintTime(dword time0) { return PrintTime(msecs() - time0); }

bool   SaveChangedFile(const char *path, String data, bool delete_empty = false);

bool IsDoc(String s);

void CopyFile(const String& dst, const String& src, bool brc = false);
void CopyFolder(const char *_dst, const char *_src, Index<String>& used, bool all, bool brc = false);

class Workspace;

struct Ide;

namespace Upp {
class  Ctrl;
class  Image;
}

String CacheDir();
String CacheFile(const char *name);
void   ReduceCache();
void   ReduceCache(int mb_limit);
void   ReduceCacheFolder(const char *path, int64 max_total);

class PPInfo {
	enum { AUTO, APPROVED, PROHIBITED };

	struct PPFile : Moveable<PPFile> {
		int                           scan_serial = 0;
		VectorMap<String, String>     flags; // "#if... flagXXXX", key - flagXXX, value - comment
		VectorMap<String, String>     all_defines; // #define ..., 1 - speculative
		VectorMap<String, String>     defines[2]; // #define ..., 1 - speculative
		Index<String>                 includes[2]; // 1 - speculative includes (in #if conditionals)
		Index<String>                 define_includes[2]; // #define LAYOUTFILE
		bool                          guarded = false; // has include guards
		int                           blitz = 0; // AUTO, APPROVED, PROHIBITED
		Time                          time = Null; // file time
		
		bool                          dirty = true; // need to be rechecked for change (filetime)
		
		void Dirty()                          { dirty = true; time = Null; }
		void Parse(Stream& in);
		void Serialize(Stream& s);
	};
	
	struct Dir : Moveable<Dir> {
		Index<String>           subdirs;
		VectorMap<String, Time> files;
		
		void Load(const String& dir);
	};
	
	ArrayMap<String, PPFile>                   files;
	Vector<String>                             includes; // include dirs
	int                                        includes_base_count; // for trimming out additional includes
	VectorMap<String, String>                  inc_cache; // cache for FindIncludeFile
	VectorMap<String, Dir>                     dir_cache; // cache for GetFileTime, FileExists, DirExists
	String                                     current_dir; // CurrentDirectory for NormalizePath
	VectorMap<String, String>                  normalize_path_cache; // cache for NormalizePath
	static std::atomic<int>                    scan_serial;
	VectorMap<String, Index<String> >          depends; // externally forced dependecies

	PPFile& File(const String& path);

public:
	static void           RescanAll()                                           { scan_serial++; }

	Event<const String&, const String&> WhenBlitzBlock;
	Time                  GetFileTime(const String& path);
	bool                  FileExists(const String& path)                        { return !IsNull(GetFileTime(path)); }
	String                NormalizePath(const String& path, const String& curr_dir);
	String                NormalizePath(const String& path)                     { return NormalizePath(path, current_dir); }

	void                  SetIncludes(Vector<String>&& includes);
	void                  SetIncludes(const String& includes);
	
	void                  BaseIncludes();
	void                  AddInclude(const String& include);

	String                FindIncludeFile(const char *s, const String& filedir, const Vector<String>& incdirs);
	String                FindIncludeFile(const char *s, const String& filedir);

	bool                  BlitzApproved(const String& path);

	void                  ClearDependencies()  { depends.Clear(); }
	void                  AddDependency(const String& file, const String& depends);

	Time                  GatherDependencies(const String& path, VectorMap<String, Time>& result,
	                                         ArrayMap<String, Index<String>>& define_includes,
	                                         bool speculative, const String& include, Vector<String>& chain, bool& found);
	Time                  GatherDependencies(const String& path, VectorMap<String, Time>& result,
	                                         ArrayMap<String, Index<String>>& define_includes,
	                                         bool speculative = true);

	Time                  GetTime(const String& path, VectorMap<String, Time> *ret_result = nullptr);
	
	const VectorMap<String, String>& GetFileDefines(const String& path) { return File(NormalizePath(path)).all_defines; }
	const VectorMap<String, String>& GetFileFlags(const String& path)   { return File(NormalizePath(path)).flags; }

	void                  Dirty();
};

void                  HdependSetIncludes(Vector<String>&& id);
void                  HdependBaseIncludes();
void                  HdependAddInclude(const String& inc);
void                  HdependTimeDirty();
void                  HdependClearDependencies();
void                  HdependAddDependency(const String& file, const String& depends);
Time                  HdependGetFileTime(const String& path, VectorMap<String, Time> *ret_result = nullptr);
Vector<String>        HdependGetDependencies(const String& file, bool bydefine_too = true);
bool                  HdependBlitzApproved(const String& path);
const Vector<String>& HdependGetDefines(const String& path);

class IdeContext
{
public:
	virtual bool      IsVerbose() const = 0;
	virtual void      PutConsole(const char *s) = 0;
	virtual void      PutVerbose(const char *s) = 0;
	virtual void      PutLinking() = 0;
	virtual void      PutLinkingEnd(bool ok) = 0;

	virtual const Workspace& IdeWorkspace() const = 0;
	virtual bool             IdeIsBuilding() const = 0;
	virtual String           IdeGetOneFile() const = 0;
	virtual int              IdeConsoleExecute(const char *cmdline, Stream *out = NULL, const char *envptr = NULL, bool quiet = false, bool noconvert = false) = 0;
	virtual int              IdeConsoleExecuteWithInput(const char *cmdline, Stream *out, const char *envptr, bool quiet, bool noconvert = false) = 0;
	virtual int              IdeConsoleExecute(One<AProcess> pick_ process, const char *cmdline, Stream *out = NULL, bool quiet = false) = 0;
	virtual int              IdeConsoleAllocSlot() = 0;
	virtual bool             IdeConsoleRun(const char *cmdline, Stream *out = NULL, const char *envptr = NULL, bool quiet = false, int slot = 0, String key = Null, int blitz_count = 1) = 0;
	virtual bool             IdeConsoleRun(One<AProcess> pick_ process, const char *cmdline, Stream *out = NULL, bool quiet = false, int slot = 0, String key = Null, int blitz_count = 1) = 0;
	virtual void             IdeConsoleFlush() = 0;
	virtual void             IdeConsoleBeginGroup(String group) = 0;
	virtual void             IdeConsoleEndGroup() = 0;
	virtual bool             IdeConsoleWait() = 0;
	virtual bool             IdeConsoleWait(int slot) = 0;
	virtual void             IdeConsoleOnFinish(Event<>  cb) = 0;
	
	virtual void             IdeProcessEvents() = 0;

	virtual bool      IdeIsDebug() const = 0;
	virtual void      IdeEndDebug() = 0;
	virtual void      IdeSetBottom(Ctrl& ctrl) = 0;
	virtual void      IdeActivateBottom() = 0;
	virtual void      IdeRemoveBottom(Ctrl& ctrl) = 0;
	virtual void      IdeSetRight(Ctrl& ctrl) = 0;
	virtual void      IdeRemoveRight(Ctrl& ctrl) = 0;

	virtual String    IdeGetFileName() const = 0;
	virtual int       IdeGetFileLine() = 0;
	virtual String    IdeGetLine(int i) const = 0;

	virtual void      IdeSetDebugPos(const String& fn, int line, const Image& img, int i) = 0;
	virtual void      IdeHidePtr() = 0;
	virtual bool      IdeDebugLock() = 0;
	virtual bool      IdeDebugUnLock() = 0;
	virtual bool      IdeIsDebugLock() const = 0;
	virtual void      IdeSetBar() = 0;
	virtual void      IdeOpenTopicFile(const String& file) = 0;
	virtual void      IdeFlushFile() = 0;
	virtual String    IdeGetFileName() = 0;
	virtual String    IdeGetNestFolder() = 0;
	virtual String    IdeGetIncludePath() = 0;

	virtual String                    GetDefaultMethod();
	virtual VectorMap<String, String> GetMethodVars(const String& method);
	virtual String                    GetMethodName(const String& method);

	virtual bool      IsPersistentFindReplace() = 0;

	virtual int       IdeGetHydraThreads() = 0;
	virtual String    IdeGetCurrentBuildMethod() = 0;
	virtual String    IdeGetCurrentMainPackage() = 0;
	virtual void      IdePutErrorLine(const String&) = 0;

	virtual ~IdeContext() {}
};

IdeContext *TheIdeContext();
void        SetTheIde(IdeContext *context);

bool      IsVerbose();
void      PutConsole(const char *s);
void      PutVerbose(const char *s);
void      PutLinking();
void      PutLinkingEnd(bool ok);

const Workspace& GetIdeWorkspace();
bool             IdeIsBuilding();
String           IdeGetOneFile();
int              IdeConsoleExecute(const char *cmdline, Stream *out = NULL, const char *envptr = NULL, bool quiet = false, bool noconvert = false);
int              IdeConsoleExecuteWithInput(const char *cmdline, Stream *out, const char *envptr, bool quiet, bool noconvert = false);
int              IdeConsoleExecute(One<AProcess> process, const char *cmdline, Stream *out = NULL, bool quiet = false);
int              IdeConsoleAllocSlot();
bool             IdeConsoleRun(const char *cmdline, Stream *out = NULL, const char *envptr = NULL, bool quiet = false, int slot = 0, String key = Null, int blitz_count = 1);
bool             IdeConsoleRun(One<AProcess> pick_ process, const char *cmdline, Stream *out = NULL, bool quiet = false, int slot = 0, String key = Null, int blitz_count = 1);
void             IdeConsoleFlush();
void             IdeConsoleBeginGroup(String group);
void             IdeConsoleEndGroup();
bool             IdeConsoleWait();
bool             IdeConsoleWait(int slot);
void             IdeConsoleOnFinish(Event<>  cb);

void             IdeProcessEvents();

String GetSourcePackage(const String& path);

String GetDefaultMethod();
VectorMap<String, String> GetMethodVars(const String& method);
String GetMethodPath(const String& method);

bool      IdeIsDebug();
void      IdeEndDebug();
void      IdeSetBottom(Ctrl& ctrl);
void      IdeActivateBottom();
void      IdeRemoveBottom(Ctrl& ctrl);
void      IdeSetRight(Ctrl& ctrl);
void      IdeRemoveRight(Ctrl& ctrl);

String    IdeGetFileName();
int       IdeGetFileLine();
String    IdeGetLine(int i);

void      IdeSetDebugPos(const String& fn, int line, const Image& img, int i);
void      IdeHidePtr();
bool      IdeDebugLock();
bool      IdeDebugUnLock();
bool      IdeIsDebugLock();

void      IdeSetBar();

int       IdeGetHydraThreads();
String    IdeGetCurrentBuildMethod();
String    IdeGetCurrentMainPackage();
void      IdePutErrorLine(const String& s);

#include "Host.h"

struct IdeMacro {
	IdeMacro();

	int hotkey;
	String menu;
	String submenu;
	EscValue code;
};

ArrayMap<String, EscValue>& UscGlobal();
Array<IdeMacro>&            UscMacros();

void UscSetCleanModules(void (*CleanModules)());
void SetIdeModuleUsc(bool (*IdeModuleUsc)(CParser& p,String&));
void UscSetReadMacro(void (*ReadMacro)(CParser& p));

void CleanUsc();
void ParseUscFile(const char *filename);

Point ReadNums(CParser& p);
Point ReadPoint(CParser& p);

Vector<String> SplitDirs(const char *s);

class Nest {
	VectorMap<String, String> var;
	VectorMap<String, String> package_cache;

	String PackageDirectory0(const String& name);

public:
	bool   Save(const char *path);
	bool   Load(const char *path);
	String Get(const String& id);
	void   Set(const String& id, const String& val);

	void   InvalidatePackageCache();
	String PackageDirectory(const String& name);
};

Nest& MainNest();

String GetUppOut();
String GetVarsIncludes();

String DefaultHubFilePath();


String GetCurrentBuildMethod();
String GetCurrentMainPackage();

String GetAnyFileName(const char *path);
String CatAnyPath(String path, const char *more);

bool   IsFullDirectory(const String& d);
bool   IsFolder(const String& path);

bool   IsCSourceFile(const char *path);
bool   IsCHeaderFile(const char *path);

String GetLocalDir();
String LocalPath(const String& filename);

void   SetHubDir(const String& path);
void   OverrideHubDir(const String& path);
String GetHubDir();
bool   InUppHub(const String& p);

String VarFilePath();
String VarFilePath(String name);

bool   SaveVarFile(const char *filename, const VectorMap<String, String>& var);
bool   LoadVarFile(const char *name, VectorMap<String, String>& var);
bool   SaveVars(const char *name);
bool   LoadVars(const char *name);
String GetVar(const String& var);
String GetVarsName();
String VarFilePath();
Vector<String> GetUppDirsRaw();
Vector<String> GetUppDirs();
bool   IsHubDir(const String& path);
String GetUppDir();

void   SetVar(const String& var, const String& val, bool save = true);
void   SetMainNest(const String& n);
String GetAssemblyId();
void   InvalidatePackageCache();

bool   IsExternalMode();
String EncodePathAsFileName(const String& path);
String DecodePathFromFileName(const String& n);
bool   IsDirectoryExternalPackage(const String& dir);

bool   IsDirectoryPackage(const String& path);
String PackageFile(const String& name);
String SourcePath(const String& package, const String& name);
String PackageDirectory(const String& name);
bool   IsNestReadOnly(const String& path);

String GetPackagePathNest(const String& path);

void   SplitPathMap(const char *path_map, Vector<String>& local, Vector<String>& remote);
String JoinPathMap(const Vector<String>& local, const Vector<String>& remote);
void   SplitHostName(const char *hostname, String& host, int& port);

Vector<String> SplitFlags0(const char *flags);
Vector<String> SplitFlags(const char *flags, bool main = false);
Vector<String> SplitFlags(const char *flags, bool main, const Vector<String>& accepts);

bool   MatchWhen(const String& when, const Vector<String>& flag);
String ReadWhen(CParser& p);
String AsStringWhen(const String& when);

struct OptItem {
	String   when;
	String   text;

	String ToString() const { return when + ": " + text ; }
};

struct CustomStep {
	String when;
	String ext;
	String command;
	String output;

	void   Load(CParser& p);
	String AsString() const;

	String GetExt() const;
	bool   MatchExt(const char *fn) const;
};

Vector<String> Combine(const Vector<String>& conf, const char *flags);
String Gather(const Array<OptItem>& set, const Vector<String>& conf, bool nospace = false);
Vector<String> GatherV(const Array<OptItem>& set, const Vector<String>& conf);

bool   HasFlag(const Vector<String>& conf, const char *flag);

enum {
	FLAG_MISMATCH = -2,
	FLAG_UNDEFINED = -1,
};

int    GetType(const Vector<String>& conf, const char *flags);
int    GetType(const Vector<String>& conf, const char *flags, int def);
bool   GetFlag(const Vector<String>& conf, const char *flag);
String RemoveType(Vector<String>& conf, const char *flags);

enum IdeCharsets {
	CHARSET_UTF8_BOM = 250, // same as TextCtrl::CHARSET_UTF8_BOM; CtrlLib not included here
	CHARSET_UTF16_LE,
	CHARSET_UTF16_BE,
	CHARSET_UTF16_LE_BOM,
	CHARSET_UTF16_BE_BOM
};

String ReadValue(CParser& p);

class Package {
	void Reset();
	void Option(bool& option, const char *name);

public:
	struct File : public String {
		Array<OptItem> option;
		Array<OptItem> depends;
		bool           readonly;
		bool           separator;
		int            tabsize;
		byte           charset;
		int            font;
		String         highlight;
		bool           pch, nopch, noblitz;
		int            spellcheck_comments;

		void operator=(const String& s)   { String::operator=(s); readonly = separator = false; }
		void Init()  { readonly = separator = false; tabsize = Null; charset = 0; font = 0;
		               pch = nopch = noblitz = false; spellcheck_comments = Null; }

		File()                            { Init(); }
		File(const String& s) : String(s) { Init(); }
	};
	struct Config {
		String name;
		String param;
	};
	byte                     charset;
	int                      tabsize;
	bool                     noblitz;
	bool                     nowarnings;
	String                   description;
	Vector<String>           accepts;
	Array<OptItem>           flag;
	Array<OptItem>           uses;
	Array<OptItem>           target;
	Array<OptItem>           library;
	Array<OptItem>           static_library;
	Array<OptItem>           link;
	Array<OptItem>           option;
	Array<OptItem>           include;
	Array<OptItem>           pkg_config;
	Array<File>              file;
	Array<Config>            config;
	Array<CustomStep>        custom;
	Time                     time;
	bool                     bold, italic;
	Color                    ink;
	int                      spellcheck_comments;
	bool                     cr = false;

	int   GetCount() const                { return file.GetCount(); }
	File& operator[](int i)               { return file[i]; }
	const File& operator[](int i) const   { return file[i]; }

	bool  Load(const char *path);
	bool  Save(const char *file) const;

	static void SetPackageResolver(bool (*Resolve)(const String& error, const String& path, int line));

	Package();
};

String IdeCharsetName(byte charset);

class Workspace {
	void     AddUses(Package& p, const Vector<String> *flag);
	void     AddLoad(const String& name);
	void     Scan(const char *prjname, const Vector<String> *flag);

public:
	ArrayMap<String, Package> package;
	Vector<int>               use_order;

	void           Clear()                     { package.Clear(); }
	String         operator[](int i) const     { return package.GetKey(i); }
	Package&       GetPackage(int i)           { return package[i]; }
	const Package& GetPackage(int i) const     { return package[i]; }
	int            GetCount() const            { return package.GetCount(); }

	void           Scan(const char *prjname)   { Scan(prjname, nullptr); }
	void           Scan(const char *prjname, const Vector<String>& flag) { Scan(prjname, &flag); }

	Vector<String> GetAllAccepts(int pk) const;

	void     Dump();
};

struct Ide;

String FindInDirs(const Vector<String>& dir, const String& file);
String FindCommand(const Vector<String>& exedir, const String& cmdline);

struct MakeFile {
	String outdir;
	String outfile;
	String output;
	String config;
	String install;
	String rules;
	String linkdep;
	String linkfiles;
	String linkfileend;
};

String GetMakePath(String fn, bool win32);
String AdjustMakePath(const char *fn);

String Join(const String& a, const String& b, const char *sep = " ");

String GetExeExt();
String NormalizeExePath(String exePath);
String NormalizePathSeparator(String path);

struct Builder {
	Host            *host;
	Index<String>    config;
	String           method;

	String           compiler;
	String           outdir;
	Vector<String>   include;
	Vector<String>   libpath;
	String           target;
	String           cpp_options;
	String           c_options;
	String           debug_options;
	String           release_options;
	String           debug_cuda;
	String           release_cuda;
	String           common_link;
	String           debug_link;
	String           release_link;
	String           version;
	String           onefile; // Support for Ctrl-F7 - build single file

	String           script;
	String           mainpackage;

	bool             doall;
	bool             main_conf;
	bool             allow_pch;
	FileTime         start_time;

	Index<String>    pkg_config; // names of packages for pkg-config
	Vector<String>   CINC;
	Vector<String>   Macro;

	VectorMap<String, int> tmpfilei; // for naming automatic response files
	VectorMap<String, Time> dependencies; // dependencies of the last HdependFileTime call
	
	static VectorMap<String, String> cmdx_cache; // caching e.g. pkg-config

	String                 CmdX(const char *s);
	
	Time                   HdependFileTime(const String& path);

	virtual bool BuildPackage(const String& package, Vector<String>& linkfile, Vector<String>& immfile,
	    String& linkoptions, const Vector<String>& all_uses, const Vector<String>& all_libraries, int optimize)
	{ return false; }
	virtual bool Link(const Vector<String>& linkfile, const String& linkoptions, bool createmap)
	{ return false; }
	virtual bool Preprocess(const String& package, const String& file, const String& result, bool asmout)
	{ return false; }
	virtual void CleanPackage(const String& package, const String& outdir) {}
	virtual void AfterClean() {}
	virtual void AddFlags(Index<String>& cfg) {}
	virtual void AddMakeFile(MakeFile& mfinfo, String package,
		const Vector<String>& all_uses, const Vector<String>& all_libraries,
		const Index<String>& common_config, bool exporting) {}
	virtual void AddCCJ(MakeFile& mfinfo, String package,
		const Index<String>& common_config, bool exporting, bool last_ws) {}
	virtual String GetTargetExt() const = 0;
	virtual void SaveBuildInfo(const String& package) {}
	virtual String GetBuildInfoPath() const { return String(); }
	virtual String CompilerName() const { return Null; }

	Builder()          { doall = false; main_conf = false; }
	virtual ~Builder() {}

	void                   ChDir(const String& path);
	String                 GetPathQ(const String& path) const;
	Vector<Host::FileInfo> GetFileInfo(const Vector<String>& path) const;
	Host::FileInfo         GetFileInfo(const String& path) const;
	Time                   GetFileTime(const String& path) const;
	int                    Execute(const char *cmdline);
	int                    Execute(const char *cl, Stream& out);
	bool                   HasFlag(const char *f) const { return config.Find(f) >= 0; }
};

VectorMap<String, Builder *(*)()>& BuilderMap();
void RegisterBuilder(const char *name, Builder *(*create)());

bool IsHeaderExt(const String& ext);

class BinObjInfo {
public:
	BinObjInfo();

	void Parse(CParser& binscript, String base_dir);

	struct Block {
		Block() : index(-1), length(0), scriptline(-1), encoding(ENC_PLAIN), flags(0), offset(-1), len_meta_offset(-1) {}

		String ident;
		int    index;
		String file;
		int    length;
		int    scriptline;
		int    encoding;
		enum {
			ENC_PLAIN,
			ENC_ZIP,
			ENC_BZ2,
			ENC_LZ4,
			ENC_LZMA,
			ENC_ZSTD,
		};
		int    flags;
		enum {
			FLG_ARRAY = 0x01,
			FLG_MASK  = 0x02,
		};

		int    offset;
		int    off_meta_offset;
		int    len_meta_offset;

		void Compress(String& data);
	};

	VectorMap< String, ArrayMap<int, Block> > blocks;
};

bool IsGLSLExt(const String& ext);

void RegisterPCHFile(const String& pch_file);
void DeletePCHFiles();

String GetLineEndings(const String& data, const String& default_eol = "\r\n");

int    HostSys(const char *cmd, String& r); // like Sys, but with current method paths added (and also internal paths in win32)
String HostSys(const char *cmd);

enum { NOT_REPO_DIR = 0, SVN_DIR, GIT_DIR };

int    GetRepoKind(const String& p);
int    GetRepo(String& path);
String GetGitPath();

enum {
	ITEM_TEXT,
	ITEM_NAME,
	ITEM_OPERATOR,
	ITEM_CPP_TYPE,
	ITEM_CPP,
	ITEM_PNAME,
	ITEM_TNAME,
	ITEM_NUMBER,
	ITEM_SIGN,
	ITEM_UPP,
	ITEM_TYPE,
	
	ITEM_PTYPE = ITEM_TYPE + 10000,
};

struct ItemTextPart : Moveable<ItemTextPart> {
	int pos;
	int len;
	int type;
	int ii;
	int pari;
};

String CleanupId(const char *s);
String CleanupPretty(const String& signature);

Vector<ItemTextPart> ParsePretty(const String& name, const String& signature, int *fn_info = NULL);

#endif