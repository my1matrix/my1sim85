/**
*
* wxform.hpp
*
* - header for main wx-based form
*
**/

#ifndef __MY1FORM_HPP__
#define __MY1FORM_HPP__

#include "my1sim85.hpp"
#include "wxpref.hpp"
#include "wx/wx.h"
#include "wx/aui/aui.h"
#include "wx/stc/stc.h"
#include "wx/grid.h"

#define MY1APP_TITLE "MY1 SIM85"
#define MY1APP_PROGNAME "my1sim85"
#ifndef MY1APP_PROGVERS
#define MY1APP_PROGVERS "build"
#endif
#define PRINT_BPL_COUNT 0x10

#define MY1ID_MAIN_OFFSET wxID_HIGHEST+1
#define MY1ID_DSEL_OFFSET wxID_HIGHEST+501
#define MY1ID_DEVC_OFFSET 0
#define MY1ID_PORT_OFFSET 30
#define MY1ID_DBIT_OFFSET 50
#define MY1ID_CBIT_OFFSET (MY1ID_DSEL_OFFSET+MY1ID_DBIT_OFFSET)

enum {
	MY1ID_EXIT = MY1ID_MAIN_OFFSET,
	MY1ID_MAIN,
	MY1ID_NEW,
	MY1ID_LOAD,
	MY1ID_SAVE,
	MY1ID_SAVEAS,
	MY1ID_VIEW_REGSPANE,
	MY1ID_VIEW_DEVSPANE,
	MY1ID_VIEW_INTRPANE,
	MY1ID_VIEW_CONSPANE,
	MY1ID_ASSEMBLE,
	MY1ID_SIMULATE,
	MY1ID_GENERATE,
	MY1ID_OPTIONS,
	MY1ID_ABOUT,
	MY1ID_FILETOOL,
	MY1ID_EDITTOOL,
	MY1ID_PROCTOOL,
	MY1ID_STAT_TIMER,
	MY1ID_SIMX_TIMER,
	MY1ID_CONSCOMM,
	MY1ID_CONSEXEC,
	MY1ID_SIMSEXEC,
	MY1ID_SIMSSTEP,
	MY1ID_SIMSINFO,
	MY1ID_SIMSPREV,
	MY1ID_SIMRESET,
	MY1ID_SIMSMIMV,
	MY1ID_SIMSBRKP,
	MY1ID_SIMSEXIT,
	MY1ID_BUILDINIT,
	MY1ID_BUILDRST,
	MY1ID_BUILDDEF,
	MY1ID_BUILDNFO,
	MY1ID_BUILDROM,
	MY1ID_BUILDRAM,
	MY1ID_BUILDPPI,
	MY1ID_BUILDOUT,
	MY1ID_CREATE_MINIMV,
	MY1ID_CREATE_DV7SEG,
	MY1ID_CREATE_DEVLED,
	MY1ID_DUMMY
};

struct my1BitSelect
{
	int mDevice;
	int mDevicePort;
	int mDeviceBit;
	void* mPointer;
	my1BitSelect():mDevice(0),mDevicePort(0),mDeviceBit(0),mPointer(0x0){}
	my1BitSelect(int anIndex) { this->UseIndex(anIndex); }
	void UseIndex(int anIndex)
	{
		mDeviceBit = anIndex%I8255_DATASIZE;
		anIndex = anIndex/I8255_DATASIZE;
		mDevicePort = anIndex%(I8255_SIZE-1);
		mDevice = anIndex/(I8255_SIZE-1);
		mPointer = 0x0;
	}
	int GetIndex(void)
	{
		int cIndex = mDevice*(I8255_SIZE-1)*I8255_DATASIZE;
		cIndex += mDevicePort*I8255_DATASIZE;
		cIndex += mDeviceBit;
		return cIndex;
	}
	void operator=(my1BitSelect& aSelect)
	{
		mDevice = aSelect.mDevice;
		mDevicePort = aSelect.mDevicePort;
		mDeviceBit = aSelect.mDeviceBit;
		mPointer = aSelect.mPointer;
	}
};

struct my1MiniViewer
{
	int mStart, mSize;
	my1Memory* pMemory;
	wxGrid* pGrid;
	my1MiniViewer* mNext;
	bool IsSelected(int anAddress)
	{
		if(anAddress>=this->mStart&&anAddress<this->mStart+this->mSize)
			return true;
		return false;
	}
};

class my1BITCtrl
{
protected:
	my1BitSelect mLink;
public:
	my1BitSelect& Link(void) { return mLink; }
	void Link(my1BitSelect& aLink) { mLink = aLink; }
	virtual void LinkThis(my1BitIO* aBitIO) = 0;
	void LinkCheck(my1BitSelect& aLink)
	{
		if(aLink.mPointer==mLink.mPointer) { mLink.mPointer = 0x0; return; }
		this->LinkThis((my1BitIO*)aLink.mPointer);
		this->Link(aLink);
	}
};

class my1Form : public wxFrame
{
private:
	friend class my1CodeEdit;
	bool mSimulationRunning, mSimulationStepping;
	double mSimulationCycle, mSimulationCycleDefault; // smallest time res?
	unsigned long mSimulationDelay; // in microsec!
	bool mSimulationMode, mBuildMode;
	my1Sim85 m8085;
	my1SimObject mFlagLink[I8085_BIT_COUNT];
	my1MiniViewer *mFirstViewer;
	wxAuiManager mMainUI;
	my1Options mOptions;
	wxTimer* mDisplayTimer;
	wxTimer* mSimExecTimer;
	wxAuiNotebook *mNoteBook;
	wxTextCtrl *mConsole;
	wxTextCtrl *mCommand;
	wxMenu *mDevicePopupMenu;
public:
	my1Form(const wxString& title);
	~my1Form();
	void CalculateSimCycle(void);
	bool ScaleSimCycle(double);
	double GetSimCycle(void);
	unsigned long GetSimDelay(void); // in microsec!
	void SimulationMode(bool aGo=true);
	void BuildMode(bool aGo=true);
	bool IsFloatingWindow(wxWindow*);
protected:
	wxAuiToolBar* CreateFileToolBar(void);
	wxAuiToolBar* CreateEditToolBar(void);
	wxAuiToolBar* CreateProcToolBar(void);
	wxBoxSizer* CreateFLAGView(wxWindow*,const wxString&,int);
	wxBoxSizer* CreateREGSView(wxWindow*,const wxString&,int);
	wxBoxSizer* CreateLEDView(wxWindow*,const wxString&,int);
	wxBoxSizer* CreateSWIView(wxWindow*,const wxString&,int);
	wxBoxSizer* CreateINTView(wxWindow*,const wxString&,int);
	wxPanel* CreateMainPanel(wxWindow*);
	wxPanel* CreateRegsPanel(void);
	wxPanel* CreateDevsPanel(void);
	wxPanel* CreateIntrPanel(void);
	wxPanel* CreateConsPanel(void);
	wxPanel* CreateSimsPanel(void);
	wxPanel* CreateBuildPanel(void);
	wxPanel* CreateConsolePanel(wxWindow*);
	wxPanel* CreateMemoryPanel(wxWindow*);
	wxPanel* CreateMemoryGridPanel(wxWindow*,int,int,int,wxGrid**);
	wxPanel* CreateDevice7SegPanel(int);
	wxPanel* CreateDeviceLEDPanel(void);
public:
	void OpenEdit(wxString&);
	void SaveEdit(wxWindow*, bool aSaveAs=false);
	void ShowStatus(wxString&);
	void OnQuit(wxCommandEvent &event);
	void OnNew(wxCommandEvent &event);
	void OnLoad(wxCommandEvent &event);
	void OnSave(wxCommandEvent &event);
	void OnAbout(wxCommandEvent &event);
	void OnAssemble(wxCommandEvent &event);
	void OnSimulate(wxCommandEvent &event);
	void OnGenerate(wxCommandEvent &event);
	void PrintMemoryContent(aword, int aSize=PRINT_BPL_COUNT);
	void PrintPeripheralInfo(void);
	void PrintSimInfo(void);
	void PrintConsoleMessage(const wxString&);
	void PrintSimChangeStart(unsigned long,bool anError=false);
	void PrintBuildAdd(const wxString&, unsigned long);
	void PrintHelp(void);
	void PrintUnknownCommand(const wxString&);
	void PrintUnknownParameter(const wxString&,const wxString&);
	void OnExecuteConsole(wxCommandEvent &event);
	void OnSimulationPick(wxCommandEvent &event);
	void OnSimulationInfo(wxCommandEvent &event);
	void OnSimulationExit(wxCommandEvent &event);
	int GetBuildAddress(const wxString&);
	void OnBuildSelect(wxCommandEvent &event);
	void OnClosePane(wxAuiManagerEvent &event);
	void OnShowPanel(wxCommandEvent &event);
	void CreateMiniMV(int);
	void CreateDv7SEG(int);
	void CreateDevLED(void);
	void OnCheckOptions(wxCommandEvent &event);
	void OnStatusTimer(wxTimerEvent &event);
	void OnSimExeTimer(wxTimerEvent &event);
	void OnPageChanging(wxAuiNotebookEvent &event);
	void OnPageChanged(wxAuiNotebookEvent &event);
	void OnPageClosing(wxAuiNotebookEvent &event);
	my1BitIO* GetDeviceBit(my1BitSelect&);
	bool UnlinkDeviceBit(my1BitIO*);
	wxMenu* GetDevicePopupMenu(void);
	void ResetDevicePopupMenu(void);
	void SimUpdateFLAG(void*);
	my1SimObject& FlagLink(int);
public: // 'wrapper' function
	bool SystemDefault(void);
	bool SystemReset(void);
	bool AddROM(int aStart=I2764_INIT);
	bool AddRAM(int aStart=I6264_INIT);
	bool AddPPI(int aStart=I8255_INIT);
public:
	static void SimUpdateREG(void*);
	static void SimUpdateMEM(void*);
	static void SimDoUpdate(void*);
	static void SimDoDelay(void*,int);
};

#endif
