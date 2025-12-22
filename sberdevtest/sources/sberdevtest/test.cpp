#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#include <string>
#include <list>
#include <iterator>
#include <algorithm>
#include <fstream>

#include "debugsystem.h"

#include "main.h"
#include "logpanel.h"
#include "menupanel.h"
#include "dialogpanel.h"
#include "sensorpanel.h"
#include "hellopanel.h"
#include "tcpclient.h"

#include "test.h"

using namespace std;

//-------------------------------------------------------------------------------------------

TTest* Test;

#undef  UNIT
#define UNIT  g_DebugSystem.units[ TEST_UNIT ]
#include "log_macros.def"

//-------------------------------------------------------------------------------------------

bool CompareTestItemByName   ( const TTestItem& t1, const TTestItem& t2 )
{
	if( t1.name == "STRESS" ) return true;
	if( t2.name == "STRESS" ) return false;
	return ( t1.name < t2.name );
}
bool CompareTestItemByComplex( const TTestItem& t1, const TTestItem& t2 ) { return ( t1.complexnum < t2.complexnum ); }
bool CompareTestItemByStress ( const TTestItem& t1, const TTestItem& t2 ) { return ( t1.stressnum < t2.stressnum ); }
bool CompareComplexItemByName( const TComplexItem& t1, const TComplexItem& t2 ) { return ( t1.test->name < t2.test->name ); }

//-------------------------------------------------------------------------------------------

TTest::TTest( )
{FUNCTION_TRACE

    moddir  = g_DebugSystem.fullpath( g_DebugSystem.conf->ReadString( "Main", "modulesdir" ) );
    inidir  = g_DebugSystem.fullpath( g_DebugSystem.conf->ReadString( "Main", "inidir" ) );
	lastini = inidir + "/last";
	testlogspace = g_DebugSystem.conf->ReadInt( "Main", "testlogspace" );

	iniitem = IniList.end( );
	ini = NULL;
	working = false;
	stop = false;

	Debug( "moddir = %s\n", moddir.c_str() );
	Debug( "inidir = %s\n", inidir.c_str() );
	Debug( "lastini = %s\n", lastini.c_str() );

	SearchInis( );

	OutStream = NULL;

	if( pipe( exepipe ) != 0 ) Error( "create exepipe\n" );
	if( pthread_create( &ExecuteThread,  NULL, TTest::ExecuteThreadFunction, this ) != 0 ) Error( "pthread_create TTest::ExecuteThreadFunction\n" );
}
//-------------------------------------------------------------------------------------------

TTest::~TTest( )
{FUNCTION_TRACE
	Stop( );
	PushExecute( TExecute::EXIT );
	pthread_join( ExecuteThread,  NULL );
	if( exepipe[ 0 ] ) close( exepipe[ 0 ] );
	if( exepipe[ 1 ] ) close( exepipe[ 1 ] );

	ModuleList.clear( );
	IniList.clear( );
	TestList.clear( );
}
//-------------------------------------------------------------------------------------------

bool compareIniItem(const TIniItem &a, const TIniItem &b) { return ( a.name < b.name ); }

void TTest::SearchInis( )
{FUNCTION_TRACE
	IniList.clear( );

	DIR* dir;
	struct dirent* entry;
	if( ( dir = opendir( inidir.c_str() ) ) != NULL )
	while( ( entry = readdir( dir ) ) != NULL )
	{
		if( strcmp( entry->d_name, "." ) == 0 || strcmp( entry->d_name, ".." ) == 0 ) continue;
		char* c = strrchr( entry->d_name, '.' );  if( !c ) continue;
		if( strcmp( c, ".ini" ) != 0 ) continue;
		if( *(c+4) != 0 ) continue;

		string name = entry->d_name;
		if( !LoadIni( name ) ) continue;

        TIniItem item = { name, ini->ReadString( "comment","0" ) };
		IniList.push_back( item );
	}
	closedir(dir);
	IniList.sort( &compareIniItem );
}
//-------------------------------------------------------------------------------------------

bool compareModuleItem(const TModuleItem &a, const TModuleItem &b) { return ( a.name < b.name ); }

void TTest::SearchModules( )
{FUNCTION_TRACE
	ModuleList.clear( );

	DIR* dir;
	struct dirent* entry;
	struct stat sb;
	if( ( dir = opendir( moddir.c_str() ) ) != NULL )
	while( ( entry = readdir( dir ) ) != NULL )
	{
		if( strcmp( entry->d_name, "." ) == 0 || strcmp( entry->d_name, ".." ) == 0 ) continue;
		if( entry->d_type == DT_DIR ) continue;
		string fullname = moddir + "/" + entry->d_name;
		bool executable = ( stat( fullname.c_str(), &sb ) == 0 && sb.st_mode & S_IXUSR );
		if( !executable ) continue;
		if( g_DebugSystem.exe == entry->d_name ) continue;

		TModuleItem item = { entry->d_name, fullname };
		ModuleList.push_back( item );
	}
	closedir(dir);
	ModuleList.sort( &compareModuleItem );
}
//-------------------------------------------------------------------------------------------

bool TTest::Scanner( )
{FUNCTION_TRACE
	string scanner_check;
	int id = 0;
	while( 1 )
	{
        string checkname = ini->GetParamNameById( "check", id++ );
		if( checkname == "" ) break;
        string checkparam = ini->ReadString( "check", checkname );
		if( checkname == stringformat( "%d", str2int(checkname) ) ) { checkname = checkparam; checkparam = ""; }
		scanner_check += "$" + checkname + "=" + checkparam + " ";
	}

	string run = scanner_run + " conf=" + scanner_conf + " 'check=" + scanner_check + "'";
	toLog( "----------------------------------------------------------\n" );
	toLog( "scanner: " CLR(f_GREEN) "%s" CLR0 " %s [%s]\n", scanner_name.c_str(), getfile(scanner_run).c_str(), getfile(scanner_conf).c_str() );

	if( !fileexist( scanner_run ) )
	{
		toLog( CLR(f_RED) "file not found '%s'" CLR0 "\n", scanner_run.c_str() );
		return false;
	}

	TModuleItem item = { getfile(scanner_run), scanner_run };
	ModuleList.push_back( item );

	FILE *instream, *outstream;
	pid_t pid = RunProcessIO( run.c_str(), instream, outstream );
	char buf[ 1024 ];
	while( instream && !feof( instream ) )
	{
		if( !fgets( buf, sizeof(buf), instream ) ) break;
		if( strncmp( buf, "T|", 2 ) == 0 )
		{
			string str = trim( buf+2 );  // name | module | params
			Tstrlist sl; stringsplit( str, '|', sl );
			if( sl.size() < 2 ) { toLog( CLR(f_RED) "incorrect '%s'" CLR0 "\n", trim(buf).c_str() ); continue; }
			string name   = sl[0];
			string module = sl[1];
			string param  = ( sl.size() >= 3 ) ? sl[2] : "";
			TModuleListIt moduleit = GetModuleItemByName( module );
			if( moduleit == ModuleList.end() ) { toLog( CLR(f_RED) "module not found '%s'" CLR0 "\n", module.c_str() ); continue; }
            string iniparams = ini->ReadString( module, "params" );
			TestList.push_back( TTestItem( moduleit, name, param + " " + iniparams ) );
		}
		else if( strncmp( buf, "C|", 2 ) == 0 )
		{
			string _CH, _DEV, _OK, _ERR, _BAD;
            FOR_EACH_TOKEN( trim( buf+2 ), '$', chlist, ch )
			{
				string key, value;
				if( !stringkeyvalue( *ch, '=', key, value ) ) continue;
				if( key == "CH"  ) _CH  = value; else
				if( key == "DEV" ) _DEV = value; else
				if( key == "OK"  ) _OK  = value; else
				if( key == "ERR" ) _ERR = value; else
				if( key == "BAD" ) _BAD = value;
			}

			TScannerItem si;
			si.name = _CH;
			si.result = false;
			si.info = "";
			si.subdevice = NULL;
            FOR_EACH_ITER( TSubDevices, SubDevices, sub )
            {
                FOR_EACH_ITER( Tstrlist, sub->masks, m )
                {
					if( maskmatch( si.name, *m ) ) si.subdevice = &(*sub);
                }
            }

			if( _BAD != "" ) si.info = _BAD + ": incorrect check params"; else
			if( _ERR != "" ) si.info = _ERR.c_str(); else
			{ si.result = true; si.info = _OK; }
			ScannerItems.push_back( si );
		}
		else toLog( "%s", buf );
	}
	WaitProcessIO( pid, instream, outstream );
	toLog( "----------------------------------------------------------\n" );

	return true;
}
//-------------------------------------------------------------------------------------------

void TTest::InitTests( )
{FUNCTION_TRACE
	if( !ini ) return;

	RunEventScript( "ini" );

    toLog( "\n%s\n───────────────────────────────────────────────────────────────────────────\n", g_DebugSystem.conf->ReadString( "Main", "title" ).c_str() );

	string on_load_cmd, on_load_timeout;
    stringkeyvalue( ini->ReadString( "cfg", "on_load" ), '|', on_load_cmd, on_load_timeout );
	if( on_load_cmd != "" )
	{
		string data;
		int result = GetProcessDataTimeout( g_DebugSystem.fullpath( on_load_cmd ), data, time2seconds( on_load_timeout )*1000 );
		if( result != 0 ) toLog( "%s\n" CLR(f_RED) "ERROR: %s" CLR0 "\n", on_load_cmd.c_str(), data.c_str() );
	}

	toLog( "----------------------------------------------------------\n" );
    toLog( "Серийный номер:\n  %s\n", HelloPanel->m_selectionPanel->m_serialNumber.c_str() );
	toLog( "----------------------------------------------------------\n" );
	toLog( "Конфигурационный файл: %s\n", ininame.c_str() );

    bool txtexist = !ini->GetParamNameById( "text", 0 ).empty();
	if( txtexist ) toLog( "\n" );
	for( int i = 0; ; i++ )
	{
        string n = ini->GetParamNameById( "text", i );
		if( n == "" ) break;
        toLog( ini->ReadString( "text", n ) + "\n" );
	}
	if( txtexist ) toLog( "\n" );

	for( int i = 0; ; i++ )
	{
        string ethname = ini->GetParamNameById( "setip", i );
		if( ethname == "" ) break;
        string ip = ini->ReadString( "setip", ethname );
        TnetIF If( ethname );
		if( !If.inited )
			toLog( CLR(f_RED)"setip: %s not found" CLR0 "\n", ethname.c_str() );
		else if( !If.updown( true ) )
			toLog( CLR(f_RED)"setip: %s UP ERROR" CLR0 "\n", ethname.c_str() );
		else if( !If.setaddr( ip ) )
			toLog( CLR(f_RED)"setip: %s %s ERROR" CLR0 "\n", ethname.c_str(), ip.c_str() );
		else
			toLog( CLR(f_GREEN)"setip: %s %s" CLR0 "\n", ethname.c_str(), ip.c_str() );
	}


    MenuPanel->m_scannerBtn->settext( "" );
    scanner_name = ini->ReadString( "scanner", "name" );
    scanner_run  = g_DebugSystem.fullpath( ini->ReadString( "scanner", "run" ) );
    scanner_conf = ini->ReadString( "scanner", "conf" );

	for( TTestListIt it = TestList.begin(); it != TestList.end(); ++it )  if( it->button ) delete it->button;
	TestList.clear( );

	SearchModules( );

	//--- SubDevices
	SubDevices.clear();
	int id = 0;
	while( 1 )
	{
        string name = ini->GetParamNameById( "device", id++ );
		if( name == "" ) break;
		TSubDevice subdevice;
		subdevice.name = name;
        stringsplit( ini->ReadString( "device", name ), ' ', subdevice.masks, true );
		SubDevices.push_back( subdevice );
	}
	if( SubDevices.empty() )
	{
		TSubDevice subdevice;
		subdevice.name = "";
		subdevice.masks.push_back( "*" );
		SubDevices.push_back( subdevice );
	}


	ScannerItems.clear();
	if( ( scanner_name != "" )&&( scanner_run != "" ) )
	{
        if( Scanner( ) ) MenuPanel->m_scannerBtn->settext( scanner_name );
	}

	for( TModuleListIt it = ModuleList.begin(); it != ModuleList.end(); ++it )
	{
		string& modname = it->name;

		if( modname == "stress" ) continue;

		string mod_count = modname + "_count";
		string mod_test = modname + "_test";
        string modcountstr = stringlower( ini->ReadString( modname, mod_count ) );
        string modteststr  = stringlower( ini->ReadString( modname, mod_test ) );

		if( ( modcountstr == "n" )||( modteststr == "n" ) )
		{
			ClearTestsByModule( it );
			continue;
		}

        string iniparams = ini->ReadString( modname, "params" );

		//--- тесты в ручную
		int id = 0;
		while( 1 )
		{
            string testname = ini->GetParamNameById( modname, id++ );
			if( testname == "" ) break;
			if( ( testname == mod_count )||( testname == mod_test ) ) continue;
			if( ( testname == "params" )||( testname == "LANserver" ) ) continue;
			if( testname.find( "subnetip" ) != std::string::npos ) continue;

            string params = ini->ReadString( modname, testname ) + " " + iniparams;
			TestList.push_back( TTestItem( it, testname, trim(params) ) );
		}

		//--- если указано количество
		if( ( modcountstr != "" )&&( modcountstr != "auto" ) )
		{
			int testcount = GetTestCountByModule( it );
			int inicount = str2int( modcountstr );
			if( inicount != testcount ) toLog( CLR(f_RED) "WARNING [%s]: указано-%d, найдено-%d" CLR0 "\n", modname.c_str(), inicount, testcount );
		}
	}

	//--- stress
	int stressnum = 0;
	Tstrlist stressmasks;
    stringsplit( ini->ReadString( "stress", "tests" ), ' ', stressmasks );
    FOR_EACH_ITER( Tstrlist, stressmasks, mask )
    {
        FOR_EACH_ITER( TTestList, TestList, t )
		{
			if( t->stressnum > 0 ) continue;
			if( maskmatch( t->name, *mask ) ) t->stressnum = ( ++stressnum );
		}
    }
	string stressstr;
	TestList.sort( CompareTestItemByStress );
    FOR_EACH_ITER( TTestList, TestList, t )
    {
		if( t->stressnum > 0 ) stressstr += t->name + " ";
    }

	//--- create stress.test
	if( stressstr != "" )
	{
		FILE* fp = NULL;
		string filename = moddir + "/stress.test";
		try
		{
			if( !(fp = fopen( filename.c_str(), "w" )) ) throw errException( "fopen '%s'", filename.c_str() );
			if( fchmod( fileno(fp), 0666 ) == -1 ) throw errException( "fchmod '%s'", filename.c_str() );

			fprintf( fp, "[RUN]\n" );
            FOR_EACH_ITER( TTestList, TestList, t )
            {
				if( t->stressnum > 0 )
					fprintf( fp, "%s = %s %s\n", t->name.c_str(), t->module->fullname.c_str(), t->param.c_str() );
            }
			fprintf( fp, "\n[SENSORS]\n" );
            fprintf( fp, "%s\n", ini->ReadString( "stress", "sensors" ).c_str() );

			xfclose( fp );
            TestList.push_back( TTestItem( GetModuleItemByName("stress"), "STRESS", ini->ReadString( "stress", "params" ) ) );
		}
		catch( errException& e )
		{
			Error( "%s\n", e.error() );
			xfclose( fp );
		}
	}

	//--- complex
	int complexnum = 0;
	Tstrlist complexmasks;
    stringsplit( ini->ReadString( "complex", "tests" ), ' ', complexmasks );
    FOR_EACH_ITER( Tstrlist, complexmasks, mask )
    {
        FOR_EACH_ITER( TTestList, TestList, t )
		{
			if( t->complexnum > 0 ) continue;
			if( maskmatch( t->name, *mask ) ) t->complexnum = ( ++complexnum );
		}
    }
	string complexstr;
	TestList.sort( CompareTestItemByComplex );
    FOR_EACH_ITER( TTestList, TestList, t )
    {
		if( t->complexnum > 0 ) complexstr += t->name + " ";
    }
    ComplexSensorMasks = ini->ReadString( "complex", "sensors" );


	//--- buttons
	TestList.sort( CompareTestItemByName );
	uint y = 0;
    FOR_EACH_ITER( TTestList, TestList, t )
	{
        t->button = new TUIButton( MenuPanel, "TestBtn", 1, 7+y,19,1, -MenuPanel->color,UIGREEN, " "+t->name,UIL, &OnTestButtonClick );
		t->button->visible = false;
		y++;
	}

	//--- создание списка тестов в комплексном тесте
	ComplexItems.clear();
	bool stressincomplex = false;
    FOR_EACH_ITER( TTestList, TestList, t )
    {
		if( ( t->complexnum > 0 ) && ( t->name == "STRESS" ) ) { stressincomplex = true; break; }
    }

    FOR_EACH_ITER( TTestList, TestList, t )
	{
		if( t->name == "STRESS" ) continue;
		if( t->complexnum > 0 || ( stressincomplex && ( t->stressnum > 0 ) ) )
		{
			TSubDevice* subdevice = NULL;
            FOR_EACH_ITER( TSubDevices, SubDevices, sub )
                FOR_EACH_ITER( Tstrlist, sub->masks, m )
					if( maskmatch( t->name, *m ) ) subdevice = &(*sub);
			ComplexItems.push_back( TComplexItem( t, subdevice ) );
		}
	}
	ComplexItems.sort( CompareComplexItemByName );


	toLog( CLR(f_CYAN) "complex:" CLR0 " %s\n", complexstr.c_str() );
	toLog( CLR(f_CYAN) "stress :" CLR0 " %s\n", stressstr.c_str() );

	toLog( "───────────────────────────────────────────────────────────────────────────\n\n" );

    FOR_EACH_ITER( TSubDevices, SubDevices, sub )
	{
		std::string text = "#SUB# " + sub->name;
        FOR_EACH_ITER( TScannerItems, ScannerItems, si )
        {
			if( si->subdevice == &(*sub) ) text += " " + si->name;
        }
        FOR_EACH_ITER( TComplexItems, ComplexItems, ci )
        {
			if( ci->subdevice == &(*sub) ) text += " " + ci->test->name;
        }
	}

    FOR_EACH_ITER( TScannerItems, ScannerItems, si )
	{
		toLog( "%s   %-*s " CLR("%s") "%s" CLR0 " %s\n", nowtime(':').c_str(), testlogspace, si->name.c_str(), si->result ? f_GREEN : f_RED, si->result ? "OK" : "ERR", si->info.c_str() );
		if( !si->result ) RunEventScript( "testerror" );
	}

}
//-------------------------------------------------------------------------------------------

void TTest::TestButtonsVisible( )
{FUNCTION_TRACE
	for( TTestListIt it = TestList.begin(); it != TestList.end(); ++it )
		if( it->button ) it->button->visible = true;
}
//-------------------------------------------------------------------------------------------

bool TTest::SetLastIni( std::string inifile )
{FUNCTION_TRACE
	bool result = false;
	std::ofstream ofs( lastini.c_str() );
	if( ofs.is_open() )
	{
		ofs << inifile;
		ofs.close( );
		result = true;
	}
	if( !result ) Error( "open %s\n", lastini.c_str() );
	return result;
}
//-------------------------------------------------------------------------------------------

std::string TTest::GetLastIni( )
{FUNCTION_TRACE
	return readfilestr( lastini );
}
//-------------------------------------------------------------------------------------------

bool TTest::LoadIni( std::string inifile )
{FUNCTION_TRACE
	if( ini ) { delete ini; ini = NULL; }
    ini = new TConf( );
	string inifull = inidir + "/" + inifile;
    if( ini->LoadFile( inifull.c_str() ) != fOK )
	{
		Error( "%s\n", inifull.c_str() );
		delete ini;
		ini = NULL;
		iniitem = IniList.end( );
		return false;
	}
	ininame = inifile;
	iniitem = GetIniItemByName( inifile );
	return true;
}
//-------------------------------------------------------------------------------------------

TModuleListIt TTest::GetModuleItemByName( std::string name )
{FUNCTION_TRACE
	for( TModuleListIt it = ModuleList.begin(); it != ModuleList.end(); ++it )
		if( it->name == name ) return it;
	return ModuleList.end();
}
//-------------------------------------------------------------------------------------------

TIniListIt TTest::GetIniItemByName( std::string name )
{FUNCTION_TRACE
	for( TIniListIt it = IniList.begin(); it != IniList.end(); ++it )
		if( it->name == name ) return it;
	return IniList.end();
}
//-------------------------------------------------------------------------------------------

TTestListIt TTest::GetTestItemByName( std::string name )
{FUNCTION_TRACE
	for( TTestListIt it = TestList.begin(); it != TestList.end(); ++it )
		if( it->name == name ) return it;
	return TestList.end();
}
//-------------------------------------------------------------------------------------------

int TTest::GetTestCountByModule( TModuleListIt module )
{FUNCTION_TRACE
	int count = 0;
	for( TTestListIt it = TestList.begin(); it != TestList.end(); ++it )
		if( it->module == module ) count++;
	return count;
}
//-------------------------------------------------------------------------------------------

void TTest::ClearTestsByModule( TModuleListIt module )
{FUNCTION_TRACE
	TTestListIt it = TestList.begin();
	while( it != TestList.end() )
		if( it->module == module )
		{
			if( it->button ) delete it->button;
			it = TestList.erase( it );
		}
		else it++;
}
//-------------------------------------------------------------------------------------------

TModuleItem* TTest::GetModuleByTestname( std::string testname )
{
	TTestListIt testitem = GetTestItemByName( testname );
	if( testitem == TestList.end() ) return NULL;
	return &(*testitem->module);
}
//-------------------------------------------------------------------------------------------


TComplexItem::TComplexItem( TTestListIt test, TSubDevice* subdevice ) : test(test),subdevice(subdevice)
{
	ok = err = war = stop = null = 0;
	total = TOTAL_NULL;
	total_error = total_warning = 0.0;
	last_result_error = false;
	last_result_warning = false;

	string paramstr;
	for( int i = 0; ; i++ )
	{
        string testnamemask = Test->ini->GetParamNameById( "complex", i );
		if( testnamemask == "" ) break;
		if( !maskmatch( test->name, testnamemask ) ) continue;
        paramstr = Test->ini->ReadString( "complex", testnamemask );
	}

	Tstrlist params; stringsplit( paramstr, ' ', params, true );
    FOR_EACH_ITER( Tstrlist, params, p )
	{
		string key,value; stringkeyvalue( *p, '=', key, value );
		if( key == "replace"       ) stringsplit( value, '|', replace );
		if( key == "cycle_error"   ) stringsplit( value, '|', cycle_error );
		if( key == "cycle_warning" ) stringsplit( value, '|', cycle_warning );
		if( key == "total_error"   ) total_error   = str2float( value ) / 100.0;
		if( key == "total_warning" ) total_warning = str2float( value ) / 100.0;
	}
}
//-------------------------------------------------------------------------------------------

TComplexItem* TTest::GetComplexItemByName( std::string name )
{
    FOR_EACH_ITER( TComplexItems, ComplexItems, t )
    {
		if( t->test->name == name ) return &(*t);
    }
	return NULL;
}
//-------------------------------------------------------------------------------------------

bool TTest::ComplexItemAddResult( std::string name, std::string result )
{FUNCTION_TRACE
	TComplexItem* item = GetComplexItemByName( name );
	if( !item ) return false;

	item->last_result_error = false;
	item->last_result_warning = false;

	Tstrlist words;
	stringsplit( result, ' ', words, true );
	if( words.size() < 1 ) return false;

	bool foundsums = false;
	int oksum=0, warsum=0, errsum=0;
	for( uint i = 1; i < words.size(); i++ )
		if( sscanf( words[i].c_str(), "@%d/%d/%d", &oksum, &warsum, &errsum ) == 3 )
		{
			foundsums = true;
			break;
		}
	if( foundsums )
	{
		item->ok  += oksum;
		item->err += errsum;
		item->war += warsum;
		if( errsum > 0 ) item->last_result_error = true;
		if( warsum > 0 ) item->last_result_warning = true;
	}
	else
	{
		string testresult = words[0];
		if( testresult == "OK"      ) { item->ok++; }
		if( testresult == "WARNING" ) { item->war++; item->last_result_warning = true; }
		if( testresult == "ERR"     ) { item->err++; item->last_result_error = true; }
		if( testresult == "no"      ) { item->err++; item->last_result_error = true; }
		if( testresult == "stop"    ) { item->stop++; }
	}
	return true;
}
//-------------------------------------------------------------------------------------------

void TTest::InfoTest( std::string /*testname*/ )
{FUNCTION_TRACE

}
//-------------------------------------------------------------------------------------------

void TTest::InfoTests( )
{FUNCTION_TRACE
	TestList.sort( CompareTestItemByName );
	toLog( "\n" );
	for( TTestListIt it = TestList.begin(); it != TestList.end(); ++it )
		toLog( CLR(f_GREEN) "%s" CLR0 " " CLR(f_CYAN) "%s" CLR0 " %s\n", it->name.c_str(), it->module->name.c_str(), it->param.c_str() );
	toLog( "\n" );
}
//-------------------------------------------------------------------------------------------

bool TTest::ModuleExist( std::string name )
{FUNCTION_TRACE
	return ( GetModuleItemByName( name ) != ModuleList.end() );
}
//-------------------------------------------------------------------------------------------

bool TTest::PushExecute( TExecute::TExecuteType type )
{
	return PushExecute( TExecute( type ) );
}

bool TTest::PushExecute( TExecute execute )
{FUNCTION_TRACE
	bool result = WritePipe( exepipe, &execute, sizeof(TExecute) );
	if( !result ) Error( "WritePipe exepipe\n" );
	return result;
}
//-------------------------------------------------------------------------------------------






void OnPressTestDialogBtn( TUIButton* btn )
{FUNCTION_TRACE
	string answer = trim(btn->text) + "\n";
	if( Test && Test->OutStream )
		if( fputs( answer.c_str(), Test->OutStream ) < 0 ) Error( "fputs Test->OutStream\n" );
    DialogPanel->Close( );
}
//-------------------------------------------------------------------------------------------

bool OnProcessInStream( char* str, void* param )
{FUNCTION_TRACE
	Trace( "<: %s", str );
	TExecute::TExecuteType type = (TExecute::TExecuteType)(long)param;
	if( type == TExecute::MODULE )
	{
		toLog( string(str) );
		return true;
	}
	else
	if( type == TExecute::TEST   )
	{
		char* s = NULL;
		if( ( s = strstr( str, "<UI>") ) != NULL ) toLog( string(s) );
        if( ( s = strstr( str, "<DIALOG>") ) != NULL ) DialogPanel->ShowMessage( UIBLUE, string(s+8), &OnPressTestDialogBtn, true );
        if( ( s = strstr( str, "<DIALOGCLOSE>") ) != NULL ) DialogPanel->Close( );
		if( ( s = strstr( str, "<IN>") ) != NULL )
			if( Test && Test->OutStream )
				if( fputs( s+4, Test->OutStream ) < 0 ) Error( "fputs Test->OutStream\n" );

		if( ( s = strstr( str, "<LOG>") ) != NULL ) toLog( s+5 );
		if( ( s = strstr( str, "<ERR>") ) != NULL ) toLog( "%s " CLR(f_RED) "ERR" CLR0 " %s", nowtime( ).c_str(), s+5 );
		if( ( s = strstr( str, "<WAR>") ) != NULL ) toLog( "%s " CLR(f_YELLOW) "WARNING" CLR0 " %s", nowtime( ).c_str(), s+5 );
		if( ( s = strstr( str, "<BUG>") ) != NULL ) toLog( CLR(f_RED) "%s %s" CLR0, nowtime( ).c_str(), str );

		if( strstr( str, "TEST" ) ) return false;
		if( strstr( str, "Connection timed out" ) ) { strcpy( str, "TEST ERR ssh connection timed out\n" ); return false; }
		if( strstr( str, "not responding" ) ) { strcpy( str, "TEST ERR ssh connection not responding\n" ); return false; }

		if( ( s = strstr( str, "<STRESS>") ) != NULL )
		{
			toLog( s+8 );
			string result = removecolor( string( s+8 ) );
			Tstrlist words; stringsplit( result, ' ', words, true );
			if( words.size() < 1 ) return true;
			result = ""; for( uint i = 1; i < words.size(); i++ ) result += words[i] + " ";
			Test->ComplexItemAddResult( words[0], result );
		}
		return true;
	}
	else
	if( type == TExecute::COMMAND )
	{
		toLog( string(str) );
		return true;
	}
	else
	return true;
}
//-------------------------------------------------------------------------------------------

void *TTest::ExecuteThreadFunction( void* arg )
{DTHRD("{test}")

	TTest* T = (TTest*)arg;

	TExecute execute;
	int cyclepause = g_DebugSystem.conf->ReadInt( "Main", "cyclepause" );

	while( 1 )
	{
		int result = ReadPipe( T->exepipe, &execute, sizeof(TExecute), 10000000 ); // >0-datasize, =0-timeout, <0-error
		if( result < 0 ) { Error( "ReadPipe exepipe [%d]\n", result ); break; }
		if( result == 0 ) continue;
		if( execute.type == TExecute::EXIT ) break;

		if( execute.type == TExecute::DONE ) { toLog( "#PROGRESS-OFF#" ); UI->DoLock( TEST_UPDATE ); continue; }

		if( execute.type == TExecute::SLEEP ) { if( ( T->cycle < T->cyclescount )&&( cyclepause > 0 ) ) Usleep( cyclepause*1000 ); continue; }

		if( execute.type == TExecute::MODULE )
		{
			string exestr = execute.module->fullname + " -i";
			ExecuteProcessIO( T->childpid, exestr.c_str(), T->OutStream, &OnProcessInStream, (void*)(long)(TExecute::MODULE) );
			continue;
		}

		if( execute.type == TExecute::TEST )
		{
			toLog( "%s \033[%dm%d\033[m %-*s ", nowtime(':').c_str(), -UIGREEN, execute.cycle, T->testlogspace, execute.test->name.c_str() );
			toLog( "#PROGRESS-ON#" );

			string exestr = execute.test->module->fullname + " " + execute.test->param;
			Log( "run: %s " CLRCYN("%s") "\n", execute.test->name.c_str(), exestr.c_str() );

			char resultstr[ 256 ];  memset( resultstr, 0, sizeof(resultstr) );

			if( T->stop ) continue;

			int status = ExecuteProcessIO( T->childpid, exestr.c_str(), T->OutStream, &OnProcessInStream, (void*)(long)(TExecute::TEST), resultstr );

            DialogPanel->Close( );
			toLog( "#PROGRESS-OFF#" );

			TComplexItem* complexitem = T->GetComplexItemByName( execute.test->name );

			if( WIFEXITED(status) )
			{

				bool error_event = false;

				char* TEST = strstr( resultstr, "TEST" );
				if( !TEST )
				{
					toLog( CLRRED( "no result" ) "\n" );
					if( complexitem ) complexitem->err++;
					error_event = true;
				}
				else
				{
					string result = removecolor( trim(string( TEST+5 )) );
					Tstrlist words; stringsplit( result, ' ', words, true );
					if( words.size() == 0 )
					{
						toLog( CLRRED( "no result" ) "\n" );
						if( complexitem ) complexitem->err++;
						error_event = true;
					}
					else
					{
						string& result = words[ 0 ];
						string info; for( uint i = 1; i < words.size(); i++ ) info += words[i] + " "; info = trim(info);


						if( complexitem && complexitem->replace.size() >= 2 )
							if( result == complexitem->replace[ 0 ] )
							{
								toLog( "replace %s -> %s\n", result.c_str(), complexitem->replace[ 1 ].c_str() );
								result = complexitem->replace[ 1 ];
							}

						if( result == "OK" )
						{
							toLog( CLRGRN( "OK" ) " %s\n", info.c_str() );
						}
						else
						if( result == "WARNING" )
						{
							toLog( CLRYLW( "WARNING" ) " %s\n", info.c_str() );
						}
						else
						if( result == "ERR" )
						{
							toLog( CLRRED( "ERR" ) " %s\n", info.c_str() );
							error_event = true;
						}
						else
						{
							toLog( CLRRED( "no result" ) " %s\n", info.c_str() );
							if( complexitem ) complexitem->err++;
							error_event = true;
						}
					}
					if( complexitem ) Test->ComplexItemAddResult( execute.test->name, result );

					execute.test->cycles++;

					if( error_event ) RunEventScript( "testerror" );
				}
			}
			else if( WIFSIGNALED(status) )
			{
				if( ( WTERMSIG(status) == SIGTERM )||( WTERMSIG(status) == SIGKILL ) ) toLog( CLR(f_RED) "stop" CLR0 "\n" );
				else toLog( CLR(f_RED) "stop" CLR0 " %d\n", status );

				if( complexitem ) complexitem->stop++;
				RunEventScript( "teststop" );
			}
			else if( WIFSTOPPED(status) )
			{
				toLog( CLR(f_RED) "stopped" CLR0 " %d\n", status ); // WSTOPSIG(status)
				if( complexitem ) complexitem->stop++;
				RunEventScript( "teststop" );
			}
			else
			{
				toLog( CLR(f_RED) "status=%d" CLR0 "\n", status );
				if( complexitem ) complexitem->null++;
			}


			continue;
		}

		if( execute.type == TExecute::COMMAND )
		{
			LogCYN( "%s", execute.str );
			ExecuteProcessIO( T->childpid, execute.str, T->OutStream, &OnProcessInStream, (void*)(long)(TExecute::COMMAND) );
			continue;
		}
	}

	pthread_exit( NULL );
	return NULL;
}
//-------------------------------------------------------------------------------------------

void TTest::StartComplex( int count )
{FUNCTION_TRACE
	if( working ) return;
	testtype = COMPLEX;
	cyclescount = count;
	cycle = 0;

    FOR_EACH_ITER( TComplexItems, ComplexItems, t )
    {
		t->ok = t->war = t->err = t->null = t->stop = 0;
    }

	RunEventScript( "complexstart" );
	toLog( "\nКомплексный тест. Циклов %d ─────────────────────────────────────────────────\n", cyclescount );
    if (Tcp) {
        if (Tcp->isConnected())
        {
            char sendMsg[10];
            sprintf(sendMsg, "#START#");
            Tcp->sendData(sendMsg, strlen(sendMsg));
        }
    }

	Update( );
}
//-------------------------------------------------------------------------------------------

void TTest::StartCustom( std::string& name, int count )
{FUNCTION_TRACE
	if( working ) return;
	TTestListIt testitem = GetTestItemByName( name );
	if( testitem == TestList.end() ) return;
	testtype = CUSTOM;
	cyclescount = count;
	cycle = 0;
	customtest = testitem;
	Update( );
}
//-------------------------------------------------------------------------------------------

void TTest::Stop( )
{FUNCTION_TRACE
	stop = true;
	if( !working ) return;
	TExecute execute;
	while( ReadPipe( exepipe, &execute, sizeof(TExecute), 1000 ) > 0 ) ;
	PushExecute( TExecute::DONE );
	Kill( );
}
//-------------------------------------------------------------------------------------------

#define OKGREEN     CLR(f_GREEN) "OK" CLR0
#define ERRRED      CLR(f_RED) "ERR" CLR0
#define STOPYELLOW  CLR(f_YELLOW) "STOP" CLR0
#define NULLMAGENTA CLR(f_MAGENTA) "NULL" CLR0

//-------------------------------------------------------------------------------------------

void addevent( Tstrlist& events, std::string event )
{
    FOR_EACH_ITER( Tstrlist, events, e ) { if( *e == event ) return; }
	events.push_back( event );
}
//-------------------------------------------------------------------------------------------

void TTest::OnCycleDone( )
{FUNCTION_TRACE
    if( ComplexSensorMasks != "" ) SensorPanel->LogSensorsByMask( ComplexSensorMasks );

	Tstrlist events;
    FOR_EACH_ITER( TComplexItems, ComplexItems, ci )
	{
		if( ci->last_result_error && !ci->cycle_error.empty() )
		{
			 //if( ci->cycle_error[ 0 ] == "script" ) runscript( ci->cycle_error[ 1 ], ci->test->name )
			 if( ci->cycle_error[ 0 ] == "event" ) addevent( events, ci->cycle_error[ 1 ] );
		}
		if( ci->last_result_warning && !ci->cycle_warning.empty() )
		{
			 //if( ci->cycle_warning[ 0 ] == "script" ) runscript( ci->cycle_warning[ 1 ], ci->test->name )
			 if( ci->cycle_warning[ 0 ] == "event" ) addevent( events, ci->cycle_warning[ 1 ] );
		}
	}
    FOR_EACH_ITER( Tstrlist, events, e )
	{
        string script = g_DebugSystem.fullpath( ini->ReadString( "complex", *e ) );
		toLog( "\n" CLRCYN( "--- EVENT %s (%s) ---" ) "\n", e->c_str(), script.c_str() );
		string data;
		int result = GetProcessDataTimeout( script, data, 10000 );
		if( result != 0 ) toLog( data );
	}
}
//-------------------------------------------------------------------------------------------

void TTest::Update( )
{FUNCTION_TRACE
	TraceD( stop );

	if( ( ++cycle > cyclescount ) || stop )
	{
        if( ( testtype == CUSTOM )&&( cyclescount != 1 ) ) MenuPanel->UpdateState( "custom" );
        if( ( testtype == CUSTOM )&&( cyclescount == 1 ) ) MenuPanel->UpdateState( "onestop" );
		if( testtype == COMPLEX )
		{
            MenuPanel->UpdateState( "complex" );

			OnCycleDone( );

            if( ComplexSensorMasks != "" ) SensorPanel->LogSensorsByMask( ComplexSensorMasks );

            FOR_EACH_ITER( TComplexItems, ComplexItems, ci )
			{

				ci->info = "";
				if( ci->ok   > 0 ) ci->info += stringformat( " | ok %d", ci->ok );
				if( ci->war  > 0 ) ci->info += stringformat( " | warning %d", ci->war );
				if( ci->err  > 0 ) ci->info += stringformat( " | error %d", ci->err );
				if( ci->stop > 0 ) ci->info += stringformat( " | stop %d", ci->stop );
				if( ci->null > 0 ) ci->info += stringformat( " | null %d", ci->null );

				int owe = ci->ok + ci->war + ci->err;
				if( owe > 0 )
				{
					if( ci->err == 0 && ci->war == 0 ) ci->total = TOTAL_OK; else
					if( ci->err > 0 ) ci->total = ( (float)ci->err/owe <= ci->total_error   ) ? TOTAL_OK : TOTAL_ERR; else
					if( ci->war > 0 ) ci->total = ( (float)ci->war/owe <= ci->total_warning ) ? TOTAL_OK : TOTAL_ERR;
				}
				else
				if( ci->stop > 0 ) ci->total = TOTAL_STOP;
				else ci->total = TOTAL_NULL;
			}

			toLog( "\nКомплексный тест завершен\n" );
			toLog( "───────────────────────────────────────────────────────────────────────────\n\n" );

            FOR_EACH_ITER( TSubDevices, SubDevices, sub )
			{
				int OKs=0, ERRs=0, STOPs=0, NULLs=0;

                FOR_EACH_ITER( TScannerItems, ScannerItems, si )
				{
					if( si->subdevice == &(*sub) )
					{
						toLog( "   %-16s %s %s\n", si->name.c_str(), si->result ? OKGREEN : ERRRED, si->info.c_str() );
						if( si->result ) OKs++; else ERRs++;
					}
				}
                FOR_EACH_ITER( TComplexItems, ComplexItems, ci )
				{
					if( ci->subdevice == &(*sub) )
					{
						string res;
						switch( ci->total )
						{
							case TOTAL_OK  : res = OKGREEN;    OKs++;   break;
							case TOTAL_ERR : res = ERRRED;     ERRs++;  break;
							case TOTAL_STOP: res = STOPYELLOW; STOPs++; break;
							case TOTAL_NULL: res = NULLMAGENTA;NULLs++; break;
						}
						toLog( "   %-16s %-12s  %s\n", ci->test->name.c_str(), res.c_str(), ci->info.c_str() );
					}
				}
				if( ERRs  > 0 ) sub->total = TOTAL_ERR;  else
				if( STOPs > 0 ) sub->total = TOTAL_STOP; else
				if( NULLs > 0 ) sub->total = TOTAL_NULL; else
				if( OKs   > 0 ) sub->total = TOTAL_OK; else sub->total = TOTAL_NULL;

				// --- for testcontrol single device: [#STOP# result] <= v1.0 build 6
				// --- for testcontrol multi device: [#DONE# device result][#STOP#] >= v1.1 build 1
				string tc3cmd = ( sub->name == "" ) ? "#STOP#" : "#DONE# " + sub->name;
				string result = NULLMAGENTA;
                bool okResult = false;

				switch( sub->total )
				{
                    case TOTAL_OK  : result = OKGREEN; okResult = true;  tc3cmd += " OK";    break;
					case TOTAL_ERR : result = ERRRED;     tc3cmd += " ERROR"; break;
					case TOTAL_STOP: result = STOPYELLOW; tc3cmd += " STOP";  break;
					case TOTAL_NULL: result = NULLMAGENTA;tc3cmd += " NULL";  break;
				}
				string name = ( sub->name != "" ) ? CLR(f_CYAN) + sub->name + " " CLR0 : "";
				toLog( "\n%sОбщий результат: %s\n", name.c_str(), result.c_str() );
				toLog( "───────────────────────────────────────────────────────────────────────────\n\n" );

                std::string resLocalCopy = LogPanel->FinalizeAndCopyLog( okResult,  HelloPanel->m_selectionPanel->m_serialNumber.c_str());
                if (!resLocalCopy.empty())
                {
                    if (Tcp) {
                        if (Tcp->isConnected())
                        {
                            std::string sendMsg;
                            std::string logText = readfilestr(resLocalCopy);
                            if (okResult) sendMsg = stringformat("#TEST_OK#%s#%d#", getFilenameFromPath(resLocalCopy).c_str(), logText.length());
                            else sendMsg = stringformat("#TEST_ERR#%s#%d#", getFilenameFromPath(resLocalCopy).c_str(), logText.length());
                            Tcp->sendData(sendMsg.c_str(), sendMsg.length());
                            Tcp->sendData(logText.c_str(), logText.length());
                        }
                    }
                    if (okResult)
                        DialogPanel->ShowMessage( UIBLUE, stringformat("170,11{}{Тестирование успешно завершено!\n\n    Лог-файл сохранен: %s!}{Ok}{Ok}", resLocalCopy.c_str()), &OnPressOkDialogBtn );
                    else
                        DialogPanel->ShowMessage( UIRED, stringformat("170,11{}{Тестирование завершено с ошибками!\n\n    Лог-файл сохранен: %s!}{Ok}{Ok}", resLocalCopy.c_str()), &OnPressOkDialogBtn );

                } else
                {
                    DialogPanel->ShowMessage( UIRED, "45,9{}{Не удалось локально сохранить лог-файл!}{Ok}{Ok}", &OnPressOkDialogBtn );
                }
			}

			RunEventScript( "complexstop" );
		}
		stop = false;
		working = false;
		TraceD( stop );
		return;
	}

    if( ( testtype == CUSTOM )&&( cyclescount == 1 ) ) MenuPanel->UpdateState( "onerun" );
    else MenuPanel->UpdateState( stringformat( "%d / %d", cycle, cyclescount ) );

	if( testtype == COMPLEX )
	{
		working = true;

		OnCycleDone( );

		toLog( "\nЦикл %d ────────────────────────────────────────────────────────────────────\n\n", cycle );

		TestList.sort( CompareTestItemByComplex );
		for( TTestListIt it = TestList.begin(); it != TestList.end(); ++it )
			if( it->complexnum > 0 )
			{
				PushExecute( TExecute( TExecute::TEST, cycle, &(*it) ) );
				PushExecute( TExecute::SLEEP );
			}
		PushExecute( TExecute::DONE );
	}
	if( testtype == CUSTOM  )
	{
		working = true;
		PushExecute( TExecute( TExecute::TEST, cycle, &(*customtest) ) );
		PushExecute( TExecute::SLEEP );
		PushExecute( TExecute::DONE );
	}
}
//-------------------------------------------------------------------------------------------

void TTest::Kill( )
{FUNCTION_TRACE
	int pid = childpid;
	if( pid == 0 ) return;
	g_DebugSystem.kill_process_and_childs( pid );
}
//-------------------------------------------------------------------------------------------
