#define  _CRT_SECURE_NO_WARNINGS 1
#define  _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 1
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <chrono>
#include <string>
#include <iostream>
#include <fstream>
#include <locale>
#include <codecvt>


#include "uevr/Plugin.hpp"

#define MAX_CVARS	256
#define MAX_ELEMENT_LEN 128
#define MAX_PATH_SIZE 512
#define MAX_LINE_SIZE 256

#if 0
on_initialize() {
    m_next_time = std::chrono::high_resolution_clock::now() + std::chrono::seconds(10);
}

on_pre_engine_tick() {
    const auto now = std::chrono::high_resolution_clock::now();

    if (now >= m_next_time) {
        do_thing();
    }
}
#endif

using namespace uevr;

#define PLUGIN_LOG_ONCE(...) \
    static bool _logged_ = false; \
    if (!_logged_) { \
        _logged_ = true; \
        API::get()->log_info(__VA_ARGS__); \
    }
	
typedef enum _ACTION_TYPE
{
	ACTION_CVAR=0,
	ACTION_EXEC,
	ACTION_DELAY
} ACTION_TYPE;

typedef struct _CVAR_ITEM CVAR_ITEM;

struct _CVAR_ITEM
{
	std::wstring Command;
	std::wstring Value;
	ACTION_TYPE	Action;
	CVAR_ITEM* Next;
};

class CVarPlugin : public uevr::Plugin {
public:
	CVAR_ITEM* m_Head = NULL;
	CVAR_ITEM* m_Current = NULL;
	
    std::string m_Path;
	
    CVarPlugin() = default;

    void on_dllmain(HANDLE handle) override {
        StoreConfigFileLocation(handle);
    }

    void on_initialize() override {
      API::get()->log_info("Cvar.dll: Config file should be: %s\n", m_Path.c_str());  
	  
      ReadConfig();
    }
  
    void on_pre_engine_tick(API::UGameEngine* engine, float delta) override {
        PLUGIN_LOG_ONCE("Cvar.dll: Pre Engine Tick: %f", delta);

        static bool once = true;

        // Unit tests for the API basically.
        if (once) {
            once=false;
			ApplyCvarScript();
		}
	}
	
	//***************************************************************************************************
	// Applies the actual cvar script called from the thread callback.
	//***************************************************************************************************
	void ApplyCvarScript() {
		CVAR_ITEM* Item = m_Head;
		CVAR_ITEM* Next = m_Head;
		int i = 0;
		
		API::get()->log_info("ApplyCvarScript: Applying the cvars script");
		while(Item != NULL) {
			API::get()->log_info("ApplyCvarScript: Working on %ls=%ls", Item->Command.c_str(), Item->Value.c_str());

            const auto console_manager = API::get()->get_console_manager();

			if (console_manager != nullptr) {
				
				// Check for delay first
				if(Item->Command == L"delay") {
					int DelayValue = std::stoi(Item->Value);
					API::get()->log_info("ApplyCvarScript: sleeping %d milliseconds", DelayValue);
					Sleep(DelayValue);
					
				// Check for exec command next.	
				} else if(Item->Command == L"exec") {
					API::get()->log_info("ApplyCvarScript: running exec command '%ls'", Item->Value.c_str());
					API::get()->sdk()->functions->execute_command(Item->Value.c_str());
					
				// Check cvar next	
				} else {
					auto cvar = console_manager->find_variable(Item->Command.c_str());
					if (cvar == nullptr) {
						API::get()->log_info("ApplyCvarScript: CVAR '%ls' is not found, so not applied", Item->Command.c_str());
					} else {
						API::get()->log_info("ApplyCvarScript: Setting cvar '%ls' to '%ls'", Item->Command.c_str(), Item->Value.c_str());
						cvar->set(Item->Value);
					}
				}
			}
			
			Item = Item->Next;
		}
		
		// We're done with the list, free it all up.
		API::get()->log_info("ApplyCvarScript: freeing the list");
		Item = m_Head;
		while(Item != NULL)
		{
			Next = Item->Next;
			free(Item);
			Item = Next;
		}
		
	}
	
	//***************************************************************************************************
	// Stores the path and file location of the cvar.txt config file.
	//***************************************************************************************************
	void StoreConfigFileLocation(HANDLE handle) {
		wchar_t wide_path[MAX_PATH]{};
		if (GetModuleFileNameW((HMODULE)handle, wide_path, MAX_PATH)) {
			const auto path = std::filesystem::path(wide_path).parent_path() / "cvars.txt";
			m_Path = path.string(); // change m_Path to a std::string
		}
	}	
	
	//***************************************************************************************************
	// Reads the config file cvars.txt and stores it in a linked list of CVAR_ITEMs.
	//***************************************************************************************************
    void ReadConfig() {
		std::string Line;
		
		CVAR_ITEM* CvarItem;
		int Length = 0;
		int i = 0;
        int LineNumber = 0;
		size_t Pos = 0;
		
		std::wstring_convert<std::codecvt_utf8<wchar_t>> Converter;
		

		std::ifstream fileStream(m_Path.c_str());
		if(!fileStream.is_open()) {
			API::get()->log_error("cvars.dll: cvars.txt cannot be opened");
			return;
		}
		
			
		while (std::getline(fileStream, Line)) {
            LineNumber++;

			Length = static_cast<int>(Line.length());

			if(Line[0] == '#') continue;
			if(Line[0] == ' ') continue;
			if(Length < 3) continue;
			
			// Strip  spaces, carriage returns from line.
			Pos = Line.find_last_not_of(" \r\n");
			if(Pos != std::string::npos) {
				Line.erase(Pos + 1);
			}

			Pos = Line.find('=');
			if(Pos == std::string::npos) {
				API::get()->log_info("cvars.dll: Invalid line, no = sign found or nothing after = found");   
				continue;
			}

			//API::get()->log_info("cvars.dll: Line %d was %d long", LineNumber, Length);   
			API::get()->log_info("cvars.dll: Processing config line: %s", Line.c_str());   

			// At this point, we are convinced we have a valid entry, create a new CVAR_ITEM* to store it.
			CvarItem = (CVAR_ITEM*)malloc(sizeof(CVAR_ITEM));	// Allocate new CVAR_ITEM to store this entry.
			if(CvarItem == NULL) continue;						// If malloc fails, move on.
			ZeroMemory(CvarItem, sizeof(CVAR_ITEM));			// Clear the memory
			if(m_Head == NULL) m_Head = CvarItem;  				// Set head to start of the list
			if(m_Current != NULL) m_Current->Next = CvarItem;	// Set the new element to the next of the list
			m_Current = CvarItem;								// Move the current pointer to this element.
			CvarItem->Next = NULL;
			
			// variable i contains location in the string of the = sign.
			CvarItem->Command = Converter.from_bytes(Line.substr(0, Pos));
			CvarItem->Value = Converter.from_bytes(Line.substr(Pos + 1, MAX_ELEMENT_LEN));
			
			if(CvarItem->Command == L"delay") CvarItem->Action = ACTION_DELAY;
			else if(CvarItem->Command == L"exec") CvarItem->Action = ACTION_EXEC;
			else CvarItem->Action = ACTION_CVAR;
			
			API::get()->log_info("cvars.dll: Added entry command: %ls, value: %ls, type:%d", CvarItem->Command.c_str(), CvarItem->Value.c_str(), CvarItem->Action);
		}		
		
		fileStream.close();
	}


};
// Actually creates the plugin. Very important that this global is created.
// The fact that it's using std::unique_ptr is not important, as long as the constructor is called in some way.
std::unique_ptr<CVarPlugin> g_plugin{new CVarPlugin()};

