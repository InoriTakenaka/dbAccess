#ifndef _DATABASE_HPP_
#define _DATABASE_HPP_
#define  _CRTDBG_MAP_ALLOC 
#include"stdafx.h"
#include<string>
#include<fstream>
#include<sstream>
#include<map>
#include<vector>
#include<stdexcept>
#include"cJSON.h"
#ifdef UNICODE 
typedef std::wstring STRING;
const std::wstring emptyField = L"NULL";
#else /* MBCS */
typedef std::string STRING;
const std::string emptyField = "NULL";
#endif

namespace DbAccess{

	enum class dbState{ open, close };
	
	class CDataRow{
	private:
		std::map<STRING, STRING> _cell;
		int m_Count;
	public:
		STRING GetField(STRING columnName){
			try{
				return _cell.at(columnName);
			}
			catch (std::out_of_range err){
				return STRING();
			}
		}

		int Count(){
			return m_Count;
		}

		STRING operator[](STRING key){
			return GetField(key);
		}

		void Insert(STRING fieldName, STRING fieldValue){
			_cell.insert(std::make_pair(fieldName, fieldValue));
			m_Count++;
		}

		CDataRow(){
			m_Count = 0;
		}

		~CDataRow(){
			_cell.clear();
		}
	};

	class CDataTable{
	private:
		_RecordsetPtr m_recordSetPtr;
		int			  m_length;
		std::vector<CDataRow> m_data;
	public:
		CDataTable(){
			m_recordSetPtr.CreateInstance(__uuidof(Recordset));
			m_length = -1;
		}

		~CDataTable(){
			//m_recordSetPtr->Release();
			m_recordSetPtr = nullptr;
		}

		bool Open(string strSQL, _ConnectionPtr connectionPtr){
			try{
				//m_recordSetPtr->CursorLocation = adUseClient;
				m_recordSetPtr->Open(strSQL.data(), connectionPtr.GetInterfacePtr(),
					adOpenDynamic, adLockOptimistic, adCmdText);
				m_length = m_recordSetPtr->GetRecordCount();
			}
			catch (_com_error err){
				string errMsg = err.Description();
				return false;
			}
			/*int test = m_recordSetPtr->Fields->Count;*/
			m_recordSetPtr->MoveFirst();
			while (!m_recordSetPtr->adoEOF){
				CDataRow row;
				for (int i = 0; i < m_recordSetPtr->Fields->Count; i++){
					STRING fieldName = m_recordSetPtr->Fields->GetItem((long)i)->Name;
					STRING fieldValue;
					_variant_t value = m_recordSetPtr->GetCollect(fieldName.c_str());
					if (value.vt == VT_NULL){
						fieldValue = emptyField;
					}
					else{
						fieldValue = (_bstr_t)value;
					}
					
					row.Insert(fieldName, fieldValue);
				}
				m_recordSetPtr->MoveNext();
				m_data.push_back(row);
			}
			m_recordSetPtr->Close();
			return true;
		}

		CDataRow operator[](int index){
			return m_data.at(index);
		}

		std::vector<CDataRow> data(){
			return m_data;
		}

		int length(){ return m_length; }
	};

	class Database
	{
	private:
		_ConnectionPtr m_connectionPtr;

		string GetConfig(){
//#ifdef UNICODE
//			wchar_t* PATH = _wgetcwd()
//#else
//
//#endif
			std::ifstream FILE("D:\\config.json");
			std::stringstream buffer;
			buffer << FILE.rdbuf();
			std::string config(buffer.str());
			cJSON* json = cJSON_Parse(config.c_str());
			cJSON* connectionStrObject = cJSON_GetObjectItem(json, "connectionString");
			return connectionStrObject->valuestring;
		}
		dbState m_dbState;
	public:
		bool OpenDefault(){
			_bstr_t _connectionStr = GetConfig().data();
			try{
				m_connectionPtr->Open(_connectionStr, "", "", adModeUnknown);
				this->m_dbState = dbState::open;
				return true;
			}
			catch (_com_error err){
				string errMsg = err.Description();
				this->m_dbState = dbState::close;
				return false;
			}
		}

		bool OpenSpecific(string connectionStr){
			_bstr_t _connectionStr = connectionStr.data();
			try{
				m_connectionPtr->Open(_connectionStr, "", "", adModeUnknown);
				this->m_dbState = dbState::open;
			}
			catch (_com_error err){
				string errMsg = err.Description();
				this->m_dbState = dbState::close;
			}
		}

		bool ExcuteNonQuery(string query){
			_CommandPtr commandPtr;
			HRESULT hr = commandPtr.CreateInstance(__uuidof(Command));
			if (hr != S_OK)
				return false;
			commandPtr->ActiveConnection = m_connectionPtr;
			commandPtr->CommandType = adCmdText;
			commandPtr->CommandText = query.data();
			commandPtr->CommandTimeout = 10;
			commandPtr->Execute(NULL, NULL, adCmdText);
			return true;
		}

		_RecordsetPtr QueryData(string query){
			_CommandPtr commandPtr;
			_RecordsetPtr recordSetPtr;
			HRESULT hr = commandPtr.CreateInstance(__uuidof(Command));
			hr = recordSetPtr.CreateInstance(__uuidof(Recordset));
			if (hr != S_OK)
				return nullptr;
			commandPtr->ActiveConnection = m_connectionPtr;
			commandPtr->CommandType = adCmdText;
			commandPtr->CommandText = query.data();
			commandPtr->CommandTimeout = 10;
			recordSetPtr = commandPtr->Execute(NULL, NULL, adCmdText);
			return recordSetPtr;
		}

		bool ExcuteWithPatameter(string query, _ParameterPtr* parameterPtr,int count){

		}

		bool CloseCurrentConnection(){
			if (m_dbState == dbState::close){
				return false;
			}
			else{
				this->m_connectionPtr->Close();
			}
		}

		CDataTable GetTable(string query){
			CDataTable dataTable;
			dataTable.Open(query, this->m_connectionPtr);
			return dataTable;
		}

		dbState state(){
			return this->m_dbState;
		}

		static Database& GetInstance(){
			static Database _instance;
			return _instance;
		}

		Database(){
			::CoInitialize(NULL);
			this->m_connectionPtr.CreateInstance("ADODB.Connection");
			this->m_dbState = dbState::open;
		};

		~Database(){
			this->m_dbState = dbState::close;
			this->m_connectionPtr->Close();
			this->m_connectionPtr->Release();
			this->m_connectionPtr = nullptr;
			::CoUninitialize();
		};
	};

	class TxtField{
	public:
		std::string FieldName;
		std::string FieldType;
		bool Nullable;
		bool PrimaryKey;
		std::string FieldBytes;
		std::string FieldLength;

		TxtField(){}
		TxtField(std::string fieldName, std::string fieldType, std::string fieldBytes,
			std::string fieldLength, std::string nullable, std::string primaryKey){
			this->FieldName = fieldName;
			this->FieldType = fieldType;
			this->FieldBytes = fieldBytes;
			this->FieldLength = fieldLength;
			this->Nullable = nullable == "1" ? true : false;
			this->PrimaryKey = primaryKey == "1" ? true : false;
		}
	};
}
#endif //_DATABASE_HPP_

