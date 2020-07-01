#ifndef wavetables_h
#define wavetables_h

// https://spin.atomicobject.com/2012/03/15/simple-fixed-point-math/
// fixed point phasor;    TODO make template class?

class Phasor {
public:
  static const size_t PRECISION_BITS = 32; //sizeof(uint32_t)*8;
protected:
  float freq, srate;
  uint32_t phase = 0, normFreq;
  static const uint32_t  MAX_PHASE      = ULONG_MAX; //(1 << PRECISION_BITS) - 1;
  static constexpr float MAX_PHASE_F    = float(MAX_PHASE);
  static const uint32_t    _180_DEGREES = 1 << (PRECISION_BITS-1);
  bool isWrap, isEven;
  uint8_t cycles;

public:   
  Phasor( float srate ) {
    this->srate = srate;
    resetPhase();
  }

  void setFrequency( float f ) {
    freq = f;
    normFreq = round(f/srate * MAX_PHASE_F);
  }

  float getFrequency() { return freq; }

  inline void incPhase() __attribute__((always_inline)) {
    phase += normFreq;
    isWrap = phase < normFreq;
    cycles += isWrap;
  }
   
  uint32_t getPhase() {
    return phase;
  }
  
  void resetPhase( float p = 0.f ) {
    if (p == 0.f) cycles = 0;
    phase = round(MAX_PHASE_F * p);
  }

  bool getEven()   { return (cycles % 2) == 0; }
  bool getAlt()    { return (cycles % 4) >> 1; }
};

namespace SineTable {
  static const uint8_t TABLE_BITS = 12;
  static const uint16_t TABLE_SIZE = 1U << TABLE_BITS;
  int32_t LUT[TABLE_SIZE+1] = {0};

  static const uint8_t INDEX_SHIFT =  (Phasor::PRECISION_BITS - TABLE_BITS);
  static const uint8_t FRAC_SHIFT  =  (Phasor::PRECISION_BITS - 2*TABLE_BITS);
  static const uint16_t BYTE_MASK = TABLE_SIZE - 1;
  
  inline int32_t linterp( uint64_t phase ) {
    uint32_t index = (phase >> INDEX_SHIFT) & BYTE_MASK;
    uint64_t frac  = (phase >> FRAC_SHIFT ) & BYTE_MASK;
    int32_t a = LUT[index];
    int32_t b = LUT[index+1];
    //// output = a + (b-a)*frac

    int64_t diff = b-a;
    int32_t offset = (frac*diff) / (1 << 16);
//    aSerial.p("  index: ").p(index).p("   frac: ").p(uint32_t(frac)).p("  a: ").p(a, HEX).p("   b: ").p(b, HEX).p(" diff: ").p(uint32_t(diff),HEX).p(" offset: ").pln(offset,HEX);
    return a + offset;
  }

  void init() {
    if (LUT[1] == 0 ) {
      int i=0;
      for (i=0; i<TABLE_SIZE; ++i) {
        LUT[i] = int32_t(float(LONG_MAX)*sin(TWO_PI*(1.0/TABLE_SIZE)*i));
      }
      LUT[i+1] = LUT[0]; // duplicate to prevent need for wrapping on linear interpolation
    }
  }

//  void print() {
//    aSerial.p("Sine Table[").p(TABLE_SIZE).p("] = ");
//    for(int i=0; i < TABLE_SIZE; i++ ) {
//      aSerial.p( LUT[i] ).p(", ");      
//    }
//    aSerial.pln();
//  }

}

class SineOsc : public Phasor {
protected:

  uint32_t lastOut;

public:  
  SineOsc( int srate ) : Phasor(float(srate)) {
//    SineTable::init();
  }

  int32_t process() {
    incPhase();
    return lastOut = SineTable::linterp(phase);
  }

};

#endif
