// usage:
// .L vadc.C
// vadc(3,100); // to draw channel 100 of the chain 3
// vadc(3,-1); // to draw next channel
Int_t vadc_count = 0;
TCanvas cvadc;
void vadc(Int_t chain, Int_t chn)
{
  cvadc.cd();
  TString cname = "chv";
  cname += chain;
  cname += "[";
  if(chn==-1) chn = vadc_count;
  else vadc_count = chn;
  cname += chn;
  vadc_count++;
  cname += "]";
  TH1I *h1 = new TH1I("h1",cname,256,0,256);
  h1->SetFillColor(45);
  TString drawcmd = cname;
  drawcmd += ">>h1";
  cout<<drawcmd<<endl;
  gTree->Draw(drawcmd);
}

