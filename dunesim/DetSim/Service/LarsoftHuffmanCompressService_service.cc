// LarsoftHuffmanCompressService.cxx

#include "dune/DetSim/Service/LarsoftHuffmanCompressService.h"
#include "fhiclcpp/ParameterSet.h"
#include "RawData/raw.h"
#include "dune/DetSim/Service/ReplaceCompressService.h"

using std::string;
using std::ostream;
using std::cout;
using std::endl;

//**********************************************************************

LarsoftHuffmanCompressService::
LarsoftHuffmanCompressService(bool useBlock, bool useHuffman, int logLevel)
: m_UseBlock(useBlock), m_UseHuffman(useHuffman), m_LogLevel(logLevel) { }

//**********************************************************************

LarsoftHuffmanCompressService::
LarsoftHuffmanCompressService(const fhicl::ParameterSet& pset, art::ActivityRegistry&)
: m_LogLevel(1) {
  const string myname = "LarsoftHuffmanCompressService::ctor: ";
  m_UseBlock   = pset.get<bool>("UseBlock");
  m_UseHuffman = pset.get<bool>("UseHuffman");
  pset.get_if_present<int>("LogLevel", m_LogLevel);
  if ( m_LogLevel > 0 ) {
    cout << myname << "   UseBlock: " << m_UseBlock << endl;
    cout << myname << " UseHuffman: " << m_UseHuffman << endl;
    cout << myname << "   LogLevel: " << m_LogLevel << endl;
  }
}
  
//**********************************************************************

int LarsoftHuffmanCompressService::
compress(AdcCountVector& sigs, const AdcFilterVector& keep, AdcCount offset,
         raw::Compress_t& comp) const {
  const string myname = "LarsoftHuffmanCompressService::compress: ";
  AdcCountVector newsigs;
  comp = raw::kNone;
  if  ( m_UseBlock ) {
    block(sigs, keep, newsigs);
    sigs = newsigs;
    comp = raw::kZeroSuppression;
  } else {
    ReplaceCompressService repsvc;
    repsvc.compress(sigs, keep, offset, comp);
  }
  unsigned int insize = sigs.size();
  if  ( m_UseHuffman ) {
    if ( m_LogLevel > 1 ) cout << myname << "Size before Huffman: " << insize << endl;
    if ( m_LogLevel > 2 ) {
      for ( unsigned int isig=0; isig<sigs.size(); ++isig ) {
        cout << myname << "  Before sigs[" << isig << "]: " << sigs[isig] << endl;
        if ( m_LogLevel<5 && isig > 50 ) {
          cout << myname << "  ..." << endl;
          break;
        }
      }
    }
    raw::CompressHuffman(sigs);
    if ( m_LogLevel > 1 ) cout << myname << "Size  after Huffman: " << sigs.size() << endl;
    if ( m_LogLevel > 2 ) {
      for ( unsigned int isig=0; isig<sigs.size(); ++isig ) {
        cout << myname << "   After sigs[" << isig << "]: " << sigs[isig] << endl;
        if ( m_LogLevel<4 && isig > 50 ) {
          cout << myname << "  ..." << endl;
          break;
        }
      }
    }
#if 1
    // Uncompress as test.
    if ( m_LogLevel > 1 ) {
      AdcCountVector usgs(insize, 0);
      raw::UncompressHuffman(sigs, usgs);
      cout << myname << "Size  after uncompress: " << usgs.size() << endl;
      if ( m_LogLevel > 2 ) {
        for ( unsigned int isig=0; isig<usgs.size(); ++isig ) {
          cout << myname << "  Uncomp usgs[" << isig << "]: " << usgs[isig] << endl;
          if ( m_LogLevel<5 && isig > 50 ) {
            cout << myname << "  ..." << endl;
            break;
          }
        }
      }
    }
#endif
    if ( m_UseBlock ) comp = raw::kZeroHuffman;
    else              comp = raw::kHuffman;
  }
  return 0;
}

//**********************************************************************

ostream& LarsoftHuffmanCompressService::print(ostream& out, string prefix) const {
  out << prefix << "LarsoftHuffmanCompressService:" << endl;
  return out;
}

//**********************************************************************

// Copied from larsoft/lardata/RawData/raw.cxx 
//   void ZeroSuppression(std::vector<short> &adc, unsigned int &zerothreshold)
// and modified to suppress using keep instead of appying a threshold.
// Also, the old algorithm kept an extra tck at the end of each block.
// This is not done here.

void LarsoftHuffmanCompressService::
block(const AdcCountVector& sigsin, const AdcFilterVector& keep, AdcCountVector& sigsout) const {
  const unsigned int adcsize = sigsin.size();
  std::vector<short> zerosuppressed(sigsin.size());
  unsigned int maxblocks = adcsize/2 + 1;
  std::vector<short> blockbegin(maxblocks);
  std::vector<short> blocksize(maxblocks);
  unsigned int nblocks = 0;
  unsigned int zerosuppressedsize = 0;
  bool inblock = false;
  for ( unsigned int i=0; i<adcsize; ++i ) {
    if ( keep[i] ){
      if ( ! inblock ) {
        blockbegin[nblocks] = i;
        blocksize[nblocks] = 0;
        inblock = true;
      }
      zerosuppressed[zerosuppressedsize] = sigsin[i];
      ++zerosuppressedsize;
      ++blocksize[nblocks];
      if ( i == adcsize-1 ) ++nblocks;
    } else if ( inblock ) {
      //zerosuppressed[zerosuppressedsize] = sigsin[i];
      //++zerosuppressedsize;
      //++blocksize[nblocks];
      ++nblocks;
      inblock = false;
    }
  }
  sigsout.resize(2+nblocks+nblocks+zerosuppressedsize);
  sigsout[0] = adcsize; //fill first entry in adc with length of uncompressed vector
  sigsout[1] = nblocks;
  for ( unsigned int i=0; i<nblocks; ++i ) sigsout[i+2] = blockbegin[i];
  for ( unsigned int i=0; i<nblocks; ++i ) sigsout[i+nblocks+2] = blocksize[i];
  for ( unsigned int i=0; i<zerosuppressedsize; ++i ) sigsout[i+nblocks+nblocks+2] = zerosuppressed[i];
}

//**********************************************************************

DEFINE_ART_SERVICE_INTERFACE_IMPL(LarsoftHuffmanCompressService, AdcCompressService)

//**********************************************************************
