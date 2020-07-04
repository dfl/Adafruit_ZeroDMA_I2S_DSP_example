#ifndef SineOsc_h
#define SineOsc_h

#define _fixedGain(a,b,bits)   int32_t(( int64_t(a) * uint64_t(b) + (1U << (bits-1)) ) >> bits)

void print64( uint64_t val, int k = DEC) {
  uint32_t val_hi = val >> 32;
  uint32_t val_lo = val & 0xFFFFFFFF;    
  Serial.print(val_hi, k);
  Serial.print(val_lo, k);
  if(k == HEX && val_lo == 0) {
    Serial.print("00000000");
  }
}

void print64( int64_t val, int k = DEC) {
  uint32_t val_hi = val >> 32;
  uint32_t val_lo = val & 0xFFFFFFFF;
  if(k == HEX) {
    if (val_hi != 0xFFFFFFFF) {
      Serial.print(val_hi, k);
    }
  }
  Serial.print(val_lo, k);
}

int32_t fixedGain( int64_t a, uint64_t b, char bits = 16 ) {
    Serial.print("~ fixedGain: ");
    Serial.print(int32_t(a), HEX);
    Serial.print(" * ");
    Serial.print(int32_t(b), HEX);
    int64_t result = ( int64_t(a)*uint64_t(b) + (1U << (bits-1)) );
    Serial.print(" multiplied: ");
    print64( result, HEX );
    result >>= bits;
    Serial.print(" shifted: ");
    Serial.println(int32_t(result), HEX);
    return result;
}

class Phasor {
public:
  static constexpr uint32_t  PRECISION_BITS = sizeof(uint32_t)*8; // 32
  static constexpr uint32_t    _180_DEGREES = 1 << (PRECISION_BITS-1);
  static constexpr uint32_t     _90_DEGREES = 1 << (PRECISION_BITS-2);
  static constexpr uint32_t  MAX_PHASE      = (1LL << PRECISION_BITS) - 1; // ULONG_MAX

protected:
  float freq, srate;
  uint32_t phase = 0, normFreq;

  bool isWrap, isEven;
  uint8_t cycles;

public:   
  Phasor( float srate ) {
    this->srate = srate;
    resetPhase();
  }

  void setFrequency( float f ) {
    freq = f;
    normFreq = round(f/srate * float(MAX_PHASE));
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
    phase = round(float(MAX_PHASE) * p);
  }

  bool getEven()   { return (cycles % 2) == 0; }
  bool getAlt()    { return (cycles % 4) >> 1; }
};

namespace SineTable {
  static const uint8_t  TABLE_BITS = 8;
  static const uint16_t TABLE_SIZE = 1U << TABLE_BITS;  
  static const uint16_t INDEX_MASK = TABLE_SIZE-1;
  static const uint8_t  FRACTIONAL_BITS = Phasor::PRECISION_BITS - TABLE_BITS;
  static const uint32_t FRAC_MASK  = ((1LL << Phasor::PRECISION_BITS)-1) >> TABLE_BITS;

  int32_t LUT[TABLE_SIZE+1] = {0};

  inline int32_t lookup( uint32_t phase ) {
    uint32_t index = (phase >> FRACTIONAL_BITS) & INDEX_MASK;
    uint32_t frac = phase & FRAC_MASK;
//    Serial.print("phase: ");
//    Serial.print(phase, HEX);
//    Serial.print(" idx: ");
//    Serial.print(index, HEX);
//    Serial.print(" frac: ");
//    Serial.println(frac, HEX);

//// linear interpolation: output = a + (b-a)*frac
    int32_t a = LUT[index];
    int32_t b = LUT[index+1];
    return a + _fixedGain( b-a, frac, FRACTIONAL_BITS );
  }

  void init() {
    bool isEmpty = LUT[10] == 0;
    if (isEmpty) {
      int i=0;
      for (i=0; i<TABLE_SIZE; i++) { // TODO: round() or floor() ?
//        LUT[i] = int32_t(float(LONG_MAX)* 0.5 * (1.f-cos(TWO_PI*(1.0/TABLE_SIZE)*i)) );
        LUT[i] = int32_t(float(LONG_MAX)*sin(TWO_PI*(1.0/TABLE_SIZE)*i));
      }
      LUT[TABLE_SIZE] = LUT[0]; // duplicate to prevent need for wrapping on linear interpolation
    }
  }

  void print() {
    Serial.print("signed sine[");
    Serial.print(TABLE_SIZE);
    Serial.print("] = ");
    for(int i=0; i < TABLE_SIZE; i++ ) {
      Serial.print( LUT[i] );
      Serial.print(", ");
    }
    Serial.println( LUT[TABLE_SIZE] );
  }
}

class SineOsc : public Phasor {
protected:

  int32_t lastOut;
  uint8_t gain;
  static const uint8_t GAIN_BITS = sizeof(gain)*8;
  static const uint32_t MAX_GAIN = (1U << GAIN_BITS)-1;
public:
  SineOsc( int srate ) : Phasor(float(srate)) {
    SineTable::init();
    gain = MAX_GAIN;
  }

  int32_t process() {
    incPhase();
    lastOut = SineTable::lookup(phase);
//    Serial.print("preGain: ");
//    Serial.print(lastOut, HEX);
//    Serial.print(" * ");
//    Serial.print(gain, HEX);

//    lastOut = fixedGain(lastOut, gain, GAIN_BITS );

//    Serial.print(" postGain: ");
//    Serial.println(lastOut, HEX);
    return lastOut;
  }

  void setGain(float g) {
    gain = floor( float(MAX_GAIN) * g );
    Serial.print("gain = ");
    Serial.println(gain, HEX);
//    Serial.print("GAIN_BITS: ");
//    Serial.print(GAIN_BITS);
//    Serial.print("  MAX_GAIN: ");
//    Serial.println(MAX_GAIN);
  }

  uint32_t getLast() { return lastOut; }

  inline int32_t makeSigned(uint32_t x) {
    return int32_t(x) + LONG_MIN;
  }
  
//#define _uGainMul32(a,b) uint32_t( (uint64_t(a) * (uint64_t(b)+1) + (1L << 31) ) >> 32)
//#define _iGainMul32(a,b)  int32_t(  (int64_t(a) * (uint64_t(b)+1) + (1L << 31) ) / (1<<32))
//
//  int32_t getSigned() {
//    int32_t _signed, scaled;
//    uint64_t gain32 = 0xFFFFFFFF; //gain * uint64_t(0x1010101);
//    _signed = makeSigned(lastOut);
//    uint32_t signMask = _signed & (1 << 31);
//    scaled = _uGainMul32( _signed, gain32 );
////    scaled = _iGainMul32( _signed, gain32 );
//    scaled ^= signMask;
//    return scaled;
//    
//
////    scaled = makeSigned( _uGainMul32( lastOut, gain32 ) );
////    scaled |= signMask;
////    return scaled;
//
////    int64_t igain32 = gain32 >> 2;
////    return makeSigned( lastOut );
//
////    Serial.println();
////    Serial.print("signed pre:  ");
////    Serial.println(makeSigned(lastOut));
////
////    Serial.print("  preGain: ");
////    Serial.print(lastOut, HEX);
////    Serial.print("  gain: ");
//////    Serial.print(gain32, HEX);
////    print64(gain32, HEX);
//////    Serial.print("  result: ");
//////    uint64_t result = uint64_t(lastOut)*(uint64_t(gain32)+1) + (1L << 32); 
//////    print64(result, HEX);
////    Serial.print("  postGain: ");
////    Serial.println(scaled, HEX);   
////
////    Serial.print("signed post: ");
////    Serial.print(makeSigned(scaled));
////
////    bool signBit = (lastOut >> 31);
////    bool negative = signBit == 0;
////    Serial.print("  signbit: ");
////    Serial.println(signBit);
////    
////    Serial.println();
//
//
////    return scaled;
//    return makeSigned( scaled );
//  }

};


#endif
