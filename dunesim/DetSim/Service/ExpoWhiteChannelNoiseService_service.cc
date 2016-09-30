// Copy of ExponentialChannelNoiseService_service.cc
// NEW ADDITION: Add a constant (white) noise to the exponential
// to better reflect 35-ton noise frequency spectrum.
// See D.Adams' talks for evidence of spectrum shape.

// m.thiesse@sheffield.ac.uk

#include "dune/DetSim/Service/ExpoWhiteChannelNoiseService.h"
#include "dune/Utilities/SignalShapingServiceDUNE.h"
#include <sstream>
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "lardata/Utilities/LArFFT.h"
#include "larcore/Geometry/Geometry.h"
#include "larsim/RandomUtils/LArSeedService.h"
#include "art/Framework/Services/Optional/TFileService.h"
#include "CLHEP/Random/JamesRandom.h"
#include "CLHEP/Random/RandFlat.h"
#include "CLHEP/Random/RandGaussQ.h"
#include "TH1F.h"
#include "TRandom3.h"

using std::cout;
using std::ostream;
using std::endl;
using std::string;
using std::ostringstream;
using sim::LArSeedService;
using CLHEP::HepJamesRandom;

//**********************************************************************

ExpoWhiteChannelNoiseService::
ExpoWhiteChannelNoiseService(fhicl::ParameterSet const& pset)
: fRandomSeed(0), fLogLevel(1),
  fNoiseHistZ(nullptr), fNoiseHistU(nullptr), fNoiseHistV(nullptr),
  fNoiseChanHist(nullptr),
  m_pran(nullptr) 
{
  const string myname = "ExpoWhiteChannelNoiseService::ctor: ";
  fNoiseNormZ        = pset.get<double>("NoiseNormZ");
  fNoiseWidthZ       = pset.get<double>("NoiseWidthZ");
  fLowCutoffZ        = pset.get<double>("LowCutoffZ");
  fNoiseNormU        = pset.get<double>("NoiseNormU");
  fNoiseWidthU       = pset.get<double>("NoiseWidthU");
  fLowCutoffU        = pset.get<double>("LowCutoffU");
  fNoiseNormV        = pset.get<double>("NoiseNormV");
  fNoiseWidthV       = pset.get<double>("NoiseWidthV");
  fLowCutoffV        = pset.get<double>("LowCutoffV");
  fNoiseArrayPoints  = pset.get<unsigned int>("NoiseArrayPoints");
  fOldNoiseIndex     = pset.get<bool>("OldNoiseIndex");
  bool haveSeed = pset.get_if_present<int>("RandomSeed", fRandomSeed);
  if ( fRandomSeed == 0 ) haveSeed = false;
  pset.get_if_present<int>("LogLevel", fLogLevel);
  fNoiseZ.resize(fNoiseArrayPoints);
  fNoiseU.resize(fNoiseArrayPoints);
  fNoiseV.resize(fNoiseArrayPoints);
  int seed = fRandomSeed;
  art::ServiceHandle<art::TFileService> tfs;
  fNoiseHistZ = tfs->make<TH1F>("znoise", ";Z Noise [ADC counts];", 1000,   -10., 10.);
  fNoiseHistU = tfs->make<TH1F>("unoise", ";U Noise [ADC counts];", 1000,   -10., 10.);
  fNoiseHistV = tfs->make<TH1F>("vnoise", ";V Noise [ADC counts];", 1000,   -10., 10.);
  fNoiseChanHist = tfs->make<TH1F>("NoiseChan", ";Noise channel;", fNoiseArrayPoints, 0, fNoiseArrayPoints);
  // Assign a unique name for the random number engine ExpoWhiteChannelNoiseServiceVIII
  // III = for each instance of this class.
  string rname = "ExpoWhiteChannelNoiseService";
  if ( haveSeed ) {
    if ( fLogLevel > 0 ) cout << myname << "WARNING: Using hardwired seed." << endl;
    m_pran = new HepJamesRandom(seed);
  } else {
    if ( fLogLevel > 0 ) cout << myname << "Using LArSeedService." << endl;
    art::ServiceHandle<LArSeedService> seedSvc;
    m_pran = new HepJamesRandom;
    if ( fLogLevel > 0 ) cout << myname << "    Initial seed: " << m_pran->getSeed() << endl;
    seedSvc->registerEngine(LArSeedService::CLHEPengineSeeder(m_pran), rname);
  }
  if ( fLogLevel > 0 ) cout << myname << "  Registered seed: " << m_pran->getSeed() << endl;
  for ( unsigned int isam=0; isam<fNoiseArrayPoints; ++isam ) {
    generateNoise(fNoiseNormZ, fNoiseWidthZ, fLowCutoffZ, fNoiseZ[isam], fNoiseHistZ);
    generateNoise(fNoiseNormU, fNoiseWidthU, fLowCutoffU, fNoiseU[isam], fNoiseHistU);
    generateNoise(fNoiseNormV, fNoiseWidthV, fLowCutoffV, fNoiseV[isam], fNoiseHistV);
  }
  if ( fLogLevel > 1 ) print() << endl;
}

//**********************************************************************

ExpoWhiteChannelNoiseService::
ExpoWhiteChannelNoiseService(fhicl::ParameterSet const& pset, art::ActivityRegistry&)
: ExpoWhiteChannelNoiseService(pset) { }

//**********************************************************************

ExpoWhiteChannelNoiseService::~ExpoWhiteChannelNoiseService() {
  const string myname = "ExpoWhiteChannelNoiseService::dtor: ";
  if ( fLogLevel > 0 ) {
    cout << myname << "Deleting random engine with seed " << m_pran->getSeed() << endl;
  }
  delete m_pran;
}

//**********************************************************************

int ExpoWhiteChannelNoiseService::addNoise(Channel chan, AdcSignalVector& sigs) const {
  CLHEP::RandFlat flat(*m_pran);
  unsigned int noisechan = 0;
  if ( fOldNoiseIndex ) {
    // Keep this strange way of choosing noise channel to be consistent with old results.
    // The relative weights of the first and last channels are 0.5 and 0.6.
    noisechan = nearbyint(flat.fire()*(1.*(fNoiseArrayPoints-1)+0.1));
  } else {
    noisechan = flat.fire()*fNoiseArrayPoints;
    if ( noisechan == fNoiseArrayPoints ) --noisechan;
  }
  fNoiseChanHist->Fill(noisechan);
  art::ServiceHandle<geo::Geometry> geo;
  const geo::View_t view = geo->View(chan);
  for ( unsigned int itck=0; itck<sigs.size(); ++itck ) {
    double tnoise = 0.0;
    if      ( view==geo::kU ) tnoise = fNoiseU[noisechan][itck];
    else if ( view==geo::kV ) tnoise = fNoiseV[noisechan][itck];
    else                      tnoise = fNoiseZ[noisechan][itck];
    sigs[itck] += tnoise;
  }

  ////////////////////////////////////////////////////

  art::ServiceHandle<util::SignalShapingServiceDUNE> sss;
  float fASICGain      = sss->GetASICGain(chan);
  double fShapingTime   = sss->GetShapingTime(chan);
  std::map< double, int > fShapingTimeOrder;
  fShapingTimeOrder = { {0.5, 0}, {1.0, 1}, {2.0, 2}, {3.0, 3} };
  DoubleVec fNoiseFactVec;
  auto tempNoiseVec = sss->GetNoiseFactVec();
  if ( fShapingTimeOrder.find(fShapingTime) != fShapingTimeOrder.end() ) {
    size_t i = 0;
    fNoiseFactVec.resize(2);
    for ( auto& item : tempNoiseVec ) {
      fNoiseFactVec[i] = item.at(fShapingTimeOrder.find( fShapingTime )->second);
      fNoiseFactVec[i] *= fASICGain/4.7;
      ++i;
    }
  } else {
    throw cet::exception("ExpoWhiteChannelNoiseService")
      << "\033[93m"
      << "Shaping Time received from signalservices_dune.fcl is not one of allowed values"
      << std::endl
      << "Allowed values: 0.5, 1.0, 2.0, 3.0 usec"
      << "\033[00m"
      << std::endl;
  }
  art::ServiceHandle<geo::Geometry> geo;
  const geo::View_t view = geo->View(chan);
#ifdef UseSeedService
  art::ServiceHandle<art::RandomNumberGenerator> rng;
  CLHEP::HepRandomEngine& engine = rng->getEngine("ExpoWhiteChannelNoiseService");
#else
  CLHEP::HepRandomEngine& engine = *m_pran;
#endif
  CLHEP::RandGaussQ rGauss_Ind(engine, 0.0, fNoiseFactVec[0]);
  CLHEP::RandGaussQ rGauss_Col(engine, 0.0, fNoiseFactVec[1]);
  for ( AdcSignal& sig : sigs ) {
    double tnoise = 0.0;
    if      ( view==geo::kU ) tnoise = rGauss_Ind.fire();
    else if ( view==geo::kV ) tnoise = rGauss_Ind.fire();
    else                      tnoise = rGauss_Col.fire();
    sig += tnoise;
  }

  return 0;
}

//**********************************************************************

ostream& ExpoWhiteChannelNoiseService::print(ostream& out, string prefix) const {
  out << prefix << "ExpoWhiteChannelNoiseService: " << endl;
  out << prefix << "        NoiseNormZ: " << fNoiseNormZ   << endl;
  out << prefix << "       NoiseWidthZ: " << fNoiseWidthZ  << endl;
  out << prefix << "        LowCutoffZ: " << fLowCutoffZ << endl;
  out << prefix << "        NoiseNormU: " << fNoiseNormU   << endl;
  out << prefix << "       NoiseWidthU: " << fNoiseWidthU  << endl;
  out << prefix << "        LowCutoffU: " << fLowCutoffU << endl;
  out << prefix << "        NoiseNormV: " << fNoiseNormV   << endl;
  out << prefix << "       NoiseWidthV: " << fNoiseWidthV  << endl;
  out << prefix << "        LowCutoffV: " << fLowCutoffV << endl;
  out << prefix << "  NoiseArrayPoints: " << fNoiseArrayPoints << endl;
  out << prefix << "     OldNoiseIndex: " << fOldNoiseIndex << endl;
  out << prefix << "        RandomSeed: " <<  fRandomSeed << endl;
  out << prefix << "          LogLevel: " <<  fLogLevel << endl;
  out << prefix << "  Actual random seed: " << m_pran->getSeed();
  return out;
}

//**********************************************************************

void ExpoWhiteChannelNoiseService::
generateNoise(float aNoiseNorm, float aNoiseWidth, float aLowCutoff,
              AdcSignalVector& noise, TH1* aNoiseHist) const {
  const string myname = "ExpoWhiteChannelNoiseService::generateNoise: ";
  if ( fLogLevel > 1 ) {
    cout << myname << "Generating noise." << endl;
    if ( fLogLevel > 2 ) {
      cout << myname << "    Norm: " << aNoiseNorm << endl;
      cout << myname << "   Width: " << aNoiseWidth << endl;
      cout << myname << "  Cutoff: " << aLowCutoff << endl;
      cout << myname << "    Seed: " << m_pran->getSeed() << endl;
    }
  }
  // Fetch sampling rate.
  auto const* detprop = lar::providerFrom<detinfo::DetectorPropertiesService>();
  float sampleRate = detprop->SamplingRate();
  // Fetch FFT service and # ticks.
  art::ServiceHandle<util::LArFFT> pfft;
  unsigned int ntick = pfft->FFTSize();
  CLHEP::RandFlat flat(*m_pran);
  // Create noise spectrum in frequency.
  unsigned nbin = ntick/2 + 1;
  std::vector<TComplex> noiseFrequency(nbin, 0.);
  double pval = 0.;
  double lofilter = 0.;
  double phase = 0.;
  double rnd[2] = {0.};
  // width of frequencyBin in kHz
  double binWidth = 1.0/(ntick*sampleRate*1.0e-6);
  for ( unsigned int i=0; i<ntick/2+1; ++i ) {
    // exponential noise spectrum 
    pval = aNoiseNorm*exp(-(double)i*binWidth/aNoiseWidth);
    // low frequency cutoff     
    lofilter = 1.0/(1.0+exp(-(i-aLowCutoff/binWidth)/0.5));
    // randomize 10%
    flat.fireArray(2, rnd, 0, 1);
    pval *= lofilter*(0.9 + 0.2*rnd[0]);
    // random phase angle
    phase = rnd[1]*2.*TMath::Pi();
    TComplex tc(pval*cos(phase),pval*sin(phase));
    noiseFrequency[i] += tc;
  }
  // Obtain time spectrum from frequency spectrum.
  noise.clear();
  noise.resize(ntick,0.0);
  std::vector<double> tmpnoise(noise.size());
  pfft->DoInvFFT(noiseFrequency, tmpnoise);
  noiseFrequency.clear();
  // Multiply each noise value by ntick as the InvFFT 
  // divides each bin by ntick assuming that a forward FFT
  // has already been done.
  // DLA Feb 2016: Change factor from ntick --> sqrt(ntick) so that the RMS 
  // does not depend on ntick (FFT size).
  for ( unsigned int itck=0; itck<noise.size(); ++itck ) {
    noise[itck] = sqrt(ntick)*tmpnoise[itck];
  }
  for ( unsigned int itck=0; itck<noise.size(); ++itck ) {
    aNoiseHist->Fill(noise[itck]);
  }
}

//**********************************************************************

void ExpoWhiteChannelNoiseService::generateNoise() {
  fNoiseZ.resize(fNoiseArrayPoints);
  fNoiseU.resize(fNoiseArrayPoints);
  fNoiseV.resize(fNoiseArrayPoints);
  for ( unsigned int inch=0; inch<fNoiseArrayPoints; ++inch ) {
    generateNoise(fNoiseNormZ, fNoiseWidthZ, fLowCutoffZ, fNoiseZ[inch], fNoiseHistZ);
    generateNoise(fNoiseNormU, fNoiseWidthU, fLowCutoffU, fNoiseZ[inch], fNoiseHistU);
    generateNoise(fNoiseNormV, fNoiseWidthV, fLowCutoffV, fNoiseZ[inch], fNoiseHistV);
  }
}

//**********************************************************************

DEFINE_ART_SERVICE_INTERFACE_IMPL(ExpoWhiteChannelNoiseService, ChannelNoiseService)

//**********************************************************************