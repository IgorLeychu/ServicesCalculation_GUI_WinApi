#undef _UNICODE
#undef UNICODE

#include <windows.h>

//**********************************************************
//код был портирован из консольной версии
//программа расчета услуг

#include <iostream>
#include <string>
#include <fstream>
#include <windows.h>
#include <iomanip>
#include <vector>
using namespace std;

#include <iostream>
#include <sstream>
#include <iomanip>

std::string doubleToString(double value, int precision = 6) {
    std::ostringstream out;
    //out << std::fixed << std::setprecision(precision) << value;
	out << std::setprecision(precision) << std::defaultfloat << value;  // std::defaultfloat для динамической формы 
    return out.str();
}

//исключения
const int EExit = 1;
const int EInvalidValue = 2;

class MyError
{
	HWND mHwnd;
public:
	MyError();
	void SetHWND(HWND aHwnd); // связываем с главным окном во время первичной инициализации(в функции WinMain)
	void ShowError (const char* message);
};

MyError::MyError()
{
	mHwnd = 0;
}

void MyError::SetHWND(HWND aHwnd)
{
	mHwnd = aHwnd;
}

void MyError::ShowError (const char* message)
{
	MessageBox(mHwnd, message, "Ошибка", MB_OK | MB_ICONERROR);
}

//************* globals *************
MyError gErr;

class MyInputEdit
{
public:
   int ReadDouble(HWND hEdit, double& num);               // если 0, всё нормально. Иначе ошибка 
   int ReadString(HWND hEdit, std::string& inputString);  // так же как в предыдущей, только когда 0 может быть просто пустая строка
                                                          // в нашей программе интерпретируется тоже как ошибка
};

int MyInputEdit::ReadString(HWND hEdit, std::string& inputString)
{
	// Получение текста из поля ввода
    char buffer[1000];
    int len = GetWindowText(hEdit, buffer, sizeof(buffer));

	std::string tempString(buffer);
	if (len > 0) 
	{
		inputString = tempString;
	}
	else
	{
		return 1; // была ошибка
	}

	return 0;
} 

int MyInputEdit::ReadDouble(HWND hEdit, double& num)
{
	std::string inputString;
	if (ReadString(hEdit, inputString) !=0) //ошибка
		return 1;

    int i;
	for (i=0; i<inputString.length(); i++)
	{
		if (inputString[i]==',')
			inputString[i]='.'; 		
	}
	//num = atof(inputString.c_str());  версия которая не даёт ошибок. плохая фунция

	try {
        num = std::stod(inputString);      
    } catch (...) {
        // std::cerr << "Ошибка: недопустимый аргумент. Строка не может быть преобразована в число." << std::endl;
		return 1; //ошибка
    } 

	//дополнительная проверка на ошибку(из за особенностей устройства функции stod)
	int res = 0;
	for (i=0; i<inputString.length(); i++)
	{
		//if ( (isdigit(inputString[i])>0) || (inputString[i] =='.') ){} //isdigit на русских символах "валится"(даёт исключение)
		if ( ((inputString[i]>='0') && (inputString[i]<='9'))|| (inputString[i] =='.') || ((inputString[i] =='-') && (i==0) )){} //цифра или точка
	    else
		{
			res = 1; //ошибка
			break;
		}			
	}
	
	return res;
}

/*
// for console version
class MyConsole
{
public:
   int ReadDouble(double& num);
   int ReadString(std::string& inputString);
};

int MyConsole::ReadString(std::string& inputString)
{
    getline(std::cin, inputString);
	while (inputString.length()==0)
		getline(std::cin, inputString);
	return 0;
}

int MyConsole::ReadDouble(double& num)
{
	 

	std::string inputString;
	getline(std::cin, inputString);
	while (inputString.length()==0)
		getline(std::cin, inputString);

	int i;
	for (i=0; i<inputString.length(); i++)
	{
		if (inputString[i]==',')
			inputString[i]='.'; 		
	}
	num = atof(inputString.c_str());

	return 0;
}
*/

class Record
{
public:
	char monthAndYear[300];
//1
	double water_cnt; // полное количество накрученных кубов(на счётчике)
	double water_cost; // стоимость одного куба
	double  water_subscription_fee; // абонплата
	//double water_full_cost; //полная стоимость услуг за воду
//2
	double gas_cnt; // полное количество накрученных кубов(на счётчике)
	double gas_cost; // стоимость одного куба
	double  gas_subscription_fee; //абонплата
	//double gas_full_cost;
//3
	double electricityNight_cnt;
	double electricityNight_cost;
//	double electricityNight_full_cost;

	double electricityDay_cnt;
	double electricityDay_cost;
//	double electricityDay_full_cost;

	//float electricityNightAndDay_full_cost;
//4 internet
	double tenet_full_cost;
//5
	double kievstar_full_cost;
//6
	double garbage_full_cost;

//	int LoadFromFile(char* fileName);
//	int WriteToFile(char* fileName);
};

const char* FILE_NAME = "uslugi_danniye";
const char* TEMP_FILE_NAME = "uslugi_danniye_tempcopy";
const char* FILE_NAME_WAS_REMOVED = "uslugi_danniye_wasremoved"; // временный файл, если существует, показывает, что главный файл(FILE_NAME) был удалён
																 // в процессе переименования, и возможно, требуется его восстановление из копии TEMP_FILE_NAME
void CreateEmptyFile(const char* aFileName)
{
	ofstream oFile(aFileName);
	if (!oFile.good())
	{
		//cout<<"создание пустого временного файла: ошибка при открытии файла "<<aFileName<<" на запись\n";
		std::string mess;
		mess = "создание пустого временного файла: ошибка при открытии файла ";
		mess+= aFileName;
		mess+= " на запись\n";
		gErr.ShowError(mess.c_str());

		throw EExit;
	} 
}

bool IsFileExist(const char* aFileName)
{
	bool bFileExist=true;
	ifstream iFile(aFileName);
	if (!iFile.good())
	{
		if (errno != 2) //любая ошибка кроме FileNotFoundError
		{
			//cout<<"проверка на существование: ошибка открытия файла "<<aFileName<<"на чтение";
			std::string mess;
			mess = "проверка на существование: ошибка открытия файла ";
			mess+= aFileName;
			mess+= "на чтение";
			gErr.ShowError(mess.c_str());

			throw EExit; 
		}
		// если ошибка FileNotFoundError, то есть если файл данных не существует
		bFileExist=false;
	} 
	return bFileExist;
}

void RestoreDataFilesAfterCrash()
{
	bool bMainFileExist;
	bool bTempMainFileExist;
	bool bTempWasRemovedFileExist;
	
	//проверяем на существование главного файла данных
	bMainFileExist = IsFileExist(FILE_NAME);
	//проверяем на существование копии(временного) главного файла данных
	bTempMainFileExist = IsFileExist(TEMP_FILE_NAME);
	//проверяем на существование копии(временного) главного файла данных
	bTempWasRemovedFileExist = IsFileExist(FILE_NAME_WAS_REMOVED);

	//если главный файл не существует, а копия существует и существует файл FILE_NAME_WAS_REMOVED, значит был такой краш, когда главный файл удалили, а переименовать
	//временный не успели. Следовательно, переименовываем копию в главный
	if (!bMainFileExist && bTempMainFileExist && bTempWasRemovedFileExist)
	{
		const char* old_name = TEMP_FILE_NAME; // старое название файла
		const char* new_name = FILE_NAME; // новое название файла

		if (std::rename(old_name, new_name) != 0) {
			//cout<<"ошибка переименования файла "<<old_name<<" errno="<<errno<<"\n";
			std::string mess;
			mess = "ошибка переименования файла ";
			mess+= old_name;
			gErr.ShowError(mess.c_str());

			throw EExit;
		}
		std::remove(FILE_NAME_WAS_REMOVED);
	}
}

class RecordsFile
{
	HWND mMainHWND;
public:
	RecordsFile();
	void SetHWND(HWND aHwnd); // связываем с главным окном во время первичной инициализации(в функции WinMain)

	//Record rec[1000];
	vector<Record> mRec;
	int mRec_count;

	int LoadFromFile();
	int WriteToFile();

	int InputNewRecord();
	int PrintReport(int numberRepFromEnd); 
	int DeleteLastRecord();
};


// ********** globals **************
HWND gHWNDMain;
RecordsFile gFile;
// *********************************


RecordsFile::RecordsFile()
{
	mRec_count = 0;
}

void RecordsFile::SetHWND(HWND aHwnd)
{
	mMainHWND = aHwnd;
}

int RecordsFile::LoadFromFile()
{
	bool createNewFile = 0;
	{//дополнительные фигурные скобки значимы, служат для того чтоб iFile удалился по выходу из области видимости
     //и отпустил открытый файл, в который другой функцией мы потом пишем
		ifstream iFile(FILE_NAME);
     	if (!iFile.good())
		{
			if (errno != 2) //любая ошибка кроме FileNotFoundError
			{
				//cout<<"ошибка открытия файла "<<FILE_NAME<<"на чтение";
				std::string mess;
				mess = "ошибка открытия файла ";
				mess+= FILE_NAME;
				mess+= "на чтение";
				gErr.ShowError(mess.c_str());

				throw EExit;
			}
		    // если ошибка FileNotFoundError

			//cout<<"файл "<<FILE_NAME<<" не найден\n";		
			//cout<<"создать новый файл(введи 1) или выдать ошибку и будете искать старый(введи любое другое число)?\n";			
			std::string mess;
			mess = "файл ";
			mess+= FILE_NAME;
			mess+= " не найден\n";
			mess+= "создать новый файл(Да) или закрыть программу и будете искать старый(Нет)?\n";

			int res = MessageBox(mMainHWND, mess.c_str(), "Предупреждение", MB_YESNO | MB_ICONWARNING);

			if (res == IDYES)
			{
				createNewFile = 1;
			}
			else
			{
				throw EExit; 
		    }		
		} 
		if (createNewFile == 0) // если не нужно создавать новый файл
		{
			iFile>>mRec_count;
			int i;
			Record curRec;
			// очищаем mRec
			mRec.clear();
			for (i=0; i<mRec_count;i++)
			{   
				iFile>>curRec.monthAndYear;
				//1 water
				iFile>>curRec.water_cnt;
				iFile>>curRec.water_cost; 
				iFile>>curRec.water_subscription_fee; 
				//2 gas
				iFile>>curRec.gas_cnt;
				iFile>>curRec.gas_cost;
				iFile>>curRec.gas_subscription_fee;
				//3 electricity
				iFile>>curRec.electricityNight_cnt;
				iFile>>curRec.electricityNight_cost;
				iFile>>curRec.electricityDay_cnt;
				iFile>>curRec.electricityDay_cost;
				//4 tenet(internet)
				iFile>>curRec.tenet_full_cost;
				//5 kievstar
				iFile>>curRec.kievstar_full_cost;
				//6 garbage
				iFile>>curRec.garbage_full_cost;

				mRec.push_back(curRec);
			}
		}
	}
	if (createNewFile)
	{
		mRec_count = 0;
		WriteToFile();
	}
	return 0;
}

int RecordsFile::WriteToFile()
{
	//безопасное сохранение: чтоб исключить полную потерю данных при записи в файл(при внутренних сбоях, например, выключили свет)
	//записываем их рядом во временный, а потом переименовываем его в целевой
	{//эти скобки значимы, файл должен быть закрыт по выходу
		ofstream oFile(TEMP_FILE_NAME);
		if (!oFile.good())
		{
			//cout<<"ошибка при открытии файла "<<TEMP_FILE_NAME<<" на запись\n";
			std::string mess;
			mess = "ошибка при открытии файла ";
			mess+= TEMP_FILE_NAME;
			mess+= " на запись\n";
			gErr.ShowError(mess.c_str());

			throw EExit;
		} 

		oFile<<mRec_count<<"\n";
		int i;
		for (i=0; i<mRec_count;i++)
		{
			oFile<<mRec[i].monthAndYear<<"\n";

			 //1 water
			oFile<<setprecision(10)<<mRec[i].water_cnt<<" ";
			oFile<<setprecision(10)<<mRec[i].water_cost<<" "; 
			oFile<<setprecision(10)<<mRec[i].water_subscription_fee<<"\n"; 
			//2 gas
			oFile<<setprecision(10)<<mRec[i].gas_cnt<<" ";
			oFile<<setprecision(10)<<mRec[i].gas_cost<<" ";
			oFile<<setprecision(10)<<mRec[i].gas_subscription_fee<<"\n";
			//3 electricity
			oFile<<setprecision(10)<<mRec[i].electricityNight_cnt<<" ";
			oFile<<setprecision(10)<<mRec[i].electricityNight_cost<<" ";
			oFile<<setprecision(10)<<mRec[i].electricityDay_cnt<<" ";
			oFile<<setprecision(10)<<mRec[i].electricityDay_cost<<"\n";
			//4
			oFile<<setprecision(10)<<mRec[i].tenet_full_cost<<"\n";
			//5
			oFile<<setprecision(10)<<mRec[i].kievstar_full_cost<<"\n";
			//6
			oFile<<setprecision(10)<<mRec[i].garbage_full_cost<<"\n";
		}
	}

	const char* old_name = TEMP_FILE_NAME; // старое название файла
    const char* new_name = FILE_NAME; // новое название файла
	if (IsFileExist(new_name))
	{
		CreateEmptyFile(FILE_NAME_WAS_REMOVED);
		std::remove(new_name); //rename не позволяет переименовать в существующий файл,
	                       //поэтому его надо предварительно удалить. И это - слабое место. Недочёты и косяки разработчиков
	                       //стандартных функций. Программа может прерваться в этом месте(выключили свет), после удаления файла данных,
	                       //но до переименования временного в целевой. Можно, конечно, понадеяться на высшие силы, что вероятность
	                       //этого на практике маленькая, но по хорошему этот случай надо обработать тоже. Поэтому в начале запуска программы в фунции восстановления после збоя
	                       //мы должны проверить только один случай: если целевой(главный) файл не существует, а временный файл существует,
	                       //и FILE_NAME_WAS_REMOVED существует тоже, то нужно переименовать этот временный в целевой
	}
	if (std::rename(old_name, new_name) != 0) {
		//cout<<"ошибка переименования файла "<<old_name<<" errno="<<errno<<"\n";
		std::string mess;
		mess = "ошибка переименования файла ";
		mess+= old_name;
		gErr.ShowError(mess.c_str());

		throw EExit;
    }
	std::remove(FILE_NAME_WAS_REMOVED);
	return 0;
}

const int ST_FirstInit = 1;  // начальная инициализация
const int ST_MainWindow = 2; // главное окно(и состояние) выбора действий
const int ST_InputRec = 3;   // окно ввода(и состояние) ввода данных(новой записи)
const int ST_Reports = 4;    // окно вывода(и состояние) и пролистывания отчетов
int g_currentState=ST_FirstInit; 
                    
                                        
// Переменная для хранения шрифта
HFONT g_hCtrlsFont;
int g_CtrlsHeight;
//HWND hEdit;

int g_numCurRep = 0; //номер текущего отчёта с конца, начиная с нуля
//HWND hButton;

struct SMainConrols
{
//1 название(подпись) элементов ввода-вывода
HWND hLabel_monthAndYear;
HWND hLabel_water_cnt;
HWND hLabel_water_cost;
HWND hLabel_water_subscription_fee; 
HWND hLabel_gas_cnt; 
HWND hLabel_gas_cost;
HWND hLabel_gas_subscription_fee; 
HWND hLabel_electricityNight_cnt; 
HWND hLabel_electricityNight_cost;
HWND hLabel_electricityDay_cnt; 
HWND hLabel_electricityDay_cost; 
HWND hLabel_tenet_full_cost;
HWND hLabel_kievstar_full_cost; 
HWND hLabel_garbage_full_cost; 
//2 элементы ввода-вывода(как edit) 
HWND hEdit_monthAndYear;
HWND hEdit_water_cnt; 
HWND hEdit_water_cost; 
HWND hEdit_water_subscription_fee; 
HWND hEdit_gas_cnt; 
HWND hEdit_gas_cost; 
HWND hEdit_gas_subscription_fee; 
HWND hEdit_electricityNight_cnt; 
HWND hEdit_electricityNight_cost; 
HWND hEdit_electricityDay_cnt; 
HWND hEdit_electricityDay_cost; 
HWND hEdit_tenet_full_cost; 
HWND hEdit_kievstar_full_cost; 
HWND hEdit_garbage_full_cost; 
//3 элементы вывода(как label)
HWND hEditAsL_monthAndYear;
HWND hEditAsL_water_cnt;
HWND hEditAsL_water_cost;
HWND hEditAsL_water_subscription_fee; 
HWND hEditAsL_gas_cnt; 
HWND hEditAsL_gas_cost;
HWND hEditAsL_gas_subscription_fee; 
HWND hEditAsL_electricityNight_cnt; 
HWND hEditAsL_electricityNight_cost;
HWND hEditAsL_electricityDay_cnt; 
HWND hEditAsL_electricityDay_cost; 
HWND hEditAsL_tenet_full_cost;
HWND hEditAsL_kievstar_full_cost; 
HWND hEditAsL_garbage_full_cost; 
};

SMainConrols gF1, gF2; //для ввода записи и вывода отчётов

HWND hLabel_total1;
HWND hLabel_total2;
HWND hLabel_totalAll;

HWND hLabel_Help;


//const int ID_FILE_EXIT = 1;
//нужно следить, чтоб все ID были уникальными
const int ID_HELP = 2;
const int ID_DELETE_LAST_RECORD = 1;

HWND hButton_Create; const HMENU idButton_Create = (HMENU) 15;
HWND hButton_ShowReports; const HMENU idButton_ShowReports = (HMENU) 16;

HWND hButton_Ok; const HMENU idButton_Ok = (HMENU) 17;
HWND hButton_Escape; const HMENU idButton_Escape = (HMENU) 18;

HWND hButton_ReportsNext; const HMENU idButton_ReportsNext = (HMENU) 19;
HWND hButton_ReportsPrev; const HMENU idButton_ReportsPrev = (HMENU) 20;


int RecordsFile::InputNewRecord()
{
		Record recTmp;
//		MyConsole con;	
		MyInputEdit edit;
		
		std::string errMess;
		int res;

		try
		{
			//cout<<"введи месяц и год записи: ";
			std::string inputString;
			res = edit.ReadString(gF1.hEdit_monthAndYear,inputString);
			if (res !=0 ) //ошибка
			{
				errMess = "месяц и год записи";
				throw EInvalidValue;
			}
			int i;
			for (i=0; i<inputString.length(); i++)
			{
				if (inputString[i]!=' ')
					recTmp.monthAndYear[i] = inputString[i]; 
				else
					recTmp.monthAndYear[i] = '-';
			}
			recTmp.monthAndYear[i]='\0';
	
			double temp;
		 
		//cout<<"1. Вода\n";
			//cout<<"На счётчике: ";
			res = edit.ReadDouble(gF1.hEdit_water_cnt, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Вода. На счётчике";
				throw EInvalidValue;
			}
			recTmp.water_cnt = temp;
	
			//cout<<"Цена за куб(если 0, значение из предыдущего месяца): ";		  
			res = edit.ReadDouble(gF1.hEdit_water_cost, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Вода. Цена за ед.";
				throw EInvalidValue;
			}
			if ((temp == -1) && (mRec_count>0)) temp = mRec[mRec_count-1].water_cost;
			recTmp.water_cost = temp;

			//cout<<"Абонплата(если 0, значение из предыдущего месяца): ";
			res = edit.ReadDouble(gF1.hEdit_water_subscription_fee, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Вода. Абонплата";
				throw EInvalidValue;
			}
			if ((temp == -1) && (mRec_count>0)) temp = mRec[mRec_count-1].water_subscription_fee;
			recTmp.water_subscription_fee = temp;
	
			//cout<<"2. Газ\n";
			//cout<<"На счётчике: ";
			res = edit.ReadDouble(gF1.hEdit_gas_cnt, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Газ. На счётчике";
				throw EInvalidValue;
			}
			recTmp.gas_cnt = temp;

			//cout<<"Цена за куб(если 0, значение из предыдущего месяца): ";		  
			res = edit.ReadDouble(gF1.hEdit_gas_cost, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Газ. Цена за ед.";
				throw EInvalidValue;
			}
			if ((temp == -1) && (mRec_count>0)) temp = mRec[mRec_count-1].gas_cost;
			recTmp.gas_cost = temp;

			//cout<<"Абонплата(распределение)(если 0, значение из предыдущего месяца): ";
			res = edit.ReadDouble(gF1.hEdit_gas_subscription_fee, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Газ. Распределение";
				throw EInvalidValue;
			}
			if ((temp == -1) && (mRec_count>0)) temp = mRec[mRec_count-1].gas_subscription_fee;
			recTmp.gas_subscription_fee = temp;
		
		//cout<<"3. Электричество\n";
			//cout<<"На счётчике(ночь): ";
			res = edit.ReadDouble(gF1.hEdit_electricityNight_cnt, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Электричество. На счётчике(ночь)";
				throw EInvalidValue;
			}
	        recTmp.electricityNight_cnt = temp;

			//cout<<"Цена за киловат(ночь)(если 0, значение из предыдущего месяца): ";		  
			res = edit.ReadDouble(gF1.hEdit_electricityNight_cost, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Электричество. Цена за ед.(ночь)";
				throw EInvalidValue;
			}
			if ((temp == -1) && (mRec_count>0)) temp = mRec[mRec_count-1].electricityNight_cost;
			recTmp.electricityNight_cost = temp;
	
			//cout<<"На счётчике(день): ";
			res = edit.ReadDouble(gF1.hEdit_electricityDay_cnt, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Электричество. На счётчике(день)";
				throw EInvalidValue;
			}
			recTmp.electricityDay_cnt = temp;

			//cout<<"Цена за киловат(день)(если 0, значение из предыдущего месяца): ";		  
			res = edit.ReadDouble(gF1.hEdit_electricityDay_cost, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Электричество. Цена за ед.(день)";
				throw EInvalidValue;
			}
			if ((temp == -1) && (mRec_count>0)) temp = mRec[mRec_count-1].electricityDay_cost;
			recTmp.electricityDay_cost = temp;
		
			//cout<<"4. Тенет(интернет)\n";
			//cout<<"Абонплата(если 0, значение из предыдущего месяца): ";		  
			res = edit.ReadDouble(gF1.hEdit_tenet_full_cost, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Тенет(интернет). Абонплата";
				throw EInvalidValue;
			}
			if ((temp == -1) && (mRec_count>0)) temp = mRec[mRec_count-1].tenet_full_cost;
			recTmp.tenet_full_cost = temp;

			//cout<<"5. Киевстар\n";
			//cout<<"Абонплата(если 0, значение из предыдущего месяца): ";		  
			res = edit.ReadDouble(gF1.hEdit_kievstar_full_cost, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Киевстар(интернет). Абонплата";
				throw EInvalidValue;
			}
			if ((temp == -1) && (mRec_count>0)) temp = mRec[mRec_count-1].kievstar_full_cost;
			recTmp.kievstar_full_cost = temp;
	
			//cout<<"6. Мусор\n";
			//cout<<"Абонплата(если 0, значение из предыдущего месяца): ";		  
			res = edit.ReadDouble(gF1.hEdit_garbage_full_cost, temp);
			if (res !=0 ) //ошибка
			{
				errMess = "Мусор. Абонплата";
				throw EInvalidValue;
			}
			if ((temp == -1) && (mRec_count>0)) temp = mRec[mRec_count-1].garbage_full_cost;
			recTmp.garbage_full_cost = temp;
		
			mRec_count++;
			mRec.push_back(recTmp);
		  
			WriteToFile();	
		}
	catch (int exep)
	{ 
		if (exep == EInvalidValue)
		{
			std::string mess = "ошибка в поле " + errMess;
			MessageBox(mMainHWND, mess.c_str() ,"ошибка ввода", MB_OK | MB_ICONINFORMATION);
			return EInvalidValue;
		}
		else
		{ throw;}// послать исключение дальше
	}
		return 0;
}

void Form3Reports_ClearFields();
//numberRep - номер отчёта отсчитывая с конца, и с нуля
int RecordsFile::PrintReport(int numberRepFromEnd)
{
	Form3Reports_ClearFields();

	if (mRec_count<=0)
	{
		//cout<<"список записей пуст, пока нечего выводить";
		MessageBox(mMainHWND, "список записей пуст, пока нечего выводить", "предупреждение", MB_OK | MB_ICONINFORMATION);
		return 1;
	}
	int numberRep = mRec_count-1-numberRepFromEnd;
	if ((numberRep<0) || (numberRep>=mRec_count))
	{
		//cout<<"номер отчёта вне диапазона допустимых номеров";
		MessageBox(mMainHWND, "номер отчёта вне диапазона допустимых номеров","предупреждение", MB_OK | MB_ICONINFORMATION);
		return 1;
	}

	bool bIsFirst; // это самая первая запись или нет
	Record cur, prev;
	if (numberRep > 0)
	{
		cur = mRec[numberRep];
		prev = mRec[numberRep-1];
		bIsFirst = false;
	}
	else
	{
		cur = mRec[numberRep];
		prev = mRec[numberRep];
		bIsFirst = true;
	}

    //заполняем поля данных вывода записи
	std::string s;// temp string for numbers

	SetWindowText(gF2.hEditAsL_monthAndYear, cur.monthAndYear);
	
	//s = std::to_string((long double)cur.water_cnt); // плохая фунция, часто отбрасывает значащие цифры
	s = doubleToString (cur.water_cnt);
	SetWindowText(gF2.hEditAsL_water_cnt, s.c_str() );

	s = doubleToString(cur.water_cost);
	SetWindowText(gF2.hEditAsL_water_cost, s.c_str() );

	s = doubleToString (cur.water_subscription_fee);
	SetWindowText(gF2.hEditAsL_water_subscription_fee, s.c_str() );

	s = doubleToString (cur.gas_cnt);
	SetWindowText(gF2.hEditAsL_gas_cnt, s.c_str() );

	s = doubleToString (cur.gas_cost);
	SetWindowText(gF2.hEditAsL_gas_cost, s.c_str() );

	s = doubleToString (cur.gas_subscription_fee);
	SetWindowText(gF2.hEditAsL_gas_subscription_fee, s.c_str() );

	s = doubleToString (cur.electricityNight_cnt);
	SetWindowText(gF2.hEditAsL_electricityNight_cnt, s.c_str() );

	s = doubleToString (cur.electricityNight_cost);
	SetWindowText(gF2.hEditAsL_electricityNight_cost, s.c_str() );

	s = doubleToString (cur.electricityDay_cnt);
	SetWindowText(gF2.hEditAsL_electricityDay_cnt, s.c_str() );

	s = doubleToString (cur.electricityDay_cost);
	SetWindowText(gF2.hEditAsL_electricityDay_cost, s.c_str() );

	s = doubleToString (cur.tenet_full_cost);
	SetWindowText(gF2.hEditAsL_tenet_full_cost, s.c_str() );

	s = doubleToString (cur.kievstar_full_cost);
	SetWindowText(gF2.hEditAsL_kievstar_full_cost, s.c_str() );

	s = doubleToString (cur.garbage_full_cost);
	SetWindowText(gF2.hEditAsL_garbage_full_cost, s.c_str() );

//рассчитываем итоги

	double water_total, gas_total, electricity_total,
		tenet_total, kievstar_total, garbage_total,
		totalAll;

	if (!bIsFirst) // формируем и выводим итоги
	{
		water_total = (cur.water_cnt - prev.water_cnt)*cur.water_cost+cur.water_subscription_fee;
		//cout<<"   Итого: "<<water_total<<"\n";

		gas_total = (cur.gas_cnt - prev.gas_cnt)*cur.gas_cost+cur.gas_subscription_fee;
		//cout<<"   Итого: "<<gas_total<<"\n";

		electricity_total = (cur.electricityNight_cnt - prev.electricityNight_cnt)*cur.electricityNight_cost
		+ (cur.electricityDay_cnt - prev.electricityDay_cnt)*cur.electricityDay_cost;
		//cout<<"   Итого: "<<electricity_total<<"\n";

		tenet_total = cur.tenet_full_cost;
		//cout<<"   Итого: "<<cur.tenet_full_cost<<"\n";

		kievstar_total = cur.kievstar_full_cost;
		//cout<<"   Итого: "<<cur.kievstar_full_cost<<"\n";

		garbage_total = cur.garbage_full_cost;
		//cout<<"   Итого: "<<cur.garbage_full_cost<<"\n";

		totalAll = water_total+gas_total+electricity_total+tenet_total+kievstar_total+garbage_total; 
		//cout<<"ИТОГО ЗА ВСЁ: "<<water_total+gas_total+electricity_total+cur.tenet_full_cost+cur.kievstar_full_cost+cur.garbage_full_cost<<"\n";

		
		s = "Итого: 1.Вода=";
		s += doubleToString (water_total);
	    s += "      2.Газ=";
		s += doubleToString (gas_total);
	    s += "      3.Электричество=";
		s += doubleToString (electricity_total);
		SetWindowText(hLabel_total1, s.c_str() );

		s = "           4.Тенет(интернет)=";
		s += doubleToString (tenet_total);
	    s += "      5.Киевстар=";
		s += doubleToString (kievstar_total);
	    s += "      6.Мусор=";
		s += doubleToString (garbage_total);
		SetWindowText(hLabel_total2, s.c_str() );

		s = "Итого За Всё=";
	    s+= doubleToString (totalAll);
		SetWindowText(hLabel_totalAll, s.c_str() );
	}
	else //если это самая первая запись
	{
		s = "Итого: это самая первая запись,";
		SetWindowText(hLabel_total1, s.c_str() );
		s = "          поэтому выводятся только данные, без рассчётов";
		SetWindowText(hLabel_total2, s.c_str() );
		//cout<<"это самый первый отчёт, поэтому выводятся только данные, без рассчётов\n";
	}

	/*cout<<"Отчёт за "<<cur.monthAndYear<<"\n";
	if (bIsFirst) cout<<"это самый первый отчёт, поэтому выводятся только данные, без рассчётов\n";

	cout<<"1. Вода\n";
	cout<<"   На счётчике: "<<cur.water_cnt<<"\n";
    cout<<"   Цена за куб: "<<cur.water_cost<<"\n";
	cout<<"   Абонплата: "<<cur.water_subscription_fee<<"\n";
	if (!bIsFirst)
	{
		water_total = (cur.water_cnt - prev.water_cnt)*cur.water_cost+cur.water_subscription_fee;
		cout<<"   Итого: "<<water_total<<"\n";
	}

	cout<<"2. Газ\n";
	cout<<"   На счётчике: "<<cur.gas_cnt<<"\n";
    cout<<"   Цена за куб: "<<cur.gas_cost<<"\n";
	cout<<"   Абонплата: "<<cur.gas_subscription_fee<<"\n";
	if (!bIsFirst)
	{
		gas_total = (cur.gas_cnt - prev.gas_cnt)*cur.gas_cost+cur.gas_subscription_fee;
		cout<<"   Итого: "<<gas_total<<"\n";
	}

	cout<<"3. Электричество\n";
	cout<<"   На счётчике(ночь): "<<cur.electricityNight_cnt<<"\n";
	cout<<"   Цена за киловат(ночь): "<<cur.electricityNight_cost<<"\n";		  
	cout<<"   На счётчике(день): "<<cur.electricityDay_cnt<<"\n";
	cout<<"   Цена за киловат(день): "<<cur.electricityDay_cost<<"\n";	
	if (!bIsFirst)
	{
		electricity_total = (cur.electricityNight_cnt - prev.electricityNight_cnt)*cur.electricityNight_cost
		+ (cur.electricityDay_cnt - prev.electricityDay_cnt)*cur.electricityDay_cost;
		cout<<"   Итого: "<<electricity_total<<"\n";
	}

	cout<<"4. Тенет(интернет)\n";
	cout<<"   Абонплата: "<<cur.tenet_full_cost<<"\n";
	if (!bIsFirst)
	{
		cout<<"   Итого: "<<cur.tenet_full_cost<<"\n";
	}

	cout<<"5. Киевстар\n";
    cout<<"   Абонплата: "<<cur.kievstar_full_cost<<"\n";		  
    if (!bIsFirst)
	{
	cout<<"   Итого: "<<cur.kievstar_full_cost<<"\n";
	}

    cout<<"6. Мусор\n";
	cout<<"   Абонплата: "<<cur.garbage_full_cost<<"\n";
	if (!bIsFirst)
	{
		cout<<"   Итого: "<<cur.garbage_full_cost<<"\n";
	}

	if (!bIsFirst)
	{
		cout<<"ИТОГО ЗА ВСЁ: "<<water_total+gas_total+electricity_total+cur.tenet_full_cost+cur.kievstar_full_cost+cur.garbage_full_cost<<"\n";
	}*/
}

int RecordsFile::DeleteLastRecord()
{
	int res = MessageBox(mMainHWND, "точно хотите удалить последнюю запись?", "Предупреждение", MB_YESNO | MB_ICONWARNING);
	if (res == IDYES)
	{
		if (mRec_count>0)
		{
			mRec_count--;
		    mRec.pop_back();
			WriteToFile();
		}
	}
	return 0;
}

void SystemInitialisation()
{
    setlocale(LC_CTYPE, "");
	//setlocale(LC_ALL, "Russian");
	
	//std::locale::global(std::locale("rus_rus.866"));
    //std::cin.imbue(std::locale());
}
/*
void Run()
{
	cout<<"Программа расчёта услуг\n";
    //дробные числа можно вводить и через точку, и через запятую, без разницы
	
	// загрузить данные из файла данных
	RecordsFile r;
	r.LoadFromFile();

	int choise;
	do
	{
		cout<<"\n0-выход, 1-ввести данные за новый месяц, 2-вывести последний отчёт\n"
		<< "3-удалить последнюю запись, 4-вывести последние N отчётов\n";

	   cin>>choise;
	   if (choise == 1)
       {
		   r.InputNewRecord();
       }
	   else if (choise ==2) 
	   {
	        r.PrintReport(0);//пока последний   
	   }
	   else if (choise ==3) 
	   {
	        r.DeleteLastRecord();	   
	   }
	   else if (choise ==4) 
	   {
		   cout<<"введи N: ";
		   int N,i;
		   cin>>N;
		   if ( (N>r.mRec_count) || (N<1))
			   N=r.mRec_count;
		   for (i=0; i<N; i++)
		   {
			   r.PrintReport(N-i-1);
			   cout<<"\n";
		   }
	   }
	}
	while (choise>0);
}
*/
/*int main()
{
	SystemInitialisation(); //локаль
	RestoreDataFilesAfterCrash();//при каждом запуске программы работающей с файлами по хорошему нужно вызывать подобную фунцию
	
	try
	{
		Run();
	}
	catch (...)
	{
		int exit;
		cout<<"\nвозникла ошибка в программе, введите любое число для выхода";
		cin>>exit;
	}

	return 0;
}
*/

//**********************************************************
//end code from console version

void Form3Reports_Show();


// Прототип функции обработки окна
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);



// Главная функция программы
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_currentState=ST_FirstInit; 
	RestoreDataFilesAfterCrash();//при каждом запуске программы работающей с файлами по хорошему нужно вызывать подобную фунцию

	
	const char CLASS_NAME[] = "SimpleWindow";

	WNDCLASS wc={};

	//wc.style = CS_HREDRAW | CS_VREDRAW; 

    wc.lpfnWndProc = WindowProc;          // Указатель на оконную процедуру
    wc.hInstance = hInstance;             // Дескриптор приложения
    wc.lpszClassName = CLASS_NAME;        // Имя класса окна

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,                                // Расширенные стили
        CLASS_NAME,                       // Имя класса окна
        "Рассчёт домашних услуг",    // Заголовок окна
        WS_OVERLAPPEDWINDOW,              // Стиль окна
        /*CW_USEDEFAULT, CW_USEDEFAULT*/10,10, 1260, 700,  // Размер и положение окна
        NULL,                             // Нет родительского окна
        NULL,                             // Нет меню
        hInstance,                        // Дескриптор экземпляра приложения
        NULL                              // Дополнительные параметры
    );

    if (hwnd == NULL)
    {
        return 0;
    }
 


	// Создание меню
    HMENU hMenu = CreateMenu();
    HMENU hSubMenu = CreatePopupMenu();

    // Добавляем пункты в меню "Файл"
   // AppendMenu(hSubMenu, MF_STRING, ID_FILE_EXIT, "Выход");
    //AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, "Файл");

   // Добавляем пункты в меню "Редактирование"
    AppendMenu(hSubMenu, MF_STRING, ID_DELETE_LAST_RECORD, "Удалить последнюю запись");
    AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, "Редактирование");


    // Добавляем пункты в меню "Справка"
    hSubMenu = CreatePopupMenu();
 //   AppendMenu(hSubMenu, MF_STRING, ID_HELP_ABOUT, "О программе");
	AppendMenu(hSubMenu, MF_STRING, ID_HELP, "Просмотр справки");
    AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, "Справка");

    // Устанавливаем меню в окне
    SetMenu(hwnd, hMenu);




    ShowWindow(hwnd, nCmdShow);

	//initialisation and main loop of program
	try
	{	
		gHWNDMain = hwnd;//здесь нужно быть осторожным, потому что в оконной процедуре по началу может быть не инициализирована
		                 //поэтому в оконной процедуре лучше пользоваться передаваемым параметром hwnd
						//кроме того со временем кроме одного главного окна в программе(и оконной процедуре) могут появиться другие окна
		gErr.SetHWND(hwnd);
		gFile.SetHWND(hwnd);
		gFile.LoadFromFile();

		g_currentState = ST_Reports;
		Form3Reports_Show();
		int res;
		g_numCurRep = 0;
		res = gFile.PrintReport(g_numCurRep);

		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	catch (int exep)
	{ 
		if (exep != EExit)
		{
			gErr.ShowError("Неизвестная ошибка в программе");
			throw;
		}
	}
/*	catch (...)	{}*/

    return 0;
}


void AllControls_Hide()
{
	bool bShow = 0;
    int intShow;
	if (bShow == 0)
		intShow = SW_HIDE;
	else
		intShow = SW_SHOW;
//form1

//form2 
	ShowWindow(gF1.hLabel_monthAndYear, intShow);
	ShowWindow(gF1.hEdit_monthAndYear, intShow); 
	ShowWindow(gF1.hLabel_water_cnt, intShow); 
	ShowWindow(gF1.hEdit_water_cnt, intShow); 
	ShowWindow(gF1.hLabel_water_cost, intShow); 
	ShowWindow(gF1.hEdit_water_cost, intShow); 
	ShowWindow(gF1.hLabel_water_subscription_fee, intShow); 
	ShowWindow(gF1.hEdit_water_subscription_fee, intShow); 
	ShowWindow(gF1.hLabel_gas_cnt, intShow); 
	ShowWindow(gF1.hEdit_gas_cnt, intShow); 
	ShowWindow(gF1.hLabel_gas_cost, intShow); 
	ShowWindow(gF1.hEdit_gas_cost, intShow); 
	ShowWindow(gF1.hLabel_gas_subscription_fee, intShow); 
	ShowWindow(gF1.hEdit_gas_subscription_fee, intShow); 
	ShowWindow(gF1.hLabel_electricityNight_cnt, intShow); 
	ShowWindow(gF1.hEdit_electricityNight_cnt, intShow); 
	ShowWindow(gF1.hLabel_electricityNight_cost, intShow);
	ShowWindow(gF1.hEdit_electricityNight_cost, intShow); 
	ShowWindow(gF1.hLabel_electricityDay_cnt, intShow); 
	ShowWindow(gF1.hEdit_electricityDay_cnt, intShow); 
	ShowWindow(gF1.hLabel_electricityDay_cost, intShow); 
	ShowWindow(gF1.hEdit_electricityDay_cost, intShow); 
	ShowWindow(gF1.hLabel_tenet_full_cost, intShow); 
	ShowWindow(gF1.hEdit_tenet_full_cost, intShow); 
	ShowWindow(gF1.hLabel_kievstar_full_cost, intShow); 
	ShowWindow(gF1.hEdit_kievstar_full_cost, intShow); 
	ShowWindow(gF1.hLabel_garbage_full_cost, intShow); 
	ShowWindow(gF1.hEdit_garbage_full_cost, intShow); 

	ShowWindow(gF1.hEditAsL_monthAndYear, intShow); 
	ShowWindow(gF1.hEditAsL_water_cnt, intShow); 
	ShowWindow(gF1.hEditAsL_water_cost, intShow); 
	ShowWindow(gF1.hEditAsL_water_subscription_fee, intShow); 
	ShowWindow(gF1.hEditAsL_gas_cnt, intShow); 
	ShowWindow(gF1.hEditAsL_gas_cost, intShow); 
	ShowWindow(gF1.hEditAsL_gas_subscription_fee, intShow); 
	ShowWindow(gF1.hEditAsL_electricityNight_cnt, intShow); 
	ShowWindow(gF1.hEditAsL_electricityNight_cost, intShow); 
	ShowWindow(gF1.hEditAsL_electricityDay_cnt, intShow); 
	ShowWindow(gF1.hEditAsL_electricityDay_cost, intShow); 
	ShowWindow(gF1.hEditAsL_tenet_full_cost, intShow); 
	ShowWindow(gF1.hEditAsL_kievstar_full_cost, intShow); 
	ShowWindow(gF1.hEditAsL_garbage_full_cost, intShow); 

	ShowWindow(hButton_Ok, intShow); 
	ShowWindow(hButton_Escape, intShow); 
	ShowWindow(hButton_ShowReports, intShow);
	ShowWindow(hLabel_Help, intShow);


//form3 
	ShowWindow(hButton_ReportsPrev, intShow);
	ShowWindow(hButton_ReportsNext, intShow);
	ShowWindow(hButton_Create, intShow);

	ShowWindow(hLabel_total1, intShow);
	ShowWindow(hLabel_total2, intShow);
	ShowWindow(hLabel_totalAll, intShow);

	ShowWindow(gF2.hLabel_monthAndYear, intShow);
	ShowWindow(gF2.hEdit_monthAndYear, intShow); 
	ShowWindow(gF2.hLabel_water_cnt, intShow); 
	ShowWindow(gF2.hEdit_water_cnt, intShow); 
	ShowWindow(gF2.hLabel_water_cost, intShow); 
	ShowWindow(gF2.hEdit_water_cost, intShow); 
	ShowWindow(gF2.hLabel_water_subscription_fee, intShow); 
	ShowWindow(gF2.hEdit_water_subscription_fee, intShow); 
	ShowWindow(gF2.hLabel_gas_cnt, intShow); 
	ShowWindow(gF2.hEdit_gas_cnt, intShow); 
	ShowWindow(gF2.hLabel_gas_cost, intShow); 
	ShowWindow(gF2.hEdit_gas_cost, intShow); 
	ShowWindow(gF2.hLabel_gas_subscription_fee, intShow); 
	ShowWindow(gF2.hEdit_gas_subscription_fee, intShow); 
	ShowWindow(gF2.hLabel_electricityNight_cnt, intShow); 
	ShowWindow(gF2.hEdit_electricityNight_cnt, intShow); 
	ShowWindow(gF2.hLabel_electricityNight_cost, intShow);
	ShowWindow(gF2.hEdit_electricityNight_cost, intShow); 
	ShowWindow(gF2.hLabel_electricityDay_cnt, intShow); 
	ShowWindow(gF2.hEdit_electricityDay_cnt, intShow); 
	ShowWindow(gF2.hLabel_electricityDay_cost, intShow); 
	ShowWindow(gF2.hEdit_electricityDay_cost, intShow); 
	ShowWindow(gF2.hLabel_tenet_full_cost, intShow); 
	ShowWindow(gF2.hEdit_tenet_full_cost, intShow); 
	ShowWindow(gF2.hLabel_kievstar_full_cost, intShow); 
	ShowWindow(gF2.hEdit_kievstar_full_cost, intShow); 
	ShowWindow(gF2.hLabel_garbage_full_cost, intShow); 
	ShowWindow(gF2.hEdit_garbage_full_cost, intShow); 

	ShowWindow(gF2.hEditAsL_monthAndYear, intShow); 
	ShowWindow(gF2.hEditAsL_water_cnt, intShow); 
	ShowWindow(gF2.hEditAsL_water_cost, intShow); 
	ShowWindow(gF2.hEditAsL_water_subscription_fee, intShow); 
	ShowWindow(gF2.hEditAsL_gas_cnt, intShow); 
	ShowWindow(gF2.hEditAsL_gas_cost, intShow); 
	ShowWindow(gF2.hEditAsL_gas_subscription_fee, intShow); 
	ShowWindow(gF2.hEditAsL_electricityNight_cnt, intShow); 
	ShowWindow(gF2.hEditAsL_electricityNight_cost, intShow); 
	ShowWindow(gF2.hEditAsL_electricityDay_cnt, intShow); 
	ShowWindow(gF2.hEditAsL_electricityDay_cost, intShow); 
	ShowWindow(gF2.hEditAsL_tenet_full_cost, intShow); 
	ShowWindow(gF2.hEditAsL_kievstar_full_cost, intShow); 
	ShowWindow(gF2.hEditAsL_garbage_full_cost, intShow); 
}

void Form1Begin_Show()
{
  /*  AllControls_Hide();

	bool bShow = 1;
	int intShow;
	if (bShow == 0)
		intShow = SW_HIDE;
	else
		intShow = SW_SHOW;

	ShowWindow(hButton_Create, intShow);
	ShowWindow(hButton_ShowReports, intShow);*/
}

void Form2InputRecord_ClearFields()
{
	SetWindowText(gF1.hEdit_monthAndYear, "");
	SetWindowText(gF1.hEdit_water_cnt, "");
	SetWindowText(gF1.hEdit_water_cost, "");
	SetWindowText(gF1.hEdit_water_subscription_fee, "");
	SetWindowText(gF1.hEdit_gas_cnt, "");
	SetWindowText(gF1.hEdit_gas_cost, "");
	SetWindowText(gF1.hEdit_gas_subscription_fee, "");
	SetWindowText(gF1.hEdit_electricityNight_cnt, "");
	SetWindowText(gF1.hEdit_electricityNight_cost, "");
	SetWindowText(gF1.hEdit_electricityDay_cnt, "");
	SetWindowText(gF1.hEdit_electricityDay_cost, "");
	SetWindowText(gF1.hEdit_tenet_full_cost, "");
	SetWindowText(gF1.hEdit_kievstar_full_cost, "");
	SetWindowText(gF1.hEdit_garbage_full_cost, "");
	return;
}

void Form3Reports_ClearFields()
{
	SetWindowText(gF2.hEditAsL_monthAndYear, "");
	SetWindowText(gF2.hEditAsL_water_cnt, "");
	SetWindowText(gF2.hEditAsL_water_cost, "");
	SetWindowText(gF2.hEditAsL_water_subscription_fee, "");
	SetWindowText(gF2.hEditAsL_gas_cnt, "");
	SetWindowText(gF2.hEditAsL_gas_cost, "");
	SetWindowText(gF2.hEditAsL_gas_subscription_fee, "");
	SetWindowText(gF2.hEditAsL_electricityNight_cnt, "");
	SetWindowText(gF2.hEditAsL_electricityNight_cost, "");
	SetWindowText(gF2.hEditAsL_electricityDay_cnt, "");
	SetWindowText(gF2.hEditAsL_electricityDay_cost, "");
	SetWindowText(gF2.hEditAsL_tenet_full_cost, "");
	SetWindowText(gF2.hEditAsL_kievstar_full_cost, "");
	SetWindowText(gF2.hEditAsL_garbage_full_cost, "");

	SetWindowText(hLabel_total1, "");
	SetWindowText(hLabel_total2, "");
	SetWindowText(hLabel_totalAll, "");

	return;
}
void Form2InputRecord_Show()
{
	//Form2InputRecord_ClearFields();
	AllControls_Hide();

	bool bShow = 1;
	int intShow;
	if (bShow == 0)
		intShow = SW_HIDE;
	else
		intShow = SW_SHOW;

	ShowWindow(gF1.hLabel_monthAndYear, intShow);
	ShowWindow(gF1.hEdit_monthAndYear, intShow); 
	ShowWindow(gF1.hLabel_water_cnt, intShow); 
	ShowWindow(gF1.hEdit_water_cnt, intShow); 
	ShowWindow(gF1.hLabel_water_cost, intShow); 
	ShowWindow(gF1.hEdit_water_cost, intShow); 
	ShowWindow(gF1.hLabel_water_subscription_fee, intShow); 
	ShowWindow(gF1.hEdit_water_subscription_fee, intShow); 
	ShowWindow(gF1.hLabel_gas_cnt, intShow); 
	ShowWindow(gF1.hEdit_gas_cnt, intShow); 
	ShowWindow(gF1.hLabel_gas_cost, intShow); 
	ShowWindow(gF1.hEdit_gas_cost, intShow); 
	ShowWindow(gF1.hLabel_gas_subscription_fee, intShow); 
	ShowWindow(gF1.hEdit_gas_subscription_fee, intShow); 
	ShowWindow(gF1.hLabel_electricityNight_cnt, intShow); 
	ShowWindow(gF1.hEdit_electricityNight_cnt, intShow); 
	ShowWindow(gF1.hLabel_electricityNight_cost, intShow);
	ShowWindow(gF1.hEdit_electricityNight_cost, intShow); 
	ShowWindow(gF1.hLabel_electricityDay_cnt, intShow); 
	ShowWindow(gF1.hEdit_electricityDay_cnt, intShow); 
	ShowWindow(gF1.hLabel_electricityDay_cost, intShow); 
	ShowWindow(gF1.hEdit_electricityDay_cost, intShow); 
	ShowWindow(gF1.hLabel_tenet_full_cost, intShow); 
	ShowWindow(gF1.hEdit_tenet_full_cost, intShow); 
	ShowWindow(gF1.hLabel_kievstar_full_cost, intShow); 
	ShowWindow(gF1.hEdit_kievstar_full_cost, intShow); 
	ShowWindow(gF1.hLabel_garbage_full_cost, intShow); 
	ShowWindow(gF1.hEdit_garbage_full_cost, intShow); 

	ShowWindow(hButton_Ok, intShow); 
	ShowWindow(hButton_Escape, intShow); 
	ShowWindow(hButton_ShowReports, intShow); 
	ShowWindow(hLabel_Help, intShow);

}

void Form3Reports_Show()
{
	//Form2InputRecord_ClearFields();
	AllControls_Hide();
	bool bShow = 1;
	int intShow;
	if (bShow == 0)
		intShow = SW_HIDE;
	else
		intShow = SW_SHOW;

	ShowWindow(hButton_ReportsPrev, intShow);
	ShowWindow(hButton_ReportsNext, intShow); 
				 	
	int numberRep = gFile.mRec_count-1-g_numCurRep;
	if (numberRep>=gFile.mRec_count-1)
		EnableWindow(hButton_ReportsNext, FALSE);
	else
		EnableWindow(hButton_ReportsNext, TRUE);
	
	if (numberRep<=0)
		EnableWindow(hButton_ReportsPrev, FALSE);
	else
		EnableWindow(hButton_ReportsPrev, TRUE);


	ShowWindow(hButton_Create, intShow);

	ShowWindow(hLabel_total1, intShow);
	ShowWindow(hLabel_total2, intShow);
	ShowWindow(hLabel_totalAll, intShow);


	ShowWindow(gF2.hLabel_monthAndYear, intShow);
	ShowWindow(gF2.hEditAsL_monthAndYear, intShow); 
	ShowWindow(gF2.hLabel_water_cnt, intShow); 
	ShowWindow(gF2.hEditAsL_water_cnt, intShow); 
	ShowWindow(gF2.hLabel_water_cost, intShow); 
	ShowWindow(gF2.hEditAsL_water_cost, intShow); 
	ShowWindow(gF2.hLabel_water_subscription_fee, intShow); 
	ShowWindow(gF2.hEditAsL_water_subscription_fee, intShow); 
	ShowWindow(gF2.hLabel_gas_cnt, intShow); 
	ShowWindow(gF2.hEditAsL_gas_cnt, intShow); 
	ShowWindow(gF2.hLabel_gas_cost, intShow); 
	ShowWindow(gF2.hEditAsL_gas_cost, intShow); 
	ShowWindow(gF2.hLabel_gas_subscription_fee, intShow); 
	ShowWindow(gF2.hEditAsL_gas_subscription_fee, intShow); 
	ShowWindow(gF2.hLabel_electricityNight_cnt, intShow); 
	ShowWindow(gF2.hEditAsL_electricityNight_cnt, intShow); 
	ShowWindow(gF2.hLabel_electricityNight_cost, intShow);
	ShowWindow(gF2.hEditAsL_electricityNight_cost, intShow); 
	ShowWindow(gF2.hLabel_electricityDay_cnt, intShow); 
	ShowWindow(gF2.hEditAsL_electricityDay_cnt, intShow); 
	ShowWindow(gF2.hLabel_electricityDay_cost, intShow); 
	ShowWindow(gF2.hEditAsL_electricityDay_cost, intShow); 
	ShowWindow(gF2.hLabel_tenet_full_cost, intShow); 
	ShowWindow(gF2.hEditAsL_tenet_full_cost, intShow); 
	ShowWindow(gF2.hLabel_kievstar_full_cost, intShow); 
	ShowWindow(gF2.hEditAsL_kievstar_full_cost, intShow); 
	ShowWindow(gF2.hLabel_garbage_full_cost, intShow); 
	ShowWindow(gF2.hEditAsL_garbage_full_cost, intShow); 

}  

void CreateForm1_Begin(HWND hwnd)
{	 
}

void CreateForm2_InputRecord(HWND hwnd)
{	
	hLabel_Help = CreateWindow("STATIC", "подсказка: там, где возможно если -1, значение подставляется с предыдущего месяца", 
                            WS_CHILD | WS_VISIBLE | SS_LEFT, 
                            750, 300, 400, g_CtrlsHeight*3, 
                            hwnd, NULL, 
                            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
     SendMessage(hLabel_Help, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
 

//0  
	int xBeg = 10, yBeg = 10;
	int x=xBeg, y=yBeg;

	gF1.hLabel_monthAndYear = CreateWindow("STATIC", "Месяц и год записи:", 
                            WS_CHILD | WS_VISIBLE | SS_LEFT, 
                            x, y, 250, g_CtrlsHeight, 
                            hwnd, NULL, 
                            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    
	x= x+250;
	gF1.hEdit_monthAndYear = CreateWindow(
    "EDIT",                    // Класс "Edit"  для текстового поля
    "",                        // Начальный текст
    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
    x, y, 300, g_CtrlsHeight,           // Позиция и размеры поля
    hwnd,                      // Родительское окно
    NULL,                  // Идентификатор элемента
    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
         
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_monthAndYear, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_monthAndYear, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);		

//1.1
	x = xBeg;
	y = y + g_CtrlsHeight + 20;
	gF1.hLabel_water_cnt = CreateWindow("STATIC", "1. Вода. На счётчике:", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 250, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x+260;
 	gF1.hEdit_water_cnt = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 95, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_water_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_water_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

//1.2
	x= x + 95 + 5;
	gF1.hLabel_water_cost = CreateWindow("STATIC", "Цена за ед.:", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 150, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	x= x + 150 + 5;
	gF1.hEdit_water_cost = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_water_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_water_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

//1.3
	x= x + 200 + 10;
	gF1.hLabel_water_subscription_fee = CreateWindow("STATIC", "Абонплата:", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 140, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
			
	x= x + 140 + 10;
	gF1.hEdit_water_subscription_fee = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_water_subscription_fee, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_water_subscription_fee, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	
//2.1
	x = xBeg;
	y = y + g_CtrlsHeight + 20;
	gF1.hLabel_gas_cnt = CreateWindow("STATIC", "2. Газ. На счётчике:", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 237, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x+237+10;
 	gF1.hEdit_gas_cnt = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_gas_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_gas_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	
//2.2
	x = x + 200 +10;
	gF1.hLabel_gas_cost = CreateWindow("STATIC", "Цена за ед.:", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 160, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x+160+10;
 	gF1.hEdit_gas_cost = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_gas_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_gas_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	
//2.3
	x = x + 200 +10;
	gF1.hLabel_gas_subscription_fee = CreateWindow("STATIC", "Распред.:", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 120, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x+120+10;
 	gF1.hEdit_gas_subscription_fee = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_gas_subscription_fee, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_gas_subscription_fee, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

//3.1
	x = xBeg;
	y = y + g_CtrlsHeight + 20;
	gF1.hLabel_electricityNight_cnt = CreateWindow("STATIC", "3. Электричество. На счётчике(ночь):", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 450, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x+450+10;
 	gF1.hEdit_electricityNight_cnt = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_electricityNight_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_electricityNight_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	
//3.2
	x = x+200+10;
	gF1.hLabel_electricityNight_cost = CreateWindow("STATIC", "Цена за ед.(ночь):", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 220, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x+220+10;
 	gF1.hEdit_electricityNight_cost = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_electricityNight_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_electricityNight_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	
//3.3
	x = xBeg +215;
	y = y + g_CtrlsHeight +3;
	gF1.hLabel_electricityDay_cnt = CreateWindow("STATIC", "На счётчике(день):", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 235, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x+235+10;
 	gF1.hEdit_electricityDay_cnt = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_electricityDay_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_electricityDay_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	
//3.4
	x = x + 200 + 10;
	gF1.hLabel_electricityDay_cost = CreateWindow("STATIC", "Цена за ед.(день):", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 220, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x+220+10;
 	gF1.hEdit_electricityDay_cost = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_electricityDay_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_electricityDay_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	
//4
	x = xBeg;
	y = y + g_CtrlsHeight + 20;
	gF1.hLabel_tenet_full_cost = CreateWindow("STATIC", "4. Тенет(интернет). Абонплата:", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 370, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x+370+10;
 	gF1.hEdit_tenet_full_cost = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_tenet_full_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_tenet_full_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	
//5
	x = xBeg;
	y = y + g_CtrlsHeight + 20;
	gF1.hLabel_kievstar_full_cost = CreateWindow("STATIC", "5. Киевстар. Абонплата:", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 300, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x+300+10;
 	gF1.hEdit_kievstar_full_cost = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_kievstar_full_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_kievstar_full_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	
//6
	x = xBeg;
	y = y + g_CtrlsHeight + 20;
	gF1.hLabel_garbage_full_cost = CreateWindow("STATIC", "6. Мусор. Абонплата:", 
				WS_CHILD | WS_VISIBLE | SS_LEFT, 
				x, y, 260, g_CtrlsHeight, 
				hwnd, NULL, 
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x+260+10;
 	gF1.hEdit_garbage_full_cost = CreateWindow(
		"EDIT",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL); 
       
	// Устанавливаем шрифт для EDIT и LABEL
    SendMessage(gF1.hEdit_garbage_full_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
    SendMessage(gF1.hLabel_garbage_full_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	
// ok и Отмена
	x = xBeg + 300;
	y = y + g_CtrlsHeight + 30;

	hButton_Escape = CreateWindow(
    "BUTTON",                   // Класс кнопки
    "Отмена",               // Текст кнопки
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Стиль кнопки
    x, y, 150, g_CtrlsHeight,            // Позиция и размер кнопки
    hwnd,                       // Родительское окно
    idButton_Escape,                   // Идентификатор кнопки
    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	x=x+150+20;
	hButton_Ok = CreateWindow(
    "BUTTON",                   // Класс кнопки
    "OK",               // Текст кнопки
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Стиль кнопки
    x, y, 100, g_CtrlsHeight,            // Позиция и размер кнопки
    hwnd,                       // Родительское окно
    idButton_Ok,                   // Идентификатор кнопки
    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
       
	SendMessage(hButton_Escape, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(hButton_Ok, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);  

	x=x+100+20;
	hButton_ShowReports = CreateWindow(
    "BUTTON",                   // Класс кнопки
    "Записи",               // Текст кнопки
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Стиль кнопки
    x, y, 150, g_CtrlsHeight,            // Позиция и размер кнопки
    hwnd,                       // Родительское окно
    idButton_ShowReports,                   // Идентификатор кнопки
    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	SendMessage(hButton_ShowReports, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

}

void CreateForm3_Reports(HWND hwnd)
{
	//0  
	int xBeg = 10, yBeg = 10;
	int x = xBeg, y = yBeg;

	gF2.hLabel_monthAndYear = CreateWindow("STATIC", "Месяц и год записи:",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 250, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	x = x + 250;
	gF2.hEditAsL_monthAndYear = CreateWindow(
		"STATIC",                    // Класс "Edit"  для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 300, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_monthAndYear, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_monthAndYear, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
//1.1
	x = xBeg;
	y = y + g_CtrlsHeight + 20;
	gF2.hLabel_water_cnt = CreateWindow("STATIC", "1. Вода. На счётчике:",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 250, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x + 260;
	gF2.hEditAsL_water_cnt = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 95, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_water_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_water_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

//1.2
	x = x + 95 + 5;
	gF2.hLabel_water_cost = CreateWindow("STATIC", "Цена за ед.:",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 150, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	x = x + 150 + 5;
	gF2.hEditAsL_water_cost = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_water_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_water_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

//1.3
	x = x + 200 + 10;
	gF2.hLabel_water_subscription_fee = CreateWindow("STATIC", "Абонплата:",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 140, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	x = x + 140 + 10;
	gF2.hEditAsL_water_subscription_fee = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_water_subscription_fee, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_water_subscription_fee, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

//2.1
	x = xBeg;
	y = y + g_CtrlsHeight + 20;
	gF2.hLabel_gas_cnt = CreateWindow("STATIC", "2. Газ. На счётчике:",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 237, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x + 237 + 10;
	gF2.hEditAsL_gas_cnt = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_gas_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_gas_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

//2.2
	x = x + 200 + 10;
	gF2.hLabel_gas_cost = CreateWindow("STATIC", "Цена за ед.:",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 160, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x + 160 + 10;
	gF2.hEditAsL_gas_cost = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_gas_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_gas_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	// ShowW x(hLabel, SW_HIDE); ShowWindow(hLabel, SW_SHOW);
//2.3
	x = x + 200 + 10;
	gF2.hLabel_gas_subscription_fee = CreateWindow("STATIC", "Распред.:",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 120, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x + 120 + 10;
	gF2.hEditAsL_gas_subscription_fee = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_gas_subscription_fee, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_gas_subscription_fee, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
//3.1
	x = xBeg;
	y = y + g_CtrlsHeight + 20;
	gF2.hLabel_electricityNight_cnt = CreateWindow("STATIC", "3. Электричество. На счётчике(ночь):",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 450, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x + 450 + 10;
	gF2.hEditAsL_electricityNight_cnt = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_electricityNight_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_electricityNight_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

//3.2
	x = x + 200 + 10;
	gF2.hLabel_electricityNight_cost = CreateWindow("STATIC", "Цена за ед.(ночь):",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 220, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x + 220 + 10;
	gF2.hEditAsL_electricityNight_cost = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_electricityNight_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_electricityNight_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

//3.3
	x = xBeg + 215;
	y = y + g_CtrlsHeight + 3;
	gF2.hLabel_electricityDay_cnt = CreateWindow("STATIC", "На счётчике(день):",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 235, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x + 235 + 10;
	gF2.hEditAsL_electricityDay_cnt = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_electricityDay_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_electricityDay_cnt, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

//3.4
	x = x + 200 + 10;
	gF2.hLabel_electricityDay_cost = CreateWindow("STATIC", "Цена за ед.(день):",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 220, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x + 220 + 10;
	gF2.hEditAsL_electricityDay_cost = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_electricityDay_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_electricityDay_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

//4
	x = xBeg;
	y = y + g_CtrlsHeight + 20;
	gF2.hLabel_tenet_full_cost = CreateWindow("STATIC", "4. Тенет(интернет). Абонплата:",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 370, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x + 370 + 10;
	gF2.hEditAsL_tenet_full_cost = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_tenet_full_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_tenet_full_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
//5
	x = xBeg;
	y = y + g_CtrlsHeight + 20;
	gF2.hLabel_kievstar_full_cost = CreateWindow("STATIC", "5. Киевстар. Абонплата:",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 300, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x + 300 + 10;
	gF2.hEditAsL_kievstar_full_cost = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_kievstar_full_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_kievstar_full_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
//6
	x = xBeg;
	y = y + g_CtrlsHeight + 20;
	gF2.hLabel_garbage_full_cost = CreateWindow("STATIC", "6. Мусор. Абонплата:",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, 260, g_CtrlsHeight,
		hwnd, NULL,
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	x = x + 260 + 10;
	gF2.hEditAsL_garbage_full_cost = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, 200, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	// Устанавливаем шрифт для EDIT и LABEL
	SendMessage(gF2.hEditAsL_garbage_full_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(gF2.hLabel_garbage_full_cost, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

	//итоги
	x = xBeg;
	y = y + g_CtrlsHeight + 20;

	int lenTotal = 1200;
	hLabel_total1 = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, lenTotal, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
		SendMessage(hLabel_total1, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	
		y = y + g_CtrlsHeight + 5;

	hLabel_total2 = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, lenTotal, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
		SendMessage(hLabel_total2, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);

		x = xBeg;
	    y = y + g_CtrlsHeight + 5;

		hLabel_totalAll = CreateWindow(
		"STATIC",                    // Класс "Edit" для текстового поля
		"",                        // Начальный текст
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,  // Стиль: обычное поле ввода с границей
		x, y, lenTotal, g_CtrlsHeight,           // Позиция и размеры поля
		hwnd,                      // Родительское окно
		NULL,                  // Идентификатор элемента
		(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
		SendMessage(hLabel_totalAll, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);


    x = xBeg + 300;
	y = y + g_CtrlsHeight + 30;

	hButton_ReportsPrev = CreateWindow(
    "BUTTON",                   // Класс кнопки
    "<-Пред.",               // Текст кнопки
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Стиль кнопки
    x, y, 150, g_CtrlsHeight,            // Позиция и размер кнопки
    hwnd,                       // Родительское окно
    idButton_ReportsPrev,                   // Идентификатор кнопки
    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	
	x = x+150+10;
	hButton_ReportsNext = CreateWindow(
    "BUTTON",                   // Класс кнопки
    "След.->",               // Текст кнопки
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Стиль кнопки
    x, y, 150, g_CtrlsHeight,            // Позиция и размер кнопки
    hwnd,                       // Родительское окно
    idButton_ReportsNext,                   // Идентификатор кнопки
    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

	SendMessage(hButton_ReportsPrev, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	SendMessage(hButton_ReportsNext, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
	
	x=x+50;
	  hButton_Create = CreateWindow(
    "BUTTON",                   // Класс кнопки
    "Новая запись",               // Текст кнопки
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Стиль кнопки
    700, y, 200, 40,            // Позиция и размер кнопки
    hwnd,                       // Родительское окно
    idButton_Create,                   // Идентификатор кнопки
    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	SendMessage(hButton_Create, WM_SETFONT, (WPARAM)g_hCtrlsFont, TRUE);
}

LRESULT HandleStateAll(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool& bMustReturn)
{
	bMustReturn = true;

	switch (uMsg)
    {
		 /*   case WM_NCPAINT: {
            // Получение контекста устройства для неклиентской области
            HDC hdc = GetWindowDC(hwnd);
            
            // Создание шрифта
            HFONT hFont = CreateFont(
                30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                VARIABLE_PITCH, TEXT("Arial"));

            // Сохранение текущего шрифта
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            
            // Определение прямоугольника для заголовка
            RECT rect;
            GetWindowRect(hwnd, &rect);
            rect.left = 0;
            rect.top = 0;
            rect.right = rect.right - rect.left;
            rect.bottom = GetSystemMetrics(SM_CYCAPTION);

            // Установка цвета текста и фона
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkColor(hdc, RGB(0, 0, 128)); // Фон для заголовка

            // Рисование текста заголовка
            DrawText(hdc, TEXT("Моё Окно"), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Восстановление оригинального шрифта и освобождение ресурсов
            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
            ReleaseDC(hwnd, hdc);

            return 0;
        }*/

	    case WM_COMMAND: //обрабатываем меню
        if (LOWORD(wParam) == (int) ID_DELETE_LAST_RECORD)
        {
			gFile.DeleteLastRecord();
			g_currentState = ST_Reports;
			
			int res;
			g_numCurRep = 0;
			res = gFile.PrintReport(g_numCurRep);

			Form3Reports_Show();
		}
		else
        if (LOWORD(wParam) == (int) ID_HELP)
        {
			MessageBox(hwnd, "Справка(как и файл данных) находится в одной папке с программой" ,"справка", MB_OK | MB_ICONINFORMATION);
		}
		else
			bMustReturn = false; // нужно передать WM_COMMAND дальше для обработки

		case WM_ERASEBKGND: //обновляем фон
			{
            // Получаем контекст устройства для фона
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // Задаем цвет фона (белый в данном примере)
            HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);
            
            // Возвращаем TRUE, чтобы указать, что фон был перерисован
            return TRUE;
            }
	    case WM_DESTROY:
	      PostQuitMessage(0);
	      return 0;
		default:
			bMustReturn = false;
	}

	if (bMustReturn == true)
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	else
		return 0; //вообще любое значение, оно всё равно не используется, пусть это будет 0;
}

LRESULT HandleState1_FirstInit(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
    {
    case WM_CREATE:
		{
			//gHWNDMain = hwnd; 

			// Создаём шрифт с нужным размером и вычисляем высоту главных элементов управления
            g_hCtrlsFont = CreateFont(30, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
			 // Получаем контекст устройства для вычисления высоты текста
            HDC hdc = GetDC(hwnd);
            SelectObject(hdc, g_hCtrlsFont);     
            // Получаем метрики текста
            TEXTMETRIC tm;
            GetTextMetrics(hdc, &tm);    
            // Рассчитываем высоту элемента EDIT в зависимости от высоты шрифта
            g_CtrlsHeight = tm.tmHeight + tm.tmExternalLeading + 6; // Добавляем 6 пикселей для отступов  
            ReleaseDC(hwnd, hdc);
			
			//создание всех форм
			CreateForm1_Begin(hwnd);
			CreateForm2_InputRecord(hwnd);
			CreateForm3_Reports(hwnd);
	            
			return 0;
		}
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT HandleState2_ST_MainWindow(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
    {
	 case WM_COMMAND:
        if (LOWORD(wParam) == (int) idButton_Create)
        {
			g_currentState = ST_InputRec;
			Form2InputRecord_Show();
		}
		else
        if (LOWORD(wParam) == (int) idButton_ShowReports)
        {
			g_currentState = ST_Reports;
			
			int res;
			g_numCurRep = 0;
			res = gFile.PrintReport(g_numCurRep);

			Form3Reports_Show();
		}
        break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT HandleState3_ST_InputRec(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
    {
	 case WM_COMMAND:
        // Обработка нажатия кнопки
        if (LOWORD(wParam) == (int) idButton_Ok)  
        {
			int res;
			res = gFile.InputNewRecord();

			if (res!=0) //ошибка
			{}
			else //запись сохранена, переходим к следующему состоянию
			{
				Form2InputRecord_ClearFields();
				g_currentState = ST_Reports;
				
				int res;
				g_numCurRep = 0;
				res = gFile.PrintReport(g_numCurRep);

				Form3Reports_Show();
				}
		}
		else  
		if (LOWORD(wParam) == (int) idButton_Escape) 
        {
			Form2InputRecord_ClearFields();
			g_currentState = ST_Reports;
			
			int res;
			g_numCurRep = 0;
			res = gFile.PrintReport(g_numCurRep);

			Form3Reports_Show();
		}
		else  
		if (LOWORD(wParam) == (int) idButton_ShowReports)  
        {
			//Form2InputRecord_ClearFields(); закомментировано специально, не удаляем заполненные поля
			g_currentState = ST_Reports;
			
			int res;
			g_numCurRep = 0;
			res = gFile.PrintReport(g_numCurRep);

			Form3Reports_Show();
		}
       break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT HandleState4_ST_Reports(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
    {
	 case WM_COMMAND:
	    if (LOWORD(wParam) == (int) idButton_Create)
        {
			g_currentState = ST_InputRec;
			Form2InputRecord_Show();
			break;
		}
		 if (LOWORD(wParam) == (int) idButton_ReportsNext) 
         {
			 	int numberRep = gFile.mRec_count-1-g_numCurRep;
				if (numberRep+1<gFile.mRec_count)
					g_numCurRep--;
		 } else
	     if (LOWORD(wParam) == (int) idButton_ReportsPrev) 
         {
			int numberRep = gFile.mRec_count-1-g_numCurRep;
			if (numberRep-1>=0)
				g_numCurRep++;
		 }
		int res;
		res = gFile.PrintReport(g_numCurRep);
		Form3Reports_Show();// обновляем окно
       break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Главная оконная процедура
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// вначале обработка для всех состояний программы
    bool bMustReturn;
	LRESULT res = HandleStateAll(hwnd, uMsg, wParam, lParam, bMustReturn);
	if (bMustReturn)
		return res;

	// В зависимости от состояния программы, вызываем соответствующий обработчик
    switch (g_currentState)
    {
        case ST_FirstInit:
            return HandleState1_FirstInit(hwnd, uMsg, wParam, lParam);
        case ST_MainWindow:
            return HandleState2_ST_MainWindow(hwnd, uMsg, wParam, lParam);
		case ST_InputRec:
			return HandleState3_ST_InputRec(hwnd, uMsg, wParam, lParam);
		case ST_Reports:
			return HandleState4_ST_Reports(hwnd, uMsg, wParam, lParam);
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
 //   return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

