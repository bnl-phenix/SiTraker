{
TTree *gTree;
Int_t gRunInProgress = 0;
//TCanvas c1("c1","Amplitudes");
//TCanvas *gc1;
Int_t gFirstEntry=1;
Int_t gNEntries=999999;
        
    cout<<"gSystem.Load(""Tdq"")"<<endl;
    gSystem->Load("Tdq");

    Tdq *gdq;
    //gh = new TH2S("adc-chn","",128,0,128,256,0,256);
    gStyle->SetPalette(1);
    
    //load script to be called by run.C
    gROOT->ProcessLine(".L process_file.C");
    
    cout<<"Now you can start the analysis using '.x run.C'"<<endl;
}


