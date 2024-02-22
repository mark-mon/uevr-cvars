#define  _CRT_SECURE_NO_WARNINGS 1
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <memory>

#include "uevr/Plugin.hpp"

#define MAX_CVARS	256
#define MAX_ELEMENT_LEN 64
#define MAX_PATH_SIZE 512
#define MAX_LINE_SIZE 256


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
	wchar_t Command[MAX_ELEMENT_LEN];
	wchar_t Value[MAX_ELEMENT_LEN];
	ACTION_TYPE	Action;
	CVAR_ITEM* Next;
};

class CVarPlugin : public uevr::Plugin {
public:
	CVAR_ITEM* m_Head = NULL;
	CVAR_ITEM* m_Current = NULL;
	int	m_ConfigSize = 0;
	bool m_ConfigRead = false;
	
    char m_Path[MAX_PATH_SIZE];
	
    CVarPlugin() = default;

    void on_dllmain(HANDLE handle) override {
		ZeroMemory(m_Path, MAX_PATH_SIZE);
        StoreConfigFileLocation(handle);
    }

    void on_initialize() override {
      API::get()->log_info("Cvar.dll: Config file should be: %s\n", m_Path);  
	  
      ReadConfig();
    }
  
    void on_pre_engine_tick(API::UGameEngine* engine, float delta) override {
        PLUGIN_LOG_ONCE("Cvar.dll: Pre Engine Tick: %f", delta);

        static bool once = true;

        // Unit tests for the API basically.
        if (once) {
			DWORD ThreadId  = 0;
            once = false;
			
			// Create a separate thread to handle the cvar setting routine so we don't take up time
			// in the render thread especially since we can have delays added by the user in the script.
			CreateThread(NULL,
						 0,
						 &TimerCallbackThreadProc,
						 this,
						 0,
						 &ThreadId);
		}
	}
	
	//***************************************************************************************************
	// Running the cvars as a thread so that we don't hang the engine tick function.
	//***************************************************************************************************
    static DWORD WINAPI TimerCallbackThreadProc(LPVOID lpParameter) {
		((CVarPlugin*)lpParameter)->ApplyCvarScript();
		return 0;
    }
  
	//***************************************************************************************************
	// Applies the actual cvar script called from the thread callback.
	//***************************************************************************************************
	void ApplyCvarScript() {
		CVAR_ITEM* Item = m_Head;
		int i = 0;
		
		API::get()->log_info("ApplyCvarScript: Applying the cvars script");
		while(Item != NULL) {
			API::get()->log_info("ApplyCvarScript: Working on %ls=%ls", Item->Command, Item->Value);

            const auto console_manager = API::get()->get_console_manager();

            if (console_manager != nullptr) {
				if (console_manager != nullptr) {
					
					// Check for delay first
					if(_wcsicmp(Item->Command, L"delay") == 0) {
						wchar_t* EndPtr;
						long DelayValue = wcstol(Item->Value, &EndPtr, 10);
	
						if (EndPtr != Item->Value) {
							API::get()->log_info("ApplyCvarScript: sleeping %ld milliseconds", DelayValue);
							
							Sleep(DelayValue);
						}
						
					// Check for exec command next.	
					} else if(_wcsicmp(Item->Command, L"exec") == 0) {
						API::get()->log_info("ApplyCvarScript: running exec command '%ls'", Item->Value);
						API::get()->sdk()->functions->execute_command(Item->Value);
						
					// Check cvar next	
					} else {
						auto cvar = console_manager->find_variable(Item->Command);
						if (cvar == nullptr) {
							API::get()->log_info("ApplyCvarScript: CVAR '%ls' is not found, so not applied", Item->Command);
						} else {
							API::get()->log_info("ApplyCvarScript: Setting cvar '%ls' to '%ls'", Item->Command, Item->Value);
							cvar->set(Item->Value);
						}
					}
				}
			}
			
			Item = Item->Next;
		}
		
	}
	
	//***************************************************************************************************
	// Stores the path and file location of the cvar.txt config file.
	//***************************************************************************************************
	void StoreConfigFileLocation(HANDLE handle) {
		int i = 0;
		GetModuleFileName((HMODULE)handle, m_Path, MAX_PATH_SIZE);
		for(i = (int)(strlen(m_Path) - 1); i>0; i--) {
		  if(m_Path[i] == '\\') {
			m_Path[i+1] = 0;
			break;
		  }
		}

		strcat(m_Path, "cvars.txt");
	}
	
	
	//***************************************************************************************************
	// Reads the config file cvars.txt and stores it in a linked list of CVAR_ITEMs.
	//***************************************************************************************************
    void ReadConfig() {
		wchar_t Line[MAX_LINE_SIZE];
		CVAR_ITEM* CvarItem;
		int Numeric = 0;
		int EqualsPosition = 0;
		int Length = 0;
		int i = 0;
		

		FILE* fp = fopen(m_Path,"r");
		if (fp == NULL){
			API::get()->log_info("cvars.dll: cvars.txt cannot be opened");
			m_ConfigRead = true;
			return;
		}
			
		while(!feof(fp)) {
			ZeroMemory(Line, sizeof(wchar_t) * MAX_LINE_SIZE);
			if(fgetws(Line, MAX_LINE_SIZE-1, fp) == NULL) break;

			Length = (int)wcslen(Line);

			if(Line[0] == L'#') continue;
			if(Length < 3) continue;
			if(Line[0] == L' ') continue;

			API::get()->log_info("cvars.dll: Processing config line: %ls", Line);   

			// Find the = sign index
			for(i=0; i<Length; i++) {
				if(Line[i] == L'=') break;
			}

			if(i+1 >= Length) {
				API::get()->log_info("cvars.dll: Invalid line, no = sign found or nothing after = found");   
				continue;
			}

			// At this point, we are convinced we have a valid entry, create a new CVAR_ITEM* to store it.
			CvarItem = (CVAR_ITEM*)malloc(sizeof(CVAR_ITEM));	// Allocate new CVAR_ITEM to store this entry.
			if(CvarItem == NULL) continue;						// If malloc fails, move on.
			ZeroMemory(CvarItem, sizeof(CVAR_ITEM));			// Clear the memory
			if(m_Head == NULL) m_Head = CvarItem;  				// Set head to start of the list
			if(m_Current != NULL) m_Current->Next = CvarItem;	// Set the new element to the next of the list
			m_Current = CvarItem;								// Move the current pointer to this element.
			CvarItem->Next = NULL;
			
			// variable i contains location in the string of the = sign.
			wcsncpy(CvarItem->Command, Line, i);
			wcsncpy(CvarItem->Value, &(Line[i+1]), MAX_ELEMENT_LEN);
			
			//Strip /r, /n, space off end of value.
			for(i = (int)wcslen(CvarItem->Value)-1; i>0; i--) {
				if(CvarItem->Value[i] == L'\r' || CvarItem->Value[i] == L'\n' || CvarItem->Value[i] == L' ')
					CvarItem->Value[i] = 0;
				else
					break;
			}
			
			// Log the type of entry.
			if(_wcsicmp(CvarItem->Command, L"delay") == 0) CvarItem->Action = ACTION_DELAY;
			else if(_wcsicmp(CvarItem->Command, L"exec") == 0) CvarItem->Action = ACTION_EXEC;
			else CvarItem->Action = ACTION_CVAR;
			
			m_ConfigSize++;
			API::get()->log_info("cvars.dll: Added entry command: %ls, value: %ls, type:%d", CvarItem->Command, CvarItem->Value, CvarItem->Action);
		}		
		fclose(fp);
		m_ConfigRead = true;
	}


};
// Actually creates the plugin. Very important that this global is created.
// The fact that it's using std::unique_ptr is not important, as long as the constructor is called in some way.
std::unique_ptr<CVarPlugin> g_plugin{new CVarPlugin()};

