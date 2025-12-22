#ifndef testH
#define testH
//-------------------------------------------------------------------------------------------

#include "debugsystem.h"

#include "ui.h"

//-------------------------------------------------------------------------------------------

struct TIniItem
{
	std::string name;
	std::string comment;
};
typedef std::list< TIniItem > TIniList;
typedef TIniList::iterator    TIniListIt;

//---------------------------------------

struct TModuleItem
{
	std::string name;
	std::string fullname;
};
typedef std::list< TModuleItem > TModuleList;
typedef TModuleList::iterator    TModuleListIt;

//---------------------------------------

struct TScanTest
{
	std::string name;
	std::string module;
	std::string param;
};
typedef std::vector< TScanTest > TScanTests;
typedef TScanTests::iterator     TScanTestsIt;

//---------------------------------------

struct TTestItem
{
	TModuleListIt module;
	std::string   name;
	std::string   param;
	TUIButton*    button;
	int           complexnum;
	int           stressnum;
	int           cycles;
	TTestItem( TModuleListIt module, const std::string& name, const std::string& param )
		: module(module),name(name),param(param)
		{ button=NULL; cycles=0; complexnum=0; stressnum=0; }
};
typedef std::list< TTestItem > TTestList;
typedef TTestList::iterator    TTestListIt;

//---------------------------------------

typedef enum { TOTAL_OK, TOTAL_ERR, TOTAL_STOP, TOTAL_NULL } TTotal;

struct TSubDevice
{
	std::string name;
	Tstrlist    masks;
	TTotal      total;
};
typedef std::vector< TSubDevice > TSubDevices;

struct TScannerItem
{
	std::string name;
	bool        result;
	std::string info;
	TSubDevice* subdevice;
};
typedef std::vector< TScannerItem > TScannerItems;

struct TComplexItem
{
	TTestListIt test;
	int         ok;
	int         err;
	int         war;
	int         stop;
	int         null;

	Tstrlist    replace;
	Tstrlist    cycle_error;
	Tstrlist    cycle_warning;
	float       total_error;
	float       total_warning;
	bool        last_result_error;
	bool        last_result_warning;

	TTotal      total;
	std::string info;
	TSubDevice* subdevice;

	TComplexItem( TTestListIt test, TSubDevice* subdevice );
};
typedef std::list< TComplexItem > TComplexItems;

//---------------------------------------

struct TExecute
{
	enum TExecuteType { EXIT, DONE, SLEEP, MODULE, TEST, COMMAND, CHECK } type;
	int  cycle;
	union
	{
		TModuleItem* module;
		TTestItem*   test;
		char         str[256];
	};
	TExecute( ) { }
	TExecute( TExecuteType type ) : type(type) { }
	TExecute( TExecuteType type, TModuleItem* module ) : type(type),module(module) { }
	TExecute( TExecuteType type, int cycle, TTestItem* test ) : type(type),cycle(cycle),test(test) { }
	TExecute( TExecuteType type, std::string str ) : type(type) { strncpy( this->str, str.c_str(), sizeof(this->str)-1 ); }
};

//-------------------------------------------------------------------------------------------

class TTest
{
	private:

	protected:
		std::string   lastini;

		int           exepipe[ 2 ];
		pthread_t     ExecuteThread;
		static void   *ExecuteThreadFunction( void* arg );

		pid_t         childpid;

		enum { COMPLEX, CUSTOM } testtype;
		int           cycle;
		int           cyclescount;
		bool          stop;
		TTestListIt   customtest;
		int           testlogspace;

		void          SearchModules( );
		bool          Scanner( );

		bool          ModuleExist( std::string name );

		bool          PushExecute( TExecute::TExecuteType type );
		bool          PushExecute( TExecute execute );

		TSubDevices   SubDevices;
		TScannerItems ScannerItems;
		TComplexItems ComplexItems;
		std::string   ComplexSensorMasks;

		float         GetOKPercent( std::string name, std::string mode );
		void          OnCycleDone( );

	public:
		TTest( );
		~TTest( );

		FILE*         OutStream;

		TIniList      IniList;
		TModuleList   ModuleList;
		TScanTests    ScanTests;
		TTestList     TestList;

		std::string   moddir;
		std::string   inidir;

		TIniListIt    iniitem;
        TConf*      ini;
		std::string   ininame;

		void          SearchInis( );

		void          TestButtonsVisible( );

		bool          SetLastIni( std::string inifile );
		std::string   GetLastIni( );
		bool          LoadIni( std::string inifile );

		TModuleListIt GetModuleItemByName( std::string name );
		TIniListIt    GetIniItemByName( std::string name );
		TTestListIt   GetTestItemByName( std::string name );
		TModuleItem*  GetModuleByTestname( std::string testname );
		TComplexItem* GetComplexItemByName( std::string name );
		int           GetTestCountByModule( TModuleListIt module );
		void          ClearTestsByModule( TModuleListIt module );
		bool          ComplexItemAddResult( std::string name, std::string result );

		void          InitTests( );
		void          InfoTest( std::string testname );
		void          InfoTests( );

		void          StartComplex( int count );
		void          StartCustom( std::string& name, int count );
		void          Stop( );
		void          Kill( );
		void          Update( );

		bool          working;

		std::string   scanner_name;
		std::string   scanner_run;
		std::string   scanner_conf;
};
//-------------------------------------------------------------------------------------------

extern TTest* Test;

//-------------------------------------------------------------------------------------------
#endif
