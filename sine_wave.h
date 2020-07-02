#ifndef SineOsc_h
#define SineOsc_h

// https://spin.atomicobject.com/2012/03/15/simple-fixed-point-math/
// fixed point phasor;    TODO make template class?

#define _uFixMul32(a,b) (int64_t(a)*int64_t(b)) / (1 << 16)
#define _uGainMul32(a,b) uint32_t( (uint64_t(a)*(uint64_t(b)+1) + (1<<31) ) >> 32)
#define _8to32bit(x) (x << 24)

//#define _iFixMul32(a,b) (int64_t(a)*int64_t(b)) / (1 << 16)

class Phasor {
public:
  static const uint32_t  PRECISION_BITS = 32; //sizeof(uint32_t)*8;
  static const uint32_t    _180_DEGREES = 1 << (PRECISION_BITS-1);
  static const uint32_t     _90_DEGREES = 1 << (PRECISION_BITS-2);
  static const uint32_t  MAX_PHASE      = 4294967295L; //(1L << PRECISION_BITS) - 1; // ULONG_MAX
  static constexpr float MAX_PHASE_F    = float(MAX_PHASE);

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

  static const uint8_t INDEX_SHIFT =  (Phasor::PRECISION_BITS - TABLE_BITS);
  static const uint8_t FRAC_SHIFT  =  (Phasor::PRECISION_BITS - 2*TABLE_BITS);
  static const uint16_t BYTE_MASK = TABLE_SIZE-1;

  uint32_t LUT[TABLE_SIZE+1] = {0};

  inline uint32_t lookup( uint32_t phase ) {
    uint32_t index = (phase >> INDEX_SHIFT) & BYTE_MASK;
    uint64_t frac  = (phase >> FRAC_SHIFT ) & BYTE_MASK;
    uint32_t a = LUT[index];
    uint32_t b = LUT[index+1];
    //// linear interpolation: output = a + (b-a)*frac
    uint32_t output, diff, offset;
    if (b>=a) {      
      diff = b-a;
      offset = _uFixMul32(frac,diff);
      output = a + offset;
    } else {
      diff = a-b;
      offset = _uFixMul32(frac,diff);
      output = a - offset;
    }
    return output;
  }
  
  void init() {
    bool isEmpty = LUT[10] == 0;
    if (isEmpty) {
      int i=0;
      for (i=0; i<TABLE_SIZE; i++) { // TODO: round() or floor() ?
        LUT[i] = uint32_t( float(ULONG_MAX) * 0.5 * (1.f-cos(TWO_PI*(1.0/TABLE_SIZE)*i)) ); // unsigned Hann
      }
      LUT[TABLE_SIZE] = LUT[0]; // duplicate to prevent need for wrapping on linear interpolation
    }
  }

  inline int32_t makeSigned(uint32_t x) {
    const uint32_t OFFSET = (1<<31);
    return int32_t(x) + OFFSET;
  }

  void print() {
    Serial.print("unsigned sine[");
    Serial.print(TABLE_SIZE);
    Serial.print("] = ");
    for(int i=0; i < TABLE_SIZE; i++ ) {
      Serial.print( LUT[i] );
      Serial.print(", ");
    }
    Serial.println( LUT[TABLE_SIZE] );
    
    Serial.print("signed sine[");
    Serial.print(TABLE_SIZE);
    Serial.print("] = ");
    for(int i=0; i < TABLE_SIZE; i++ ) {
      Serial.print( makeSigned(LUT[i]) );
      Serial.print(", ");
    }
    Serial.println( LUT[TABLE_SIZE] );
  }
}

class SineOsc : public Phasor {
protected:

  uint32_t lastOut;
  uint8_t gain;

public:
  SineOsc( int srate ) : Phasor(float(srate)) {
    SineTable::init();
    setGain(1.0);
  }

  uint32_t process() {
    incPhase();
    return lastOut = SineTable::lookup(phase);
  }

  void setGain(float g) {
    gain = floor( float(UCHAR_MAX) * g );
  }
  uint32_t getLast() { return lastOut; }

  inline int32_t makeSigned(uint32_t x) {
    const uint32_t OFFSET = (1<<31);
//    return int32_t(x) + OFFSET;
    const uint32_t MAX_LONG = OFFSET - 1;
    return int32_t(x) - MAX_LONG;
  }

#define _uGainMul32(a,b) uint32_t( (uint64_t(a)*uint64_t(b) + (1L << 32) ) >> 32)

  int32_t getSigned() {
    uint32_t scaled;
    uint32_t gain32 = gain * 0x01010101;
    scaled = _uGainMul32( lastOut, gain32 );

    Serial.print("preGain: ");
    Serial.print(lastOut, HEX);
    Serial.print("  gain: ");
    Serial.print(gain32, HEX);
//    print64(gain32, HEX);
    Serial.print("  result: ");
    uint64_t result = uint64_t(lastOut)*uint64_t(gain32) + (1L << 31); 
    print64(result, HEX);
    Serial.print("  postGain: ");
    Serial.println(scaled, HEX);    
    return makeSigned( scaled );
  }

  void print64( uint64_t val, int k = DEC) {
    uint32_t val_hi = val >> 32;
    uint32_t val_lo = val & 0xFFFFFFFF;    
    Serial.print(val_hi, k);
    Serial.print(val_lo, k);
    if(k == HEX && val_lo == 0) {
      Serial.print("00000000");
    }
  }  

};


#endif
