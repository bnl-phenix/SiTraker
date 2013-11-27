/*
    2013-09-11	Andrey Sukhanov
    
    *version 3 Analysis package for *.dq4 files, adopted from previous year.
    added PARst_ExTrig - delay between PARst and the external trigger

    2013-10-31	AS
*  - Binning of fhchns corrected xrange changed to 0,256. 
*  - Printing FPGA version.
*  - hex position for 'EOE not found'

    2013-11-01	AS
*  - Take care of EOE at the end of file

 2013-11-04	AS
*  - Better bookkeeping of errors.
   FindEOE prints corrected excess.

 2013-11-08	AS
*  - The EOD stamp is tested first in the event processing.
   - fpos is set only in the GetHeader and it points to the beginning of event.

TODO:
  add fevnum difference in the tree
*/

#include "globals.h"

//&RA/131108/ Instead of recompiling for DBG, use gDebug memmber.
// To use it in root instantiate 'side' Tdq just to access its static member gDebug
// i.e. Tdq* gg = new Tdq(""); and then manipulate gg.gDebug
// Uncomment the next line for debugging, if DBG>1 the printout will be very intense
//#define DBG 1

//#ifdef DBG
#define MAXERRPRINT 1000000
//#else
//#define MAXERRPRINT 4
//#endif 

//Uncoment the next line (and also in the process_file.C) to switch from svx4 channel number to strip number
//#define SWAP 1

// for ntohl
//#ifndef WIN32
//#include <arpa/inet.h> 
//#else
//#include <winsock2.h> 
//#endif

#include <TROOT.h>
#include <TSystem.h>
#include <TTree.h>
#include <TH2S.h>
#include <TMath.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <sys/stat.h> // for stat()

using namespace std;
using namespace TMath;

#include "Tdq.h"

Int_t Tdq::gDebug=0;
Int_t dbg_cmnoise=0;
Int_t   Tdq::gCMNLimit = 40;	// this rejects 10% of events
Int_t   Tdq::gCMNControl = 0;	// &1: call DoCMNoise, &2: subtract CMNoise
Float_t Tdq::gCMNQuantile = 0.25;

void Tdq::Swap(Int_t svx4ch, Int_t stripch)
{
	if(svx4ch<=MAX_STRIP_IN_PLANE)
	strip[svx4ch] = stripch;
}


void Tdq::SetPed(Int_t chain, Int_t ich, Float_t pedestal, Int_t stats)
{
	if(ich<=MAX_STRIP_IN_PLANE)
	{
	ped[chain][ich]=pedestal;
	status[chain][ich]=stats;
	}
	
}

void Tdq::SetCMNoiseControl(Int_t value)
{
	printf("CMNControl is changed from %x to %x\n",
			gCMNControl,value);
	gCMNControl = value;
}

UChar_t Tdq::Bin2Gray(UChar_t gray)
{
	UChar_t rc=gray;
	rc ^= (gray>>1)&0x7f;
	return rc;
}
Int_t Tdq::UpdateCRC(UShort_t *crc, UShort_t* ptrs, Int_t len)
{
    Int_t ii;
    //UShort_t *ptr = (UShort_t*)ptrb;
    for(ii=0;ii<len;ii++) *crc ^= *ptrs++;
    return 0;
}
#define DATA_ARE_ADC ((f1sthdr[2])&0x80)

Int_t Tdq::GetHeader()
{
    Int_t ii;
    //UChar_t hdr[DQ_MINHDR];
    UShort_t ww;
    fpos = ftell(fD);
    Int_t nitems = fread(fhdr, sizeof(fhdr), 1, fD);
    if(nitems!=1) return ERRDQ_IO;
    if(gDebug) {
    printf("fhdr @ 0x%lx:",fpos);
    for(ii=0;ii<(Int_t)sizeof(fhdr);ii++) printf("%02x ",fhdr[ii]);
    printf("\n");
    }//#endif
    if((fhdr[4] != 0xf0) || (fhdr[5] != 0xc1)) 
    {
	ferr |= ERRDQ_STAMP;
	if(ferrcount<MAXERRPRINT)printf("ERRDQ_STAMP %02x%02x != F0C1 @ %08lx\n",fhdr[4],fhdr[5],fpos);
	FillErr();
	return ferr;
    }
    // header
    fcrc = 0;
    UpdateCRC(&fcrc,(UShort_t*)fhdr,sizeof(fhdr)/2);
    if(fevhl == 0) 
    {
	// First event
	for(ii=0;ii<DQ_MINHDR;ii++) f1sthdr[ii]=fhdr[ii];
	//estimate event length
	fevhl = f1sthdr[11]&0xf;
	fevtl = ((f1sthdr[11])>>4)&0xf;
	fevnasics = ((f1sthdr[3])>>4)&0xf;
	fevchains = f1sthdr[3]&0xf;
	ii = fevnasics*CH_IN_ASIC;
	printf("header constructed hl=%i, tl=%i, vers=%02x, na=%i, chain_flag=0x%1x[%i], data are ",
		fevhl,fevtl,f1sthdr[10],fevnasics,fevchains,ii);
	if(DATA_ARE_ADC) printf("ADC\n"); else printf("Channel numbers\n");
	flchain[0] = (fevchains&1)?ii:0;
	flchain[1] = (fevchains&2)?ii:0;
	flchain[2] = (fevchains&4)?ii:0;
	flchain[3] = (fevchains&8)?ii:0;
	//if(ftree==NULL)
	TString ss = GetFileName();
	{
	    TString hname;
	    TString mname;
	    TString htitle = fhtitle;
	    htitle += ", run "; 
	    ii = ss.Last('/')+1;
	    ss = ss(ii, ss.Last('.')-ii);
	    htitle += ss.Data();
	    htitle += ", CMN="; htitle += gCMNControl;
	    for(ii=0;ii<NCHAINS;ii++)
	    {
		if(flchain[ii]==0) continue;
		hname = "hchain";
		hname +=ii;
		mname = "adchain";
		mname +=ii;
		printf("Creating %s[%i]\n",hname.Data(),flchain[ii]);
		fhchns[ii] = new TH2S(hname,htitle,flchain[ii],0,flchain[ii],256,0,256);
		histoadc[ii] = new TH2S(mname,htitle,flchain[ii],0,flchain[ii],256,0,256);
	    }
	}
    }

    // The real header could be longer than the pre-defined one
    // skip the rest of the header
    for(ii=sizeof(fhdr)/2;ii<fevhl;ii++)
    {
	nitems = fread(&ww, sizeof(ww), 1, fD);
	if(gDebug) { 
		printf("Skipping %04x\n",ww);
	}//#endif
	fcrc ^= ww;
    }
    return 0;
}
Int_t Tdq::FindEOE()
{
    UShort_t ww;
    Int_t ii,nn,rc=0;
    
#define FINDEOE_RANGE 4000
//#define FINDEOE_ROLLBACK 32
//    fseek(fD,-(fevhl+FINDEOE_ROLLBACK)*2,SEEK_CUR);	// roll back 16 bytes to deal with possible unexpected padding
    fseek(fD,fpos+sizeof(fhdr),SEEK_SET); // roll back to past header, this is to correctly handle empty events
    for(ii=0;ii<FINDEOE_RANGE;ii++)
    {
	  nn = fread(&ww, sizeof(ww), 1, fD);
	  if(nn!=1) {
//if(gDebug) {
		  printf("ERRDQ_IO nn = %d\n",nn);
		  perror("fread");
//}//#endif
		return ERRDQ_IO;
	  }
	  if(ww == DQ_EOE) break;
    }

    if(ii>=FINDEOE_RANGE) 
	rc |= ERRDQ_NOEOE;
    else
    	nn = fread(&ww, sizeof(ww), 1, fD);//skip CRC to position for next event
    //fpos = ftell(fD);
    //#ifdef DBG
    //printf("EOE not found. FindEOE=%i excess=%i @ %lx\n",rc,FINDEOE_ROLLBACK-ii,ftell(fD));
    printf("EOE not found. FindEOE=%i skipped %i bytes @ %lx\n",rc,ftell(fD)-fpos,ftell(fD));
    //#endif
    return rc;
}
void	Tdq::FillErr()
{
	Int_t ii,ib;
	{
		ferrcount++;
        if(ferrcount==MAXERRPRINT) printf("Too many errors, will not print anymore\n");
        for(ii=0,ib=1;ii<32;ii++)
        {
            if(ferr&ib) fherr->Fill(ii);
            ib = ib<<1;
        }
    }
}
Int_t Tdq::Process()
{
    Int_t ii,chain,nn,bytes_per_chip=NCHAINS*(CH_IN_ASIC+1);
    #define MAXICH MAXCH+64
    UShort_t body[MAXICH],nw;
    UChar_t *bbody = (UChar_t*)body;
	UChar_t byte;
    ULong_t cclk=0;
    
    ferr = 0;
    // get body of the event
    //&RA 130903/nw = fevtl + fevnasics*bytes_per_chip/2 +2;// the +2 could be an artifact in the hardware
    nw = fevtl + fevnasics*bytes_per_chip/2 +1;
    if(gDebug) {
    printf("At %lx, Expected nw=%04x, hdr crc=0x%04x\n",ftell(fD),nw,fcrc);
    }//#endif
    nn = fread(body, sizeof(UShort_t), nw, fD);
    if(nn!=nw)
    {
	printf("READ ERROR in Process @ %lx\n",ftell(fD));
	return ERRDQ_IO;
    }
    //fpos = ftell(fD);
    // Check if trailer is correct
    if(body[nw-2] != DQ_EOE)
    {
        ferr |= ERRDQ_EVSIZE;
	printf("ERROR ERRDQ_EVSIZE %04x!=%04x @ %08lx\n",body[nw-2],DQ_EOE,ftell(fD));
	return ERRDQ_EVSIZE;
    }
    // CRC
    //for(ii=0;ii<nw;ii++) fcrc ^= body[ii];
    UShort_t crch = fcrc,crcb=0,crct=0;
    UpdateCRC(&crcb,body,nw-fevtl);
    UpdateCRC(&crct,body+nw-fevtl,fevtl);
    fcrc ^= crcb ^ crct;
    if(gDebug) {
    printf("crc=%04x (h,b,t=%04x,%04x,%04x), top/bottom:\n",fcrc,crch,crcb,crct);
    for(ii=0;ii<16;ii++) printf("%04x ",body[ii]);
    printf("\n");
    for(ii=nw-16;ii<nw;ii++) printf("%04x ",body[ii]);
    printf("\n");
    }//#endif
    #ifdef ERRDQ_CRC
    if(fcrc) {ferr |= ERRDQ_CRC;}
    if((ferr&ERRDQ_CRC)!=0)
    {
	if(ferrcount<MAXERRPRINT) printf("ERROR ERRDQ_CRC %04x @ %08lx, nerrs=%i\n",fcrc,fpos,ferrcount);
	#undef ERREXIT
	#ifdef ERREXIT
		FillErr();
		return ERRDQ_CRC;
	#endif
    }
    #endif // ERRDQ_CRC

   // event should be OK, fill the members
    fevnum = fhdr[0]*256+fhdr[1];
    fevsize = (fevhl + nw)*2;
    //fcelln = Bin2Gray(bbody[3]);
    fbclk = fhdr[8]*256+fhdr[9];
    fbclkx = bbody[(nw-fevtl+1)*2]*256 + bbody[(nw-fevtl+1)*2+1];
    //fclkphase = fbclkx & 0x7;
    fclkphase =  bbody[(nw-fevtl+1)*2+1] & 0x7;
    fbclkx = fbclkx>>3;
    cclk = (fbclkx<<16) | fbclk;
    Long_t clkdiff; 
    clkdiff=cclk-fclkprev;
    fclkprev = cclk;
    if(clkdiff<0) clkdiff += 16777216; // add 2^24 if overflow
    if(fentry>0) fclkdiff = clkdiff;
    fPARst_ExTrig = bbody[(nw-fevtl)*2]*256 + bbody[(nw-fevtl)*2+1];
    if(gDebug) {
    for(ii=0;ii<16;ii++) printf("%02x ",bbody[ii]);
    printf("\nbclk=%04x, x=%04x, phase=%04x, diff=%li:, cc=%li\n",
	   fbclk,fbclkx,fclkphase,fclkdiff,cclk);
    }//#endif
    if(DATA_ARE_ADC)
    {
	// check for valid cell numbers
	// find first non-zero celln and assign it to fcelln
	for(ii=0;ii<NCHAINS;ii++) if(bbody[ii]) break;
	byte = Bin2Gray(bbody[ii]);
	for(ii=0;ii<NCHAINS;ii++) fcelln[ii][0] = byte;
	if(gDebug) { 
	printf("fcelln=%02x, celln0: %02x,%02x,%02x,%02x\n",
			byte,bbody[0],bbody[1],bbody[2],bbody[3]);
	}//#endif
	#ifdef ERRDQ_CELLN
	Int_t i1;
	for(i1=bytes_per_chip; i1<nw*2-bytes_per_chip; i1 += bytes_per_chip)
	{
	    nn = i1/bytes_per_chip;// module number
	    if(gDebug) {
	    printf("celln%i:%02x,%02x,%02x,%02x\n",
			    nn,bbody[i1+0],bbody[i1+1],bbody[i1+2],bbody[i1+3]);
	    }//#endif
	    for(ii=0;ii<NCHAINS;ii++)
	    {
		chain = ii^1;
		if(bbody[i1+ii] == 0) continue;
		fcelln[chain][nn] = Bin2Gray(bbody[i1+ii]);
		if(fcelln[chain][nn]>47) 
		{
		    ferr |= ERRDQ_CELLN; 
		    if(ferrcount<MAXERRPRINT) 
			printf("ERR=%04lx. Cell[%i]# %i>47. @ %08lx, nerrs=%i\n",
			    ferr,chain,fcelln[chain][nn],fpos,ferrcount);
		}
		if(fcelln[chain][nn]!=fcelln[chain][0])
		{
		    ferr |= ERRDQ_FCELLN_0<<ii; 
		    if(ferrcount<MAXERRPRINT) 
			printf("ERR=%04lx in ev %04x, mod %04x. Cell[%i]# %02x!=%02x. @ %08lx\n",ferr,fevnum,nn,chain,fcelln[chain][nn],fcelln[chain][0],fpos);
		}
	    }
	}
	#endif //ERRDQ_CELLN
    }

    Int_t channel =-999;
    for(ii=NCHAINS;ii<(nw-fevtl)*2;ii++)
    {
	channel = ii/NCHAINS-1;
	if(channel>=ASICS_IN_CHAIN*CH_IN_ASIC)
	    if(fentry<2) {cout<<"Skipping channel "<<channel<<endl;	continue;}
	//Added by Arbin
	#ifdef SWAP
	channel=strip[channel];
	#endif

	if(NCHAINS == 1) chain = 0;
	else
	    chain = (ii%NCHAINS)^1; //TODO: check logic here
	if(flchain[chain]==0)
	    if(fentry<2) {cout<<"chain "<<chain<<" empty "<<channel<<","<<ii<<endl;   continue;}

	//Skip dead channels
	if(status[chain][channel]==0) 
	    if(fentry<2) {cout<<"dead "<<chain<<","<<channel<<endl; continue;}

	//Do pedestal subtraction
	#ifdef PEDESTAL_PROCESSING
	fchv[chain][channel] = (CHV_t)bbody[ii] - ped[chain][channel] + 50;
	#else
	fchv[chain][channel] = (CHV_t)bbody[ii];
	#endif
	
	/*
	if(gDebug) {
	#if DBG > 1
	if(fentry<2)
	    if(chain==0)
	    printf("%i[%02x]=%02x ",chain,channel,bbody[ii]);
	if(fhchns[chain])
	{
		printf("Filling %i %i,%i\n",chain,channel,bbody[ii]);
		fhchns[chain]->Print();
	}
	#endif
	}//#endif
	*/
    }
    if(gCMNControl && DATA_ARE_ADC) 
      for(ii=0;ii<NCHAINS;ii++) if(flchain[ii]) DoCMNoise(ii);

    //Do the hits
    for(chain=0;chain<NCHAINS;chain++)
    {
	for(Int_t mod =0;mod<6;mod++) 
	{
	    hitn[chain][mod] = 1;
	    hitc[chain][mod] = 0;
	}
    }
    for(chain=0;chain<NCHAINS;chain++)
    {
	for(channel=0;channel<flchain[chain];channel++) 
	{
	    hitadc[chain][channel] = 0.;
	    //Define hit
	    if(fchv[chain][channel]>54)
	    {
		histoadc[chain]->Fill((Double_t) channel, (Double_t) fchv[chain][channel]);
		hitadc[chain][channel] = (Float_t) fchv[chain][channel];
		module = channel/CH_IN_ASIC;
		hitn[chain][module] = 2;
		hitc[chain][module] += 1;
	    }
	}
    }
    //fill histogram
    for(chain=0;chain<NCHAINS;chain++)
	if(fhchns[chain]) 
	    for(channel=0;channel<flchain[chain];channel++) 
	    {
		//note: start with channel=1 only if SWAP
		fhchns[chain]->Fill((Double_t) channel, (Double_t) fchv[chain][channel]);
	    }
    if(gDebug) {
    printf("\n");
    }//#endif
    //fpos = ftell(fD);
    if(ftree) ftree->Fill();
    if(ferr) 
    { 
	    fnerr++;
	    FillErr();
    }
    return 0;
}


Int_t Tdq::Next()
{
    Int_t rc;
    rc = GetHeader();
    if(rc<0) return rc;
    if(rc&ERRDQ_STAMP)
    {
	if((rc = FindEOE())!=0) return -rc;
	rc = GetHeader();
    }
    if (rc)	return rc;
    // event seems to be OK
    return Process();
}
void Tdq::DoCMNoise(Int_t chain)
{
    Int_t ic,nasics,ii,chnn;
    Int_t nErrsToPrint = 10;
    Long64_t size = CH_IN_ASIC;
    Long64_t order = Long64_t((Float_t)(CH_IN_ASIC)*gCMNQuantile);
    Long64_t work[CH_IN_ASIC];
    nasics = flchain[chain]/CH_IN_ASIC;
    //Int_t chnn;
    for(ic=0;ic<nasics;ic++)
    {
        fcmnoise[chain][ic] =  (TMath::KOrdStat(size, fchv[chain]+ic*CH_IN_ASIC, order, work));
		if(abs(fcmnoise[chain][ic])>gCMNLimit)
        {
	    if(fcmnoise[chain][ic]==-255) {ferr |= ERRDQ_0CHIP;}
	    else
	    {
		ferr |= ERRDQ_CMNOISE;
		if(dbg_cmnoise<nErrsToPrint)
		{
		    dbg_cmnoise++;
		    printf("***Error cmn noise %i>%i in chip %i.%i, ev%i, err#%i\n",
		    fcmnoise[chain][ic],gCMNLimit,chain,ic,fevnum, nErrsToPrint-dbg_cmnoise);
		}
	    }
        }

	if(gCMNControl&2)
	{
	    //Int_t chnn = 0;
	    for(ii=0;ii<CH_IN_ASIC;ii++)
	    {
		//Added by Arbin
		chnn = ic*CH_IN_ASIC+ii;
		#ifdef SWAP
		chnn=strip[ic*CH_IN_ASIC+ii];
		#endif
		    //Skip dead channels
		if(status[chain][chnn]==0) continue;

		fchv[chain][chnn] -= (CHV_t)(fcmnoise[chain][ic] - 50.);
	    }
	}
    }
}
Bool_t Tdq::IsOpen()
{
	return fD != 0;
}
Tdq::Tdq(Char_t *name, Int_t cmnproc_control, Char_t *htitle)
{
    Int_t rc,ii;
    struct stat statv;
    const Char_t *tname;
    Char_t	oname[256];
    fhtitle = htitle;

    fentry = 0;
    fD = NULL;
    ffile = NULL;
    ftree = NULL;
    ferrcount = 0;
    fclkprev = 0;
    fevhl = 0;
    //fchnmap.Set(MAXCH);
    //fchnmap.Reset();
    if((tname = gSystem->ExpandPathName(name))!=0)
    {
    	strcpy(fname,tname);

		// Crashes Win32 as deleting unallocated memory?
		// That error doesn't seem right, but commenting this out allows things to continue
		// properly. I think ExpandPathName is using the string class, which is *managed*
		// under Win32 - JGL 10/19/2013
#ifndef WIN32
   		delete [](char*)tname;
#endif

    }
if(gDebug) {
    printf("Opening %s\n",fname);
}//#endif
    fD = fopen(fname,"rb");
    if(fD==NULL)
    {
	perror("Could not open file ");
	perror(fname);
	return;
    }
    rc = stat(fname,&statv);
    if(rc!=0) {perror("Cannot fstat"); fsize = -1;}
    fsize = statv.st_size;
    printf("File opened %s[%d]\n",fname,fsize);
    strcpy(oname,fname);
    char *substr = strrchr(oname,'.');
    strcpy(substr+1,"root");
if(gDebug) {
    printf("Output file %s\n",oname);
}//#endif
    if(ffile) {printf("deleting file\n");delete ffile;}
    ffile = new TFile(oname,"recreate");
    if(ffile == NULL) {printf("ERROR. Could not open %s\n",oname); return;}	
if(gDebug) {
    printf("File opened\n");
}//#endif
fnchn = ASICS_IN_CHAIN*CH_IN_ASIC;
    for(ii=0;ii<fnchn;ii++) fchn[ii] = ii;
    fherr = new TH1I("herr","Format Errors",32,0,32);
    printf("Tdq is constructed\n");
    gCMNControl = cmnproc_control;
    printf("Common mode noise subtraction is ");
    if(gCMNControl&2) printf("ON\n");
    else	printf("OFF\n");
    return;
}
Tdq::~Tdq()
{
    if(gDebug) {
    printf("Deleting fD, tree and file\n");
    }//#endif
    if(fD) {fclose(fD); fD = NULL;}
    if(ftree) {delete ftree; ftree = NULL;}
    if(ffile) {delete ffile; ffile = NULL;}
}
TTree* Tdq::MakeTree(Int_t mode)
{
    Int_t ii;
    if(mode)
    {
	printf("Making Tree (%i)\n",mode);
	if(ftree) {printf("deleting tree\n");delete ftree;}
	ftree = new TTree("dqtree","dq analysis tree");
	if(ftree==NULL){printf("failed to create tree\n");return ftree;}
	    //printf("Tree created\n");
	ftree->Branch("entry",&fentry,"fentry/i");
	ftree->Branch("fevnasics",&fevnasics,"fevnasics/b");
	ftree->Branch("celln0",fcelln[0],"fcelln0[fevnasics]/b");
	//ftree->Branch("celln1",fcelln[1],"fcelln1[fevnasics]/b");
	//ftree->Branch("celln2",fcelln[2],"fcelln2[fevnasics]/b");
	//ftree->Branch("celln3",fcelln[3],"fcelln3[fevnasics]/b");
	ftree->Branch("evsize",(Int_t*)&fevsize,"evsize/s");
	ftree->Branch("error",(Int_t*)&ferr,"ferr/i");
	ftree->Branch("evnum",(Int_t*)&fevnum,"fevnum/i");
	ftree->Branch("fnchn",&fnchn,"fnchn/I");
	ftree->Branch("chn",fchn,"chn[fnchn]/s");
	ftree->Branch("f1chain0",&(flchain[0]),"flchain0/I");
	//ftree->Branch("f1chain1",&(flchain[1]),"flchain1/I");
	//ftree->Branch("f1chain2",&(flchain[2]),"flchain2/I");
	//ftree->Branch("f1chain3",&(flchain[3]),"flchain3/I");
	ftree->Branch("chv0",fchv[0],"fchv0[flchain0]/s");
	//ftree->Branch("chv1",fchv[1],"fchv1[flchain1]/s");
	//ftree->Branch("chv2",fchv[2],"fchv2[flchain2]/s");
	//ftree->Branch("chv3",fchv[3],"fchv3[flchain3]/s");
	ftree->Branch("hitadcs0",hitadc[0],"hitadc0[flchain0]/F");
	//ftree->Branch("hitadcs1",hitadc[1],"hitadc1[flchain1]/F");
	//ftree->Branch("hitadcs2",hitadc[2],"hitadc2[flchain2]/F");
	//ftree->Branch("hitadcs3",hitadc[3],"hitadc3[flchain3]/F");
	ftree->Branch("hitn0",hitn[0],"hitn0[fevnasics]/I");
	//ftree->Branch("hitn1",hitn[1],"hitn1[fevnasics]/I");
	//ftree->Branch("hitn2",hitn[2],"hitn2[fevnasics]/I");
	//ftree->Branch("hitn3",hitn[3],"hitn3[fevnasics]/I");
	ftree->Branch("hitc0",hitc[0],"hitc0[fevnasics]/I");
	//ftree->Branch("hitc1",hitc[1],"hitc1[fevnasics]/I");
	//ftree->Branch("hitc2",hitc[2],"hitc2[fevnasics]/I");
	//ftree->Branch("hitc3",hitc[3],"hitc3[fevnasics]/I");
	ftree->Branch("cmnn0",fcmnoise[0],"fcmnn0[fevnasics]/s");
	//ftree->Branch("cmnn1",fcmnoise[1],"fcmnn1[fevnasics]/s");
	//ftree->Branch("cmnn2",fcmnoise[2],"fcmnn2[fevnasics]/s");
	//ftree->Branch("cmnn3",fcmnoise[3],"fcmnn3[fevnasics]/s");
	ftree->Branch("clkphase",(Int_t*)&fclkphase,"fclkphase/b");
	ftree->Branch("clkdiff",&fclkdiff,"clkdiff/i");
	ftree->Branch("nerr",(Int_t*)&fnerr,"fnerr/s");
	ftree->Branch("bclk",&fbclk,"bclk/s");
	ftree->Branch("bclkx",&fbclkx,"bclkx/s");
	ftree->Branch("PARst_ExTrig",&fPARst_ExTrig,"PARst_ExTrig/s");
    }
    fevcount = 0;
    ferrcount = 0;
   
	//printf("Event loop\n");
    Int_t maxEntry = 1000000;
    if(gDebug) {
	    maxEntry = 20;
    }
    for(fentry=0;fentry<maxEntry;fentry++)
    {
		if(Next()<0) break;
		if((fentry%100)==0) printf("nev=%i,nerr=%i,err=%08lx\n",fentry,ferrcount,ferr);
    }
    printf("Break after %i events\n",fentry);
    if(fentry)
    {
	if(ftree) ftree->Write();
	if(fherr) fherr->Write();
	for(ii=0;ii<NCHAINS;ii++)
	{
	    if(fhchns[ii]) fhchns[ii]->Write();
	    if(histoadc[ii]) histoadc[ii]->Write();
	}
    }
    return ftree;
}
