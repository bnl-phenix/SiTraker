#ifndef dq_H
#define dq_H

#ifdef _Win32
// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN             
// Windows Header Files:
#include <windows.h>
#endif

#include <TFile.h>
#include <TTree.h>
#include <TH1S.h>
#include <TH2S.h>

#include "globals.h"

#define DQ_EOE 0x0DFE

#define ERRDQ_IO -1
#define ERRDQ_STAMP 2

// &RA 20130903//disable this error checking by now
#define ERRDQ_CRC 4
#define ERRDQ_EVSIZE 8
#define ERRDQ_NOEOE 0x10	// bit4
// &RA 20130903//disable this error checking by now
//#define ERRDQ_CELLN 0x20	// bit5
#define ERRDQ_CMNOISE 0x40	// bit6
#define ERRDQ_0CHIP 0x80	// but7
#define ERRDQ_FCELLN_0 0x100	//bit8
#define ERRDQ_FCELLN_1 0x200	//bit9
#define ERRDQ_FCELLN_2 0x400	//bit10
#define ERRDQ_FCELLN_3 0x800	//bit11

//Added by Arbin
#define MAX_STRIP_IN_PLANE ASICS_IN_CHAIN*CH_IN_ASIC

typedef Short_t CHV_t;
//typedef UChar_t CHV_t;

class Tdq : public TFile 
{
private:
	//&RA/Char_t fname[1024];
	TTree	*ftree;
	Char_t	fname[256];
	Char_t	*fhtitle;
	FILE    *fD; //file descriptor
	TFile   *ffile;
	Int_t	fsize;
	Int_t	fevcount;
	UInt_t	fevnum;
	Int_t	fnchn; // length of the longest chain
	UShort_t	fchn[ASICS_IN_CHAIN*CH_IN_ASIC];
	Int_t flchain[NCHAINS];
	CHV_t	fchv[NCHAINS][MAX_STRIP_IN_PLANE];
	CHV_t	fcmnoise[NCHAINS][6];
	//$RA/made global in ,cxx/TTree	*gtree;

	//Added by Arbin
	Int_t strip[MAX_STRIP_IN_PLANE];
	Float_t ped[NCHAINS][MAX_STRIP_IN_PLANE];
	Int_t status[NCHAINS][MAX_STRIP_IN_PLANE];
	Float_t hitadc[NCHAINS][MAX_STRIP_IN_PLANE];
	TH2S *histoadc[NCHAINS];
	Int_t hitn[NCHAINS][MAX_STRIP_IN_PLANE];
	Int_t hitc[NCHAINS][MAX_STRIP_IN_PLANE];
	Int_t module;
	Int_t nmodule;
	
	#define DQ_MINHDR 14
	UChar_t f1sthdr[DQ_MINHDR];
	UChar_t fhdr[DQ_MINHDR];
	
	//elements of the tree
	ULong_t fpos; // file position	
	UShort_t fcrc;
	UChar_t fevhl,fevtl;
	UChar_t fevnasics,fevchains;
        Int_t	fentry;
	UChar_t fcelln[NCHAINS][6]; // bunch number
	UShort_t fbclk, fbclkx;
	UShort_t fPARst_ExTrig; // time (in bclk*8) from preamp reset to external trigger, the bits [2:0] also could be used as clockphase of the ExTrig
	UChar_t fclkphase; // clock phase of the trigger 0:7
	ULong_t fevsize;    // event length		
	ULong_t ferr;   // error. each bit has its meaning
	ULong_t ferrcount; // number off error events
	Int_t	fnerr;	// number of errors in event
	ULong_t fclkprev;
	ULong_t fclkdiff;
	//ULong_t ftimediff,fprevtime; // time difference with prev event
	//ULong_t ftime80MHz;	// 80MHz timer counter, reset by external signal 
	//ULong_t ftime80MHzDiff, fprevtime8;
	TH2S *fhchns[NCHAINS];
	static Float_t gCMNQuantile;
	static Int_t gCMNLimit;
	static Int_t gCMNControl;
	
	UChar_t Bin2Gray(UChar_t gray);
	Int_t GetHeader();
	Int_t FindEOE(); // position file to next event
	Int_t Process();
	Int_t Next();
	Int_t UpdateCRC(UShort_t*, UShort_t* ptr, Int_t len);
	void	DoCMNoise(Int_t chain);

  public:
	static	Int_t gDebug;
	void FillErr();
	TH1I *fherr;
	TArrayI fchnmap;

	Tdq(Char_t *name="", Int_t cmnproc_mode=0, Char_t *htitle="");
	~Tdq();
	//Bool_t IsOpen();
	//void SetChnMap(Int_t ich, Int_t pedestal);
	Bool_t IsOpen();

	//Added by Arbin
	void Swap(Int_t svx4ch, Int_t stripch);
        void SetPed(Int_t chain, Int_t ich, Float_t pedestal, Int_t status);

	Char_t* GetFileName() {return fname;}
	Int_t GetLChain(Int_t chain) {return (flchain[chain]);}
	TH2S* GetChain(Int_t chain) {return (fhchns[chain]);}
	TH2S* GetHits(Int_t chain) {return (histoadc[chain]);}
	TTree* MakeTree(Int_t mode);
	void SetCMNoiseQuantile(Float_t value) {gCMNQuantile = value;}
	//gCmnLimit is a level of the adc[quantile], above which an error 
	//will be assigned to the event
	void SetCMNoiseLimit(Int_t value) {gCMNLimit = value;}
	void SetCMNoiseControl(Int_t value);
	Int_t GetCMNoiseControl() {return gCMNControl;}
	ClassDef(Tdq,0) // processing class for PHENIX NCC data 
};
#endif
